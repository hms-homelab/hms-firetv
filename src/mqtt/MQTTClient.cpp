#include "mqtt/MQTTClient.h"
#include "repositories/DeviceRepository.h"
#include <iostream>

namespace hms_firetv {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MQTTClient::MQTTClient(const std::string& client_id)
    : topic_prefix_("maestro_hub/firetv"),
      connected_(false),
      auto_reconnect_(true) {

    // Create async MQTT client (no connection yet)
    client_ = nullptr;  // Will be created in connect()

    std::cout << "[MQTTClient] Initialized with client_id: " << client_id << std::endl;
}

MQTTClient::~MQTTClient() {
    disconnect();
}

// ============================================================================
// CONNECTION
// ============================================================================

bool MQTTClient::connect(const std::string& broker_address,
                         const std::string& username,
                         const std::string& password) {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    broker_address_ = broker_address;
    username_ = username;
    password_ = password;

    std::cout << "[MQTTClient] Connecting to " << broker_address << "..." << std::endl;

    try {
        // Create client with unique ID
        std::string client_id = "hms_firetv_" + std::to_string(std::time(nullptr));
        client_ = std::make_unique<mqtt::async_client>(broker_address, client_id);

        // Set callbacks
        client_->set_message_callback([this](mqtt::const_message_ptr msg) {
            onMessageArrived(msg);
        });

        client_->set_connection_lost_handler([this](const std::string& cause) {
            onConnectionLost(cause);
        });

        client_->set_connected_handler([this](const std::string& cause) {
            onReconnected(cause);
        });

        // Connection options
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        connOpts.set_user_name(username);
        connOpts.set_password(password);
        connOpts.set_automatic_reconnect(1, 64);

        // Connect (blocking)
        mqtt::token_ptr conntok = client_->connect(connOpts);
        conntok->wait();

        connected_ = true;
        initial_connect_done_ = true;
        std::cout << "[MQTTClient] Connected successfully" << std::endl;

        return true;

    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTTClient] Connection failed: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

void MQTTClient::disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (client_ && connected_) {
        try {
            std::cout << "[MQTTClient] Disconnecting..." << std::endl;
            client_->disconnect()->wait();
            connected_ = false;
            std::cout << "[MQTTClient] Disconnected" << std::endl;
        } catch (const mqtt::exception& e) {
            std::cerr << "[MQTTClient] Disconnect error: " << e.what() << std::endl;
        }
    }
}

bool MQTTClient::isConnected() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return connected_ && client_ && client_->is_connected();
}

// ============================================================================
// SUBSCRIPTIONS
// ============================================================================

bool MQTTClient::subscribeToCommands(const std::string& device_id, CommandCallback callback) {
    if (!isConnected()) {
        std::cerr << "[MQTTClient] Not connected, cannot subscribe" << std::endl;
        return false;
    }

    std::string topic = buildTopic(device_id, "set");

    try {
        std::cout << "[MQTTClient] Subscribing to: " << topic << std::endl;
        client_->subscribe(topic, 1)->wait();  // QoS 1

        // Store callback
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            command_callbacks_[device_id] = callback;
        }

        std::cout << "[MQTTClient] ✅ Subscribed to commands for " << device_id << std::endl;
        return true;

    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTTClient] ❌ Subscribe failed: " << e.what() << std::endl;
        return false;
    }
}

bool MQTTClient::subscribeToAllCommands(CommandCallback callback) {
    if (!isConnected()) {
        std::cerr << "[MQTTClient] Not connected, cannot subscribe" << std::endl;
        return false;
    }

    // ========================================================================
    // DATABASE-DRIVEN SUBSCRIPTIONS (NO WILDCARDS)
    // ========================================================================
    // Instead of subscribing to maestro_hub/colada/+/+ (wildcard), we:
    // 1. Query all devices from the database
    // 2. Subscribe to maestro_hub/colada/{device_id}/+ for each device
    // 3. This avoids wildcard issues while still covering all devices

    try {
        auto devices = DeviceRepository::getInstance().getAllDevices();

        if (devices.empty()) {
            std::cerr << "[MQTTClient] No devices found in database" << std::endl;
            return false;
        }

        // Store callback for wildcard (key: "*")
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            command_callbacks_["*"] = callback;
        }

        // Build topic list — one per device + homeassistant/status
        std::vector<std::string> all_topics;
        for (const auto& device : devices) {
            all_topics.push_back("maestro_hub/colada/" + device.device_id + "/+");
        }
        all_topics.push_back("homeassistant/status");

        // Create QoS vector (QoS 1 for all)
        std::vector<int> qos_levels(all_topics.size(), 1);

        try {
            auto topics_ptr = mqtt::string_collection::create(all_topics);
            auto token = client_->subscribe(topics_ptr, qos_levels);
            token->wait();

            std::cout << "[MQTTClient] Subscribed to " << all_topics.size() << " topics" << std::endl;
            return true;

        } catch (const mqtt::exception& e) {
            std::cerr << "[MQTTClient] Batch subscription failed: " << e.what() << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        std::cerr << "[MQTTClient] Failed to query devices or subscribe: " << e.what() << std::endl;
        return false;
    }
}

