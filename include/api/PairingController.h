#pragma once

#include <drogon/HttpController.h>
#include "clients/LightningClient.h"
#include "repositories/DeviceRepository.h"

using namespace drogon;

namespace hms_firetv {

/**
 * PairingController - REST API for Fire TV device pairing
 *
 * Pairing Flow:
 * 1. POST /api/devices/:id/pair/start   - Wake device and display PIN on TV
 * 2. POST /api/devices/:id/pair/verify  - User enters PIN, complete pairing
 * 3. POST /api/devices/:id/pair/reset   - Clear pairing and start over
 *
 * Endpoints:
 * - POST /api/devices/:id/pair/start    - Start pairing (display PIN)
 * - POST /api/devices/:id/pair/verify   - Verify PIN and complete pairing
 * - POST /api/devices/:id/pair/reset    - Reset/unpair device
 * - GET  /api/devices/:id/pair/status   - Get pairing status
 */
class PairingController : public drogon::HttpController<PairingController> {
public:
    METHOD_LIST_BEGIN

    // Start pairing (display PIN on TV)
    ADD_METHOD_TO(PairingController::startPairing, "/api/devices/{1}/pair/start", Post);

    // Verify PIN and complete pairing
    ADD_METHOD_TO(PairingController::verifyPairing, "/api/devices/{1}/pair/verify", Post);

    // Reset/unpair device
    ADD_METHOD_TO(PairingController::resetPairing, "/api/devices/{1}/pair/reset", Post);

    // Get pairing status
    ADD_METHOD_TO(PairingController::getPairingStatus, "/api/devices/{1}/pair/status", Get);

    METHOD_LIST_END

    /**
     * Start pairing process
     * POST /api/devices/:id/pair/start
     *
     * This will:
     * 1. Wake the device
     * 2. Generate a 6-digit PIN
     * 3. Display PIN on TV screen
     * 4. Store PIN in database with expiration (5 minutes)
     */
    void startPairing(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     std::string device_id);

    /**
     * Verify PIN and complete pairing
     * POST /api/devices/:id/pair/verify
     * Body: {"pin": "123456"}
     *
     * This will:
     * 1. Verify PIN matches and hasn't expired
     * 2. Complete pairing with Fire TV
     * 3. Store client token in database
     * 4. Clear PIN
     */
    void verifyPairing(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      std::string device_id);

    /**
     * Reset/unpair device
     * POST /api/devices/:id/pair/reset
     *
     * This will:
     * 1. Clear client token
     * 2. Clear PIN
     * 3. Update device status to offline
     */
    void resetPairing(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     std::string device_id);

    /**
     * Get pairing status
     * GET /api/devices/:id/pair/status
     *
     * Returns:
     * - is_paired: boolean
     * - pairing_in_progress: boolean
     * - pin_expires_at: timestamp (if pairing in progress)
     */
    void getPairingStatus(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback,
                         std::string device_id);

private:
    /**
     * Generate random 6-digit PIN
     */
    std::string generatePin();

    /**
     * Check if PIN has expired
     */
    bool isPinExpired(const std::string& expires_at);

    /**
     * Get or create Lightning client for device
     */
    std::shared_ptr<LightningClient> getClient(const std::string& device_id);

    /**
     * Send error response
     */
    void sendError(std::function<void(const HttpResponsePtr&)>&& callback,
                   HttpStatusCode status,
                   const std::string& message);

    // Cache of Lightning clients per device
    std::map<std::string, std::shared_ptr<LightningClient>> clients_;
    std::mutex clients_mutex_;
};

} // namespace hms_firetv
