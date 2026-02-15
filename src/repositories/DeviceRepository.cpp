#include "repositories/DeviceRepository.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace hms_firetv {

// ============================================================================
// SINGLETON
// ============================================================================

DeviceRepository& DeviceRepository::getInstance() {
    static DeviceRepository instance;
    return instance;
}

// ============================================================================
// CREATE
// ============================================================================

std::optional<Device> DeviceRepository::createDevice(const Device& device) {
    std::cout << "[DeviceRepository] Creating device: " << device.device_id << std::endl;

    std::ostringstream query;
    query << "INSERT INTO fire_tv_devices "
          << "(device_id, name, ip_address, api_key, status, adb_enabled, created_at, updated_at) "
          << "VALUES ("
          << "'" << device.device_id << "', "
          << "'" << device.name << "', "
          << "'" << device.ip_address << "', "
          << "'" << device.api_key << "', "
          << "'" << device.status << "', "
          << (device.adb_enabled ? "true" : "false") << ", "
          << "NOW(), NOW()) "
          << "RETURNING id";

    auto result = DatabaseService::getInstance().executeQuery(query.str());

    if (result.empty()) {
        std::cerr << "[DeviceRepository] Failed to create device" << std::endl;
        return std::nullopt;
    }

    // Get the created device
    return getDeviceById(device.device_id);
}

// ============================================================================
// READ
// ============================================================================

std::optional<Device> DeviceRepository::getDeviceById(const std::string& device_id) {
    std::ostringstream query;
    query << "SELECT * FROM fire_tv_devices WHERE device_id = '" << device_id << "'";

    auto result = DatabaseService::getInstance().executeQuery(query.str());

    if (result.empty()) {
        return std::nullopt;
    }

    return parseDeviceFromRow(result[0]);
}

std::vector<Device> DeviceRepository::getAllDevices() {
    std::string query = "SELECT * FROM fire_tv_devices ORDER BY created_at DESC";
    auto result = DatabaseService::getInstance().executeQuery(query);

    std::vector<Device> devices;
    for (const auto& row : result) {
        devices.push_back(parseDeviceFromRow(row));
    }

    return devices;
}

std::vector<Device> DeviceRepository::getDevicesByStatus(const std::string& status) {
    std::ostringstream query;
    query << "SELECT * FROM fire_tv_devices WHERE status = '" << status
          << "' ORDER BY created_at DESC";

    auto result = DatabaseService::getInstance().executeQuery(query.str());

    std::vector<Device> devices;
    for (const auto& row : result) {
        devices.push_back(parseDeviceFromRow(row));
    }

    return devices;
}

// ============================================================================
// UPDATE
// ============================================================================

bool DeviceRepository::updateDevice(const Device& device) {
    std::cout << "[DeviceRepository] Updating device: " << device.device_id << std::endl;

    std::ostringstream query;
    query << "UPDATE fire_tv_devices SET "
          << "name = '" << device.name << "', "
          << "ip_address = '" << device.ip_address << "', "
          << "status = '" << device.status << "', "
          << "adb_enabled = " << (device.adb_enabled ? "true" : "false") << ", "
          << "updated_at = NOW() "
          << "WHERE device_id = '" << device.device_id << "'";

    return DatabaseService::getInstance().executeCommand(query.str());
}

bool DeviceRepository::updateLastSeen(const std::string& device_id, const std::string& status) {
    std::ostringstream query;
    query << "UPDATE fire_tv_devices SET "
          << "last_seen_at = NOW(), "
          << "status = '" << status << "', "
          << "updated_at = NOW() "
          << "WHERE device_id = '" << device_id << "'";

    return DatabaseService::getInstance().executeCommand(query.str());
}

// ============================================================================
// DELETE
// ============================================================================

bool DeviceRepository::deleteDevice(const std::string& device_id) {
    std::cout << "[DeviceRepository] Deleting device: " << device_id << std::endl;

    std::ostringstream query;
    query << "DELETE FROM fire_tv_devices WHERE device_id = '" << device_id << "'";

    return DatabaseService::getInstance().executeCommand(query.str());
}

// ============================================================================
// PAIRING OPERATIONS
// ============================================================================

