#include <gtest/gtest.h>
#include "api/StatsController.h"
#include "database/SQLiteDatabase.h"
#include <drogon/drogon.h>
#include <memory>

using namespace hms_firetv;
using namespace drogon;

class StatsControllerTest : public ::testing::Test {
protected:
    std::shared_ptr<SQLiteDatabase> db;

    void SetUp() override {
        db = std::make_shared<SQLiteDatabase>(":memory:");
        db->connect();
        StatsController::setDatabase(db);
    }
};

TEST_F(StatsControllerTest, GetOverallStatsSuccess) {
    StatsController controller;
    bool called = false;
    Json::Value data;

    auto req = HttpRequest::newHttpRequest();
    controller.getOverallStats(req, [&](const HttpResponsePtr& resp) {
        called = true;
        Json::Reader reader;
        reader.parse(std::string(resp->getBody()), data);
    });

    EXPECT_TRUE(called);
    EXPECT_TRUE(data["success"].asBool());
    EXPECT_TRUE(data.isMember("devices"));
    EXPECT_TRUE(data.isMember("apps"));
    EXPECT_TRUE(data.isMember("commands"));
    EXPECT_TRUE(data["devices"].isMember("total"));
}

TEST_F(StatsControllerTest, GetDeviceStatsSuccess) {
    StatsController controller;
    bool called = false;
    Json::Value data;

    auto req = HttpRequest::newHttpRequest();
    controller.getDeviceStats(req, [&](const HttpResponsePtr& resp) {
        called = true;
        Json::Reader reader;
        reader.parse(std::string(resp->getBody()), data);
    });

    EXPECT_TRUE(called);
    EXPECT_TRUE(data["success"].asBool());
    EXPECT_TRUE(data.isMember("devices"));
    EXPECT_TRUE(data.isMember("count"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
