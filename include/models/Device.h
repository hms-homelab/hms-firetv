#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <json/json.h>

namespace hms_firetv {

/**
 * Device - Represents a Fire TV device
 *
 * Maps to fire_tv_devices table in PostgreSQL database
 */
struct Device {
    // Database fields
    int id;                                                      // Primary key
    std::string device_id;                                       // Unique identifier (e.g., "living_room")
    std::string name;                                            // Friendly name
    std::string ip_address;                                      // Device IP
    std::string api_key;                                         // Lightning API key (default: "0987654321")
    std::optional<std::string> client_token;                     // Auth token from pairing
    std::optional<std::string> pin_code;                         // Current PIN for pairing
    std::optional<std::chrono::system_clock::time_point> pin_expires_at;  // PIN expiration
    std::string status;                                          // online|offline|pairing|error
    bool adb_enabled;                                            // ADB debugging enabled
    std::optional<std::chrono::system_clock::time_point> last_seen_at;    // Last successful command
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    // Constructor with defaults
    Device() : id(0), api_key("0987654321"), status("offline"), adb_enabled(false) {
        auto now = std::chrono::system_clock::now();
        created_at = now;
        updated_at = now;
    }

    /**
     * Check if device is currently paired
     */
    bool isPaired() const {
        return client_token.has_value() && !client_token->empty();
    }

    /**
     * Check if device is online
     */
    bool isOnline() const {
        return status == "online";
    }

    /**
     * Check if PIN is still valid
     */
    bool isPinValid() const {
        if (!pin_code.has_value() || !pin_expires_at.has_value()) {
            return false;
        }
        return std::chrono::system_clock::now() < pin_expires_at.value();
    }

    /**
     * Convert device to JSON for API responses
     */
    Json::Value toJson() const {
        Json::Value json;
        json["id"] = id;
        json["device_id"] = device_id;
        json["name"] = name;
        json["ip_address"] = ip_address;
        json["api_key"] = api_key;
        json["status"] = status;
        json["adb_enabled"] = adb_enabled;
        json["is_paired"] = isPaired();
        json["is_online"] = isOnline();

        if (client_token.has_value()) {
            json["client_token"] = client_token.value();
        }

        if (pin_code.has_value()) {
            json["pin_code"] = pin_code.value();
            json["pin_valid"] = isPinValid();
        }

        if (last_seen_at.has_value()) {
            auto time_t = std::chrono::system_clock::to_time_t(last_seen_at.value());
            char buffer[32];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&time_t));
            json["last_seen_at"] = buffer;
        }

        auto created_time_t = std::chrono::system_clock::to_time_t(created_at);
        auto updated_time_t = std::chrono::system_clock::to_time_t(updated_at);
        char created_buffer[32], updated_buffer[32];
        strftime(created_buffer, sizeof(created_buffer), "%Y-%m-%d %H:%M:%S", localtime(&created_time_t));
        strftime(updated_buffer, sizeof(updated_buffer), "%Y-%m-%d %H:%M:%S", localtime(&updated_time_t));
        json["created_at"] = created_buffer;
        json["updated_at"] = updated_buffer;

        return json;
    }

    /**
     * Create device from JSON (for API requests)
     */
    static Device fromJson(const Json::Value& json) {
        Device device;

        if (json.isMember("device_id")) device.device_id = json["device_id"].asString();
        if (json.isMember("name")) device.name = json["name"].asString();
        if (json.isMember("ip_address")) device.ip_address = json["ip_address"].asString();
        if (json.isMember("api_key")) device.api_key = json["api_key"].asString();
        if (json.isMember("adb_enabled")) device.adb_enabled = json["adb_enabled"].asBool();

        return device;
    }
};

} // namespace hms_firetv
