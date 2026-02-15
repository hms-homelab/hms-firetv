#pragma once

#include "models/Device.h"
#include "mqtt/MQTTClient.h"
#include <json/json.h>
#include <string>

namespace hms_firetv {

/**
 * DiscoveryPublisher - Home Assistant MQTT Discovery
 *
 * Publishes device configuration to Home Assistant using MQTT Discovery protocol.
 * https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
 */
class DiscoveryPublisher {
public:
    /**
     * Constructor
     *
     * @param mqtt_client MQTT client for publishing
     */
    explicit DiscoveryPublisher(MQTTClient& mqtt_client);

    /**
     * Publish device discovery config to Home Assistant
     *
     * Creates media_player entity in HA with full capabilities.
     *
     * @param device Device to publish
     * @return true if published successfully
     */
    bool publishDevice(const Device& device);

    /**
     * Remove device from Home Assistant
     *
     * Publishes empty retained message to clear discovery.
     *
     * @param device_id Device identifier
     * @return true if removed successfully
     */
    bool removeDevice(const std::string& device_id);

    /**
     * Publish availability for device
     *
     * @param device_id Device identifier
     * @param online true for "online", false for "offline"
     * @return true if published successfully
     */
    bool publishAvailability(const std::string& device_id, bool online);

private:
    /**
     * Build button configuration JSON (matching Python service)
     *
     * @param device Device to build config for
     * @param button_id Button identifier (e.g., "up", "play", "volume_up")
     * @return Button configuration
     */
    Json::Value buildButtonConfig(const Device& device, const std::string& button_id);

    /**
     * Build device info for HA (matching Python discovery.py)
     *
     * @param device Device
     * @return Device info JSON
     */
    Json::Value buildDeviceInfo(const Device& device);

    /**
     * Publish text entity for keyboard input
     *
     * @param device Device to publish text entity for
     * @return true if published successfully
     */
    bool publishTextEntity(const Device& device);

    // MQTT client reference
    MQTTClient& mqtt_client_;
};

} // namespace hms_firetv
