#include <gtest/gtest.h>
#include "api/PairingController.h"
#include "repositories/DeviceRepository.h"
#include "services/DatabaseService.h"
#include <drogon/drogon.h>

using namespace hms_firetv;
using namespace drogon;

class PairingControllerTest : public ::testing::Test {
protected:
    std::string test_device_id = "unittest_pair_device";

    void SetUp() override {
        try {
            DatabaseService::getInstance().initialize(
                "localhost", 5432, "firetv_test", "maestro", "maestro_postgres_2026_secure"
            );
        } catch (...) {
            DatabaseService::getInstance().initialize(
                "192.168.2.15", 5432, "firetv", "maestro", "maestro_postgres_2026_secure"
            );
        }

        // Create test device
        Device device;
        device.device_id = test_device_id;
        device.name = "Unit Test Pairing Device";
        device.ip_address = "192.168.1.202";
        device.api_key = "test_key";
        device.status = "offline";
        device.adb_enabled = false;

        DeviceRepository::getInstance().createDevice(device);
    }

    void TearDown() override {
        try {
            std::string cleanup = "DELETE FROM fire_tv_devices WHERE device_id LIKE 'unittest_%'";
            DatabaseService::getInstance().executeQuery(cleanup);
        } catch (...) {}
    }
};

// Test: Get pairing status (unpaired)
TEST_F(PairingControllerTest, GetPairingStatusUnpaired) {
    PairingController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getPairingStatus(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id);

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_EQ(response_data["device_id"].asString(), test_device_id);
    EXPECT_FALSE(response_data["is_paired"].asBool());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
