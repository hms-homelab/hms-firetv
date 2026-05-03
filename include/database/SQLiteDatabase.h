#pragma once
#include "database/IDatabase.h"
#include <sqlite3.h>
#include <mutex>
#include <string>

namespace hms_firetv {

class SQLiteDatabase : public IDatabase {
public:
    explicit SQLiteDatabase(const std::string& db_path);
    ~SQLiteDatabase() override;
    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

    DbType dbType() const override { return DbType::SQLITE; }
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;

    std::optional<Device> createDevice(const Device& device) override;
    std::optional<Device> getDeviceById(const std::string& device_id) override;
    std::vector<Device> getAllDevices() override;
    std::vector<Device> getDevicesByStatus(const std::string& status) override;
    bool updateDevice(const Device& device) override;
    bool deleteDevice(const std::string& device_id) override;
    bool deviceExists(const std::string& device_id) override;
    bool updateLastSeen(const std::string& device_id, const std::string& status) override;
    bool setPairingPin(const std::string& device_id, const std::string& pin_code,
                       int expires_secs) override;
    bool verifyPinAndSetToken(const std::string& device_id, const std::string& pin_code,
                              const std::string& client_token) override;
    bool completePairing(const std::string& device_id, const std::string& client_token) override;
    bool clearPairing(const std::string& device_id) override;

    std::vector<DeviceApp> getAppsForDevice(const std::string& device_id) override;
    std::optional<DeviceApp> getApp(const std::string& device_id,
                                    const std::string& package_name) override;
    bool addApp(const DeviceApp& app) override;
    bool updateApp(const DeviceApp& app) override;
    bool deleteApp(const std::string& device_id, const std::string& package_name) override;
    bool deleteAllApps(const std::string& device_id) override;
    bool setFavorite(const std::string& device_id, const std::string& package_name,
                     bool is_favorite) override;
    bool updateSortOrder(const std::string& device_id, const std::string& package_name,
                         int sort_order) override;
    std::vector<DeviceApp> getPopularApps(const std::string& category) override;
    bool addPopularAppsToDevice(const std::string& device_id,
                                const std::string& category) override;

    Json::Value getOverallStats() override;
    Json::Value getAllDeviceStats() override;

private:
    std::string db_path_;
    sqlite3* db_ = nullptr;
    mutable std::recursive_mutex mutex_;

    void createSchema();
    bool exec(const std::string& sql);
    Device parseDevice(sqlite3_stmt* stmt);
    DeviceApp parseApp(sqlite3_stmt* stmt);

    static std::chrono::system_clock::time_point parseTs(const char* s);
    static std::optional<std::chrono::system_clock::time_point> parseTsOpt(const char* s);
};

} // namespace hms_firetv
