#include <iostream>
#include <thread>
#include <chrono>
#include "services/DatabaseService.h"
#include "repositories/DeviceRepository.h"
#include "clients/LightningClient.h"

using namespace hms_firetv;

int main() {
    std::cout << "Volume Control Test - Living Room Fire TV" << std::endl;
    std::cout << "==========================================" << std::endl;

    // Connect to database
    DatabaseService::getInstance().initialize(
        "192.168.2.15", 5432, "firetv",
        "firetv_user", "firetv_postgres_2026_secure"
    );

    // Get living room device
    auto device = DeviceRepository::getInstance().getDeviceById("livingroom_colada");
    if (!device.has_value()) {
        std::cerr << "Device not found" << std::endl;
        return 1;
    }

    std::cout << "Device: " << device->name << " (" << device->ip_address << ")" << std::endl;

    // Create client
    LightningClient client(
        device->ip_address,
        device->api_key,
        device->client_token.value_or("")
    );

    // Volume Down
    std::cout << "\n[1] Volume DOWN..." << std::endl;
    auto down = client.sendNavigationCommand("volume_down");
    std::cout << "    Result: " << (down.success ? "SUCCESS" : "FAILED")
              << " (status=" << down.status_code << ", " << down.response_time_ms << "ms)" << std::endl;
    if (down.error.has_value()) {
        std::cout << "    Error: " << down.error.value() << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Volume Up
    std::cout << "\n[2] Volume UP..." << std::endl;
    auto up = client.sendNavigationCommand("volume_up");
    std::cout << "    Result: " << (up.success ? "SUCCESS" : "FAILED")
              << " (status=" << up.status_code << ", " << up.response_time_ms << "ms)" << std::endl;
    if (up.error.has_value()) {
        std::cout << "    Error: " << up.error.value() << std::endl;
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << "Done!" << std::endl;

    return 0;
}
