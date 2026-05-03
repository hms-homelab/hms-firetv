#pragma once
#include <drogon/HttpController.h>
#include "database/IDatabase.h"
#include <memory>

using namespace drogon;

namespace hms_firetv {

class StatsController : public drogon::HttpController<StatsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(StatsController::getOverallStats,  "/api/stats",         Get);
    ADD_METHOD_TO(StatsController::getDeviceStats,   "/api/stats/devices", Get);
    METHOD_LIST_END

    static void setDatabase(std::shared_ptr<IDatabase> db);

    void getOverallStats(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback);
    void getDeviceStats(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback);

private:
    static std::shared_ptr<IDatabase> db_;
    void sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                   HttpStatusCode status, const std::string& message);
};

} // namespace hms_firetv
