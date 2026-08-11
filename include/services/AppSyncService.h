#pragma once

#include "clients/LightningClient.h"
#include "models/DeviceApp.h"

#include <json/json.h>
#include <string>
#include <vector>

namespace hms_firetv {

/**
 * AppSyncService - pull the real installed-app list off a Fire TV
 *
 * Before this, the apps table was seeded from a hardcoded "popular apps"
 * list, so Home Assistant offered apps the device might not have and missed
 * every app it did. GET /v1/FireTV/appsV2 returns what is actually
 * installed - the same list the official remote app shows in its Apps panel.
 *
 * RESPONSE SHAPE (verified against the living room device)
 * ========================================================
 * A bare JSON array, one object per app:
 *
 *   [{"appId": "com.netflix.ninja",
 *     "name": "Netflix",
 *     "tvIconArt": "https://m.media-amazon.com/images/I/416aTengF1L.png",
 *     "isInstalled": true,
 *     "isShortcutApp": false,
 *     "appShortcutLaunchIntent": ""}, ...]
 *
 * `appId` is the Android package that launchApp() takes.
 */
class AppSyncService {
public:
    struct Result {
        bool success = false;
        std::string error;
        int found = 0;     // apps the device reported
        int stored = 0;    // rows written
        std::vector<DeviceApp> apps;
    };

    /**
     * Fetch appsV2 from the device and replace the stored app list.
     *
     * Waking is the caller's business except that this will wake the device
     * if 8080 is not answering, since a sleeping Fire TV cannot be queried at
     * all.
     *
     * @param device_id Device to sync
     * @return What was found and stored
     */
    static Result syncDevice(const std::string& device_id);

    /** Same, against an already-built client (avoids a second DB lookup). */
    static Result syncDevice(const std::string& device_id, LightningClient& client);

    /** Parse an appsV2 payload into DeviceApp rows without touching the DB. */
    static std::vector<DeviceApp> parseApps(const std::string& device_id,
                                            const Json::Value& payload);
};

}  // namespace hms_firetv
