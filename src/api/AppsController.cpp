#include "api/AppsController.h"
#include <iostream>

namespace hms_firetv {

// ============================================================================
// LIST APPS
// ============================================================================

void AppsController::listApps(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback,
                              std::string device_id) {
    try {
        auto apps = AppsRepository::getInstance().getAppsForDevice(device_id);

        Json::Value response;
        response["success"] = true;
        response["device_id"] = device_id;
        response["count"] = static_cast<unsigned int>(apps.size());
        response["apps"] = Json::arrayValue;

        for (const auto& app : apps) {
            response["apps"].append(appToJson(app));
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[AppsController] Listed " << apps.size() << " apps for device: "
                  << device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[AppsController] Error listing apps: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to list apps");
    }
}

// ============================================================================
// ADD APP
// ============================================================================

void AppsController::addApp(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           std::string device_id) {
    try {
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("package") || !json->isMember("name")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'package' or 'name' field");
            return;
        }

        DeviceApp app;
        app.device_id = device_id;
        app.package_name = (*json)["package"].asString();
        app.app_name = (*json)["name"].asString();
        app.icon_url = (*json).get("icon_url", "").asString();
        app.is_favorite = (*json).get("is_favorite", false).asBool();
        app.sort_order = (*json).get("sort_order", 0).asInt();

        bool success = AppsRepository::getInstance().addApp(app);

        if (!success) {
            sendError(std::move(callback), k500InternalServerError, "Failed to add app");
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["message"] = "App added successfully";
        response["app"] = appToJson(app);

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k201Created);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[AppsController] Error adding app: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to add app");
    }
}

// ============================================================================
// UPDATE APP
// ============================================================================

void AppsController::updateApp(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback,
                              std::string device_id,
                              std::string package) {
    try {
        // Check if app exists
        auto existing = AppsRepository::getInstance().getApp(device_id, package);
        if (!existing.has_value()) {
            sendError(std::move(callback), k404NotFound, "App not found");
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        // Update fields
        DeviceApp app = existing.value();

        if (json->isMember("name")) {
            app.app_name = (*json)["name"].asString();
        }
        if (json->isMember("icon_url")) {
            app.icon_url = (*json)["icon_url"].asString();
        }
        if (json->isMember("is_favorite")) {
            app.is_favorite = (*json)["is_favorite"].asBool();
        }
        if (json->isMember("sort_order")) {
            app.sort_order = (*json)["sort_order"].asInt();
        }

        bool success = AppsRepository::getInstance().updateApp(app);

        if (!success) {
            sendError(std::move(callback), k500InternalServerError, "Failed to update app");
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["message"] = "App updated successfully";
        response["app"] = appToJson(app);

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[AppsController] Error updating app: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to update app");
    }
}

// ============================================================================
// DELETE APP
// ============================================================================

void AppsController::deleteApp(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback,
                              std::string device_id,
                              std::string package) {
    try {
        // Check if app exists
        auto existing = AppsRepository::getInstance().getApp(device_id, package);
        if (!existing.has_value()) {
            sendError(std::move(callback), k404NotFound, "App not found");
            return;
        }

        bool success = AppsRepository::getInstance().deleteApp(device_id, package);

        if (!success) {
            sendError(std::move(callback), k500InternalServerError, "Failed to delete app");
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["message"] = "App deleted successfully";

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[AppsController] Error deleting app: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to delete app");
    }
}

// ============================================================================
// TOGGLE FAVORITE
// ============================================================================

void AppsController::toggleFavorite(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback,
                                   std::string device_id,
                                   std::string package) {
    try {
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("is_favorite")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'is_favorite' field");
            return;
        }

        bool is_favorite = (*json)["is_favorite"].asBool();

        bool success = AppsRepository::getInstance().setFavorite(device_id, package, is_favorite);

        if (!success) {
            sendError(std::move(callback), k500InternalServerError, "Failed to set favorite");
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["message"] = is_favorite ? "App marked as favorite" : "App unmarked as favorite";
        response["is_favorite"] = is_favorite;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[AppsController] Error toggling favorite: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to toggle favorite");
    }
}

// ============================================================================
// GET POPULAR APPS
// ============================================================================

void AppsController::getPopularApps(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        std::string category = "";

        auto params = req->getParameters();
        if (params.find("category") != params.end()) {
            category = params.at("category");
        }

        auto apps = AppsRepository::getInstance().getPopularApps(category);

        Json::Value response;
        response["success"] = true;
        response["count"] = static_cast<unsigned int>(apps.size());
        response["apps"] = Json::arrayValue;

        for (const auto& app : apps) {
            Json::Value app_json;
            app_json["package"] = app.package_name;
            app_json["name"] = app.app_name;
            if (!app.icon_url.empty()) {
                app_json["icon_url"] = app.icon_url;
            }
            response["apps"].append(app_json);
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[AppsController] Error getting popular apps: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get popular apps");
    }
}

// ============================================================================
// BULK ADD APPS
// ============================================================================

void AppsController::bulkAddApps(const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& callback,
                                std::string device_id) {
    try {
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        std::string category = (*json).get("category", "streaming").asString();

        bool success = AppsRepository::getInstance().addPopularAppsToDevice(device_id, category);

        if (!success) {
            sendError(std::move(callback), k500InternalServerError, "Failed to add apps");
            return;
        }

        // Get the updated app list
        auto apps = AppsRepository::getInstance().getAppsForDevice(device_id);

        Json::Value response;
        response["success"] = true;
        response["message"] = "Popular apps added successfully";
        response["category"] = category;
        response["total_apps"] = static_cast<unsigned int>(apps.size());

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[AppsController] Error bulk adding apps: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to bulk add apps");
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

Json::Value AppsController::appToJson(const DeviceApp& app) {
    Json::Value json;
    json["id"] = app.id;
    json["package"] = app.package_name;
    json["name"] = app.app_name;

    if (!app.icon_url.empty()) {
        json["icon_url"] = app.icon_url;
    }

    json["is_favorite"] = app.is_favorite;
    json["sort_order"] = app.sort_order;
    json["created_at"] = app.created_at;
    json["updated_at"] = app.updated_at;

    return json;
}

void AppsController::sendError(std::function<void(const HttpResponsePtr&)>&& callback,
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
