#pragma once

#include "models/Device.h"
#include "services/DatabaseService.h"
#include <vector>
#include <optional>

namespace hms_firetv {

/**
 * DeviceRepository - Data access layer for Fire TV devices
 *
 * Provides CRUD operations for fire_tv_devices table.
 * All methods are thread-safe and use DatabaseService singleton.
 */
class DeviceRepository {
public:
    /**
     * Get singleton instance
     */
    static DeviceRepository& getInstance();

    // Disable copy/move
    DeviceRepository(const DeviceRepository&) = delete;
    DeviceRepository& operator=(const DeviceRepository&) = delete;

    /**
     * Create a new device
     *
     * @param device Device data (id will be auto-generated)
     * @return Created device with id populated, or std::nullopt if failed
     */
    std::optional<Device> createDevice(const Device& device);

    /**
     * Get device by device_id (unique identifier like "living_room")
     *
     * @param device_id Unique device identifier
     * @return Device if found, std::nullopt if not found
     */
    std::optional<Device> getDeviceById(const std::string& device_id);

    /**
     * Get all devices
     *
     * @return Vector of all devices (empty if none or error)
     */
    std::vector<Device> getAllDevices();

    /**
     * Get devices by status
     *
     * @param status Status filter ("online", "offline", "pairing", "error")
     * @return Vector of devices matching status
     */
    std::vector<Device> getDevicesByStatus(const std::string& status);

    /**
     * Update device
     *
     * Updates mutable fields: name, ip_address, status, adb_enabled, last_seen_at
     *
     * @param device Device with updated fields
     * @return true if updated, false if failed
     */
    bool updateDevice(const Device& device);

    /**
     * Delete device
     *
     * @param device_id Device identifier to delete
     * @return true if deleted, false if failed
     */
    bool deleteDevice(const std::string& device_id);

    /**
     * Set pairing PIN for device
     *
     * @param device_id Device identifier
     * @param pin_code PIN code to set
     * @param expires_in_seconds Seconds until PIN expires (default: 300 = 5 min)
     * @return true if set, false if failed
     */
    bool setPairingPin(const std::string& device_id, const std::string& pin_code,
                       int expires_in_seconds = 300);

    /**
     * Verify PIN and set client token
     *
     * @param device_id Device identifier
     * @param pin_code PIN to verify
     * @param client_token Token to set if PIN is valid
     * @return true if PIN valid and token set, false otherwise
     */
    bool verifyPinAndSetToken(const std::string& device_id, const std::string& pin_code,
                               const std::string& client_token);

    /**
     * Clear pairing data (reset device)
     *
     * Clears client_token, pin_code, pin_expires_at
     *
     * @param device_id Device identifier
     * @return true if cleared, false if failed
     */
    bool clearPairing(const std::string& device_id);

    /**
     * Update last seen timestamp and status
     *
     * @param device_id Device identifier
     * @param status New status ("online", "offline")
     * @return true if updated, false if failed
     */
    bool updateLastSeen(const std::string& device_id, const std::string& status = "online");

    /**
     * Check if device exists
     *
     * @param device_id Device identifier
     * @return true if exists, false otherwise
     */
    bool deviceExists(const std::string& device_id);

private:
    /**
     * Private constructor for singleton
     */
    DeviceRepository() = default;

    /**
     * Parse database row into Device struct
     */
    Device parseDeviceFromRow(const pqxx::row& row);
};

} // namespace hms_firetv
