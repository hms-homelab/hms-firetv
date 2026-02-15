#pragma once

#include <string>
#include <vector>
#include <optional>

namespace hms_firetv {

/**
 * DeviceApp - Represents an app installed on a Fire TV device
 */
struct DeviceApp {
    int id;
    std::string device_id;
    std::string package_name;
    std::string app_name;
    std::string icon_url;
    bool is_favorite;
    int sort_order;
    std::string created_at;
    std::string updated_at;
};

/**
 * AppsRepository - Database operations for device apps
 *
 * Manages the device_apps and popular_apps tables
 */
class AppsRepository {
public:
    /**
     * Get singleton instance
     */
    static AppsRepository& getInstance();

    /**
     * Get all apps for a device
     */
    std::vector<DeviceApp> getAppsForDevice(const std::string& device_id);

    /**
     * Get a specific app
     */
    std::optional<DeviceApp> getApp(const std::string& device_id, const std::string& package_name);

    /**
     * Add app to device
     */
    bool addApp(const DeviceApp& app);

    /**
     * Update app
     */
    bool updateApp(const DeviceApp& app);

    /**
     * Delete app
     */
    bool deleteApp(const std::string& device_id, const std::string& package_name);

    /**
     * Set app as favorite
     */
    bool setFavorite(const std::string& device_id, const std::string& package_name, bool is_favorite);

    /**
     * Update sort order
     */
    bool updateSortOrder(const std::string& device_id, const std::string& package_name, int sort_order);

    /**
     * Get popular apps (from catalog)
     */
    std::vector<DeviceApp> getPopularApps(const std::string& category = "");

    /**
     * Add popular apps to device (bulk insert from catalog)
     */
    bool addPopularAppsToDevice(const std::string& device_id, const std::string& category = "streaming");

    /**
     * Delete all apps for a device
     */
    bool deleteAllApps(const std::string& device_id);

private:
    AppsRepository() = default;
    AppsRepository(const AppsRepository&) = delete;
    AppsRepository& operator=(const AppsRepository&) = delete;
};

} // namespace hms_firetv
