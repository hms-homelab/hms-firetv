#include "api/PairingController.h"
#include "services/DatabaseService.h"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

namespace hms_firetv {

// ============================================================================
// START PAIRING
// ============================================================================

void PairingController::startPairing(const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback,
                                    std::string device_id) {
    try {
        // Get device
        auto device = DeviceRepository::getInstance().getDeviceById(device_id);
        if (!device.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        // Check if already paired
        if (device->client_token.has_value() && !device->client_token.value().empty()) {
            sendError(std::move(callback), k409Conflict,
                     "Device already paired. Use /pair/reset to unpair first.");
            return;
        }

        // Get Lightning client
        auto client = getClient(device_id);
        if (!client) {
            sendError(std::move(callback), k500InternalServerError, "Failed to create client");
            return;
        }

        // Trigger PIN display on TV — the TV generates and shows its own PIN
        if (!client->displayPin()) {
            sendError(std::move(callback), k500InternalServerError,
                     "Failed to display PIN on TV. Check device connectivity.");
            return;
        }

        // Store pairing session expiry (5 minutes) — no pin_code stored, TV owns the PIN
        auto now = std::chrono::system_clock::now();
        auto expires = now + std::chrono::minutes(5);
        auto expires_time_t = std::chrono::system_clock::to_time_t(expires);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&expires_time_t), "%Y-%m-%d %H:%M:%S");
        std::string expires_at = oss.str();

        std::string query = "UPDATE fire_tv_devices "
                          "SET pin_code = NULL, pin_expires_at = $1, status = 'pairing' "
                          "WHERE device_id = $2";

        DatabaseService::getInstance().executeQueryParams(query, {expires_at, device_id});

        // Return response
        Json::Value response;
        response["success"] = true;
        response["message"] = "PIN displayed on TV. Enter the PIN to complete pairing.";
        response["device_id"] = device_id;
        response["pin_expires_at"] = expires_at;
        response["expires_in_seconds"] = 300;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[PairingController] Started pairing for " << device_id
                  << ", PIN expires at " << expires_at << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[PairingController] Error starting pairing: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to start pairing");
    }
}

// ============================================================================
// VERIFY PAIRING
// ============================================================================

void PairingController::verifyPairing(const HttpRequestPtr& req,
                                     std::function<void(const HttpResponsePtr&)>&& callback,
                                     std::string device_id) {
    try {
        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        if (!json->isMember("pin")) {
            sendError(std::move(callback), k400BadRequest, "Missing 'pin' field");
            return;
        }

        std::string entered_pin = (*json)["pin"].asString();

        // Get device
        auto device = DeviceRepository::getInstance().getDeviceById(device_id);
        if (!device.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        // Check if pairing session is in progress
        if (device->status != "pairing") {
            sendError(std::move(callback), k400BadRequest,
                     "No pairing in progress. Start pairing first.");
            return;
        }

        // Check if pairing session has expired
        std::string expires_at_str = "";
        if (device->pin_expires_at.has_value()) {
            auto expires_time_t = std::chrono::system_clock::to_time_t(device->pin_expires_at.value());
            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&expires_time_t), "%Y-%m-%d %H:%M:%S");
            expires_at_str = oss.str();
        }

        if (isPinExpired(expires_at_str)) {
            std::string clear_query = "UPDATE fire_tv_devices "
                                     "SET pin_expires_at = NULL, status = 'offline' "
                                     "WHERE device_id = $1";
            DatabaseService::getInstance().executeQueryParams(clear_query, {device_id});

            sendError(std::move(callback), k410Gone, "Pairing session expired. Start pairing again.");
            return;
        }

        // Get Lightning client
        auto client = getClient(device_id);
        if (!client) {
            sendError(std::move(callback), k500InternalServerError, "Failed to create client");
            return;
        }

        // Complete pairing with Fire TV — retry up to 3 times in case TV returns "OK" first
        std::string client_token;
        for (int attempt = 0; attempt < 3 && client_token.empty(); ++attempt) {
            if (attempt > 0) std::this_thread::sleep_for(std::chrono::seconds(1));
            client_token = client->verifyPin(entered_pin);
        }

        if (client_token.empty()) {
            sendError(std::move(callback), k500InternalServerError,
                     "Failed to complete pairing with device");
            return;
        }

        // Store client token and clear PIN
        std::string update_query = "UPDATE fire_tv_devices "
                                  "SET client_token = $1, pin_code = NULL, pin_expires_at = NULL, "
                                  "status = 'online' "
                                  "WHERE device_id = $2";

        DatabaseService::getInstance().executeQueryParams(update_query, {client_token, device_id});

        // Update client with token
        client->setClientToken(client_token);

        // Return success
        Json::Value response;
        response["success"] = true;
        response["message"] = "Device paired successfully";
        response["device_id"] = device_id;
        response["is_paired"] = true;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[PairingController] Successfully paired device: " << device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[PairingController] Error verifying pairing: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to verify pairing");
    }
}

