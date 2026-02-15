#include "api/DeviceController.h"
#include "api/CommandController.h"
#include "repositories/DeviceRepository.h"
#include <iostream>

namespace hms_firetv {

// ============================================================================
// LIST ALL DEVICES
// ============================================================================

void DeviceController::listDevices(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        auto devices = DeviceRepository::getInstance().getAllDevices();

        Json::Value response;
        response["success"] = true;
        response["count"] = static_cast<unsigned int>(devices.size());
        response["devices"] = Json::arrayValue;

        for (const auto& device : devices) {
            response["devices"].append(deviceToJson(device));
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[DeviceController] Listed " << devices.size() << " devices" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DeviceController] Error listing devices: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to list devices");
    }
}

// ============================================================================
// GET DEVICE BY ID
// ============================================================================

void DeviceController::getDeviceById(const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& callback,
                                std::string device_id) {
    try {
        auto device = DeviceRepository::getInstance().getDeviceById(device_id);

        if (!device.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["device"] = deviceToJson(device.value());

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[DeviceController] Retrieved device: " << device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DeviceController] Error getting device: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get device");
    }
}

// ============================================================================
// CREATE NEW DEVICE
// ============================================================================

void DeviceController::createDevice(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        // Validate required fields
        if (!json->isMember("device_id") || !json->isMember("name") ||
            !json->isMember("ip_address")) {
            sendError(std::move(callback), k400BadRequest,
                     "Missing required fields: device_id, name, ip_address");
            return;
        }

        // Create device object
        Device device;
        device.device_id = (*json)["device_id"].asString();
        device.name = (*json)["name"].asString();
        device.ip_address = (*json)["ip_address"].asString();
        device.api_key = (*json).get("api_key", "0987654321").asString();
        device.status = "offline";
        device.adb_enabled = (*json).get("adb_enabled", false).asBool();

        // Check if device already exists
        auto existing = DeviceRepository::getInstance().getDeviceById(device.device_id);
        if (existing.has_value()) {
            sendError(std::move(callback), k409Conflict, "Device already exists");
            return;
        }

        // Save to database (createDevice returns optional<Device>, not bool)
        auto created = DeviceRepository::getInstance().createDevice(device);

        if (!created.has_value()) {
            sendError(std::move(callback), k500InternalServerError, "Failed to create device");
            return;
        }

        // Update device with returned data (includes generated id)
        device = created.value();

        // Return created device
        Json::Value response;
        response["success"] = true;
        response["message"] = "Device created successfully";
        response["device"] = deviceToJson(device);

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k201Created);
        callback(resp);

        std::cout << "[DeviceController] Created device: " << device.device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DeviceController] Error creating device: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to create device");
    }
}

// ============================================================================
// UPDATE DEVICE
// ============================================================================

void DeviceController::updateDevice(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback,
                                   std::string device_id) {
    try {
        // Check if device exists
        auto existing = DeviceRepository::getInstance().getDeviceById(device_id);
        if (!existing.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            sendError(std::move(callback), k400BadRequest, "Invalid JSON body");
            return;
        }

        // Update fields (keep existing values if not provided)
        Device device = existing.value();

        if (json->isMember("name")) {
            device.name = (*json)["name"].asString();
        }
        if (json->isMember("ip_address")) {
            device.ip_address = (*json)["ip_address"].asString();
        }
        if (json->isMember("api_key")) {
            device.api_key = (*json)["api_key"].asString();
        }
        if (json->isMember("status")) {
            device.status = (*json)["status"].asString();
        }
        if (json->isMember("adb_enabled")) {
            device.adb_enabled = (*json)["adb_enabled"].asBool();
        }
        if (json->isMember("client_token")) {
            device.client_token = (*json)["client_token"].asString();
        }

        // Save to database
        bool success = DeviceRepository::getInstance().updateDevice(device);

        if (!success) {
            sendError(std::move(callback), k500InternalServerError, "Failed to update device");
            return;
        }

        // Invalidate cached Lightning client (IP or token may have changed)
        CommandController::invalidateClient(device_id);

        // Return updated device
        Json::Value response;
        response["success"] = true;
        response["message"] = "Device updated successfully";
        response["device"] = deviceToJson(device);

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[DeviceController] Updated device: " << device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DeviceController] Error updating device: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to update device");
    }
}

