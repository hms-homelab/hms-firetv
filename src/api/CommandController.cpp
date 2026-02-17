#include "api/CommandController.h"
#include "services/DatabaseService.h"
#include <drogon/HttpClient.h>
#include <iostream>
#include <chrono>

using namespace drogon;

namespace hms_firetv {

// Static cache initialization
LRUCache<std::string, std::shared_ptr<LightningClient>> CommandController::clients_cache_{100, 3600};

// Static background logger initialization
BackgroundLogger CommandController::background_logger_{1000};  // Max 1000 pending logs
std::once_flag CommandController::logger_init_flag_;

// ============================================================================
// SEND GENERIC COMMAND
// ============================================================================

void CommandController::sendCommand(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback,
                                   std::string device_id) {
    try {
        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("command")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'command' field");
            return;
        }

        std::string command = (*json)["command"].asString();

        // Route to specific handler based on command
        if (command == "navigate") {
            navigate(req, std::move(callback), device_id);
        } else if (command == "media_play" || command == "media_pause") {
            mediaControl(req, std::move(callback), device_id);
        } else if (command == "volume_up" || command == "volume_down" || command == "mute") {
            volumeControl(req, std::move(callback), device_id);
        } else if (command == "launch_app") {
            launchApp(req, std::move(callback), device_id);
        } else if (command == "send_text") {
            sendText(req, std::move(callback), device_id);
        } else {
            sendError(std::move(callback), k400BadRequest, "Unknown command: " + command);
        }

    } catch (const std::exception& e) {
        std::cerr << "[CommandController] Error in sendCommand: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Command execution failed");
    }
}

// ============================================================================
// NAVIGATION
// ============================================================================

void CommandController::navigate(const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& callback,
                                std::string device_id) {
    try {
        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        std::string action;
        if (json->isMember("action")) {
            action = (*json)["action"].asString();
        } else if (json->isMember("direction")) {
            action = (*json)["direction"].asString();
        } else {
            sendError(std::move(callback), k400BadRequest, "Missing 'action' or 'direction' field");
            return;
        }

        // Map action names to Lightning protocol format
        std::string lightning_action = action;
        if (action == "up" || action == "down" || action == "left" || action == "right") {
            lightning_action = "dpad_" + action;
        }

        // Build endpoint for Fire TV Lightning API
        std::string endpoint = "/v1/FireTV?action=" + lightning_action;
        Json::Value fire_tv_body;  // Empty body for navigation

        // Make async Fire TV API call (non-blocking)
        makeAsyncFireTVCall(device_id, endpoint, fire_tv_body,
            [this, device_id, action, callback = std::move(callback)]
            (bool success, int response_time_ms, const std::string& error_msg) mutable {

            // Log to database (async via background logger)
            Json::Value command_data;
            command_data["action"] = action;
            logCommand(device_id, "navigation", command_data, success, response_time_ms, error_msg);

            // Send response to client
            Json::Value response;
            response["success"] = success;
            response["message"] = success ? "Navigation command sent" : "Navigation command failed";
            response["action"] = action;
            response["response_time_ms"] = response_time_ms;

            if (!error_msg.empty()) {
                response["error"] = error_msg;
            }

            auto resp = HttpResponse::newHttpJsonResponse(response);
            resp->setStatusCode(success ? k200OK : k500InternalServerError);
            callback(resp);

            std::cout << "[CommandController] Navigate " << action << " on " << device_id
                      << " (" << response_time_ms << "ms)" << std::endl;
        });

    } catch (const std::exception& e) {
        std::cerr << "[CommandController] Error in navigate: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Navigation failed");
    }
}

// ============================================================================
// MEDIA CONTROL
// ============================================================================

void CommandController::mediaControl(const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback,
                                    std::string device_id) {
    try {
        auto client = getClient(device_id);
        if (!client) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("action")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'action' field");
            return;
        }

        std::string action = (*json)["action"].asString();

        // Execute media command
        CommandResult result = client->sendMediaCommand(action);

        // Log to database
        Json::Value command_data;
        command_data["action"] = action;
        logCommand(device_id, "media", command_data, result.success, result.response_time_ms,
                  result.error.has_value() ? result.error.value() : "");

        Json::Value response;
        response["success"] = result.success;
        response["message"] = result.success ? "Media command sent" : "Media command failed";
        response["action"] = action;
        response["response_time_ms"] = result.response_time_ms;

        if (result.error.has_value()) {
            response["error"] = result.error.value();
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(result.success ? k200OK : k500InternalServerError);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[CommandController] Error in mediaControl: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Media control failed");
    }
}

// ============================================================================
// VOLUME CONTROL
// ============================================================================

