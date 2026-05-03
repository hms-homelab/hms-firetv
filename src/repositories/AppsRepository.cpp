#include "repositories/AppsRepository.h"

namespace hms_firetv {

std::shared_ptr<IDatabase> AppsRepository::db_;

AppsRepository& AppsRepository::getInstance() {
    static AppsRepository instance;
    return instance;
}

void AppsRepository::setDatabase(std::shared_ptr<IDatabase> db) { db_ = std::move(db); }

std::vector<DeviceApp> AppsRepository::getAppsForDevice(const std::string& device_id) {
    if (!db_) return {};
    return db_->getAppsForDevice(device_id);
}

std::optional<DeviceApp> AppsRepository::getApp(const std::string& device_id,
                                                   const std::string& package_name) {
    if (!db_) return std::nullopt;
    return db_->getApp(device_id, package_name);
}

bool AppsRepository::addApp(const DeviceApp& app) {
    if (!db_) return false;
    return db_->addApp(app);
}

bool AppsRepository::updateApp(const DeviceApp& app) {
    if (!db_) return false;
    return db_->updateApp(app);
}

bool AppsRepository::deleteApp(const std::string& device_id, const std::string& package_name) {
    if (!db_) return false;
    return db_->deleteApp(device_id, package_name);
}

bool AppsRepository::setFavorite(const std::string& device_id, const std::string& package_name,
                                  bool is_favorite) {
    if (!db_) return false;
    return db_->setFavorite(device_id, package_name, is_favorite);
}

bool AppsRepository::updateSortOrder(const std::string& device_id,
                                      const std::string& package_name, int sort_order) {
    if (!db_) return false;
    return db_->updateSortOrder(device_id, package_name, sort_order);
}

std::vector<DeviceApp> AppsRepository::getPopularApps(const std::string& category) {
    if (!db_) return {};
    return db_->getPopularApps(category);
}

bool AppsRepository::addPopularAppsToDevice(const std::string& device_id,
                                             const std::string& category) {
    if (!db_) return false;
    return db_->addPopularAppsToDevice(device_id, category);
}

bool AppsRepository::deleteAllApps(const std::string& device_id) {
    if (!db_) return false;
    return db_->deleteAllApps(device_id);
}

} // namespace hms_firetv