// ============================================================================
// DELETE DEVICE
// ============================================================================

void DeviceController::deleteDevice(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback,
                                   std::string device_id) {
    try {
        // Check if device exists
        auto existing = DeviceRepository::getInstance().getDeviceById(device_id);
        if (!existing.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        // Delete from database
        bool success = DeviceRepository::getInstance().deleteDevice(device_id);

        if (!success) {
            sendError(std::move(callback), k500InternalServerError, "Failed to delete device");
            return;
        }

        // Invalidate cached Lightning client
        CommandController::invalidateClient(device_id);

        // Return success response
        Json::Value response;
        response["success"] = true;
        response["message"] = "Device deleted successfully";

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

        std::cout << "[DeviceController] Deleted device: " << device_id << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DeviceController] Error deleting device: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to delete device");
    }
}

// ============================================================================
// GET DEVICE STATUS
// ============================================================================

void DeviceController::getDeviceStatus(const HttpRequestPtr& req,
                                      std::function<void(const HttpResponsePtr&)>&& callback,
                                      std::string device_id) {
    try {
        auto device = DeviceRepository::getInstance().getDeviceById(device_id);

        if (!device.has_value()) {
            sendError(std::move(callback), k404NotFound, "Device not found");
            return;
        }

        Json::Value response;
        response["success"] = true;
        response["device_id"] = device->device_id;
        response["name"] = device->name;
        response["status"] = device->status;
        response["ip_address"] = device->ip_address;
        response["is_paired"] = device->client_token.has_value() && !device->client_token.value().empty();
        response["adb_enabled"] = device->adb_enabled;

        if (device->last_seen_at.has_value()) {
            auto last_seen_time_t = std::chrono::system_clock::to_time_t(device->last_seen_at.value());
            char buffer[32];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&last_seen_time_t));
            response["last_seen_at"] = buffer;
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        callback(resp);

    } catch (const std::exception& e) {
        std::cerr << "[DeviceController] Error getting device status: " << e.what() << std::endl;
        sendError(std::move(callback), k500InternalServerError, "Failed to get device status");
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

Json::Value DeviceController::deviceToJson(const Device& device) {
    Json::Value json;
    json["id"] = device.id;
    json["device_id"] = device.device_id;
    json["name"] = device.name;
    json["ip_address"] = device.ip_address;
    json["api_key"] = device.api_key;
    json["status"] = device.status;
    json["adb_enabled"] = device.adb_enabled;
    json["is_paired"] = device.client_token.has_value() && !device.client_token.value().empty();

    // Include client_token only if present (sensitive data)
    json["has_client_token"] = device.client_token.has_value() && !device.client_token.value().empty();
    // Don't expose the actual token for security

    // Format timestamps
    auto created_time_t = std::chrono::system_clock::to_time_t(device.created_at);
    auto updated_time_t = std::chrono::system_clock::to_time_t(device.updated_at);
    char created_buffer[32], updated_buffer[32];
    strftime(created_buffer, sizeof(created_buffer), "%Y-%m-%d %H:%M:%S", localtime(&created_time_t));
    strftime(updated_buffer, sizeof(updated_buffer), "%Y-%m-%d %H:%M:%S", localtime(&updated_time_t));
    json["created_at"] = created_buffer;
    json["updated_at"] = updated_buffer;

    if (device.last_seen_at.has_value()) {
        auto last_seen_time_t = std::chrono::system_clock::to_time_t(device.last_seen_at.value());
        char last_seen_buffer[32];
        strftime(last_seen_buffer, sizeof(last_seen_buffer), "%Y-%m-%d %H:%M:%S", localtime(&last_seen_time_t));
        json["last_seen_at"] = last_seen_buffer;
    }

    return json;
}

void DeviceController::sendError(std::function<void(const HttpResponsePtr&)>&& callback,
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