void CommandController::volumeControl(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback,
                                     std::string device_id) {
    try {
        auto client = getClient(device_id);
        if (!client) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("action")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'action' field");
            return;
        }

        std::string action = (*json)["action"].asString();

        // Volume control uses navigation commands
        CommandResult result = client->sendNavigationCommand(action);

        // Log to database
        Json::Value command_data;
        command_data["action"] = action;
        logCommand(device_id, "volume", command_data, result.success, result.response_time_ms,
                  result.error.has_value() ? result.error.value() : "");

        Json::Value response;
        response["success"] = result.success;
        response["message"] = result.success ? "Volume command sent" : "Volume command failed";
        response["action"] = action;
        response["response_time_ms"] = result.response_time_ms;

        if (result.error.has_value()) {
            response["error"] = result.error.value();
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(result.success ? k200OK : k500InternalServerError);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[CommandController] Error in volumeControl: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Volume control failed");
    }
}

// ============================================================================
// LAUNCH APP
// ============================================================================

void CommandController::launchApp(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback,
                                 std::string device_id) {
    try {
        auto client = getClient(device_id);
        if (!client) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("package")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'package' field");
            return;
        }

        std::string package = (*json)["package"].asString();

        // Execute launch app command
        CommandResult result = client->launchApp(package);

        // Log to database
        Json::Value command_data;
        command_data["package"] = package;
        logCommand(device_id, "app", command_data, result.success, result.response_time_ms,
                  result.error.has_value() ? result.error.value() : "");

        Json::Value response;
        response["success"] = result.success;
        response["message"] = result.success ? "App launched" : "App launch failed";
        response["package"] = package;
        response["response_time_ms"] = result.response_time_ms;

        if (result.error.has_value()) {
            response["error"] = result.error.value();
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(result.success ? k200OK : k500InternalServerError);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[CommandController] Error in launchApp: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "App launch failed");
    }
}

// ============================================================================
// SEND TEXT
// ============================================================================

void CommandController::sendText(const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& callback,
                                std::string device_id) {
    try {
        auto client = getClient(device_id);
        if (!client) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("text")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'text' field");
            return;
        }

        std::string text = (*json)["text"].asString();

        // Execute send text command
        CommandResult result = client->sendKeyboardInput(text);

        // Log to database
        Json::Value command_data;
        command_data["text"] = text;
        logCommand(device_id, "text", command_data, result.success, result.response_time_ms,
                  result.error.has_value() ? result.error.value() : "");

        Json::Value response;
        response["success"] = result.success;
        response["message"] = result.success ? "Text sent" : "Text send failed";
        response["text_length"] = static_cast<unsigned int>(text.length());
        response["response_time_ms"] = result.response_time_ms;

        if (result.error.has_value()) {
            response["error"] = result.error.value();
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(result.success ? k200OK : k500InternalServerError);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[CommandController] Error in sendText: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Text send failed");
    }
}

// ============================================================================
// GET COMMAND HISTORY
// ============================================================================

