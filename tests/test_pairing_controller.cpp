#include <gtest/gtest.h>
#include "api/PairingController.h"
#include "repositories/DeviceRepository.h"
#include "database/SQLiteDatabase.h"
#include <memory>
#include <drogon/drogon.h>

using namespace hms_firetv;
using namespace drogon;

class PairingControllerTest : public ::testing::Test {
protected:
    std::shared_ptr<SQLiteDatabase> db;
    std::string test_device_id = "unittest_pair_device";

    void SetUp() override {
        // An in-memory SQLite database per test, wired into the repositories.
        //
        // These tests used to call DatabaseService::initialize() and nothing
        // else, which left DeviceRepository::db_ null - every repository call
        // then short-circuited to nullopt before touching a database, so the
        // controllers answered 500 and the assertions failed. They also fell
        // back to the PRODUCTION database and ran DELETEs against it. An
        // in-memory database fixes both: the repositories are actually wired,
        // and no test can reach real data.
        db = std::make_shared<SQLiteDatabase>(":memory:");
        db->connect();
        DeviceRepository::setDatabase(db);

        // Create test device
        Device device;
        device.device_id = test_device_id;
        device.name = "Unit Test Pairing Device";
        device.ip_address = "192.168.1.202";
        device.api_key = "test_key";
        device.status = "offline";

        DeviceRepository::getInstance().createDevice(device);
    }

    void TearDown() override {
        // Nothing to clean up - the in-memory database dies with the test.
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
