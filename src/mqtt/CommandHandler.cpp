#include "mqtt/CommandHandler.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace hms_firetv {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

CommandHandler::CommandHandler() {
    std::cout << "[CommandHandler] Initialized" << std::endl;

    // Initialize app package mappings
    app_packages_["Netflix"] = "com.netflix.ninja";
    app_packages_["Prime Video"] = "com.amazon.avod.thirdpartyclient";
    app_packages_["YouTube"] = "com.google.android.youtube.tv";
    app_packages_["Disney+"] = "com.disney.disneyplus";
    app_packages_["Hulu"] = "com.hulu.plus";
    app_packages_["HBO Max"] = "com.hbo.hbonow";
    app_packages_["Spotify"] = "com.spotify.tv.android";
    app_packages_["Plex"] = "com.plexapp.android";
}

// ============================================================================
// COMMAND HANDLING
// ============================================================================

void CommandHandler::handleCommand(const std::string& device_id, const Json::Value& payload) {
    std::cout << "[CommandHandler] Handling command for " << device_id << std::endl;

    // Get command from payload
    if (!payload.isMember("command")) {
        std::cerr << "[CommandHandler] No 'command' field in payload" << std::endl;
        return;
    }

    std::string command = payload["command"].asString();
    std::cout << "[CommandHandler] Command: " << command << std::endl;

    // Get Lightning client for device
    auto client = getClientForDevice(device_id);
    if (!client) {
        std::cerr << "[CommandHandler] Failed to get client for device: " << device_id << std::endl;
        return;
    }

    // Route command
    if (command.find("media_") == 0) {
        handleMediaCommand(*client, command);
    } else if (command.find("volume_") == 0) {
        handleVolumeCommand(*client, command);
    } else if (command == "turn_on" || command == "turn_off") {
        handlePowerCommand(*client, command);
    } else if (command == "navigate") {
        handleNavigationCommand(*client, payload);
    } else if (command == "select_source" || command == "launch_app") {
        handleAppLaunchCommand(*client, payload);
    } else if (command == "send_text" || command == "keyboard_input") {
        handleTextInputCommand(*client, payload);
    } else {
        std::cerr << "[CommandHandler] Unknown command: " << command << std::endl;
    }

    // Update last seen
    DeviceRepository::getInstance().updateLastSeen(device_id, "online");
}

// ============================================================================
// CLIENT MANAGEMENT
// ============================================================================

