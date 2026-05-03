#include <gtest/gtest.h>
#include "repositories/AppsRepository.h"
#include "repositories/DeviceRepository.h"
#include "database/SQLiteDatabase.h"
#include <memory>

using namespace hms_firetv;

class AppsRepositoryTest : public ::testing::Test {
protected:
    std::string test_device_id = "unittest_repo_device";
    std::shared_ptr<SQLiteDatabase> db;

    void SetUp() override {
        db = std::make_shared<SQLiteDatabase>(":memory:");
        db->connect();
        DeviceRepository::setDatabase(db);
        AppsRepository::setDatabase(db);

        Device device;
        device.device_id = test_device_id;
        device.name      = "Unit Test Repo Device";
        device.ip_address = "192.168.1.203";
        device.api_key   = "test_key";
        device.status    = "online";
        device.adb_enabled = false;
        DeviceRepository::getInstance().createDevice(device);
    }

    void TearDown() override {
        DeviceRepository::getInstance().deleteDevice(test_device_id);
    }
};

TEST_F(AppsRepositoryTest, AddAndGetApp) {
    DeviceApp app;
    app.device_id    = test_device_id;
    app.package_name = "com.test.repo";
    app.app_name     = "Test Repo App";
    app.is_favorite  = false;

    ASSERT_TRUE(AppsRepository::getInstance().addApp(app));

    auto retrieved = AppsRepository::getInstance().getApp(test_device_id, "com.test.repo");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->package_name, "com.test.repo");
    EXPECT_EQ(retrieved->app_name, "Test Repo App");
}

TEST_F(AppsRepositoryTest, GetAppsForDevice) {
    for (int i = 0; i < 3; i++) {
        DeviceApp app;
        app.device_id    = test_device_id;
        app.package_name = "com.test.repo" + std::to_string(i);
        app.app_name     = "Test App " + std::to_string(i);
        AppsRepository::getInstance().addApp(app);
    }

    auto apps = AppsRepository::getInstance().getAppsForDevice(test_device_id);
    EXPECT_GE(apps.size(), 3u);
}

TEST_F(AppsRepositoryTest, DeleteApp) {
    DeviceApp app;
    app.device_id    = test_device_id;
    app.package_name = "com.test.delete";
    app.app_name     = "Delete Test";
    AppsRepository::getInstance().addApp(app);

    EXPECT_TRUE(AppsRepository::getInstance().deleteApp(test_device_id, "com.test.delete"));
    EXPECT_FALSE(AppsRepository::getInstance().getApp(test_device_id, "com.test.delete").has_value());
}

TEST_F(AppsRepositoryTest, SetFavorite) {
    DeviceApp app;
    app.device_id    = test_device_id;
    app.package_name = "com.test.favorite";
    app.app_name     = "Favorite Test";
    app.is_favorite  = false;
    AppsRepository::getInstance().addApp(app);

    EXPECT_TRUE(AppsRepository::getInstance().setFavorite(test_device_id, "com.test.favorite", true));

    auto retrieved = AppsRepository::getInstance().getApp(test_device_id, "com.test.favorite");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->is_favorite);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
