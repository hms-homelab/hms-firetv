#include <gtest/gtest.h>
#include "repositories/AppsRepository.h"
#include "repositories/DeviceRepository.h"
#include "services/DatabaseService.h"

using namespace hms_firetv;

class AppsRepositoryTest : public ::testing::Test {
protected:
    std::string test_device_id = "unittest_repo_device";

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
        device.name = "Unit Test Repo Device";
        device.ip_address = "192.168.1.203";
        device.api_key = "test_key";
        device.status = "online";
        device.adb_enabled = false;

        DeviceRepository::getInstance().createDevice(device);
    }

    void TearDown() override {
        try {
            std::string cleanup_devices = "DELETE FROM fire_tv_devices WHERE device_id LIKE 'unittest_%'";
            DatabaseService::getInstance().executeQuery(cleanup_devices);

            std::string cleanup_apps = "DELETE FROM device_apps WHERE device_id LIKE 'unittest_%'";
            DatabaseService::getInstance().executeQuery(cleanup_apps);
        } catch (...) {}
    }
};

TEST_F(AppsRepositoryTest, AddAndGetApp) {
    DeviceApp app;
    app.device_id = test_device_id;
    app.package_name = "com.test.repo";
    app.app_name = "Test Repo App";
    app.icon_url = "";
    app.is_favorite = false;
    app.sort_order = 0;

    // Add app
    bool added = AppsRepository::getInstance().addApp(app);
    ASSERT_TRUE(added);

    // Get app
    auto retrieved = AppsRepository::getInstance().getApp(test_device_id, "com.test.repo");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->package_name, "com.test.repo");
    EXPECT_EQ(retrieved->app_name, "Test Repo App");
}

TEST_F(AppsRepositoryTest, GetAppsForDevice) {
    // Add multiple apps
    for (int i = 0; i < 3; i++) {
        DeviceApp app;
        app.device_id = test_device_id;
        app.package_name = "com.test.repo" + std::to_string(i);
        app.app_name = "Test App " + std::to_string(i);
        app.icon_url = "";
        app.is_favorite = false;
        app.sort_order = 0;
        AppsRepository::getInstance().addApp(app);
    }

    // Get all apps
    auto apps = AppsRepository::getInstance().getAppsForDevice(test_device_id);
    EXPECT_GE(apps.size(), 3);
}

TEST_F(AppsRepositoryTest, DeleteApp) {
    DeviceApp app;
    app.device_id = test_device_id;
    app.package_name = "com.test.delete";
    app.app_name = "Delete Test";
    app.icon_url = "";
    app.is_favorite = false;
    app.sort_order = 0;

    AppsRepository::getInstance().addApp(app);

    // Delete it
    bool deleted = AppsRepository::getInstance().deleteApp(test_device_id, "com.test.delete");
    EXPECT_TRUE(deleted);

    // Verify it's gone
    auto retrieved = AppsRepository::getInstance().getApp(test_device_id, "com.test.delete");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(AppsRepositoryTest, SetFavorite) {
    DeviceApp app;
    app.device_id = test_device_id;
    app.package_name = "com.test.favorite";
    app.app_name = "Favorite Test";
    app.icon_url = "";
    app.is_favorite = false;
    app.sort_order = 0;

    AppsRepository::getInstance().addApp(app);

    // Set as favorite
    bool updated = AppsRepository::getInstance().setFavorite(test_device_id, "com.test.favorite", true);
    EXPECT_TRUE(updated);

    // Verify
    auto retrieved = AppsRepository::getInstance().getApp(test_device_id, "com.test.favorite");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->is_favorite);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
