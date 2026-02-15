#pragma once

#include <drogon/HttpController.h>
#include "clients/LightningClient.h"
#include "repositories/DeviceRepository.h"
#include "utils/LRUCache.h"
#include "utils/BackgroundLogger.h"

using namespace drogon;

namespace hms_firetv {

/**
 * CommandController - REST API for sending commands to Fire TV devices
 *
 * Endpoints:
 * - POST /api/devices/:id/command      - Send generic command
 * - POST /api/devices/:id/navigate     - Navigation command
 * - POST /api/devices/:id/media        - Media control
 * - POST /api/devices/:id/volume       - Volume control
 * - POST /api/devices/:id/app          - Launch app
 * - POST /api/devices/:id/text         - Send text
 * - GET  /api/devices/:id/history      - Command history
 */
class CommandController : public drogon::HttpController<CommandController> {
public:
    METHOD_LIST_BEGIN

    // Generic command
    ADD_METHOD_TO(CommandController::sendCommand, "/api/devices/{1}/command", Post);

    // Navigation commands
    ADD_METHOD_TO(CommandController::navigate, "/api/devices/{1}/navigate", Post);

    // Media commands
    ADD_METHOD_TO(CommandController::mediaControl, "/api/devices/{1}/media", Post);

    // Volume commands
    ADD_METHOD_TO(CommandController::volumeControl, "/api/devices/{1}/volume", Post);

    // Launch app
    ADD_METHOD_TO(CommandController::launchApp, "/api/devices/{1}/app", Post);

    // Send text
    ADD_METHOD_TO(CommandController::sendText, "/api/devices/{1}/text", Post);

    // Command history
    ADD_METHOD_TO(CommandController::getHistory, "/api/devices/{1}/history", Get);

    METHOD_LIST_END

    /**
     * Send generic command
     * POST /api/devices/:id/command
     * Body: {"command": "...", "params": {...}}
     */
    void sendCommand(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    std::string device_id);

    /**
     * Navigation command
     * POST /api/devices/:id/navigate
     * Body: {"action": "up|down|left|right|select|home|back|menu"}
     */
    void navigate(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback,
                 std::string device_id);

    /**
     * Media control
     * POST /api/devices/:id/media
     * Body: {"action": "play|pause|stop"}
     */
    void mediaControl(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     std::string device_id);

    /**
     * Volume control
     * POST /api/devices/:id/volume
     * Body: {"action": "volume_up|volume_down|mute"}
     */
    void volumeControl(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      std::string device_id);

    /**
     * Launch app
     * POST /api/devices/:id/app
     * Body: {"package": "com.netflix.ninja"}
     */
    void launchApp(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback,
                  std::string device_id);

    /**
     * Send text
     * POST /api/devices/:id/text
     * Body: {"text": "search query"}
     */
    void sendText(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback,
                 std::string device_id);

    /**
     * Get command history
     * GET /api/devices/:id/history?limit=50&offset=0
     */
    void getHistory(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   std::string device_id);

public:
    /**
     * Invalidate cached client for a device (call when device is updated/deleted)
     */
    static void invalidateClient(const std::string& device_id);

    /**
     * Initialize background logger (call once at startup)
     */
    static void initBackgroundLogger();

    /**
     * Shutdown background logger (call at shutdown)
     */
    static void shutdownBackgroundLogger();

private:
    /**
     * Get or create Lightning client for device
     */
    std::shared_ptr<LightningClient> getClient(const std::string& device_id);

    /**
     * Make async Fire TV API call (non-blocking)
     *
     * @param device_id Device identifier
     * @param endpoint API endpoint (e.g., "/navigation")
     * @param json_body JSON request body
     * @param completion_callback Callback invoked with (success, response_time_ms, error_msg)
     */
    void makeAsyncFireTVCall(const std::string& device_id,
                             const std::string& endpoint,
                             const Json::Value& json_body,
                             std::function<void(bool, int, const std::string&)> completion_callback);

    /**
     * Log command to database
     */
    void logCommand(const std::string& device_id,
                   const std::string& command_type,
                   const Json::Value& command_data,
                   bool success,
                   int response_time_ms,
                   const std::string& error_message = "");

    /**
     * Send error response
     */
    void sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                   HttpStatusCode status,
                   const std::string& message);

    // Static LRU cache of Lightning clients per device (max 100 entries, 1 hour TTL)
    // Static to persist across controller instances
    static LRUCache<std::string, std::shared_ptr<LightningClient>> clients_cache_;

    // Static background logger for async command history logging (max 1000 entries)
    // Static to persist across controller instances
    static BackgroundLogger background_logger_;
    static std::once_flag logger_init_flag_;
};

} // namespace hms_firetv
