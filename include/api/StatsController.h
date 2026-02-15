#pragma once

#include <drogon/HttpController.h>
#include "repositories/DeviceRepository.h"
#include "repositories/AppsRepository.h"
#include "services/DatabaseService.h"

using namespace drogon;

namespace hms_firetv {

/**
 * StatsController - REST API for dashboard statistics
 *
 * Endpoints:
 * - GET /api/stats           - Overall system statistics
 * - GET /api/stats/devices   - Per-device statistics
 */
class StatsController : public drogon::HttpController<StatsController> {
public:
    METHOD_LIST_BEGIN

    // Get overall statistics
    ADD_METHOD_TO(StatsController::getOverallStats, "/api/stats", Get);

    // Get per-device statistics
    ADD_METHOD_TO(StatsController::getDeviceStats, "/api/stats/devices", Get);

    METHOD_LIST_END

    /**
     * Get overall system statistics
     * GET /api/stats
     */
    void getOverallStats(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * Get per-device statistics
     * GET /api/stats/devices
     */
    void getDeviceStats(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback);

private:
    /**
     * Send error response
     */
    void sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                   HttpStatusCode status,
                   const std::string& message);
};

} // namespace hms_firetv
