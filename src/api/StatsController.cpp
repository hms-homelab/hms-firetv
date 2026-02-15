#include "api/StatsController.h"
#include <iostream>

namespace hms_firetv {

// ============================================================================
// GET OVERALL STATISTICS
// ============================================================================

void StatsController::getOverallStats(const HttpRequestPtr& req,
                                      std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        // Get device counts by status
        std::string device_counts_query = "SELECT status, COUNT(*) as count "
                                         "FROM fire_tv_devices "
                                         "GROUP BY status";

        auto device_counts = DatabaseService::getInstance().executeQuery(device_counts_query);

        int total_devices = 0;
        int online_devices = 0;
        int offline_devices = 0;
        int pairing_devices = 0;

        for (const auto& row : device_counts) {
            std::string status = row["status"].as<std::string>();
            int count = row["count"].as<int>();
            total_devices += count;

            if (status == "online") {
                online_devices = count;
            } else if (status == "offline") {
                offline_devices = count;
            } else if (status == "pairing") {
                pairing_devices = count;
            }
        }

        // Get total app count
        std::string app_count_query = "SELECT COUNT(*) as count FROM device_apps";
        auto app_count_result = DatabaseService::getInstance().executeQuery(app_count_query);
        int total_apps = app_count_result[0]["count"].as<int>();

        // Get command statistics (last 24 hours)
        std::string command_stats_query =
            "SELECT "
            "  COUNT(*) as total_commands, "
            "  SUM(CASE WHEN success = true THEN 1 ELSE 0 END) as successful_commands, "
            "  AVG(response_time_ms) as avg_response_time "
            "FROM command_history "
            "WHERE created_at > NOW() - INTERVAL '24 hours'";

        auto command_stats = DatabaseService::getInstance().executeQuery(command_stats_query);

        int commands_24h = 0;
        int successful_commands_24h = 0;
        double avg_response_time = 0.0;

        if (!command_stats.empty()) {
            commands_24h = command_stats[0]["total_commands"].is_null() ? 0 : command_stats[0]["total_commands"].as<int>();
            successful_commands_24h = command_stats[0]["successful_commands"].is_null() ? 0 : command_stats[0]["successful_commands"].as<int>();
            avg_response_time = command_stats[0]["avg_response_time"].is_null() ? 0.0 : command_stats[0]["avg_response_time"].as<double>();
        }

        // Calculate success rate
        double success_rate = commands_24h > 0 ? (double)successful_commands_24h / commands_24h * 100.0 : 0.0;

        // Get paired device count
        std::string paired_query = "SELECT COUNT(*) as count FROM fire_tv_devices WHERE client_token IS NOT NULL";
        auto paired_result = DatabaseService::getInstance().executeQuery(paired_query);
        int paired_devices = paired_result[0]["count"].as<int>();

        // Build response
        Json::Value response;
        response["success"] = true;

        Json::Value devices;
        devices["total"] = total_devices;
        devices["online"] = online_devices;
        devices["offline"] = offline_devices;
        devices["pairing"] = pairing_devices;
        devices["paired"] = paired_devices;
        response["devices"] = devices;

        Json::Value apps;
        apps["total"] = total_apps;
        response["apps"] = apps;

        Json::Value commands;
        commands["last_24h"] = commands_24h;
        commands["successful_24h"] = successful_commands_24h;
        commands["success_rate"] = success_rate;
        commands["avg_response_time_ms"] = avg_response_time;
        response["commands"] = commands;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[StatsController] Retrieved overall statistics" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[StatsController] Error getting overall stats: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get statistics");
    }
}

// ============================================================================
// GET PER-DEVICE STATISTICS
// ============================================================================

void StatsController::getDeviceStats(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        // Use the device_stats view
        std::string query = "SELECT * FROM device_stats ORDER BY name";

        auto result = DatabaseService::getInstance().executeQuery(query);

        Json::Value response;
        response["success"] = true;
        response["count"] = static_cast<unsigned int>(result.size());
        response["devices"] = Json::arrayValue;

        for (const auto& row : result) {
            Json::Value device;
            device["device_id"] = row["device_id"].as<std::string>();
            device["name"] = row["name"].as<std::string>();
            device["status"] = row["status"].as<std::string>();

            if (!row["last_seen_at"].is_null()) {
                device["last_seen_at"] = row["last_seen_at"].as<std::string>();
            }

            device["app_count"] = row["app_count"].is_null() ? 0 : row["app_count"].as<int>();
            device["commands_24h"] = row["commands_24h"].is_null() ? 0 : row["commands_24h"].as<int>();
            device["successful_commands_24h"] = row["successful_commands_24h"].is_null() ? 0 : row["successful_commands_24h"].as<int>();

            double avg_response = row["avg_response_time_ms_24h"].is_null() ? 0.0 : row["avg_response_time_ms_24h"].as<double>();
            device["avg_response_time_ms_24h"] = avg_response;

            if (!row["last_command_at"].is_null()) {
                device["last_command_at"] = row["last_command_at"].as<std::string>();
            }

            // Calculate success rate
            int total_commands = device["commands_24h"].asInt();
            int successful_commands = device["successful_commands_24h"].asInt();
            double success_rate = total_commands > 0 ? (double)successful_commands / total_commands * 100.0 : 0.0;
            device["success_rate_24h"] = success_rate;

            response["devices"].append(device);
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[StatsController] Retrieved device statistics for "
                  << result.size() << " devices" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[StatsController] Error getting device stats: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get device statistics");
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void StatsController::sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                                HttpStatusCode status,
                                const std::string& message) {
    Json::Value response;
    response["success"] = false;
    response["error"] = message;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(status);
    callback(resp);
}

} // namespace hms_firetv
