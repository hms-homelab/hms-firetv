#pragma once
#include "models/Device.h"
#include "database/IDatabase.h"
#include <memory>
#include <optional>
#include <vector>

namespace hms_firetv {

class DeviceRepository {
public:
    static DeviceRepository& getInstance();
    static void setDatabase(std::shared_ptr<IDatabase> db);

    DeviceRepository(const DeviceRepository&) = delete;
    DeviceRepository& operator=(const DeviceRepository&) = delete;

    std::optional<Device> createDevice(const Device& device);
    std::optional<Device> getDeviceById(const std::string& device_id);
    std::vector<Device> getAllDevices();
    std::vector<Device> getDevicesByStatus(const std::string& status);
    bool updateDevice(const Device& device);
    bool deleteDevice(const std::string& device_id);
    bool setPairingPin(const std::string& device_id, const std::string& pin_code,
                       int expires_in_seconds = 300);
    bool verifyPinAndSetToken(const std::string& device_id, const std::string& pin_code,
                               const std::string& client_token);
    bool clearPairing(const std::string& device_id);
    bool updateLastSeen(const std::string& device_id, const std::string& status = "online");
    bool deviceExists(const std::string& device_id);

private:
    DeviceRepository() = default;
    static std::shared_ptr<IDatabase> db_;
};

} // namespace hms_firetv