bool MQTTClient::subscribe(const std::string& topic, std::function<void(const std::string&, const std::string&)> callback) {
    if (!connected_) {
        std::cerr << "[MQTTClient] Not connected, cannot subscribe" << std::endl;
        return false;
    }

    try {
        // Subscribe to topic
        client_->subscribe(topic, 1)->wait();  // QoS 1

        // Store callback
        {
            std::lock_guard<std::mutex> lock(topic_callbacks_mutex_);
            topic_callbacks_[topic] = callback;
        }

        std::cout << "[MQTTClient] ✅ Subscribed to: " << topic << std::endl;
        return true;

    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTTClient] ❌ Subscribe failed: " << e.what() << std::endl;
        return false;
    }
}

void MQTTClient::registerTopicCallback(const std::string& topic, std::function<void(const std::string&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(topic_callbacks_mutex_);
    topic_callbacks_[topic] = callback;
    std::cout << "[MQTTClient] ✅ Registered callback for topic: " << topic << " (no MQTT subscription made)" << std::endl;
}

// ============================================================================
// PUBLISHING
// ============================================================================

bool MQTTClient::publishState(const std::string& device_id, const Json::Value& state) {
    std::string topic = buildTopic(device_id, "state");
    Json::StreamWriterBuilder writer;
    std::string payload = Json::writeString(writer, state);

    return publish(topic, payload, 1, false);  // QoS 1, not retained
}

bool MQTTClient::publishAvailability(const std::string& device_id, bool online) {
    std::string topic = buildTopic(device_id, "availability");
    std::string payload = online ? "online" : "offline";

    return publish(topic, payload, 1, true);  // QoS 1, retained
}

bool MQTTClient::publishDiscovery(const std::string& device_id,
                                   const Json::Value& config,
                                   bool retain) {
    // Home Assistant discovery topic
    std::string topic = "homeassistant/media_player/" + device_id + "/config";

    Json::StreamWriterBuilder writer;
    std::string payload = Json::writeString(writer, config);

    return publish(topic, payload, 1, retain);  // QoS 1, retained by default
}

bool MQTTClient::removeDevice(const std::string& device_id) {
    // Publish empty retained message to clear discovery
    std::string topic = "homeassistant/media_player/" + device_id + "/config";
    return publish(topic, "", 1, true);  // Empty payload, retained
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

bool MQTTClient::publish(const std::string& topic,
                         const std::string& payload,
                         int qos,
                         bool retain) {
    if (!isConnected()) {
        std::cerr << "[MQTTClient] Not connected, cannot publish" << std::endl;
        return false;
    }

    try {
        mqtt::message_ptr pubmsg = mqtt::make_message(topic, payload);
        pubmsg->set_qos(qos);
        pubmsg->set_retained(retain);

        client_->publish(pubmsg);  // Async, no wait

        std::cout << "[MQTTClient] Published to " << topic
                  << " (" << payload.length() << " bytes)"
                  << (retain ? " [retained]" : "") << std::endl;

        return true;

    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTTClient] Publish failed: " << e.what() << std::endl;
        return false;
    }
}

std::string MQTTClient::buildTopic(const std::string& device_id, const std::string& suffix) const {
    return topic_prefix_ + "/" + device_id + "/" + suffix;
}

std::string MQTTClient::extractDeviceId(const std::string& topic) const {
    // Extract device_id from: maestro_hub/colada/{device_id}/{action}
    std::string prefix = "maestro_hub/colada/";

    size_t prefix_pos = topic.find(prefix);
    if (prefix_pos == std::string::npos) {
        return "";
    }

    size_t start = prefix_pos + prefix.length();
    size_t slash_pos = topic.find('/', start);

    if (slash_pos == std::string::npos) {
        return "";
    }

    return topic.substr(start, slash_pos - start);
}

// ============================================================================
// CALLBACKS
// ============================================================================

void MQTTClient::onMessageArrived(mqtt::const_message_ptr msg) {
    std::string topic = msg->get_topic();
    std::string payload_str = msg->to_string();

    // Check for exact match in topic callbacks first (e.g., homeassistant/status)
    {
        std::lock_guard<std::mutex> lock(topic_callbacks_mutex_);
        auto it = topic_callbacks_.find(topic);
        if (it != topic_callbacks_.end()) {
            it->second(topic, payload_str);
            return; // Stop processing after exact match
        }
    }

    // If no exact match, continue to maestro_hub command handler below...

    // Extract device_id from topic: maestro_hub/colada/{device_id}/{action}
    std::string device_id = extractDeviceId(topic);
    if (device_id.empty()) {
        std::cerr << "[MQTTClient] Failed to extract device_id from topic: " << topic << std::endl;
        return;
    }

    // Extract action from topic (last part after device_id)
    std::string prefix = "maestro_hub/colada/" + device_id + "/";
    std::string action;
    size_t action_pos = topic.find(prefix);
    if (action_pos != std::string::npos) {
        action = topic.substr(action_pos + prefix.length());
    }

    // Convert button press to JSON command format that CommandHandler expects
    Json::Value payload;
    if (action == "send_text") {
        // Text input from text entity - payload is the text string
        payload["command"] = "send_text";
        payload["text"] = payload_str;
    } else if (payload_str == "PRESS" && !action.empty()) {
        // Button press - convert action to command format
        // Map Python action names to our command format
        if (action.find("dpad_") == 0) {
            // Navigation: dpad_up, dpad_down, etc.
            std::string direction = action.substr(5); // Remove "dpad_" prefix
            payload["command"] = "navigate";
            payload["direction"] = direction;
        } else if (action == "select" || action == "home" || action == "back" || action == "menu") {
            // Navigation actions
            payload["command"] = "navigate";
            payload["action"] = action;
        } else if (action == "play" || action == "pause") {
            // Media controls
            payload["command"] = "media_" + action;
        } else if (action == "volume_up" || action == "volume_down" || action == "mute") {
            // Volume controls
            payload["command"] = action;
        } else if (action == "sleep") {
            // Power off
            payload["command"] = "turn_off";
        } else if (action == "wake") {
            // Power on
            payload["command"] = "turn_on";
        } else {
            std::cerr << "[MQTTClient] Unknown button action: " << action << std::endl;
            return;
        }
    } else {
        // Try to parse as JSON (for backwards compatibility)
        Json::CharReaderBuilder reader;
        std::string errors;
        std::istringstream stream(payload_str);

        if (!Json::parseFromStream(reader, stream, &payload, &errors)) {
            std::cerr << "[MQTTClient] JSON parse error: " << errors << std::endl;
            return;
        }
    }

    // Find and call callback
    std::lock_guard<std::mutex> lock(callbacks_mutex_);

    // Try device-specific callback first
    auto it = command_callbacks_.find(device_id);
    if (it != command_callbacks_.end()) {
        it->second(device_id, payload);
        return;
    }

    // Try wildcard callback
    auto wildcard_it = command_callbacks_.find("*");
    if (wildcard_it != command_callbacks_.end()) {
        wildcard_it->second(device_id, payload);
        return;
    }

    std::cerr << "[MQTTClient] No callback registered for device: " << device_id << std::endl;
}

void MQTTClient::onConnectionLost(const std::string& cause) {
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        connected_ = false;
    }
    std::cerr << "[MQTTClient] Connection lost: " << cause << std::endl;

    if (auto_reconnect_) {
        std::cout << "[MQTTClient] Auto-reconnect enabled (handled by paho-mqtt)" << std::endl;
    }
}

