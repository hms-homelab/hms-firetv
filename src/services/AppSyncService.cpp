#include "services/AppSyncService.h"
#include "repositories/AppsRepository.h"
#include "repositories/DeviceRepository.h"

#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <thread>

namespace hms_firetv {

std::vector<DeviceApp> AppSyncService::parseApps(const std::string& device_id,
                                                 const Json::Value& payload) {
    std::vector<DeviceApp> apps;

    // The device answers with a bare array. Tolerate an object wrapper too, in
    // case a different firmware generation wraps it.
    const Json::Value* list = &payload;
    if (payload.isObject()) {
        for (const char* key : {"apps", "appsV2", "result"}) {
            if (payload.isMember(key) && payload[key].isArray()) {
                list = &payload[key];
                break;
            }
        }
    }
    if (!list->isArray()) return apps;

    int order = 0;
    for (const auto& item : *list) {
        if (!item.isObject()) continue;

        std::string package = item.get("appId", "").asString();
        if (package.empty()) continue;

        // Shortcut entries are launcher tiles, not installed packages, and
        // launchApp() cannot start them.
        if (item.get("isShortcutApp", false).asBool()) continue;
        if (item.isMember("isInstalled") && !item["isInstalled"].asBool()) continue;

        DeviceApp app;
        app.device_id = device_id;
        app.package_name = package;
        app.app_name = item.get("name", package).asString();
        app.icon_url = item.get("tvIconArt", "").asString();
        app.sort_order = order++;
        apps.push_back(app);
    }

    return apps;
}

AppSyncService::Result AppSyncService::syncDevice(const std::string& device_id) {
    Result result;

    auto device = DeviceRepository::getInstance().getDeviceById(device_id);
    if (!device.has_value()) {
        result.error = "unknown device: " + device_id;
        return result;
    }

    LightningClient client(device->ip_address, device->api_key,
                           device->client_token.value_or(""));
    return syncDevice(device_id, client);
}

AppSyncService::Result AppSyncService::syncDevice(const std::string& device_id,
                                                  LightningClient& client) {
    Result result;

    // A sleeping Fire TV does not listen on 8080 at all, so there is nothing
    // to query until it is awake.
    if (!client.isLightningApiAvailable()) {
        std::cout << "[AppSync] device asleep, waking before app query" << std::endl;
        client.wakeDevice();
        for (int i = 0; i < 5 && !client.isLightningApiAvailable(); ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto response = client.listApps();
    if (!response.success) {
        result.error = "appsV2 request failed (HTTP " +
                       std::to_string(response.status_code) + ")";
        return result;
    }

    result.apps = parseApps(device_id, response.response_body);
    result.found = static_cast<int>(result.apps.size());

    if (result.apps.empty()) {
        result.error = "device returned no installed apps";
        return result;
    }

    auto& repo = AppsRepository::getInstance();

    // Keep whatever the user marked as a favourite - the device knows what is
    // installed, it does not know what anyone likes.
    std::map<std::string, bool> favourites;
    for (const auto& existing : repo.getAppsForDevice(device_id))
        favourites[existing.package_name] = existing.is_favorite;

    std::set<std::string> live;
    for (auto& app : result.apps) {
        live.insert(app.package_name);

        auto it = favourites.find(app.package_name);
        if (it != favourites.end()) app.is_favorite = it->second;

        // Insert is ON CONFLICT DO NOTHING, so update afterwards to refresh
        // a renamed app or a changed icon URL.
        repo.addApp(app);
        repo.updateApp(app);
        repo.updateSortOrder(device_id, app.package_name, app.sort_order);
        result.stored++;
    }

    // Drop apps that were uninstalled since the last sync.
    int removed = 0;
    for (const auto& [package, _] : favourites) {
        if (!live.count(package)) {
            repo.deleteApp(device_id, package);
            removed++;
        }
    }

    std::cout << "[AppSync] " << device_id << ": " << result.found << " apps from device, "
              << result.stored << " stored, " << removed << " removed" << std::endl;

    result.success = true;
    return result;
}

}  // namespace hms_firetv
