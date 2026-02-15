#include <iostream>
#include "services/DatabaseService.h"
#include "repositories/DeviceRepository.h"
#include "models/Device.h"

using namespace hms_firetv;

int main() {
    std::cout << "Testing DeviceRepository..." << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Initialize database connection
        DatabaseService::getInstance().initialize(
            "192.168.2.15", 5432, "firetv",
            "firetv_user", "firetv_postgres_2026_secure"
        );
        std::cout << "✓ Database connected" << std::endl;

        // Test 1: Get all devices
        std::cout << "\n1. Testing getAllDevices()..." << std::endl;
        auto devices = DeviceRepository::getInstance().getAllDevices();
        std::cout << "   Found " << devices.size() << " devices in database" << std::endl;
        for (const auto& device : devices) {
            std::cout << "   - " << device.device_id << " (" << device.name << ") - "
                      << device.status << std::endl;
        }

        // Test 2: Check if test device exists
        std::cout << "\n2. Checking for test device 'test_cpp_device'..." << std::endl;
        bool exists = DeviceRepository::getInstance().deviceExists("test_cpp_device");
        std::cout << "   Device exists: " << (exists ? "yes" : "no") << std::endl;

        // Test 3: Create or update test device
        if (!exists) {
            std::cout << "\n3. Creating test device..." << std::endl;
            Device test_device;
            test_device.device_id = "test_cpp_device";
            test_device.name = "C++ Test Device";
            test_device.ip_address = "192.168.2.99";
            test_device.api_key = "0987654321";
            test_device.status = "offline";
            test_device.adb_enabled = true;

            auto created = DeviceRepository::getInstance().createDevice(test_device);
            if (created.has_value()) {
                std::cout << "   ✓ Device created with ID: " << created->id << std::endl;
                std::cout << "   Device JSON: " << created->toJson().toStyledString() << std::endl;
            } else {
                std::cout << "   ✗ Failed to create device" << std::endl;
                return 1;
            }
        } else {
            std::cout << "\n3. Test device already exists" << std::endl;
        }

        // Test 4: Get device by ID
        std::cout << "\n4. Testing getDeviceById()..." << std::endl;
        auto device = DeviceRepository::getInstance().getDeviceById("test_cpp_device");
        if (device.has_value()) {
            std::cout << "   ✓ Device retrieved: " << device->name << std::endl;
            std::cout << "   Status: " << device->status << std::endl;
            std::cout << "   IP: " << device->ip_address << std::endl;
        } else {
            std::cout << "   ✗ Device not found" << std::endl;
        }

        // Test 5: Update last seen
        std::cout << "\n5. Testing updateLastSeen()..." << std::endl;
        bool updated = DeviceRepository::getInstance().updateLastSeen("test_cpp_device", "online");
        std::cout << "   Update result: " << (updated ? "success" : "failed") << std::endl;

        // Test 6: Get devices by status
        std::cout << "\n6. Testing getDevicesByStatus('online')..." << std::endl;
        auto online_devices = DeviceRepository::getInstance().getDevicesByStatus("online");
        std::cout << "   Found " << online_devices.size() << " online devices" << std::endl;
        for (const auto& d : online_devices) {
            std::cout << "   - " << d.device_id << std::endl;
        }

        std::cout << "\n==========================================" << std::endl;
        std::cout << "All tests completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
