#include "mqtt/DiscoveryPublisher.h"
#include "repositories/AppsRepository.h"
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

    std::vector<std::string> buttons = {
        // Navigation
        "up", "down", "left", "right", "select",
        // Held navigation. A Home Assistant button is a single press, so
        // scrolling a long list meant tapping Down twenty times. These send a
        // keyDown/keyUp pair with a gap between them, which is how the
        // official app produces key repeat.
        "hold_up", "hold_down", "hold_left", "hold_right",
        // Media
        "play", "pause", "fast_forward", "rewind",
        // System
        "home", "back", "menu",
        // Volume
        "volume_up", "volume_down", "mute",
        // Power
        "sleep", "wake",
        // Voice session, without audio - the audio modes are the text entity
        // below and the REST relay.
        "voice_start", "voice_stop",
        // Re-read the installed app list off the device.
        "apps_refresh"
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

    // Voice: type a phrase, the service speaks it to Alexa.
    if (publishVoiceEntity(device)) {
        std::cout << "[DiscoveryPublisher] ✅ Published voice entity for " << device.name << std::endl;
    } else {
        std::cerr << "[DiscoveryPublisher] ⚠️  Failed to publish voice entity" << std::endl;
    }

    // App picker, from the apps the device itself reported. Absent until a
    // sync has run, which is not an error on a first-seen device.
    publishAppSelect(device);

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
        // Held navigation
        {"hold_up", {"Scroll Up", "mdi:chevron-double-up"}},
        {"hold_down", {"Scroll Down", "mdi:chevron-double-down"}},
        {"hold_left", {"Scroll Left", "mdi:chevron-double-left"}},
        {"hold_right", {"Scroll Right", "mdi:chevron-double-right"}},
        // Media
        {"play", {"Play", "mdi:play"}},
        {"pause", {"Pause", "mdi:pause"}},
        {"fast_forward", {"Fast Forward", "mdi:fast-forward"}},
        {"rewind", {"Rewind", "mdi:rewind"}},
        // Voice
        {"voice_start", {"Voice Start", "mdi:microphone"}},
        {"voice_stop", {"Voice Stop", "mdi:microphone-off"}},
        // Apps
        {"apps_refresh", {"Refresh Apps", "mdi:refresh"}},
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
        {"wake", "wake"},
        {"hold_up", "hold_up"},
        {"hold_down", "hold_down"},
        {"hold_left", "hold_left"},
        {"hold_right", "hold_right"},
        {"fast_forward", "fast_forward"},
        {"rewind", "rewind"},
        {"voice_start", "voice_start"},
        {"voice_stop", "voice_stop"},
        {"apps_refresh", "apps_refresh"}
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

bool DiscoveryPublisher::publishAppSelect(const Device& device) {
    auto apps = AppsRepository::getInstance().getAppsForDevice(device.device_id);
    if (apps.empty()) {
        // Nothing synced yet. Publishing a select with no options would give
        // Home Assistant an empty dropdown, which reads as broken; the
        // Refresh Apps button fills this in and discovery runs again.
        std::cout << "[DiscoveryPublisher] no apps stored for " << device.device_id
                  << " yet - skipping app picker (press Refresh Apps)" << std::endl;
        return false;
    }

    Json::Value options(Json::arrayValue);
    for (const auto& app : apps) options.append(app.app_name);

    Json::Value config;
    config["name"] = device.name + " App";
    config["unique_id"] = "colada_" + device.device_id + "_app";
    config["device"] = buildDeviceInfo(device);
    config["command_topic"] = "maestro_hub/colada/" + device.device_id + "/launch_app";
    config["options"] = options;
    config["icon"] = "mdi:apps";

    std::string topic = "homeassistant/select/colada/" + device.device_id + "_app/config";
    bool published =
        mqtt_client_.publish(topic, Json::writeString(Json::StreamWriterBuilder(), config), 1, true);

    if (published)
        std::cout << "[DiscoveryPublisher] ✅ Published app picker with " << options.size()
                  << " apps for " << device.name << std::endl;
    return published;
}

bool DiscoveryPublisher::publishVoiceEntity(const Device& device) {
    Json::Value config;

    config["name"] = device.name + " Voice Command";
    config["unique_id"] = "colada_" + device.device_id + "_voice";
    config["device"] = buildDeviceInfo(device);
    config["command_topic"] = "maestro_hub/colada/" + device.device_id + "/voice_say";
    config["icon"] = "mdi:microphone-message";
    config["mode"] = "text";
    // Alexa utterances are short; the cap keeps a runaway template from
    // synthesising a paragraph at the TV.
    config["max"] = 255;

    std::string topic = "homeassistant/text/colada/" + device.device_id + "_voice/config";
    return mqtt_client_.publish(topic, Json::writeString(Json::StreamWriterBuilder(), config), 1,
                                true);
}

} // namespace hms_firetv
