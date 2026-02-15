#pragma once

#include <string>
#include <cstdlib>

namespace hms_firetv {

/**
 * ConfigManager - Simple configuration manager using environment variables
 *
 * Provides helpers to read configuration from environment variables with defaults.
 * Used for 12-factor app configuration pattern.
 */
class ConfigManager {
public:
    static std::string getEnv(const std::string& key, const std::string& default_value = "") {
        const char* value = std::getenv(key.c_str());
        return value ? std::string(value) : default_value;
    }

    static int getEnvInt(const std::string& key, int default_value = 0) {
        const char* value = std::getenv(key.c_str());
        return value ? std::atoi(value) : default_value;
    }

    static bool getEnvBool(const std::string& key, bool default_value = false) {
        const char* value = std::getenv(key.c_str());
        if (!value) return default_value;
        std::string str_value(value);
        return str_value == "true" || str_value == "1" || str_value == "yes";
    }
};

} // namespace hms_firetv
