#include "repositories/AppsRepository.h"
#include "services/DatabaseService.h"
#include <iostream>

namespace hms_firetv {

// ============================================================================
// SINGLETON
// ============================================================================

AppsRepository& AppsRepository::getInstance() {
    static AppsRepository instance;
    return instance;
}

// ============================================================================
// GET APPS FOR DEVICE
// ============================================================================

std::vector<DeviceApp> AppsRepository::getAppsForDevice(const std::string& device_id) {
    std::vector<DeviceApp> apps;

    try {
        std::string query = "SELECT id, device_id, package_name, app_name, icon_url, "
                          "is_favorite, created_at, updated_at "
                          "FROM device_apps "
                          "WHERE device_id = $1 "
                          "ORDER BY is_favorite DESC, app_name";

        auto result = DatabaseService::getInstance().executeQueryParams(query, {device_id});

        for (const auto& row : result) {
            DeviceApp app;
            app.id = row["id"].as<int>();
            app.device_id = row["device_id"].as<std::string>();
            app.package_name = row["package_name"].as<std::string>();
            app.app_name = row["app_name"].as<std::string>();
            app.icon_url = row["icon_url"].is_null() ? "" : row["icon_url"].as<std::string>();
            app.is_favorite = row["is_favorite"].as<bool>();
            app.sort_order = 0;  // Default value (column doesn't exist in this schema)
            app.created_at = row["created_at"].as<std::string>();
            app.updated_at = row["updated_at"].as<std::string>();

            apps.push_back(app);
        }

        std::cout << "[AppsRepository] Retrieved " << apps.size() << " apps for device: "
                  << device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error getting apps for device: " << e.what() << std::endl;
    }

    return apps;
}

// ============================================================================
// GET SPECIFIC APP
// ============================================================================

std::optional<DeviceApp> AppsRepository::getApp(const std::string& device_id,
                                               const std::string& package_name) {
    try {
        std::string query = "SELECT id, device_id, package_name, app_name, icon_url, "
                          "is_favorite, created_at, updated_at "
                          "FROM device_apps "
                          "WHERE device_id = $1 AND package_name = $2";

        auto result = DatabaseService::getInstance().executeQueryParams(query, {device_id, package_name});

        if (result.empty()) {
            return std::nullopt;
        }

        const auto& row = result[0];
        DeviceApp app;
        app.id = row["id"].as<int>();
        app.device_id = row["device_id"].as<std::string>();
        app.package_name = row["package_name"].as<std::string>();
        app.app_name = row["app_name"].as<std::string>();
        app.icon_url = row["icon_url"].is_null() ? "" : row["icon_url"].as<std::string>();
        app.is_favorite = row["is_favorite"].as<bool>();
        app.sort_order = 0;  // Default value
        app.created_at = row["created_at"].as<std::string>();
        app.updated_at = row["updated_at"].as<std::string>();

        return app;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error getting app: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// ============================================================================
// ADD APP
// ============================================================================

bool AppsRepository::addApp(const DeviceApp& app) {
    try {
        std::string query = "INSERT INTO device_apps "
                          "(device_id, package_name, app_name, icon_url, is_favorite) "
                          "VALUES ($1, $2, $3, $4, $5) "
                          "ON CONFLICT (device_id, package_name) DO NOTHING";

        auto result = DatabaseService::getInstance().executeQueryParams(query, {
            app.device_id,
            app.package_name,
            app.app_name,
            app.icon_url.empty() ? "" : app.icon_url,
            app.is_favorite ? "true" : "false"
        });

        std::cout << "[AppsRepository] Added app: " << app.package_name
                  << " to device: " << app.device_id << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error adding app: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// UPDATE APP
// ============================================================================

bool AppsRepository::updateApp(const DeviceApp& app) {
    try {
        std::string query = "UPDATE device_apps "
                          "SET app_name = $1, icon_url = $2, is_favorite = $3 "
                          "WHERE device_id = $4 AND package_name = $5";

        DatabaseService::getInstance().executeQueryParams(query, {
            app.app_name,
            app.icon_url,
            app.is_favorite ? "true" : "false",
            app.device_id,
            app.package_name
        });

        std::cout << "[AppsRepository] Updated app: " << app.package_name << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error updating app: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// DELETE APP
// ============================================================================

bool AppsRepository::deleteApp(const std::string& device_id, const std::string& package_name) {
    try {
        std::string query = "DELETE FROM device_apps "
                          "WHERE device_id = $1 AND package_name = $2";

        DatabaseService::getInstance().executeQueryParams(query, {device_id, package_name});

        std::cout << "[AppsRepository] Deleted app: " << package_name
                  << " from device: " << device_id << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error deleting app: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// SET FAVORITE
// ============================================================================

bool AppsRepository::setFavorite(const std::string& device_id,
                                 const std::string& package_name,
                                 bool is_favorite) {
    try {
        std::string query = "UPDATE device_apps "
                          "SET is_favorite = $1 "
                          "WHERE device_id = $2 AND package_name = $3";

        DatabaseService::getInstance().executeQueryParams(query, {
            is_favorite ? "true" : "false",
            device_id,
            package_name
        });

        std::cout << "[AppsRepository] Set favorite " << is_favorite
                  << " for app: " << package_name << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error setting favorite: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// UPDATE SORT ORDER
// ============================================================================

bool AppsRepository::updateSortOrder(const std::string& device_id,
                                     const std::string& package_name,
                                     int sort_order) {
    try {
        std::string query = "UPDATE device_apps "
                          "SET sort_order = $1 "
                          "WHERE device_id = $2 AND package_name = $3";

        DatabaseService::getInstance().executeQueryParams(query, {
            std::to_string(sort_order),
            device_id,
            package_name
        });

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error updating sort order: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// GET POPULAR APPS
// ============================================================================

std::vector<DeviceApp> AppsRepository::getPopularApps(const std::string& category) {
    std::vector<DeviceApp> apps;

    try {
        std::string query = "SELECT package_name, app_name, icon_url, category "
                          "FROM popular_apps";

        if (!category.empty()) {
            query += " WHERE category = $1";
        }

        query += " ORDER BY app_name";

        auto result = category.empty() ?
                     DatabaseService::getInstance().executeQuery(query) :
                     DatabaseService::getInstance().executeQueryParams(query, {category});

        for (const auto& row : result) {
            DeviceApp app;
            app.id = 0;  // Not from device_apps table
            app.device_id = "";  // Not associated with device yet
            app.package_name = row["package_name"].as<std::string>();
            app.app_name = row["app_name"].as<std::string>();
            app.icon_url = row["icon_url"].is_null() ? "" : row["icon_url"].as<std::string>();
            app.is_favorite = false;
            app.sort_order = 0;

            apps.push_back(app);
        }

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error getting popular apps: " << e.what() << std::endl;
    }

    return apps;
}

// ============================================================================
// ADD POPULAR APPS TO DEVICE
// ============================================================================

bool AppsRepository::addPopularAppsToDevice(const std::string& device_id,
                                           const std::string& category) {
    try {
        std::string query = "INSERT INTO device_apps (device_id, package_name, app_name, icon_url) "
                          "SELECT $1, package_name, app_name, icon_url "
                          "FROM popular_apps "
                          "WHERE category = $2 "
                          "ON CONFLICT (device_id, package_name) DO NOTHING";

        DatabaseService::getInstance().executeQueryParams(query, {device_id, category});

        std::cout << "[AppsRepository] Added popular " << category << " apps to device: "
                  << device_id << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error adding popular apps: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// DELETE ALL APPS
// ============================================================================

bool AppsRepository::deleteAllApps(const std::string& device_id) {
    try {
        std::string query = "DELETE FROM device_apps WHERE device_id = $1";

        DatabaseService::getInstance().executeQueryParams(query, {device_id});

        std::cout << "[AppsRepository] Deleted all apps for device: " << device_id << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[AppsRepository] Error deleting all apps: " << e.what() << std::endl;
        return false;
    }
}

} // namespace hms_firetv
