#pragma once

#include "clients/LightningClient.h"
#include "repositories/DeviceRepository.h"
#include <json/json.h>
#include <string>
#include <map>
#include <memory>
#include <mutex>

namespace hms_firetv {

/**
 * CommandHandler - Routes MQTT commands to Lightning protocol
 *
 * Handles incoming MQTT commands from Home Assistant and routes them
 * to the appropriate Lightning client methods.
 */
class CommandHandler {
public:
    /**
     * Constructor
     */
    CommandHandler();

    /**
     * Handle incoming MQTT command
     *
     * @param device_id Device identifier
     * @param payload Command payload (JSON)
     */
    void handleCommand(const std::string& device_id, const Json::Value& payload);

protected:
    /**
     * Get or create Lightning client for device
     *
     * Caches clients for reuse.
     *
     * @param device_id Device identifier
     * @return Lightning client or nullptr if device not found
     */
    std::shared_ptr<LightningClient> getClientForDevice(const std::string& device_id);

    /**
     * Handle media control command
     *
     * @param client Lightning client
     * @param command Command string
     */
    void handleMediaCommand(LightningClient& client, const std::string& command);

    /**
     * Handle volume command
     *
     * @param client Lightning client
     * @param command Command string
     */
    void handleVolumeCommand(LightningClient& client, const std::string& command);

    /**
     * Handle navigation command
     *
     * @param client Lightning client
     * @param payload Full command payload
     */
    void handleNavigationCommand(LightningClient& client, const Json::Value& payload);

    /**
     * Handle power command
     *
     * @param client Lightning client
     * @param command Command string ("turn_on" or "turn_off")
     */
    void handlePowerCommand(LightningClient& client, const std::string& command);

    /**
     * Handle app launch command
     *
     * @param client Lightning client
     * @param payload Full command payload
     */
    void handleAppLaunchCommand(LightningClient& client, const Json::Value& payload);

    /**
     * Handle text input command
     *
     * @param client Lightning client
     * @param payload Full command payload with text field
     */
    void handleTextInputCommand(LightningClient& client, const Json::Value& payload);

    /**
     * Map app name to package name
     *
     * @param app_name Friendly app name (e.g., "Netflix")
     * @return Package name (e.g., "com.netflix.ninja")
     */
    std::string getPackageForApp(const std::string& app_name);

    /**
     * Ensure device is awake before sending commands
     *
     * Checks if Lightning API is responding. If not, wakes the device
     * and waits for it to become available.
     *
     * @param client Lightning client
     * @return true if device is awake and ready, false if wake failed
     */
    bool ensureDeviceAwake(LightningClient& client);

    // Lightning client cache
    std::map<std::string, std::shared_ptr<LightningClient>> clients_;
    std::mutex clients_mutex_;

    // App name → package mapping
    std::map<std::string, std::string> app_packages_;
};

} // namespace hms_firetv
