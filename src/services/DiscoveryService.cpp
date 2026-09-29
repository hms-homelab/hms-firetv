#include <thread>
#include "services/DiscoveryService.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <future>
#include <optional>
#include <netdb.h>
#include <netdb.h>

namespace hms_firetv {

    static DiscoveryService* s_instance = nullptr;

    void DiscoveryService::initialize(const std::string& subnet_prefix, int scan_interval_seconds) {
        static DiscoveryService instance(subnet_prefix, scan_interval_seconds);
        s_instance = &instance;
    }

    DiscoveryService& DiscoveryService::getInstance() {
        return *s_instance;
    }

    DiscoveryService::DiscoveryService(const std::string &subnet_prefix,
                                       int scan_interval_seconds)
        : subnet_prefix_(subnet_prefix),
          scan_interval_seconds_(scan_interval_seconds) {
        std::cout << "[DiscoveryService] Initialized for subnet " << subnet_prefix
                << ".0/24, interval=" << scan_interval_seconds << "s\n";
    }

    DiscoveryService::~DiscoveryService() {
        stop();
    }

    void DiscoveryService::start() {
        if (running_.load()) return;
        running_.store(true);
        scan_thread_ = std::thread(&DiscoveryService::scanLoop, this);
        std::cout << "[DiscoveryService] Started\n";
    }

    void DiscoveryService::stop() {
        running_.store(false);
        if (scan_thread_.joinable()) {
            scan_thread_.join();
        }
        std::cout << "[DiscoveryService] Stopped\n";
    }

    void DiscoveryService::setMqttClient(std::shared_ptr<MQTTClient> mqtt_client) {
        mqtt_client_ = std::move(mqtt_client);
    }

