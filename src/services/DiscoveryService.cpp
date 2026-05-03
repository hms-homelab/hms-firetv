#include "services/DiscoveryService.h"
#include "services/DatabaseService.h"
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
                   bool lightning_open = tcpProbe(ip, 8080);
                   return DiscoveredDevice{ip, "", true, lightning_open};
                }));

            if (!tcpProbe(ip, 8009)) continue;

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

    bool DiscoveryService::probeLightningWithToken(const std::string &ip,
                                                   const std::string &api_key,
                                                   const std::string &client_token) {
        if (client_token.empty()) return false;

        CURL *curl = curl_easy_init();
        if (!curl) return false;

        std::string url = "https://" + ip + ":8080/v1/FireTV";
        std::string response;

        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, ("X-Api-Key: " + api_key).c_str());
        headers = curl_slist_append(headers, ("X-Client-Token: " + client_token).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return res == CURLE_OK && http_code == 200;
    }

    void DiscoveryService::matchAndUpdate(const std::vector<DiscoveredDevice> &discovered) {
        auto devices = DeviceRepository::getInstance().getAllDevices();

        for (const auto &device: devices) {
            // Check if device is still at its known IP
            bool found_at_current = false;
            for (const auto &d: discovered) {
                if (d.ip_address == device.ip_address) {
                    found_at_current = true;
                    break;
                }
            }

            if (found_at_current) {
                DeviceRepository::getInstance().updateLastSeen(device.device_id, "online");
                continue;
            }

            // Device not at known IP — try to find it at a new IP using its token
            if (!device.client_token.has_value() || device.client_token->empty()) {
                continue;
            }

            for (const auto &d: discovered) {
                if (!d.has_lightning) continue;

                // Skip IPs already assigned to other devices
                bool ip_taken = false;
                for (const auto &other: devices) {
                    if (other.device_id != device.device_id &&
                        other.ip_address == d.ip_address) {
                        ip_taken = true;
                        break;
                    }
                }
                if (ip_taken) continue;

                if (probeLightningWithToken(d.ip_address, device.api_key,
                                            device.client_token.value())) {
                    std::cout << "[DiscoveryService] Device '" << device.device_id
                            << "' moved: " << device.ip_address
                            << " -> " << d.ip_address << "\n";

                    std::ostringstream query;
                    query << "UPDATE fire_tv_devices SET "
                            << "ip_address = '" << d.ip_address << "', "
                            << "last_seen_at = NOW(), "
                            << "status = 'online', "
                            << "updated_at = NOW() "
                            << "WHERE device_id = '" << device.device_id << "'";
                    DatabaseService::getInstance().executeCommand(query.str());

                    if (mqtt_client_) {
                        mqtt_client_->publishAvailability(device.device_id, true);
                    }

                    break;
                }
            }
        }
    }
} // namespace hms_firetv
