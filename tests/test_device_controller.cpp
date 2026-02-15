#include <gtest/gtest.h>
#include "api/DeviceController.h"
#include "repositories/DeviceRepository.h"
#include "services/DatabaseService.h"
#include <drogon/drogon.h>

using namespace hms_firetv;
using namespace drogon;

class DeviceControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize database for testing
        try {
            DatabaseService::getInstance().initialize(
                "localhost", 5432, "firetv_test", "maestro", "maestro_postgres_2026_secure"
            );
        } catch (...) {
            // Use production DB if test DB doesn't exist
            DatabaseService::getInstance().initialize(
                "192.168.2.15", 5432, "firetv", "maestro", "maestro_postgres_2026_secure"
            );
        }
    }

    void TearDown() override {
        // Cleanup test devices
        try {
            std::string cleanup = "DELETE FROM fire_tv_devices WHERE device_id LIKE 'unittest_%'";
            DatabaseService::getInstance().executeQuery(cleanup);
        } catch (...) {}
    }
};

// Test: List devices endpoint
TEST_F(DeviceControllerTest, ListDevicesReturnsValidResponse) {
    DeviceController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.listDevices(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    });

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data.isMember("success"));
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_TRUE(response_data.isMember("count"));
    EXPECT_TRUE(response_data.isMember("devices"));
    EXPECT_TRUE(response_data["devices"].isArray());
}

// Test: Create device
TEST_F(DeviceControllerTest, CreateDeviceSuccess) {
    DeviceController controller;

    Json::Value device_data;
    device_data["device_id"] = "unittest_device_1";
    device_data["name"] = "Unit Test Device";
    device_data["ip_address"] = "192.168.1.100";
    device_data["api_key"] = "test_key";

    bool callback_called = false;
    Json::Value response_data;
    HttpStatusCode status_code;

    auto req = HttpRequest::newHttpJsonRequest(device_data);

    controller.createDevice(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        status_code = resp->getStatusCode();
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    });

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(status_code, k201Created);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_EQ(response_data["device"]["device_id"].asString(), "unittest_device_1");
}

// Test: Get device by ID
TEST_F(DeviceControllerTest, GetDeviceByIdSuccess) {
    // First create a device
    Device device;
    device.device_id = "unittest_device_2";
    device.name = "Test Device 2";
    device.ip_address = "192.168.1.101";
    device.api_key = "test_key";
    device.status = "offline";
    device.adb_enabled = false;

    auto created = DeviceRepository::getInstance().createDevice(device);
    ASSERT_TRUE(created.has_value());

    // Now get it via controller
    DeviceController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getDeviceById(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, "unittest_device_2");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_EQ(response_data["device"]["device_id"].asString(), "unittest_device_2");
}

// Test: Update device
TEST_F(DeviceControllerTest, UpdateDeviceSuccess) {
    // Create device first
    Device device;
    device.device_id = "unittest_device_3";
    device.name = "Test Device 3";
    device.ip_address = "192.168.1.102";
    device.api_key = "test_key";
    device.status = "offline";
    device.adb_enabled = false;

    DeviceRepository::getInstance().createDevice(device);

    // Update it
    DeviceController controller;

    Json::Value update_data;
    update_data["name"] = "Updated Test Device 3";
    update_data["status"] = "online";

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpJsonRequest(update_data);

    controller.updateDevice(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, "unittest_device_3");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_EQ(response_data["device"]["name"].asString(), "Updated Test Device 3");
    EXPECT_EQ(response_data["device"]["status"].asString(), "online");
}

// Test: Delete device
TEST_F(DeviceControllerTest, DeleteDeviceSuccess) {
    // Create device first
    Device device;
    device.device_id = "unittest_device_4";
    device.name = "Test Device 4";
    device.ip_address = "192.168.1.103";
    device.api_key = "test_key";
    device.status = "offline";
    device.adb_enabled = false;

    DeviceRepository::getInstance().createDevice(device);

    // Delete it
    DeviceController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.deleteDevice(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, "unittest_device_4");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());

    // Verify it's deleted
    auto deleted_device = DeviceRepository::getInstance().getDeviceById("unittest_device_4");
    EXPECT_FALSE(deleted_device.has_value());
}

// Test: Get device status
TEST_F(DeviceControllerTest, GetDeviceStatusSuccess) {
    // Create device first
    Device device;
    device.device_id = "unittest_device_5";
    device.name = "Test Device 5";
    device.ip_address = "192.168.1.104";
    device.api_key = "test_key";
    device.status = "online";
    device.adb_enabled = true;

    DeviceRepository::getInstance().createDevice(device);

    // Get status
    DeviceController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getDeviceStatus(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, "unittest_device_5");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_EQ(response_data["device_id"].asString(), "unittest_device_5");
    EXPECT_EQ(response_data["status"].asString(), "online");
    EXPECT_TRUE(response_data["adb_enabled"].asBool());
}

// Test: Get non-existent device returns 404
TEST_F(DeviceControllerTest, GetNonExistentDeviceReturns404) {
    DeviceController controller;

    bool callback_called = false;
    HttpStatusCode status_code;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getDeviceById(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        status_code = resp->getStatusCode();
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, "nonexistent_device_xyz");

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(status_code, k404NotFound);
    EXPECT_FALSE(response_data["success"].asBool());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
