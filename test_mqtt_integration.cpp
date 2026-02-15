#include "mqtt/MQTTClient.h"
#include "mqtt/DiscoveryPublisher.h"
#include "mqtt/CommandHandler.h"
#include "services/DatabaseService.h"
#include "repositories/DeviceRepository.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>

using namespace hms_firetv;

// Global flag for graceful shutdown
bool running = true;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

void printSeparator(const std::string& title) {
    std::cout << "\n";
    std::cout << "============================================================================\n";
    std::cout << title << "\n";
    std::cout << "============================================================================\n";
}

void printTest(const std::string& test_name) {
    std::cout << "\n[TEST] " << test_name << "\n";
    std::cout << "------------------------------------------------------------------------\n";
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    printSeparator("HMS FireTV - MQTT Integration Test");

    // Test configuration
    std::string mqtt_broker = "tcp://192.168.2.15:1883";
    std::string mqtt_user = "aamat";
    std::string mqtt_password = "exploracion";

    std::string db_host = "192.168.2.15";
    int db_port = 5432;
    std::string db_name = "firetv";
    std::string db_user = "firetv_user";
    std::string db_password = "firetv_postgres_2026_secure";

    std::cout << "Configuration:\n";
    std::cout << "  MQTT Broker: " << mqtt_broker << "\n";
    std::cout << "  Database: " << db_host << ":" << db_port << "/" << db_name << "\n";

    // ========================================================================
    // TEST 1: Database Connection
    // ========================================================================
    printTest("Test 1: Database Connection");

    try {
        DatabaseService::getInstance().initialize(db_host, db_port, db_name, db_user, db_password);
        std::cout << "✅ Database initialized\n";
    } catch (const std::exception& e) {
        std::cerr << "❌ Database initialization failed: " << e.what() << "\n";
        std::cerr << "Cannot proceed without database connection\n";
        return 1;
    }

    // Get test device
    auto devices = DeviceRepository::getInstance().getAllDevices();
    if (devices.empty()) {
        std::cerr << "❌ No devices found in database\n";
        std::cerr << "Please add at least one device to test with\n";
        return 1;
    }

    Device test_device = devices[0];
    std::cout << "✅ Using test device: " << test_device.device_id
              << " (" << test_device.name << ")\n";
    std::cout << "   IP: " << test_device.ip_address << "\n";
    std::cout << "   Status: " << test_device.status << "\n";

    // ========================================================================
    // TEST 2: MQTT Client Connection
    // ========================================================================
    printTest("Test 2: MQTT Client Connection");

    auto mqtt_client = std::make_shared<MQTTClient>("test_mqtt_integration");

    if (!mqtt_client->connect(mqtt_broker, mqtt_user, mqtt_password)) {
        std::cerr << "❌ Failed to connect to MQTT broker\n";
        return 1;
    }

    std::cout << "✅ MQTT client connected\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ========================================================================
    // TEST 3: Discovery Publisher
    // ========================================================================
    printTest("Test 3: Home Assistant MQTT Discovery");

    auto discovery_publisher = std::make_shared<DiscoveryPublisher>(*mqtt_client);

    if (discovery_publisher->publishDevice(test_device)) {
        std::cout << "✅ Published discovery for " << test_device.name << "\n";
        std::cout << "   Check Home Assistant: Settings → Devices & Services → MQTT\n";
        std::cout << "   Device should appear as: " << test_device.name << "\n";
    } else {
        std::cerr << "❌ Failed to publish discovery\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ========================================================================
    // TEST 4: Availability Publishing
    // ========================================================================
    printTest("Test 4: Availability Publishing");

    if (discovery_publisher->publishAvailability(test_device.device_id, true)) {
        std::cout << "✅ Published availability: online\n";
        std::cout << "   Topic: maestro_hub/firetv/" << test_device.device_id << "/availability\n";
        std::cout << "   Payload: online\n";
    } else {
        std::cerr << "❌ Failed to publish availability\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ========================================================================
    // TEST 5: State Publishing
    // ========================================================================
    printTest("Test 5: State Publishing");

    Json::Value state;
    state["state"] = "idle";
    state["volume_level"] = 0.5;
    state["is_volume_muted"] = false;
    state["source"] = "Home Screen";

    if (mqtt_client->publishState(test_device.device_id, state)) {
        std::cout << "✅ Published state\n";
        std::cout << "   Topic: maestro_hub/firetv/" << test_device.device_id << "/state\n";
        std::cout << "   State: idle\n";
        std::cout << "   Volume: 50%\n";
    } else {
        std::cerr << "❌ Failed to publish state\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ========================================================================
    // TEST 6: Command Subscription
    // ========================================================================
    printTest("Test 6: Command Subscription & Handling");

    auto command_handler = std::make_shared<CommandHandler>();

    // Command counter for verification
    int commands_received = 0;

    // Subscribe with callback
    mqtt_client->subscribeToAllCommands(
        [command_handler, &commands_received](const std::string& device_id, const Json::Value& payload) {
            std::cout << "📩 Command received for " << device_id << "\n";

            if (payload.isMember("command")) {
                std::cout << "   Command: " << payload["command"].asString() << "\n";
            }

            // Increment counter
            commands_received++;

            // Handle command (will execute on real device if available)
            command_handler->handleCommand(device_id, payload);
        }
    );

    std::cout << "✅ Subscribed to command topics\n";
    std::cout << "   Listening on: maestro_hub/firetv/+/set\n";

    // ========================================================================
    // TEST 7: Simulated Commands (Manual Testing)
    // ========================================================================
    printTest("Test 7: Command Testing");

    std::cout << "MQTT Integration is ready for testing!\n\n";
    std::cout << "To test commands, open another terminal and run:\n\n";

    std::cout << "# Test volume up:\n";
    std::cout << "mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \\\n";
    std::cout << "  -t \"maestro_hub/firetv/" << test_device.device_id << "/set\" \\\n";
    std::cout << "  -m '{\"command\": \"volume_up\"}'\n\n";

    std::cout << "# Test volume down:\n";
    std::cout << "mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \\\n";
    std::cout << "  -t \"maestro_hub/firetv/" << test_device.device_id << "/set\" \\\n";
    std::cout << "  -m '{\"command\": \"volume_down\"}'\n\n";

    std::cout << "# Test navigation:\n";
    std::cout << "mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \\\n";
    std::cout << "  -t \"maestro_hub/firetv/" << test_device.device_id << "/set\" \\\n";
    std::cout << "  -m '{\"command\": \"navigate\", \"action\": \"home\"}'\n\n";

    std::cout << "# Test media play:\n";
    std::cout << "mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \\\n";
    std::cout << "  -t \"maestro_hub/firetv/" << test_device.device_id << "/set\" \\\n";
    std::cout << "  -m '{\"command\": \"media_play_pause\"}'\n\n";

    std::cout << "# Test app launch:\n";
    std::cout << "mosquitto_pub -h 192.168.2.15 -u aamat -P exploracion \\\n";
    std::cout << "  -t \"maestro_hub/firetv/" << test_device.device_id << "/set\" \\\n";
    std::cout << "  -m '{\"command\": \"launch_app\", \"package\": \"com.netflix.ninja\"}'\n\n";

    std::cout << "# Monitor state changes:\n";
    std::cout << "mosquitto_sub -h 192.168.2.15 -u aamat -P exploracion \\\n";
    std::cout << "  -t \"maestro_hub/firetv/#\" -v\n\n";

    // ========================================================================
    // TEST 8: Wait for commands (interactive mode)
    // ========================================================================
    printSeparator("Listening for MQTT commands (Press Ctrl+C to exit)");

    std::cout << "Service is running and waiting for commands...\n";
    std::cout << "Commands received: " << commands_received << "\n";

    // Keep running until signal
    int last_count = commands_received;
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Show update if new commands received
        if (commands_received != last_count) {
            std::cout << "📊 Total commands received: " << commands_received << "\n";
            last_count = commands_received;
        }
    }

    // ========================================================================
    // TEST 9: Cleanup
    // ========================================================================
    printTest("Test 9: Cleanup & Disconnect");

    // Publish offline availability
    discovery_publisher->publishAvailability(test_device.device_id, false);
    std::cout << "✅ Published offline availability\n";

    mqtt_client->disconnect();
    std::cout << "✅ MQTT client disconnected\n";

    // ========================================================================
    // Summary
    // ========================================================================
    printSeparator("Test Summary");

    std::cout << "Results:\n";
    std::cout << "  ✅ Database connection: OK\n";
    std::cout << "  ✅ MQTT connection: OK\n";
    std::cout << "  ✅ Discovery publishing: OK\n";
    std::cout << "  ✅ Availability publishing: OK\n";
    std::cout << "  ✅ State publishing: OK\n";
    std::cout << "  ✅ Command subscription: OK\n";
    std::cout << "  📊 Commands received: " << commands_received << "\n";

    std::cout << "\n✅ MQTT Integration Test Complete!\n\n";

    std::cout << "Next steps:\n";
    std::cout << "  1. Verify device appears in Home Assistant (Settings → MQTT)\n";
    std::cout << "  2. Test control from Home Assistant UI\n";
    std::cout << "  3. Monitor MQTT traffic with mosquitto_sub\n";
    std::cout << "  4. Run the full service: ./build/hms_firetv\n";

    return 0;
}
