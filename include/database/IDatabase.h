#pragma once
#include "models/Device.h"
#include "models/DeviceApp.h"
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>

namespace hms_firetv {

enum class DbType { SQLITE, POSTGRESQL };

class IDatabase {
public:
    virtual ~IDatabase() = default;

    virtual DbType dbType() const = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // ── Devices ──────────────────────────────────────────────────────────────

    virtual std::optional<Device> createDevice(const Device& device) = 0;
    virtual std::optional<Device> getDeviceById(const std::string& device_id) = 0;
    virtual std::vector<Device> getAllDevices() = 0;
    virtual std::vector<Device> getDevicesByStatus(const std::string& status) = 0;
    virtual bool updateDevice(const Device& device) = 0;
    virtual bool deleteDevice(const std::string& device_id) = 0;
    virtual bool deviceExists(const std::string& device_id) = 0;
    virtual bool updateLastSeen(const std::string& device_id, const std::string& status) = 0;
    virtual bool setPairingPin(const std::string& device_id, const std::string& pin_code,
                               int expires_secs) = 0;
    virtual bool verifyPinAndSetToken(const std::string& device_id, const std::string& pin_code,
                                      const std::string& client_token) = 0;
    virtual bool clearPairing(const std::string& device_id) = 0;

    // ── Apps ─────────────────────────────────────────────────────────────────

    virtual std::vector<DeviceApp> getAppsForDevice(const std::string& device_id) = 0;
    virtual std::optional<DeviceApp> getApp(const std::string& device_id,
                                            const std::string& package_name) = 0;
    virtual bool addApp(const DeviceApp& app) = 0;
    virtual bool updateApp(const DeviceApp& app) = 0;
    virtual bool deleteApp(const std::string& device_id, const std::string& package_name) = 0;
    virtual bool deleteAllApps(const std::string& device_id) = 0;
    virtual bool setFavorite(const std::string& device_id, const std::string& package_name,
                             bool is_favorite) = 0;
    virtual bool updateSortOrder(const std::string& device_id, const std::string& package_name,
                                 int sort_order) = 0;
    virtual std::vector<DeviceApp> getPopularApps(const std::string& category) = 0;
    virtual bool addPopularAppsToDevice(const std::string& device_id,
                                        const std::string& category) = 0;

    // ── Stats (for StatsController) ───────────────────────────────────────────

    virtual Json::Value getOverallStats() = 0;
    virtual Json::Value getAllDeviceStats() = 0;
};

} // namespace hms_firetv
