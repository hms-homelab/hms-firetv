#pragma once

#include <mqtt/async_client.h>
#include <string>
#include <functional>
#include <mutex>
#include <memory>
#include <json/json.h>

namespace hms_firetv {

/**
 * MQTTClient - Thread-safe MQTT client wrapper
 *
 * Wraps Eclipse Paho MQTT C++ client with simplified interface for:
 * - Publishing state/availability/discovery messages
 * - Subscribing to command topics
 * - Auto-reconnect on connection loss
 * - Thread-safe operations
 */
class MQTTClient {
public:
    /**
     * Command callback type
     *
     * @param device_id Device identifier extracted from topic
     * @param payload JSON command payload
     */
    using CommandCallback = std::function<void(const std::string& device_id, const Json::Value& payload)>;

    /**
     * Constructor
     *
     * @param client_id MQTT client identifier (must be unique)
     */
    explicit MQTTClient(const std::string& client_id);

    /**
     * Destructor - cleanup and disconnect
     */
    ~MQTTClient();

    // Disable copy
    MQTTClient(const MQTTClient&) = delete;
    MQTTClient& operator=(const MQTTClient&) = delete;

    /**
     * Connect to MQTT broker
     *
     * @param broker_address Broker address (e.g., "tcp://192.168.2.15:1883")
     * @param username MQTT username
     * @param password MQTT password
     * @return true if connected successfully
     */
    bool connect(const std::string& broker_address,
                 const std::string& username,
                 const std::string& password);

    /**
     * Disconnect from broker
     */
    void disconnect();

    /**
     * Check if connected
     *
     * @return true if connected to broker
     */
    bool isConnected() const;

    /**
     * Subscribe to command topic for a device
     *
     * Topic pattern: maestro_hub/firetv/{device_id}/set
     *
     * @param device_id Device identifier
     * @param callback Function to call when command received
     * @return true if subscribed successfully
     */
    bool subscribeToCommands(const std::string& device_id, CommandCallback callback);

    /**
     * Subscribe to all device commands (wildcard)
     *
     * Topic pattern: maestro_hub/firetv/+/set
     *
     * @param callback Function to call when command received
     * @return true if subscribed successfully
     */
    bool subscribeToAllCommands(CommandCallback callback);

    /**
     * Subscribe to button command topics (matching Python service)
     *
     * Topic pattern: maestro_hub/colada/+/+
     *
     * @param callback Function to call when button pressed
     * @return true if subscribed successfully
     */
    bool subscribeToButtonCommands(std::function<void(const std::string& device_id, const std::string& action)> callback);

    /**
     * Subscribe to a custom topic with a generic callback
     *
     * @param topic MQTT topic (can include wildcards +/# )
     * @param callback Function to call with topic and payload
     * @return true if subscribed successfully
     */
    bool subscribe(const std::string& topic, std::function<void(const std::string& topic, const std::string& payload)> callback);

    /**
     * Register a callback for a topic WITHOUT making an MQTT subscription
     * Use this when the topic will be included in a batch subscription later
     *
     * @param topic MQTT topic (can include wildcards +/# )
     * @param callback Function to call with topic and payload
     */
    void registerTopicCallback(const std::string& topic, std::function<void(const std::string& topic, const std::string& payload)> callback);

    /**
     * Publish device state
     *
     * Topic: maestro_hub/firetv/{device_id}/state
     *
     * @param device_id Device identifier
     * @param state State payload (JSON)
     * @return true if published successfully
     */
    bool publishState(const std::string& device_id, const Json::Value& state);

    /**
     * Publish device availability
     *
     * Topic: maestro_hub/firetv/{device_id}/availability
     *
     * @param device_id Device identifier
     * @param online true for "online", false for "offline"
     * @return true if published successfully
     */
    bool publishAvailability(const std::string& device_id, bool online);

    /**
     * Publish Home Assistant MQTT Discovery config
     *
     * Topic: homeassistant/media_player/{device_id}/config
     *
     * @param device_id Device identifier
     * @param config Discovery configuration (JSON)
     * @param retain Retain message (default: true)
     * @return true if published successfully
     */
    bool publishDiscovery(const std::string& device_id, const Json::Value& config, bool retain = true);

    /**
     * Remove device from Home Assistant (clear discovery)
     *
     * Publishes empty retained message to discovery topic
     *
     * @param device_id Device identifier
     * @return true if published successfully
     */
    bool removeDevice(const std::string& device_id);

    /**
     * Publish to any MQTT topic
     *
     * @param topic MQTT topic
     * @param payload Message payload (JSON string)
     * @param qos Quality of Service (0, 1, or 2)
     * @param retain Retain message flag
     * @return true if published successfully
     */
    bool publish(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false);

private:

    /**
     * Build topic for device
     *
     * @param device_id Device identifier
     * @param suffix Topic suffix (e.g., "state", "set", "availability")
     * @return Full topic path
     */
    std::string buildTopic(const std::string& device_id, const std::string& suffix) const;

    /**
     * Extract device_id from topic
     *
     * maestro_hub/firetv/{device_id}/set -> device_id
     *
     * @param topic Full topic path
     * @return Device identifier or empty string
     */
    std::string extractDeviceId(const std::string& topic) const;

    /**
     * Message arrived callback (internal)
     */
    void onMessageArrived(mqtt::const_message_ptr msg);

    /**
     * Connection lost callback (internal)
     */
    void onConnectionLost(const std::string& cause);

    // MQTT client
    std::unique_ptr<mqtt::async_client> client_;

    // Topic prefix
    std::string topic_prefix_;  // "maestro_hub/firetv"

    // Command callbacks
    std::map<std::string, CommandCallback> command_callbacks_;
    mutable std::mutex callbacks_mutex_;

    // Generic topic callbacks
    std::map<std::string, std::function<void(const std::string&, const std::string&)>> topic_callbacks_;
    mutable std::mutex topic_callbacks_mutex_;

    // Connection state
    std::string broker_address_;
    std::string username_;
    std::string password_;
    bool connected_;
    mutable std::mutex connection_mutex_;

    // Auto-reconnect
    bool auto_reconnect_;
    void attemptReconnect();
};

} // namespace hms_firetv
