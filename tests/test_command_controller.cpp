#include <gtest/gtest.h>
#include "api/CommandController.h"
#include "repositories/DeviceRepository.h"
#include "services/DatabaseService.h"
#include <drogon/drogon.h>
#include <cstdlib>

namespace {
/** Command history lives in PostgreSQL, so this test needs a real server and
 *  is opt-in: set TEST_PG_HOST (and TEST_PG_PORT / TEST_PG_DB / TEST_PG_USER /
 *  TEST_PG_PASSWORD as needed). Use a throwaway test database - TearDown
 *  deletes the unittest_% devices it created. There is deliberately no
 *  fallback host: an unset variable must never land on a real database. */
inline const char* envOr(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : fallback;
}
}  // namespace


using namespace hms_firetv;
using namespace drogon;

class CommandControllerTest : public ::testing::Test {
protected:
    std::string test_device_id = "unittest_cmd_device";
    bool live_ = false;

    void SetUp() override {
        const std::string host = envOr("TEST_PG_HOST", "");
        if (host.empty()) {
            GTEST_SKIP() << "TEST_PG_HOST not set; command history test skipped";
        }
        try {
            DatabaseService::getInstance().initialize(
                host, std::atoi(envOr("TEST_PG_PORT", "5432")),
                envOr("TEST_PG_DB", "firetv_test"), envOr("TEST_PG_USER", "hms"),
                envOr("TEST_PG_PASSWORD", "")
            );
            live_ = true;
        } catch (const std::exception& e) {
            GTEST_SKIP() << "test database unavailable: " << e.what();
        }

        // Create test device
        Device device;
        device.device_id = test_device_id;
        device.name = "Unit Test Command Device";
        device.ip_address = "192.168.1.201";
        device.api_key = "test_key";
        device.status = "online";

        DeviceRepository::getInstance().createDevice(device);
    }

    void TearDown() override {
        if (!live_) return;
        try {
            std::string cleanup = "DELETE FROM fire_tv_devices WHERE device_id LIKE 'unittest_%'";
            DatabaseService::getInstance().executeQuery(cleanup);
        } catch (...) {}
    }
};

// Test: Get command history
TEST_F(CommandControllerTest, GetHistoryReturnsValidResponse) {
    CommandController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getHistory(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id);

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_TRUE(response_data.isMember("history"));
    EXPECT_TRUE(response_data["history"].isArray());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
