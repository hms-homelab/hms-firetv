#pragma once

#include <drogon/HttpController.h>
#include "repositories/DeviceRepository.h"
#include "models/Device.h"

using namespace drogon;

namespace hms_firetv {

/**
 * DeviceController - REST API for device management
 *
 * Endpoints:
 * - GET    /api/devices          - List all devices
 * - GET    /api/devices/:id      - Get device by ID
 * - POST   /api/devices          - Create new device
 * - PUT    /api/devices/:id      - Update device
 * - DELETE /api/devices/:id      - Delete device
 */
class DeviceController : public drogon::HttpController<DeviceController> {
public:
    METHOD_LIST_BEGIN

    // List all devices
    ADD_METHOD_TO(DeviceController::listDevices, "/api/devices", Get);

    // Get device by ID
    ADD_METHOD_TO(DeviceController::getDeviceById, "/api/devices/{1}", Get);

    // Create new device
    ADD_METHOD_TO(DeviceController::createDevice, "/api/devices", Post);

    // Update device
    ADD_METHOD_TO(DeviceController::updateDevice, "/api/devices/{1}", Put);

    // Delete device
    ADD_METHOD_TO(DeviceController::deleteDevice, "/api/devices/{1}", Delete);

    // Get device status
    ADD_METHOD_TO(DeviceController::getDeviceStatus, "/api/devices/{1}/status", Get);

    METHOD_LIST_END

    /**
     * List all devices
     * GET /api/devices
     */
    void listDevices(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * Get device by ID
     * GET /api/devices/:id
     */
    void getDeviceById(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback,
                  std::string device_id);

    /**
     * Create new device
     * POST /api/devices
     * Body: {"device_id": "...", "name": "...", "ip_address": "...", "api_key": "..."}
     */
    void createDevice(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);

    /**
     * Update device
     * PUT /api/devices/:id
     * Body: {"name": "...", "ip_address": "...", ...}
     */
    void updateDevice(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     std::string device_id);

    /**
     * Delete device
     * DELETE /api/devices/:id
     */
    void deleteDevice(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     std::string device_id);

    /**
     * Get device status
     * GET /api/devices/:id/status
     */
    void getDeviceStatus(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback,
                        std::string device_id);

private:
    /**
     * Convert Device model to JSON
     */
    Json::Value deviceToJson(const Device& device);

    /**
     * Send error response
     */
    void sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                   HttpStatusCode status,
                   const std::string& message);
};

} // namespace hms_firetv