bool DeviceRepository::setPairingPin(const std::string& device_id, const std::string& pin_code,
                                      int expires_in_seconds) {
    std::cout << "[DeviceRepository] Setting PIN for device: " << device_id << std::endl;

    std::ostringstream query;
    query << "UPDATE fire_tv_devices SET "
          << "pin_code = '" << pin_code << "', "
          << "pin_expires_at = NOW() + INTERVAL '" << expires_in_seconds << " seconds', "
          << "status = 'pairing', "
          << "updated_at = NOW() "
          << "WHERE device_id = '" << device_id << "'";

    return DatabaseService::getInstance().executeCommand(query.str());
}

bool DeviceRepository::verifyPinAndSetToken(const std::string& device_id,
                                              const std::string& pin_code,
                                              const std::string& client_token) {
    std::cout << "[DeviceRepository] Verifying PIN for device: " << device_id << std::endl;

    // First, verify the PIN is valid
    std::ostringstream verify_query;
    verify_query << "SELECT pin_code, pin_expires_at FROM fire_tv_devices "
                 << "WHERE device_id = '" << device_id << "'";

    auto result = DatabaseService::getInstance().executeQuery(verify_query.str());

    if (result.empty()) {
        std::cerr << "[DeviceRepository] Device not found" << std::endl;
        return false;
    }

    auto row = result[0];
    if (row["pin_code"].is_null()) {
        std::cerr << "[DeviceRepository] No PIN set" << std::endl;
        return false;
    }

    std::string stored_pin = row["pin_code"].as<std::string>();
    if (stored_pin != pin_code) {
        std::cerr << "[DeviceRepository] PIN mismatch" << std::endl;
        return false;
    }

    // Check PIN expiration
    if (!row["pin_expires_at"].is_null()) {
        // TODO: Add proper timestamp comparison
        // For now, assume PIN is valid
    }

    // PIN is valid, set the client token
    std::ostringstream update_query;
    update_query << "UPDATE fire_tv_devices SET "
                 << "client_token = '" << client_token << "', "
                 << "pin_code = NULL, "
                 << "pin_expires_at = NULL, "
                 << "status = 'online', "
                 << "updated_at = NOW() "
                 << "WHERE device_id = '" << device_id << "'";

    return DatabaseService::getInstance().executeCommand(update_query.str());
}

bool DeviceRepository::clearPairing(const std::string& device_id) {
    std::cout << "[DeviceRepository] Clearing pairing for device: " << device_id << std::endl;

    std::ostringstream query;
    query << "UPDATE fire_tv_devices SET "
          << "client_token = NULL, "
          << "pin_code = NULL, "
          << "pin_expires_at = NULL, "
          << "status = 'offline', "
          << "updated_at = NOW() "
          << "WHERE device_id = '" << device_id << "'";

    return DatabaseService::getInstance().executeCommand(query.str());
}

// ============================================================================
// UTILITY
// ============================================================================

bool DeviceRepository::deviceExists(const std::string& device_id) {
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM fire_tv_devices WHERE device_id = '" << device_id << "'";

    auto result = DatabaseService::getInstance().executeQuery(query.str());

    if (result.empty()) {
        return false;
    }

    int count = result[0][0].as<int>();
    return count > 0;
}

// ============================================================================
// PARSING
// ============================================================================

Device DeviceRepository::parseDeviceFromRow(const pqxx::row& row) {
    Device device;

    device.id = row["id"].as<int>();
    device.device_id = row["device_id"].as<std::string>();
    device.name = row["name"].as<std::string>();
    device.ip_address = row["ip_address"].as<std::string>();
    device.api_key = row["api_key"].as<std::string>();
    device.status = row["status"].as<std::string>();
    device.adb_enabled = row["adb_enabled"].as<bool>();

    // Optional fields
    if (!row["client_token"].is_null()) {
        device.client_token = row["client_token"].as<std::string>();
    }

    if (!row["pin_code"].is_null()) {
        device.pin_code = row["pin_code"].as<std::string>();
    }

    // TODO: Parse timestamps properly
    // device.created_at = ...
    // device.updated_at = ...
    // device.last_seen_at = ...
    // device.pin_expires_at = ...

    return device;
}

} // namespace hms_firetv
