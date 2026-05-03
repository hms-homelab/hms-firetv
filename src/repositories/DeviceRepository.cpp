#include "repositories/DeviceRepository.h"
#include <iostream>

namespace hms_firetv {

std::shared_ptr<IDatabase> DeviceRepository::db_;

DeviceRepository& DeviceRepository::getInstance() {
    static DeviceRepository instance;
    return instance;
}

void DeviceRepository::setDatabase(std::shared_ptr<IDatabase> db) { db_ = std::move(db); }

std::optional<Device> DeviceRepository::createDevice(const Device& device) {
    if (!db_) return std::nullopt;
    return db_->createDevice(device);
}

std::optional<Device> DeviceRepository::getDeviceById(const std::string& device_id) {
    if (!db_) return std::nullopt;
    return db_->getDeviceById(device_id);
}

std::vector<Device> DeviceRepository::getAllDevices() {
    if (!db_) return {};
    return db_->getAllDevices();
}

std::vector<Device> DeviceRepository::getDevicesByStatus(const std::string& status) {
    if (!db_) return {};
    return db_->getDevicesByStatus(status);
}

bool DeviceRepository::updateDevice(const Device& device) {
    if (!db_) return false;
    return db_->updateDevice(device);
}

bool DeviceRepository::deleteDevice(const std::string& device_id) {
    if (!db_) return false;
    return db_->deleteDevice(device_id);
}

bool DeviceRepository::setPairingPin(const std::string& device_id, const std::string& pin_code,
                                      int expires_in_seconds) {
    if (!db_) return false;
    return db_->setPairingPin(device_id, pin_code, expires_in_seconds);
}

bool DeviceRepository::verifyPinAndSetToken(const std::string& device_id,
                                             const std::string& pin_code,
                                             const std::string& client_token) {
    if (!db_) return false;
    return db_->verifyPinAndSetToken(device_id, pin_code, client_token);
}

bool DeviceRepository::clearPairing(const std::string& device_id) {
    if (!db_) return false;
    return db_->clearPairing(device_id);
}

bool DeviceRepository::updateLastSeen(const std::string& device_id, const std::string& status) {
    if (!db_) return false;
    return db_->updateLastSeen(device_id, status);
}

bool DeviceRepository::deviceExists(const std::string& device_id) {
    if (!db_) return false;
    return db_->deviceExists(device_id);
}

} // namespace hms_firetv
