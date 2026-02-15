#include <iostream>
#include <thread>
#include <chrono>
#include "services/DatabaseService.h"
#include "repositories/DeviceRepository.h"
#include "clients/LightningClient.h"

using namespace hms_firetv;

int main() {
    std::cout << "Testing Living Room Fire TV" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Connect to database
        DatabaseService::getInstance().initialize(
            "192.168.2.15", 5432, "firetv",
            "firetv_user", "firetv_postgres_2026_secure"
        );

        // Get living room device
        auto device = DeviceRepository::getInstance().getDeviceById("livingroom_colada");

        if (!device.has_value()) {
            std::cerr << "✗ Living room device not found" << std::endl;
            return 1;
        }

        std::cout << "\nDevice: " << device->name << std::endl;
        std::cout << "IP: " << device->ip_address << std::endl;
        std::cout << "Status: " << device->status << std::endl;
        std::cout << "Paired: " << (device->isPaired() ? "yes" : "no") << std::endl;

        // Create Lightning client
        LightningClient client(
            device->ip_address,
            device->api_key,
            device->client_token.value_or("")
        );

        // Check device state
        std::cout << "\n[Health Check]" << std::endl;
        bool wake_responds = client.healthCheck();
        bool api_responds = client.isLightningApiAvailable();

        std::cout << "Wake endpoint: " << (wake_responds ? "responding" : "not responding") << std::endl;
        std::cout << "Lightning API: " << (api_responds ? "responding" : "not responding") << std::endl;

        if (!wake_responds) {
            std::cerr << "\n✗ Device is offline or unreachable" << std::endl;
            return 1;
        }

        // Wake if needed
        if (!api_responds) {
            std::cout << "\n[Wake Device]" << std::endl;
            std::cout << "Device is in standby, waking up..." << std::endl;
            client.wakeDevice();
            std::this_thread::sleep_for(std::chrono::seconds(3));

            api_responds = client.isLightningApiAvailable();
            if (!api_responds) {
                std::cerr << "✗ Failed to wake device" << std::endl;
                return 1;
            }
            std::cout << "✓ Device is now awake" << std::endl;
        }

        // Send commands
        std::cout << "\n[Sending Commands]" << std::endl;

        // Home button
        std::cout << "\n1. Pressing HOME button..." << std::endl;
        auto result1 = client.home();
        std::cout << "   Status: " << result1.status_code
                  << " - " << (result1.success ? "SUCCESS" : "FAILED")
                  << " (" << result1.response_time_ms << "ms)" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        // Volume Down (try as navigation command)
        std::cout << "\n2. Testing VOLUME_DOWN..." << std::endl;
        auto result2 = client.sendNavigationCommand("volume_down");
        std::cout << "   Status: " << result2.status_code
                  << " - " << (result2.success ? "SUCCESS" : "FAILED")
                  << " (" << result2.response_time_ms << "ms)" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Volume Up (try as navigation command)
        std::cout << "\n3. Testing VOLUME_UP..." << std::endl;
        auto result3 = client.sendNavigationCommand("volume_up");
        std::cout << "   Status: " << result3.status_code
                  << " - " << (result3.success ? "SUCCESS" : "FAILED")
                  << " (" << result3.response_time_ms << "ms)" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // D-pad Down
        std::cout << "\n4. Pressing DPAD_DOWN..." << std::endl;
        auto result4 = client.dpadDown();
        std::cout << "   Status: " << result4.status_code
                  << " - " << (result4.success ? "SUCCESS" : "FAILED")
                  << " (" << result4.response_time_ms << "ms)" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // D-pad Up
        std::cout << "\n5. Pressing DPAD_UP..." << std::endl;
        auto result5 = client.dpadUp();
        std::cout << "   Status: " << result5.status_code
                  << " - " << (result5.success ? "SUCCESS" : "FAILED")
                  << " (" << result5.response_time_ms << "ms)" << std::endl;

        // Update database
        if (result1.success) {
            DeviceRepository::getInstance().updateLastSeen("livingroom_colada", "online");
            std::cout << "\n✓ Database updated with last seen timestamp" << std::endl;
        }

        std::cout << "\n==========================================" << std::endl;
        std::cout << "Test complete!" << std::endl;

        // Summary
        std::cout << "\nNOTE: Volume control may not work via Lightning API" << std::endl;
        std::cout << "Fire TV volume is typically controlled via:" << std::endl;
        std::cout << "  - HDMI CEC (TV controls volume)" << std::endl;
        std::cout << "  - IR remote (infrared signals)" << std::endl;
        std::cout << "  - Bluetooth remote" << std::endl;
        std::cout << "\nNavigation and media commands should work!" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << std::endl;
        return 1;
    }
}