void MQTTClient::onReconnected(const std::string& cause) {
    if (!initial_connect_done_) {
        return;
    }

    {
        std::unique_lock<std::mutex> lock(connection_mutex_, std::try_to_lock);
        connected_ = true;
    }
    std::cout << "[MQTTClient] Reconnected: " << cause << std::endl;

    // Re-subscribe — fire-and-forget (no ->wait()) to avoid paho callback thread deadlock
    bool has_wildcard = false;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        has_wildcard = command_callbacks_.count("*") > 0;
    }

    if (has_wildcard) {
        try {
            auto devices = DeviceRepository::getInstance().getAllDevices();
            std::vector<std::string> all_topics;
            for (const auto& device : devices) {
                all_topics.push_back("maestro_hub/colada/" + device.device_id + "/+");
            }
            all_topics.push_back("homeassistant/status");
            std::vector<int> qos_levels(all_topics.size(), 1);
            auto topics_ptr = mqtt::string_collection::create(all_topics);
            client_->subscribe(topics_ptr, qos_levels);
            std::cout << "[MQTTClient] Re-subscribed to " << all_topics.size() << " topics after reconnect" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[MQTTClient] Re-subscribe failed: " << e.what() << std::endl;
        }
    }

    {
        std::lock_guard<std::mutex> lock(topic_callbacks_mutex_);
        for (const auto& [topic, callback] : topic_callbacks_) {
            try {
                client_->subscribe(topic, 1);
                std::cout << "[MQTTClient] Re-subscribed to " << topic << std::endl;
            } catch (const mqtt::exception& e) {
                std::cerr << "[MQTTClient] Re-subscribe failed for " << topic << ": " << e.what() << std::endl;
            }
        }
    }
}

} // namespace hms_firetv