void CommandController::getHistory(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback,
                                  std::string device_id) {
    try {
        // Get query parameters
        int limit = 50;  // default
        int offset = 0;  // default

        auto params = req->getParameters();
        if (params.find("limit") != params.end()) {
            limit = std::stoi(params.at("limit"));
        }
        if (params.find("offset") != params.end()) {
            offset = std::stoi(params.at("offset"));
        }

        // Query database
        std::string query = "SELECT id, device_id, command_type, command_data::text, "
                          "success, response_time_ms, error_message, created_at "
                          "FROM command_history "
                          "WHERE device_id = $1 "
                          "ORDER BY created_at DESC "
                          "LIMIT $2 OFFSET $3";

        auto result = DatabaseService::getInstance().executeQueryParams(
            query, {device_id, std::to_string(limit), std::to_string(offset)}
        );

        Json::Value response;
        response["success"] = true;
        response["device_id"] = device_id;
        response["limit"] = limit;
        response["offset"] = offset;
        response["count"] = static_cast<unsigned int>(result.size());
        response["history"] = Json::arrayValue;

        for (const auto& row : result) {
            Json::Value entry;
            entry["id"] = row["id"].as<int>();
            entry["command_type"] = row["command_type"].as<std::string>();
            entry["command_data"] = row["command_data"].as<std::string>();
            entry["success"] = row["success"].as<bool>();

            if (!row["response_time_ms"].is_null()) {
                entry["response_time_ms"] = row["response_time_ms"].as<int>();
            }

            if (!row["error_message"].is_null() && !row["error_message"].as<std::string>().empty()) {
                entry["error_message"] = row["error_message"].as<std::string>();
            }

            entry["created_at"] = row["created_at"].as<std::string>();

            response["history"].append(entry);
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[CommandController] Error getting history: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get history");
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void CommandController::makeAsyncFireTVCall(const std::string& device_id,
                                             const std::string& endpoint,
                                             const Json::Value& json_body,
                                             std::function<void(bool, int, const std::string&)> completion_callback) {
    // Get device from database
    auto device = DeviceRepository::getInstance().getDeviceById(device_id);
    if (!device.has_value()) {
        completion_callback(false, 0, "Device not found");
        return;
    }

    // Build Fire TV URL (HTTPS on port 8080)
    // Note: Fire TV uses self-signed SSL certificates
    std::string url = "https://" + device->ip_address + ":8080";

    // Create async HTTP client with SSL verification disabled (last param = false)
    // Parameters: hostString, loop, useOldTLS, validateCert
    auto client = HttpClient::newHttpClient(url, nullptr, false, false);
    client->enableCookies();

    // Create POST request
    auto req = HttpRequest::newHttpJsonRequest(json_body);
    req->setMethod(Post);
    req->setPath(endpoint);

    // Add Lightning API headers
    req->addHeader("X-Api-Key", device->api_key);
    if (device->client_token.has_value()) {
        req->addHeader("X-Client-Token", device->client_token.value());
    }
    req->addHeader("Content-Type", "application/json");

    // Record start time
    auto start_time = std::chrono::steady_clock::now();

    // Send async request
    client->sendRequest(req,
        [completion_callback, start_time, device_id]
        (ReqResult result, const HttpResponsePtr& response) {

        // Calculate response time
        auto end_time = std::chrono::steady_clock::now();
        int response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        ).count();

        // Handle response
        if (result == ReqResult::Ok && response) {
            int status_code = static_cast<int>(response->getStatusCode());
            bool success = (status_code >= 200 && status_code < 300);

            std::string error_msg;
            if (!success) {
                error_msg = "HTTP " + std::to_string(status_code);
            }

            completion_callback(success, response_time_ms, error_msg);

        } else {
            // Request failed (timeout, network error, etc.)
            std::string error_msg;
            switch (result) {
                case ReqResult::Timeout:
                    error_msg = "Timeout";
                    break;
                case ReqResult::NetworkFailure:
                    error_msg = "Network failure";
                    break;
                case ReqResult::BadResponse:
                    error_msg = "Bad response";
                    break;
                case ReqResult::BadServerAddress:
                    error_msg = "Bad server address";
                    break;
                case ReqResult::HandshakeError:
                    error_msg = "SSL handshake error";
                    break;
                default:
                    error_msg = "Unknown error";
            }

            std::cerr << "[CommandController] Fire TV API call failed for " << device_id
                      << ": " << error_msg << " (" << response_time_ms << "ms)" << std::endl;

            completion_callback(false, response_time_ms, error_msg);
        }
    }, FIRETV_API_TIMEOUT_SECONDS);  // Pass timeout to sendRequest
}

std::shared_ptr<LightningClient> CommandController::getClient(const std::string& device_id) {
    // Check cache first
    auto cached = clients_cache_.get(device_id);
    if (cached.has_value()) {
        return cached.value();
    }

    // Get device from database
    auto device = DeviceRepository::getInstance().getDeviceById(device_id);
    if (!device.has_value()) {
        return nullptr;
    }

    // Create new client
    auto client = std::make_shared<LightningClient>(
        device->ip_address,
        device->api_key
    );

    // Set client token if device is paired
    if (device->client_token.has_value()) {
        client->setClientToken(device->client_token.value());
    }

    // Cache client (will evict LRU if at capacity)
    clients_cache_.put(device_id, client);

    return client;
}

void CommandController::invalidateClient(const std::string& device_id) {
    clients_cache_.remove(device_id);
    std::cout << "[CommandController] Invalidated cached client for device: " << device_id << std::endl;
}

void CommandController::initBackgroundLogger() {
    std::call_once(logger_init_flag_, []() {
        background_logger_.start();
        std::cout << "[CommandController] Background logger initialized" << std::endl;
    });
}

void CommandController::shutdownBackgroundLogger() {
    background_logger_.stop();
    std::cout << "[CommandController] Background logger shutdown complete" << std::endl;
}

void CommandController::logCommand(const std::string& device_id,
                                  const std::string& command_type,
                                  const Json::Value& command_data,
                                  bool success,
                                  int response_time_ms,
                                  const std::string& error_message) {
    // Ensure background logger is started
    initBackgroundLogger();

    // Serialize JSON once in foreground to avoid race conditions
    Json::StreamWriterBuilder writer;
    std::string command_data_str = Json::writeString(writer, command_data);

    // Enqueue log task to background thread (non-blocking)
    bool enqueued = background_logger_.enqueue([=]() {
        try {
            std::string query = "INSERT INTO command_history "
                              "(device_id, command_type, command_data, success, response_time_ms, error_message) "
                              "VALUES ($1, $2, $3::jsonb, $4, $5, $6)";

            DatabaseService::getInstance().executeQueryParams(query, {
                device_id,
                command_type,
                command_data_str,
                success ? "true" : "false",
                std::to_string(response_time_ms),
                error_message
            });

        } catch (const std::exception& e) {
            std::cerr << "[CommandController] Failed to log command: " << e.what() << std::endl;
            // Don't throw - logging failures shouldn't crash the worker thread
        }
    });

    if (!enqueued) {
        std::cerr << "[CommandController] Warning: Log queue full, dropped entry for "
                  << device_id << std::endl;
    }
}

void CommandController::sendError(std::function<void(const HttpResponsePtr&)>&& callback,
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
