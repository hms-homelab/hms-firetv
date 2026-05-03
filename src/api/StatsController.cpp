#include "api/StatsController.h"
#include <iostream>

namespace hms_firetv {

std::shared_ptr<IDatabase> StatsController::db_;

void StatsController::setDatabase(std::shared_ptr<IDatabase> db) { db_ = std::move(db); }

void StatsController::getOverallStats(const HttpRequestPtr& req,
                                      std::function<void(const HttpResponsePtr&)>&& callback) {
    if (!db_) { sendError(std::move(callback), k503ServiceUnavailable, "Database unavailable"); return; }
    try {
        auto resp = HttpResponse::newHttpJsonResponse(db_->getOverallStats());
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const std::exception& e) {
        std::cerr << "[StatsController] getOverallStats error: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get statistics");
    }
}

void StatsController::getDeviceStats(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback) {
    if (!db_) { sendError(std::move(callback), k503ServiceUnavailable, "Database unavailable"); return; }
    try {
        Json::Value response;
        response["success"] = true;
        auto devices = db_->getAllDeviceStats();
        response["count"] = static_cast<int>(devices.size());
        response["devices"] = devices;
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);
    } catch (const std::exception& e) {
        std::cerr << "[StatsController] getDeviceStats error: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get device statistics");
    }
}

void StatsController::sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                                HttpStatusCode status, const std::string& message) {
    Json::Value r; r["success"] = false; r["error"] = message;
    auto resp = HttpResponse::newHttpJsonResponse(r);
    resp->setStatusCode(status);
    callback(resp);
}

} // namespace hms_firetv
