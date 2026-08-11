#pragma once

#include "clients/LightningClient.h"
#include "repositories/DeviceRepository.h"
#include <json/json.h>
#include <functional>
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

    /**
     * Register a callback that republishes a device's discovery config.
     *
     * Syncing the app list changes the options of the Home Assistant `select`
     * entity, and a select's options only change by republishing its
     * discovery payload. Without this, pressing "Refresh Apps" would fill the
     * database but leave the dropdown stale until the service restarted.
     *
     * Held as a callback rather than a DiscoveryPublisher reference because
     * the publisher is created inside the MQTT connect thread, after this
     * handler exists.
     */
    void setDiscoveryRepublish(std::function<void(const std::string&)> callback);

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
    void handleAppLaunchCommand(const std::string& device_id, LightningClient& client,
                                const Json::Value& payload);

    /**
     * Handle text input command
     *
     * @param client Lightning client
     * @param payload Full command payload with text field
     */
    void handleTextInputCommand(LightningClient& client, const Json::Value& payload);

    /**
     * Handle press-and-hold.
     *
     * The official app sends navigation as a keyDown/keyUp pair; holding the
     * key between them is what produces key repeat on the device. A plain
     * button press can only ever be one discrete step, which is why scrolling
     * a long list from Home Assistant used to mean mashing the button.
     *
     * @param client Lightning client
     * @param payload {"action": "dpad_down", "ms": 800}
     */
    void handleHoldCommand(LightningClient& client, const Json::Value& payload);

    /**
     * Handle a raw keyDown or keyUp, for automations that want to hold a key
     * across separate messages rather than for a fixed duration.
     */
    void handleKeyEdgeCommand(LightningClient& client, const Json::Value& payload,
                              const std::string& key_action_type);

    /**
     * Re-read the installed app list from the device into the database.
     */
    void handleAppsRefreshCommand(const std::string& device_id, LightningClient& client);

    /**
     * Handle the voice commands.
     *
     * The Fire TV voice channel carries audio, never text, so "say this"
     * means synthesising speech first - see VoiceService.
     *
     * @param device_id Device identifier
     * @param command One of voice_say / voice_start / voice_stop / voice_audio
     * @param payload Full command payload
     */
    void handleVoiceCommand(const std::string& device_id, const std::string& command,
                            const Json::Value& payload);

    /**
     * Map app name to package name
     *
     * @param app_name Friendly app name (e.g., "Netflix")
     * @return Package name (e.g., "com.netflix.ninja")
     */
    std::string getPackageForApp(const std::string& app_name);

    /**
     * Resolve a friendly app name against the apps synced from a device.
     *
     * Matches case-insensitively, and also accepts a prefix so a picker
     * showing a truncated name still launches (the device reports titles
     * like "Tubi: Watch Free Movies & TV Shows").
     *
     * @return Package name, or empty if this device has no such app
     */
    std::string getPackageForAppOnDevice(const std::string& device_id,
                                         const std::string& app_name);

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

    // Republishes a device's HA discovery config; see setDiscoveryRepublish.
    std::function<void(const std::string&)> discovery_republish_;
    std::mutex republish_mutex_;
};

} // namespace hms_firetv
