#pragma once

#include "repositories/DeviceRepository.h"
#include "mqtt/MQTTClient.h"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <curl/curl.h>

namespace hms_firetv {

struct DiscoveredDevice {
    std::string ip_address;
    bool has_wake_port;
    bool has_lightning;
};

class DiscoveryService {
public:
    DiscoveryService(const std::string& subnet_prefix,
                     int scan_interval_seconds = 300);
    ~DiscoveryService();

    DiscoveryService(const DiscoveryService&) = delete;
    DiscoveryService& operator=(const DiscoveryService&) = delete;

    void start();
    void stop();

    void setMqttClient(std::shared_ptr<MQTTClient> mqtt_client);

    void runOnce();

private:
    void scanLoop();
    std::vector<DiscoveredDevice> scanSubnet();
    bool probeWakeEndpoint(const std::string& ip);
    bool probeLightningWithToken(const std::string& ip,
                                 const std::string& api_key,
                                 const std::string& client_token);
    void matchAndUpdate(const std::vector<DiscoveredDevice>& discovered);

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    std::string subnet_prefix_;
    int scan_interval_seconds_;
    std::atomic<bool> running_{false};
    std::thread scan_thread_;
    std::shared_ptr<MQTTClient> mqtt_client_;
};

} // namespace hms_firetv