std::shared_ptr<LightningClient> CommandHandler::getClientForDevice(const std::string& device_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    // Check cache first
    auto it = clients_.find(device_id);
    if (it != clients_.end()) {
        return it->second;
    }

    // Get device from database
    auto device = DeviceRepository::getInstance().getDeviceById(device_id);
    if (!device.has_value()) {
        std::cerr << "[CommandHandler] Device not found: " << device_id << std::endl;
        return nullptr;
    }

    // Create Lightning client
    auto client = std::make_shared<LightningClient>(
        device->ip_address,
        device->api_key,
        device->client_token.value_or("")
    );

    // Cache client
    clients_[device_id] = client;

    return client;
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void CommandHandler::handleMediaCommand(LightningClient& client, const std::string& command) {
    CommandResult result;

    if (command == "media_play_pause" || command == "media_play") {
        result = client.play();
    } else if (command == "media_pause") {
        result = client.pause();
    } else if (command == "media_stop") {
        result = client.pause();  // Fire TV doesn't have explicit stop
    } else if (command == "media_next_track") {
        result = client.scanForward();
    } else if (command == "media_previous_track") {
        result = client.scanBackward();
    } else {
        std::cerr << "[CommandHandler] Unknown media command: " << command << std::endl;
        return;
    }

    if (result.success) {
        std::cout << "[CommandHandler] ✅ Media command succeeded ("
                  << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[CommandHandler] ❌ Media command failed: "
                  << result.status_code << std::endl;
    }
}

void CommandHandler::handleVolumeCommand(LightningClient& client, const std::string& command) {
    CommandResult result;

    if (command == "volume_up") {
        result = client.sendNavigationCommand("volume_up");
    } else if (command == "volume_down") {
        result = client.sendNavigationCommand("volume_down");
    } else if (command == "volume_mute") {
        result = client.sendNavigationCommand("volume_mute");
    } else {
        std::cerr << "[CommandHandler] Unknown volume command: " << command << std::endl;
        return;
    }

    if (result.success) {
        std::cout << "[CommandHandler] ✅ Volume command succeeded ("
                  << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[CommandHandler] ❌ Volume command failed: "
                  << result.status_code << std::endl;
    }
}

void CommandHandler::handleNavigationCommand(LightningClient& client, const Json::Value& payload) {
    CommandResult result;

    // Check for direction
    if (payload.isMember("direction")) {
        std::string direction = payload["direction"].asString();

        if (direction == "up") {
            result = client.dpadUp();
        } else if (direction == "down") {
            result = client.dpadDown();
        } else if (direction == "left") {
            result = client.dpadLeft();
        } else if (direction == "right") {
            result = client.dpadRight();
        } else {
            std::cerr << "[CommandHandler] Unknown direction: " << direction << std::endl;
            return;
        }
    }
    // Check for action
    else if (payload.isMember("action")) {
        std::string action = payload["action"].asString();

        if (action == "select") {
            result = client.select();
        } else if (action == "home") {
            result = client.home();
        } else if (action == "back") {
            result = client.back();
        } else if (action == "menu") {
            result = client.menu();
        } else {
            std::cerr << "[CommandHandler] Unknown action: " << action << std::endl;
            return;
        }
    } else {
        std::cerr << "[CommandHandler] Navigate command missing direction or action" << std::endl;
        return;
    }

    if (result.success) {
        std::cout << "[CommandHandler] ✅ Navigation command succeeded ("
                  << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[CommandHandler] ❌ Navigation command failed: "
                  << result.status_code << std::endl;
    }
}

void CommandHandler::handlePowerCommand(LightningClient& client, const std::string& command) {
    CommandResult result;

    if (command == "turn_on") {
        // Wake device
        bool woke = client.wakeDevice();
        if (woke) {
            std::cout << "[CommandHandler] ✅ Device wake command sent" << std::endl;
            // Wait for device to boot
            std::this_thread::sleep_for(std::chrono::seconds(3));
        } else {
            std::cerr << "[CommandHandler] ❌ Wake command failed" << std::endl;
        }
    } else if (command == "turn_off") {
        // Send sleep command
        result = client.sleep();
        if (result.success) {
            std::cout << "[CommandHandler] ✅ Sleep command succeeded" << std::endl;
        } else {
            std::cerr << "[CommandHandler] ❌ Sleep command failed" << std::endl;
        }
    }
}

void CommandHandler::handleAppLaunchCommand(LightningClient& client, const Json::Value& payload) {
    std::string package;

    // Check for package name directly
    if (payload.isMember("package")) {
        package = payload["package"].asString();
    }
    // Check for source/app name
    else if (payload.isMember("source")) {
        std::string app_name = payload["source"].asString();
        package = getPackageForApp(app_name);

        if (package.empty()) {
            std::cerr << "[CommandHandler] Unknown app: " << app_name << std::endl;
            return;
        }
    } else {
        std::cerr << "[CommandHandler] App launch missing 'package' or 'source'" << std::endl;
        return;
    }

    // Launch app
    std::cout << "[CommandHandler] Launching app: " << package << std::endl;
    auto result = client.launchApp(package);

    if (result.success) {
        std::cout << "[CommandHandler] ✅ App launched successfully ("
                  << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[CommandHandler] ❌ App launch failed: "
                  << result.status_code << std::endl;
    }
}

// ============================================================================
// HELPERS
// ============================================================================

std::string CommandHandler::getPackageForApp(const std::string& app_name) {
    auto it = app_packages_.find(app_name);
    if (it != app_packages_.end()) {
        return it->second;
    }

    // Try case-insensitive match
    for (const auto& pair : app_packages_) {
        std::string lower_name = app_name;
        std::string lower_key = pair.first;

        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

        if (lower_name == lower_key) {
            return pair.second;
        }
    }

    return "";  // Not found
}

void CommandHandler::handleTextInputCommand(LightningClient& client, const Json::Value& payload) {
    std::string text;

    // Check for text field (from text entity)
    if (payload.isMember("text")) {
        text = payload["text"].asString();
    }
    // Check for direct string (from MQTT)
    else if (payload.isString()) {
        text = payload.asString();
    }
    else {
        std::cerr << "[CommandHandler] Text input missing 'text' field" << std::endl;
        return;
    }

    if (text.empty()) {
        std::cerr << "[CommandHandler] Text input is empty" << std::endl;
        return;
    }

    // Send keyboard input
    std::cout << "[CommandHandler] Sending keyboard input: " << text << std::endl;
    auto result = client.sendKeyboardInput(text);

    if (result.success) {
        std::cout << "[CommandHandler] ✅ Text input sent successfully ("
                  << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[CommandHandler] ❌ Text input failed: "
                  << result.status_code << std::endl;
    }
}

} // namespace hms_firetv
