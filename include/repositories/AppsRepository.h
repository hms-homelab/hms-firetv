#pragma once
#include "models/DeviceApp.h"
#include "database/IDatabase.h"
#include <memory>
#include <optional>
#include <vector>
#include <string>

namespace hms_firetv {

class AppsRepository {
public:
    static AppsRepository& getInstance();
    static void setDatabase(std::shared_ptr<IDatabase> db);

    AppsRepository(const AppsRepository&) = delete;
    AppsRepository& operator=(const AppsRepository&) = delete;

    std::vector<DeviceApp> getAppsForDevice(const std::string& device_id);
    std::optional<DeviceApp> getApp(const std::string& device_id, const std::string& package_name);
    bool addApp(const DeviceApp& app);
    bool updateApp(const DeviceApp& app);
    bool deleteApp(const std::string& device_id, const std::string& package_name);
    bool setFavorite(const std::string& device_id, const std::string& package_name, bool is_favorite);
    bool updateSortOrder(const std::string& device_id, const std::string& package_name, int sort_order);
    std::vector<DeviceApp> getPopularApps(const std::string& category = "");
    bool addPopularAppsToDevice(const std::string& device_id, const std::string& category = "streaming");
    bool deleteAllApps(const std::string& device_id);

private:
    AppsRepository() = default;
    static std::shared_ptr<IDatabase> db_;
};

} // namespace hms_firetv
