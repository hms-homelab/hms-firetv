#include <gtest/gtest.h>
#include "api/AppsController.h"
#include "repositories/AppsRepository.h"
#include "repositories/DeviceRepository.h"
#include "services/DatabaseService.h"
#include <drogon/drogon.h>

using namespace hms_firetv;
using namespace drogon;

class AppsControllerTest : public ::testing::Test {
protected:
    std::string test_device_id = "unittest_apps_device";

    void SetUp() override {
        // Initialize database
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
        device.name = "Unit Test Apps Device";
        device.ip_address = "192.168.1.200";
        device.api_key = "test_key";
        device.status = "online";
        device.adb_enabled = false;

        DeviceRepository::getInstance().createDevice(device);
    }

    void TearDown() override {
        // Cleanup
        try {
            std::string cleanup_devices = "DELETE FROM fire_tv_devices WHERE device_id LIKE 'unittest_%'";
            DatabaseService::getInstance().executeQuery(cleanup_devices);

            std::string cleanup_apps = "DELETE FROM device_apps WHERE device_id LIKE 'unittest_%'";
            DatabaseService::getInstance().executeQuery(cleanup_apps);
        } catch (...) {}
    }
};

// Test: List apps for device
TEST_F(AppsControllerTest, ListAppsReturnsValidResponse) {
    AppsController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.listApps(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id);

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_TRUE(response_data.isMember("count"));
    EXPECT_TRUE(response_data.isMember("apps"));
    EXPECT_TRUE(response_data["apps"].isArray());
}

// Test: Add app to device
TEST_F(AppsControllerTest, AddAppSuccess) {
    AppsController controller;

    Json::Value app_data;
    app_data["package"] = "com.test.app";
    app_data["name"] = "Test App";

    bool callback_called = false;
    Json::Value response_data;
    HttpStatusCode status_code;

    auto req = HttpRequest::newHttpJsonRequest(app_data);

    controller.addApp(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        status_code = resp->getStatusCode();
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(status_code, k201Created);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_EQ(response_data["app"]["package"].asString(), "com.test.app");
    EXPECT_EQ(response_data["app"]["name"].asString(), "Test App");
}

// Test: Update app
TEST_F(AppsControllerTest, UpdateAppSuccess) {
    // Add app first
    DeviceApp app;
    app.device_id = test_device_id;
    app.package_name = "com.test.updateapp";
    app.app_name = "Update Test App";
    app.icon_url = "";
    app.is_favorite = false;
    app.sort_order = 0;

    AppsRepository::getInstance().addApp(app);

    // Update it
    AppsController controller;

    Json::Value update_data;
    update_data["name"] = "Updated Test App";
    update_data["is_favorite"] = true;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpJsonRequest(update_data);

    controller.updateApp(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id, "com.test.updateapp");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_EQ(response_data["app"]["name"].asString(), "Updated Test App");
}

// Test: Delete app
TEST_F(AppsControllerTest, DeleteAppSuccess) {
    // Add app first
    DeviceApp app;
    app.device_id = test_device_id;
    app.package_name = "com.test.deleteapp";
    app.app_name = "Delete Test App";
    app.icon_url = "";
    app.is_favorite = false;
    app.sort_order = 0;

    AppsRepository::getInstance().addApp(app);

    // Delete it
    AppsController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.deleteApp(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id, "com.test.deleteapp");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());

    // Verify it's deleted
    auto deleted = AppsRepository::getInstance().getApp(test_device_id, "com.test.deleteapp");
    EXPECT_FALSE(deleted.has_value());
}

// Test: Toggle favorite
TEST_F(AppsControllerTest, ToggleFavoriteSuccess) {
    // Add app first
    DeviceApp app;
    app.device_id = test_device_id;
    app.package_name = "com.test.favoriteapp";
    app.app_name = "Favorite Test App";
    app.icon_url = "";
    app.is_favorite = false;
    app.sort_order = 0;

    AppsRepository::getInstance().addApp(app);

    // Toggle favorite
    AppsController controller;

    Json::Value favorite_data;
    favorite_data["is_favorite"] = true;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpJsonRequest(favorite_data);

    controller.toggleFavorite(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id, "com.test.favoriteapp");

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());

    // Verify it's favorited
    auto updated = AppsRepository::getInstance().getApp(test_device_id, "com.test.favoriteapp");
    ASSERT_TRUE(updated.has_value());
    EXPECT_TRUE(updated->is_favorite);
}

// Test: Get popular apps
TEST_F(AppsControllerTest, GetPopularAppsSuccess) {
    AppsController controller;

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpRequest();

    controller.getPopularApps(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    });

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_TRUE(response_data.isMember("apps"));
    EXPECT_TRUE(response_data["apps"].isArray());
    EXPECT_GT(response_data["apps"].size(), 0);  // Should have popular apps
}

// Test: Bulk add apps
TEST_F(AppsControllerTest, BulkAddAppsSuccess) {
    AppsController controller;

    Json::Value bulk_data;
    bulk_data["category"] = "streaming";

    bool callback_called = false;
    Json::Value response_data;

    auto req = HttpRequest::newHttpJsonRequest(bulk_data);

    controller.bulkAddApps(req, [&](const HttpResponsePtr& resp) {
        callback_called = true;
        std::string body(resp->getBody());
        Json::Reader reader;
        reader.parse(body, response_data);
    }, test_device_id);

    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(response_data["success"].asBool());
    EXPECT_TRUE(response_data.isMember("total_apps"));
    EXPECT_GT(response_data["total_apps"].asInt(), 0);  // Should have added apps
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
