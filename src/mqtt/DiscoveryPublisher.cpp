#include "mqtt/DiscoveryPublisher.h"
#include <iostream>

namespace hms_firetv {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

DiscoveryPublisher::DiscoveryPublisher(MQTTClient& mqtt_client)
    : mqtt_client_(mqtt_client) {
    std::cout << "[DiscoveryPublisher] Initialized" << std::endl;
}

// ============================================================================
// PUBLIC METHODS
// ============================================================================

bool DiscoveryPublisher::publishDevice(const Device& device) {
    std::cout << "[DiscoveryPublisher] Publishing button discovery for " << device.device_id << std::endl;

    // Python service publishes 15 button entities per device
    std::vector<std::string> buttons = {
        // Navigation
        "up", "down", "left", "right", "select",
        // Media
        "play", "pause",
        // System
        "home", "back", "menu",
        // Volume
        "volume_up", "volume_down", "mute",
        // Power
        "sleep", "wake"
    };

    size_t published = 0;
    for (const auto& button : buttons) {
        Json::Value config = buildButtonConfig(device, button);

        // Topic: homeassistant/button/colada/{device_id}_{button}/config
        std::string topic = "homeassistant/button/colada/" + device.device_id + "_" + button + "/config";

        if (mqtt_client_.publish(topic, Json::writeString(Json::StreamWriterBuilder(), config), 1, true)) {
            published++;
        }
    }

    if (published == buttons.size()) {
        std::cout << "[DiscoveryPublisher] ✅ Published " << published << " buttons for " << device.name << std::endl;
    } else {
        std::cerr << "[DiscoveryPublisher] ⚠️  Only published " << published << "/" << buttons.size() << " buttons" << std::endl;
    }

    // Publish text entity for keyboard input
    if (publishTextEntity(device)) {
        std::cout << "[DiscoveryPublisher] ✅ Published text entity for " << device.name << std::endl;
    } else {
        std::cerr << "[DiscoveryPublisher] ⚠️  Failed to publish text entity" << std::endl;
    }

    // Publish initial availability
    publishAvailability(device.device_id, device.status == "online");
    return published == buttons.size();
}

bool DiscoveryPublisher::removeDevice(const std::string& device_id) {
    std::cout << "[DiscoveryPublisher] Removing device " << device_id << std::endl;
    return mqtt_client_.removeDevice(device_id);
}

bool DiscoveryPublisher::publishAvailability(const std::string& device_id, bool online) {
    return mqtt_client_.publishAvailability(device_id, online);
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

Json::Value DiscoveryPublisher::buildButtonConfig(const Device& device, const std::string& button_id) {
    Json::Value config;

    // Map button IDs to friendly names and icons (matching Python service)
    std::map<std::string, std::pair<std::string, std::string>> button_info = {
        // Navigation
        {"up", {"Up", "mdi:arrow-up"}},
        {"down", {"Down", "mdi:arrow-down"}},
        {"left", {"Left", "mdi:arrow-left"}},
        {"right", {"Right", "mdi:arrow-right"}},
        {"select", {"Select", "mdi:checkbox-blank-circle"}},
        // Media
        {"play", {"Play", "mdi:play"}},
        {"pause", {"Pause", "mdi:pause"}},
        // System
        {"home", {"Home", "mdi:home"}},
        {"back", {"Back", "mdi:arrow-left-circle"}},
        {"menu", {"Menu", "mdi:menu"}},
        // Volume
        {"volume_up", {"Volume Up", "mdi:volume-plus"}},
        {"volume_down", {"Volume Down", "mdi:volume-minus"}},
        {"mute", {"Mute", "mdi:volume-mute"}},
        // Power
        {"sleep", {"Sleep", "mdi:power-sleep"}},
        {"wake", {"Wake", "mdi:power"}}
    };

    auto info = button_info[button_id];
    std::string friendly_name = info.first;
    std::string icon = info.second;

    // Map button IDs to actions (matching Python line 48-70)
    std::map<std::string, std::string> button_actions = {
        {"up", "dpad_up"},
        {"down", "dpad_down"},
        {"left", "dpad_left"},
        {"right", "dpad_right"},
        {"select", "select"},
        {"play", "play"},
        {"pause", "pause"},
        {"home", "home"},
        {"back", "back"},
        {"menu", "menu"},
        {"volume_up", "volume_up"},
        {"volume_down", "volume_down"},
        {"mute", "mute"},
        {"sleep", "sleep"},
        {"wake", "wake"}
    };

    std::string action = button_actions[button_id];

    // Button configuration (matching Python discovery.py line 75-87)
    config["name"] = device.name + " " + friendly_name;
    config["unique_id"] = "colada_" + device.device_id + "_" + button_id;
    config["device"] = buildDeviceInfo(device);
    config["command_topic"] = "maestro_hub/colada/" + device.device_id + "/" + action;
    config["payload_press"] = "PRESS";
    config["icon"] = icon;

    return config;
}

Json::Value DiscoveryPublisher::buildDeviceInfo(const Device& device) {
    Json::Value device_info;

    // Device identifiers (must be array) - matching Python line 38
    Json::Value identifiers(Json::arrayValue);
    identifiers.append("colada_" + device.device_id);  // Changed from "firetv_" to "colada_"
    device_info["identifiers"] = identifiers;

    // Device details - matching Python line 39-42
    device_info["name"] = device.name;
    device_info["manufacturer"] = "Amazon";
    device_info["model"] = "Fire TV";

    // Connection info - matching Python line 43
    Json::Value connections(Json::arrayValue);
    Json::Value connection(Json::arrayValue);
    connection.append("ip");
    connection.append(device.ip_address);
    connections.append(connection);
    device_info["connections"] = connections;

    return device_info;
}

bool DiscoveryPublisher::publishTextEntity(const Device& device) {
    Json::Value config;

    // Text entity configuration for keyboard input
    config["name"] = device.name + " Text Input";
    config["unique_id"] = "colada_" + device.device_id + "_text_input";
    config["device"] = buildDeviceInfo(device);
    config["command_topic"] = "maestro_hub/colada/" + device.device_id + "/send_text";
    config["icon"] = "mdi:keyboard";
    config["mode"] = "text";  // Single-line text input

    // Topic: homeassistant/text/colada/{device_id}_text_input/config
    std::string topic = "homeassistant/text/colada/" + device.device_id + "_text_input/config";

    return mqtt_client_.publish(topic, Json::writeString(Json::StreamWriterBuilder(), config), 1, true);
}

} // namespace hms_firetv
