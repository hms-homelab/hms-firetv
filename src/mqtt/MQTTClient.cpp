#include "mqtt/MQTTClient.h"
#include "repositories/DeviceRepository.h"
#include <iostream>
#include <thread>
#include <chrono>

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

    std::cout << "[MQTTClient] 🔍 ========== CONNECTING ==========" << std::endl;
    std::cout << "[MQTTClient] Connecting to " << broker_address << "..." << std::endl;
    std::cout << "[MQTTClient] 🔍 Username: " << username << std::endl;

    try {
        // Create client with unique ID
        std::string client_id = "hms_firetv_" + std::to_string(std::time(nullptr));
        std::cout << "[MQTTClient] 🔍 Client ID: " << client_id << std::endl;

        client_ = std::make_unique<mqtt::async_client>(broker_address, client_id);
        std::cout << "[MQTTClient] 🔍 async_client created" << std::endl;

        // Set callbacks
        std::cout << "[MQTTClient] 🔍 Setting message callback..." << std::endl;
        client_->set_message_callback([this](mqtt::const_message_ptr msg) {
            std::cout << "[MQTTClient] 🎯 Callback lambda invoked!" << std::endl;
            onMessageArrived(msg);
        });

        std::cout << "[MQTTClient] 🔍 Setting connection lost handler..." << std::endl;
        client_->set_connection_lost_handler([this](const std::string& cause) {
            onConnectionLost(cause);
        });

        // Connection options
        std::cout << "[MQTTClient] 🔍 Configuring connection options..." << std::endl;
        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        connOpts.set_user_name(username);
        connOpts.set_password(password);

        // Enable auto-reconnect with exponential backoff
        // Min delay: 1 second, Max delay: 64 seconds
        connOpts.set_automatic_reconnect(1, 64);  // min_retry_interval, max_retry_interval
        std::cout << "[MQTTClient] 🔍 Auto-reconnect enabled (1-64 seconds)" << std::endl;

        // Connect (blocking)
        std::cout << "[MQTTClient] 🔍 Calling client_->connect()..." << std::endl;
        mqtt::token_ptr conntok = client_->connect(connOpts);
        std::cout << "[MQTTClient] 🔍 Waiting for CONNACK..." << std::endl;
        conntok->wait();  // Wait for connection

        connected_ = true;
        std::cout << "[MQTTClient] ✅ Connected successfully (CONNACK received)" << std::endl;
        std::cout << "[MQTTClient] 📊 Connection state: is_connected=" << client_->is_connected()
                  << ", can_publish=" << (client_->is_connected() ? "yes" : "no") << std::endl;
        std::cout << "[MQTTClient] 🔍 =====================================" << std::endl;

        return true;

    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTTClient] ❌ Connection failed: " << e.what() << std::endl;
        std::cerr << "[MQTTClient] 🔍 Exception code: " << e.get_reason_code()
                  << ", message: " << e.get_message() << std::endl;
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
    std::cout << "[MQTTClient] 🔍 subscribeToAllCommands() called" << std::endl;
    std::cout << "[MQTTClient] 🔍 Pre-check: connected_=" << connected_
              << ", client_=" << (client_ ? "valid" : "null") << std::endl;

    if (client_) {
        std::cout << "[MQTTClient] 🔍 Pre-check: client_->is_connected()=" << client_->is_connected() << std::endl;
    }

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

    std::cout << "[MQTTClient] 📊 Querying devices from database..." << std::endl;

    try {
        // Get all devices from database
        auto devices = DeviceRepository::getInstance().getAllDevices();
        std::cout << "[MQTTClient] 🔍 Found " << devices.size() << " devices in database" << std::endl;

        if (devices.empty()) {
            std::cerr << "[MQTTClient] ⚠️  No devices found in database, no subscriptions made" << std::endl;
            return false;
        }

        // Store callback for wildcard (key: "*")
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            command_callbacks_["*"] = callback;
            std::cout << "[MQTTClient] 🔍 Wildcard callback registered" << std::endl;
        }

        // CRITICAL: Use batch subscription API to subscribe to ALL topics at once
        // Sequential subscriptions don't work properly - only the last one works!
        std::cout << "[MQTTClient] 📡 Preparing batch subscription for " << devices.size() << " device topics + homeassistant/status" << std::endl;

        // Build topic list
        std::vector<std::string> all_topics;
        for (const auto& device : devices) {
            std::string device_topic = "maestro_hub/colada/" + device.device_id + "/+";
            all_topics.push_back(device_topic);
            std::cout << "[MQTTClient] 📋 Adding to batch: " << device_topic << std::endl;
        }

        // Add homeassistant/status
        all_topics.push_back("homeassistant/status");
        std::cout << "[MQTTClient] 📋 Adding to batch: homeassistant/status" << std::endl;

        // Create QoS vector (QoS 1 for all)
        std::vector<int> qos_levels(all_topics.size(), 1);

        try {
            // Create string_collection for Paho MQTT
            auto topics_ptr = mqtt::string_collection::create(all_topics);

            // Subscribe to ALL topics in a SINGLE call
            std::cout << "[MQTTClient] 📡 Subscribing to " << all_topics.size() << " topics in ONE batch..." << std::endl;
            auto token = client_->subscribe(topics_ptr, qos_levels);
            token->wait();  // Wait for SUBACK

            std::cout << "[MQTTClient] ✅ Successfully batch-subscribed to " << all_topics.size() << " topics" << std::endl;
            return true;

        } catch (const mqtt::exception& e) {
            std::cerr << "[MQTTClient] ❌ Batch subscription failed: " << e.what() << std::endl;
            return false;
        }

    } catch (const std::exception& e) {
        std::cerr << "[MQTTClient] ❌ Failed to query devices or subscribe: " << e.what() << std::endl;
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
    std::cout << "[MQTTClient] 🔍 publish() called: topic=" << topic
              << ", payload=" << payload << ", qos=" << qos << ", retain=" << retain << std::endl;
    std::cout << "[MQTTClient] 🔍 Pre-publish check: connected_=" << connected_
              << ", client_=" << (client_ ? "valid" : "null") << std::endl;

    if (client_) {
        std::cout << "[MQTTClient] 🔍 Pre-publish check: client_->is_connected()=" << client_->is_connected() << std::endl;
    }

    if (!isConnected()) {
        std::cerr << "[MQTTClient] Not connected, cannot publish" << std::endl;
        return false;
    }

    try {
        std::cout << "[MQTTClient] 🔍 Creating MQTT message..." << std::endl;
        mqtt::message_ptr pubmsg = mqtt::make_message(topic, payload);
        pubmsg->set_qos(qos);
        pubmsg->set_retained(retain);

        std::cout << "[MQTTClient] 🔍 Message created, calling client_->publish()..." << std::endl;
        std::cout << "[MQTTClient] 🔍 Connection state before publish: is_connected="
                  << client_->is_connected() << std::endl;

        auto token = client_->publish(pubmsg);

        std::cout << "[MQTTClient] 🔍 Publish token created, waiting for PUBACK..." << std::endl;
        token->wait();

        std::cout << "[MQTTClient] 🔍 PUBACK received" << std::endl;
        std::cout << "[MQTTClient] 🔍 Connection state after publish: is_connected="
                  << client_->is_connected() << std::endl;

        std::cout << "[MQTTClient] Published to " << topic
                  << " (" << payload.length() << " bytes)"
                  << (retain ? " [retained]" : "") << std::endl;

        return true;

    } catch (const mqtt::exception& e) {
        std::cerr << "[MQTTClient] ❌ Publish failed: " << e.what() << std::endl;
        std::cerr << "[MQTTClient] 🔍 Exception code: " << e.get_reason_code()
                  << ", message: " << e.get_message() << std::endl;
        std::cerr << "[MQTTClient] 🔍 Connection state after exception: connected_=" << connected_
                  << ", client_->is_connected()=" << (client_ ? std::to_string(client_->is_connected()) : "N/A")
                  << std::endl;
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
    std::cout << "[MQTTClient] 🚀 onMessageArrived() ENTERED!" << std::endl;

    std::string topic = msg->get_topic();
    std::string payload_str = msg->to_string();

    std::cout << "[MQTTClient] 📨 ========== MESSAGE ARRIVED ==========" << std::endl;
    std::cout << "[MQTTClient] 📨 Topic: " << topic << std::endl;
    std::cout << "[MQTTClient] 📨 Payload length: " << payload_str.length() << " bytes" << std::endl;
    std::cout << "[MQTTClient] 📄 Payload: " << payload_str << std::endl;
    std::cout << "[MQTTClient] 🔍 QoS: " << msg->get_qos() << std::endl;
    std::cout << "[MQTTClient] 🔍 Retained: " << msg->is_retained() << std::endl;
    std::cout << "[MQTTClient] 🔍 Thread ID: " << std::this_thread::get_id() << std::endl;
    std::cout << "[MQTTClient] 🔍 Connection state: is_connected=" << client_->is_connected() << std::endl;

    // Check for exact match in topic callbacks first (e.g., homeassistant/status)
    {
        std::lock_guard<std::mutex> lock(topic_callbacks_mutex_);
        auto it = topic_callbacks_.find(topic);
        if (it != topic_callbacks_.end()) {
            std::cout << "[MQTTClient] 🎯 Found exact match callback for topic: " << topic << std::endl;
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
    std::cerr << "[MQTTClient] ⚠️  ========== CONNECTION LOST ==========" << std::endl;
    std::cerr << "[MQTTClient] ⚠️  Cause: " << cause << std::endl;
    std::cerr << "[MQTTClient] 🔍 Thread ID: " << std::this_thread::get_id() << std::endl;
    std::cerr << "[MQTTClient] 🔍 Time: " << std::time(nullptr) << std::endl;

    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        std::cerr << "[MQTTClient] 🔍 Before update: connected_=" << connected_ << std::endl;
        connected_ = false;
        std::cerr << "[MQTTClient] 🔍 After update: connected_=" << connected_ << std::endl;
    }

    if (client_) {
        std::cerr << "[MQTTClient] 🔍 client_->is_connected()=" << client_->is_connected() << std::endl;
    } else {
        std::cerr << "[MQTTClient] 🔍 client_ is NULL!" << std::endl;
    }

    if (auto_reconnect_) {
        std::cout << "[MQTTClient] Auto-reconnect is enabled by paho-mqtt" << std::endl;
    }

    std::cerr << "[MQTTClient] ⚠️  ======================================" << std::endl;
}

} // namespace hms_firetv