    void DiscoveryService::scanLoop() {
        // Initial delay — let services finish starting
        for (int i = 0; i < 30 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (running_.load()) {
            try {
                runOnce();
            } catch (const std::exception &e) {
                std::cerr << "[DiscoveryService] Scan error: " << e.what() << "\n";
            }

            for (int i = 0; i < scan_interval_seconds_ && running_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    void DiscoveryService::runOnce() {
        std::cout << "[DiscoveryService] Starting subnet scan...\n";
        auto discovered = scanSubnet();
        std::cout << "[DiscoveryService] Found " << discovered.size()
                << " devices with port 8009 open\n";

        if (!discovered.empty()) {
            matchAndUpdate(discovered);
        }
    }

    std::vector<DiscoveredDevice> DiscoveryService::getUnregisteredDevices() {
        std::cout << "[DiscoveryService] Starting subnet scan...\n";
        std::vector<DiscoveredDevice> unregistered = scanSubnet();
        auto registeredDevices = DeviceRepository::getInstance().getAllDevices(); // Ensure devices are loaded
        unregistered.erase(
            std::remove_if(unregistered.begin(), unregistered.end(),
                           [&registeredDevices](const DiscoveredDevice &d) {
                               return std::any_of(registeredDevices.begin(), registeredDevices.end(),
                                                  [&d](const Device &rd) {
                                                      return rd.ip_address == d.ip_address;
                                                  });
                           }),
            unregistered.end()
        );

        for (auto& d : unregistered) {
            struct sockaddr_in sa{};
            sa.sin_family = AF_INET;
            inet_pton(AF_INET, d.ip_address.c_str(), &sa.sin_addr);
            char host[256] = "";
            if (getnameinfo((struct sockaddr*)&sa, sizeof(sa),
                            host, sizeof(host), nullptr, 0, NI_NOFQDN) == 0) {
                std::string resolved(host);
                if (resolved != d.ip_address) {
                    d.hostname = resolved;
                }
            }
        }

        return unregistered;
    }

    static bool tcpProbe(const std::string &ip, int port, int timeout_ms = 500) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return false;

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        int ret = connect(sock, (struct sockaddr *) &addr, sizeof(addr));
        if (ret == 0) {
            close(sock);
            return true;
        }

        if (errno != EINPROGRESS) {
            close(sock);
            return false;
        }

        struct pollfd pfd{};
        pfd.fd = sock;
        pfd.events = POLLOUT;

        ret = poll(&pfd, 1, timeout_ms);
        if (ret > 0 && (pfd.revents & POLLOUT)) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
            close(sock);
            return err == 0;
        }

        close(sock);
        return false;
    }

    std::vector<DiscoveredDevice> DiscoveryService::scanSubnet() {
        std::vector<DiscoveredDevice> results;
        std::vector<std::future<std::optional<DiscoveredDevice>>> futures;
        for (int i = 1; i < 255; ++i) {
            if (!running_.load()) break;

            std::string ip = subnet_prefix_ + "." + std::to_string(i);
            futures.push_back(
                std::async(std::launch::async,[this, ip]()
                    -> std::optional<DiscoveredDevice> {
                   if (!running_.load()) return std::nullopt;
                   if (!tcpProbe(ip, 8009)) return std::nullopt;
                   if (!probeWakeEndpoint(ip)) return std::nullopt;
                   /* has_lightning USED TO BE tcpProbe(ip, 8080), and that made
                    * self-healing impossible: a SLEEPING Fire TV does not listen
                    * on 8080, which is exactly when a moved device needs to be
                    * rediscovered. matchAndUpdate() gates on this flag, so every
                    * candidate was skipped and an IP change was never applied.
                    * Reaching this line already proves it is a Fire TV: 8009 is
                    * open AND the wake endpoint answered, both of which work
                    * while asleep. */
                   return DiscoveredDevice{ip, "", true, true, fetchDialUdn(ip)};
                }));
        }
        for (auto& f: futures) {
            auto r = f.get();
            if (r) results.push_back(*r);
        }
        return results;
    }

    size_t DiscoveryService::WriteCallback(void *contents, size_t size,
                                           size_t nmemb, void *userp) {
        size_t total = size * nmemb;
        static_cast<std::string *>(userp)->append(static_cast<char *>(contents), total);
        return total;
    }

    std::string DiscoveryService::parseDialUdn(const std::string &dd_xml) {
        const std::string open = "<UDN>", close = "</UDN>";
        auto start = dd_xml.find(open);
        if (start == std::string::npos) return "";
        start += open.size();
        auto end = dd_xml.find(close, start);
        if (end == std::string::npos) return "";
        return dd_xml.substr(start, end - start);
    }

    /* Identity WITHOUT waking. Discovery used to POST the wake endpoint on
     * 8009 and then test the pairing token on 8080, which only answers awake.
     * That woke every candidate it tested, and a Fire TV that wakes also
     * powers its TV on over HDMI-CEC: a Fire TV that was not the missing
     * device got woken every scan and kept turning the living room TV back
     * on. The DIAL description on 60000 answers while asleep and its UDN is
     * unique per device, so nothing needs waking to tell devices apart. */
    std::string DiscoveryService::fetchDialUdn(const std::string &ip) {
        CURL *curl = curl_easy_init();
        if (!curl) return "";

        std::string url = "http://" + ip + ":60000/dd.xml";
        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK || http_code != 200) return "";
        return parseDialUdn(response);
    }

    bool DiscoveryService::probeWakeEndpoint(const std::string &ip) {
        CURL *curl = curl_easy_init();
        if (!curl) return false;

        std::string url = "http://" + ip + ":8009/apps/FireTVRemote";
        std::string response;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        return res == CURLE_OK && http_code > 0;
    }

    /* Nothing here may wake a device (see fetchDialUdn). A device is known by
     * its DIAL UDN, learned while it sits at its registered IP; when it goes
     * missing, the UDN finds it at its new IP. A device whose UDN has not been
     * learned yet is left alone: guessing meant waking strangers. */
    void DiscoveryService::matchAndUpdate(const std::vector<DiscoveredDevice> &discovered) {
        auto &repo = DeviceRepository::getInstance();
        auto devices = repo.getAllDevices();

        for (const auto &device: devices) {
            const std::string known_udn = device.dial_udn.value_or("");

            const DiscoveredDevice *at_ip = nullptr;
            for (const auto &d: discovered) {
                if (d.ip_address == device.ip_address) {
                    at_ip = &d;
                    break;
                }
            }

            // Something answers at the registered IP. It is this device unless
            // both UDNs are known and differ: then another device took the IP.
            if (at_ip && (known_udn.empty() || at_ip->dial_udn.empty() ||
                          at_ip->dial_udn == known_udn)) {
                repo.updateLastSeen(device.device_id, "online");
                if (known_udn.empty() && !at_ip->dial_udn.empty()) {
                    repo.setDialUdn(device.device_id, at_ip->dial_udn);
                    std::cout << "[DiscoveryService] '" << device.device_id
                            << "' is " << at_ip->dial_udn << "\n";
                }
                continue;
            }

            if (known_udn.empty()) {
                std::cout << "[DiscoveryService] '" << device.device_id
                        << "' not at " << device.ip_address
                        << " and its UDN is not known yet; not searching\n";
                continue;
            }

            for (const auto &d: discovered) {
                if (d.dial_udn != known_udn) continue;

                std::cout << "[DiscoveryService] Device '" << device.device_id
                        << "' moved: " << device.ip_address
                        << " -> " << d.ip_address << "\n";

                Device moved = device;
                moved.ip_address = d.ip_address;
                moved.status = "online";
                repo.updateDevice(moved);
                repo.updateLastSeen(device.device_id, "online");

                if (mqtt_client_) {
                    mqtt_client_->publishAvailability(device.device_id, true);
                }
                break;
            }
        }
    }
} // namespace hms_firetv