// ============================================================================
// RESET PAIRING
// ============================================================================

void PairingController::resetPairing(const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback,
                                    std::string device_id) {
    try {
        // Get device
        auto device = DeviceRepository::getInstance().getDeviceById(device_id);
        if (!device.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        // Clear pairing data
        std::string query = "UPDATE fire_tv_devices "
                          "SET client_token = NULL, pin_code = NULL, pin_expires_at = NULL, "
                          "status = 'offline' "
                          "WHERE device_id = $1";

        DatabaseService::getInstance().executeQueryParams(query, {device_id});

        // Clear cached client
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.erase(device_id);
        }

        // Return success
        Json::Value response;
        response["success"] = true;
        response["message"] = "Device unpaired successfully";
        response["device_id"] = device_id;
        response["is_paired"] = false;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[PairingController] Reset pairing for device: " << device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[PairingController] Error resetting pairing: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to reset pairing");
    }
}

// ============================================================================
// GET PAIRING STATUS
// ============================================================================

void PairingController::getPairingStatus(const HttpRequestPtr& req,
                                        std::function<void(const HttpResponsePtr&)>&& callback,
                                        std::string device_id) {
    try {
        // Get device
        auto device = DeviceRepository::getInstance().getDeviceById(device_id);
        if (!device.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["device_id"] = device_id;
        response["is_paired"] = device->client_token.has_value() && !device->client_token.value().empty();

        // Check if pairing is in progress
        std::string expires_at_str = "";
        if (device->pin_expires_at.has_value()) {
            auto expires_time_t = std::chrono::system_clock::to_time_t(device->pin_expires_at.value());
            std::ostringstream oss;
            oss << std::put_time(std::gmtime(&expires_time_t), "%Y-%m-%d %H:%M:%S");
            expires_at_str = oss.str();
        }

        bool pairing_in_progress = device->pin_code.has_value() &&
                                   !device->pin_code.value().empty() &&
                                   !isPinExpired(expires_at_str);

        response["pairing_in_progress"] = pairing_in_progress;

        if (pairing_in_progress) {
            response["pin_expires_at"] = expires_at_str;
        }

        response["status"] = device->status;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[PairingController] Error getting pairing status: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get pairing status");
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

std::string PairingController::generatePin() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);

    int pin_number = dis(gen);

    std::ostringstream oss;
    oss << std::setw(6) << std::setfill('0') << pin_number;
    return oss.str();
}

bool PairingController::isPinExpired(const std::string& expires_at) {
    if (expires_at.empty()) {
        return false;  // no expiry set — session still valid
    }

    try {
        std::tm tm = {};
        std::istringstream ss(expires_at);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

        if (ss.fail()) {
            return true;
        }

        // timegm interprets tm as UTC, matching how we stored it with gmtime
        auto expires_time = std::chrono::system_clock::from_time_t(timegm(&tm));
        auto now = std::chrono::system_clock::now();

        return now >= expires_time;

    } catch (const std::exception& e) {
        std::cerr << "[PairingController] Error parsing expiration time: " << e.what() << std::endl;
        return true;
    }
}

std::shared_ptr<LightningClient> PairingController::getClient(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    // Check if client already exists
    auto it = clients_.find(device_id);
    if (it != clients_.end()) {
        return it->second;
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
    if (device->client_token.has_value() && !device->client_token.value().empty()) {
        client->setClientToken(device->client_token.value());
    }

    // Cache client
    clients_[device_id] = client;

    return client;
}

void PairingController::sendError(std::function<void(const HttpResponsePtr&)>&& callback,
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
