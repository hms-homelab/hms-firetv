#include <gtest/gtest.h>
#include "api/StatsController.h"
#include "services/DatabaseService.h"
#include <drogon/drogon.h>

using namespace hms_firetv;
using namespace drogon;

class StatsControllerTest : public ::testing::Test {
protected:
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
    }
};

TEST_F(StatsControllerTest, GetOverallStatsSuccess) {
    StatsController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getOverallStats(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    });

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_TRUE(response_data.isMember("devices"));
    EXPECT_TRUE(response_data.isMember("apps"));
    EXPECT_TRUE(response_data.isMember("commands"));
    EXPECT_TRUE(response_data["devices"].isMember("total"));
}

TEST_F(StatsControllerTest, GetDeviceStatsSuccess) {
    StatsController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getDeviceStats(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    });

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_TRUE(response_data.isMember("count"));
    EXPECT_TRUE(response_data.isMember("devices"));
    EXPECT_TRUE(response_data["devices"].isArray());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
