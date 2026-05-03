#pragma once
#include "database/IDatabase.h"
#include <string>

#ifdef WITH_POSTGRESQL
#include <pqxx/pqxx>
#endif

namespace hms_firetv {

class PostgresDatabase : public IDatabase {
public:
    PostgresDatabase(const std::string& host, int port, const std::string& name,
                     const std::string& user, const std::string& password);
    ~PostgresDatabase() override = default;

    DbType dbType() const override { return DbType::POSTGRESQL; }
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
    std::string host_;
    int port_;
    std::string name_;
    std::string user_;
    std::string password_;

#ifdef WITH_POSTGRESQL
    Device parseDevice(const pqxx::row& row);
    DeviceApp parseApp(const pqxx::row& row);
#endif
};

} // namespace hms_firetv
