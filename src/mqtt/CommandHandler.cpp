#include "mqtt/CommandHandler.h"
#include "repositories/AppsRepository.h"
#include "services/AppSyncService.h"
#include "services/VoiceService.h"
#include <algorithm>
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

    // Voice runs its own session lifecycle (bookends, WebSocket, TTS) and
    // wakes the device itself, so it does not go through the client cache.
    if (command.rfind("voice_", 0) == 0) {
        handleVoiceCommand(device_id, command, payload);
        DeviceRepository::getInstance().updateLastSeen(device_id, "online");
        return;
    }

    // Get Lightning client for device
    auto client = getClientForDevice(device_id);
    if (!client) {
        std::cerr << "[CommandHandler] Failed to get client for device: " << device_id << std::endl;
        return;
    }

    // Ensure device is awake (skip for turn_on which handles this itself)
    if (command != "turn_on") {
        if (!ensureDeviceAwake(*client)) {
            std::cerr << "[CommandHandler] Failed to wake device " << device_id << std::endl;
            return;
        }
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
        handleAppLaunchCommand(device_id, *client, payload);
    } else if (command == "send_text" || command == "keyboard_input") {
        handleTextInputCommand(*client, payload);
    } else if (command == "hold") {
        handleHoldCommand(*client, payload);
    } else if (command == "key_down") {
        handleKeyEdgeCommand(*client, payload, "keyDown");
    } else if (command == "key_up") {
        handleKeyEdgeCommand(*client, payload, "keyUp");
    } else if (command == "apps_refresh") {
        handleAppsRefreshCommand(device_id, *client);
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

void CommandHandler::handleAppLaunchCommand(const std::string& device_id,
                                            LightningClient& client,
                                            const Json::Value& payload) {
    std::string package;

    // Check for package name directly
    if (payload.isMember("package")) {
        package = payload["package"].asString();
    }
    // Check for source/app name
    else if (payload.isMember("source")) {
        std::string app_name = payload["source"].asString();

        // Prefer the apps synced from this device - that list is the real
        // one and is what the Home Assistant picker offers. The static map
        // below only covers eight well-known apps and cannot name anything
        // the device actually has installed.
        package = getPackageForAppOnDevice(device_id, app_name);
        if (package.empty()) package = getPackageForApp(app_name);

        if (package.empty()) {
            std::cerr << "[CommandHandler] Unknown app: " << app_name
                      << " (try apps_refresh to sync this device's app list)" << std::endl;
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
// PRESS AND HOLD
// ============================================================================

void CommandHandler::handleHoldCommand(LightningClient& client, const Json::Value& payload) {
    std::string action;
    if (payload.isMember("action")) {
        action = payload["action"].asString();
    } else if (payload.isMember("direction")) {
        action = "dpad_" + payload["direction"].asString();
    } else {
        std::cerr << "[CommandHandler] Hold command missing 'action' or 'direction'" << std::endl;
        return;
    }

    int ms = payload.get("ms", 500).asInt();
    // Bounded so a bad payload cannot pin a key down indefinitely - the
    // device keeps repeating until it sees the keyUp.
    ms = std::max(50, std::min(ms, 10000));

    auto down = client.holdKey(action);
    if (!down.success) {
        std::cerr << "[CommandHandler] ❌ keyDown failed: " << down.status_code << std::endl;
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ms));

    auto up = client.releaseKey(action);
    if (up.success) {
        std::cout << "[CommandHandler] ✅ Held " << action << " for " << ms << "ms" << std::endl;
    } else {
        // Leaving a key down would make the device unusable, so this is worth
        // shouting about rather than logging quietly.
        std::cerr << "[CommandHandler] ⚠️  keyUp failed for " << action << " ("
                  << up.status_code << ") - key may be stuck down" << std::endl;
    }
}

void CommandHandler::handleKeyEdgeCommand(LightningClient& client, const Json::Value& payload,
                                          const std::string& key_action_type) {
    std::string action;
    if (payload.isMember("action")) {
        action = payload["action"].asString();
    } else if (payload.isMember("direction")) {
        action = "dpad_" + payload["direction"].asString();
    } else {
        std::cerr << "[CommandHandler] " << key_action_type
                  << " missing 'action' or 'direction'" << std::endl;
        return;
    }

    auto result = client.sendNavigationCommand(action, key_action_type);
    if (result.success) {
        std::cout << "[CommandHandler] ✅ " << key_action_type << " " << action << std::endl;
    } else {
        std::cerr << "[CommandHandler] ❌ " << key_action_type << " " << action
                  << " failed: " << result.status_code << std::endl;
    }
}

// ============================================================================
// APP LIST
// ============================================================================

void CommandHandler::handleAppsRefreshCommand(const std::string& device_id,
                                              LightningClient& client) {
    auto result = AppSyncService::syncDevice(device_id, client);
    if (!result.success) {
        std::cerr << "[CommandHandler] ❌ App sync failed: " << result.error << std::endl;
        return;
    }

    std::cout << "[CommandHandler] ✅ Synced " << result.stored << " apps for " << device_id
              << std::endl;

    // The select entity's options live in its discovery payload, so the
    // dropdown only picks up the new list once that is republished.
    std::function<void(const std::string&)> republish;
    {
        std::lock_guard<std::mutex> lock(republish_mutex_);
        republish = discovery_republish_;
    }
    if (republish) republish(device_id);
}

void CommandHandler::setDiscoveryRepublish(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(republish_mutex_);
    discovery_republish_ = std::move(callback);
}

// ============================================================================
// VOICE
// ============================================================================

void CommandHandler::handleVoiceCommand(const std::string& device_id,
                                        const std::string& command,
                                        const Json::Value& payload) {
    auto& voice = VoiceService::getInstance();
    Json::Value result;

    if (command == "voice_start") {
        result = voice.startSession(device_id);
    } else if (command == "voice_stop") {
        result = voice.stopSession(device_id);
    } else if (command == "voice_say") {
        std::string text = payload.isMember("text") ? payload["text"].asString()
                                                    : payload.get("value", "").asString();
        if (text.empty()) {
            std::cerr << "[CommandHandler] voice_say with no text" << std::endl;
            return;
        }
        result = voice.say(device_id, text);
    } else if (command == "voice_audio") {
        if (payload.isMember("url")) {
            result = voice.speakUrl(device_id, payload["url"].asString());
        } else if (payload.isMember("file")) {
            result = voice.speakFile(device_id, payload["file"].asString());
        } else {
            std::cerr << "[CommandHandler] voice_audio needs 'url' or 'file'" << std::endl;
            return;
        }
    } else {
        std::cerr << "[CommandHandler] Unknown voice command: " << command << std::endl;
        return;
    }

    if (result.get("success", false).asBool()) {
        std::cout << "[CommandHandler] ✅ " << command << ": "
                  << result.get("message", "ok").asString() << std::endl;
    } else {
        std::cerr << "[CommandHandler] ❌ " << command << ": "
                  << result.get("error", "failed").asString() << std::endl;
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

std::string CommandHandler::getPackageForAppOnDevice(const std::string& device_id,
                                                     const std::string& app_name) {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    };

    std::string wanted = lower(app_name);
    auto apps = AppsRepository::getInstance().getAppsForDevice(device_id);

    // Exact name first.
    for (const auto& app : apps)
        if (lower(app.app_name) == wanted) return app.package_name;

    // Then the package itself, so a picker can pass either.
    for (const auto& app : apps)
        if (lower(app.package_name) == wanted) return app.package_name;

    // Then a prefix, because device titles carry marketing tails - asking for
    // "Tubi" should still launch "Tubi: Watch Free Movies & TV Shows".
    for (const auto& app : apps)
        if (lower(app.app_name).rfind(wanted, 0) == 0) return app.package_name;

    return "";
}

bool CommandHandler::ensureDeviceAwake(LightningClient& client) {
    // Check if Lightning API is responding
    if (client.isLightningApiAvailable()) {
        return true;  // Already awake
    }

    std::cout << "[CommandHandler] Device appears to be asleep, attempting wake..." << std::endl;

    // Try to wake the device
    if (!client.wakeDevice()) {
        std::cerr << "[CommandHandler] Wake request failed" << std::endl;
        return false;
    }

    // Wait for device to wake up (typically takes 2-5 seconds)
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if (client.isLightningApiAvailable()) {
            std::cout << "[CommandHandler] Device woke up after " << (attempt + 1) << "s" << std::endl;
            return true;
        }
    }

    std::cerr << "[CommandHandler] Device did not wake up after 5 seconds" << std::endl;
    return false;
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
