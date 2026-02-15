#pragma once

#include <drogon/HttpController.h>
#include "repositories/AppsRepository.h"

using namespace drogon;

namespace hms_firetv {

/**
 * AppsController - REST API for device app management
 *
 * Endpoints:
 * - GET    /api/devices/:id/apps            - List all apps for device
 * - POST   /api/devices/:id/apps            - Add app to device
 * - PUT    /api/devices/:id/apps/:package   - Update app
 * - DELETE /api/devices/:id/apps/:package   - Delete app
 * - POST   /api/devices/:id/apps/:package/favorite - Toggle favorite
 * - GET    /api/apps/popular                - Get popular apps catalog
 * - POST   /api/devices/:id/apps/bulk       - Bulk add popular apps
 */
class AppsController : public drogon::HttpController<AppsController> {
public:
    METHOD_LIST_BEGIN

    // List apps for device
    ADD_METHOD_TO(AppsController::listApps, "/api/devices/{1}/apps", Get);

    // Add app to device
    ADD_METHOD_TO(AppsController::addApp, "/api/devices/{1}/apps", Post);

    // Update app
    ADD_METHOD_TO(AppsController::updateApp, "/api/devices/{1}/apps/{2}", Put);

    // Delete app
    ADD_METHOD_TO(AppsController::deleteApp, "/api/devices/{1}/apps/{2}", Delete);

    // Toggle favorite
    ADD_METHOD_TO(AppsController::toggleFavorite, "/api/devices/{1}/apps/{2}/favorite", Post);

    // Get popular apps catalog
    ADD_METHOD_TO(AppsController::getPopularApps, "/api/apps/popular", Get);

    // Bulk add popular apps
    ADD_METHOD_TO(AppsController::bulkAddApps, "/api/devices/{1}/apps/bulk", Post);

    METHOD_LIST_END

    /**
     * List all apps for device
     * GET /api/devices/:id/apps
     */
    void listApps(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback,
                 std::string device_id);

    /**
     * Add app to device
     * POST /api/devices/:id/apps
     * Body: {"package": "com.netflix.ninja", "name": "Netflix"}
     */
    void addApp(const HttpRequestPtr& req,
               std::function<void(const HttpResponsePtr&)>&& callback,
               std::string device_id);

    /**
     * Update app
     * PUT /api/devices/:id/apps/:package
     * Body: {"name": "Netflix", "icon_url": "...", "sort_order": 1}
     */
    void updateApp(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback,
                  std::string device_id,
                  std::string package);

    /**
     * Delete app
     * DELETE /api/devices/:id/apps/:package
     */
    void deleteApp(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback,
                  std::string device_id,
                  std::string package);

    /**
     * Toggle favorite
     * POST /api/devices/:id/apps/:package/favorite
     * Body: {"is_favorite": true}
     */
    void toggleFavorite(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       std::string device_id,
                       std::string package);

    /**
     * Get popular apps catalog
     * GET /api/apps/popular?category=streaming
     */
    void getPopularApps(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * Bulk add popular apps to device
     * POST /api/devices/:id/apps/bulk
     * Body: {"category": "streaming"}
     */
    void bulkAddApps(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    std::string device_id);

private:
    /**
     * Convert DeviceApp to JSON
     */
    Json::Value appToJson(const DeviceApp& app);

    /**
     * Send error response
     */
    void sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                   HttpStatusCode status,
                   const std::string& message);
};

} // namespace hms_firetv
