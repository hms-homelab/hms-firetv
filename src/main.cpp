#include <drogon/drogon.h>
#include <iostream>
#include <csignal>
#include "utils/ConfigManager.h"
#include "services/DatabaseService.h"
#include "repositories/DeviceRepository.h"
#include "repositories/AppsRepository.h"
#include "mqtt/MQTTClient.h"
#include "mqtt/DiscoveryPublisher.h"
#include "mqtt/CommandHandler.h"
#include "api/DeviceController.h"
#include "api/CommandController.h"
#include "api/PairingController.h"
#include "api/AppsController.h"
#include "api/StatsController.h"

using namespace drogon;
using namespace hms_firetv;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    app().quit();
}

int main() {
    // Install signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "================================================================================\n";
    std::cout << "Starting HMS FireTV v1.0.0\n";
    std::cout << "================================================================================\n";

    try {
        // Load configuration from environment variables
        std::string api_host = ConfigManager::getEnv("API_HOST", "0.0.0.0");
        int api_port = ConfigManager::getEnvInt("API_PORT", 8888);
        int thread_num = ConfigManager::getEnvInt("THREAD_NUM", 4);
        int idle_timeout = ConfigManager::getEnvInt("IDLE_CONNECTION_TIMEOUT", 60);
        std::string log_level = ConfigManager::getEnv("LOG_LEVEL", "info");

        // Database configuration
        std::string db_host = ConfigManager::getEnv("DB_HOST", "localhost");
        int db_port = ConfigManager::getEnvInt("DB_PORT", 5432);
        std::string db_name = ConfigManager::getEnv("DB_NAME", "firetv");
        std::string db_user = ConfigManager::getEnv("DB_USER", "firetv_user");
        std::string db_password = ConfigManager::getEnv("DB_PASSWORD", "");

        // MQTT configuration
        std::string mqtt_broker = ConfigManager::getEnv("MQTT_BROKER_HOST", "localhost");
        int mqtt_port = ConfigManager::getEnvInt("MQTT_BROKER_PORT", 1883);
        std::string mqtt_user = ConfigManager::getEnv("MQTT_USER", "");
        std::string mqtt_password = ConfigManager::getEnv("MQTT_PASS", "");
        std::string mqtt_broker_address = "tcp://" + mqtt_broker + ":" + std::to_string(mqtt_port);

        std::cout << "Configuration loaded:\n";
        std::cout << "  API Endpoint: " << api_host << ":" << api_port << "\n";
        std::cout << "  Thread Pool: " << thread_num << " threads\n";
        std::cout << "  Idle Timeout: " << idle_timeout << "s\n";
        std::cout << "  Log Level: " << log_level << "\n";
        std::cout << "  Database: " << db_host << ":" << db_port << "/" << db_name << "\n";
        std::cout << "  MQTT Broker: " << mqtt_broker_address << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        // Initialize DatabaseService
        std::cout << "Initializing services...\n";
        try {
            DatabaseService::getInstance().initialize(db_host, db_port, db_name, db_user, db_password);
            std::cout << "  ✓ DatabaseService initialized\n";
        } catch (const std::exception& e) {
            std::cerr << "  ✗ DatabaseService initialization failed: " << e.what() << "\n";
            std::cerr << "  Service will continue but database operations will fail\n";
        }

        // Initialize background logger for async command logging
        CommandController::initBackgroundLogger();
        std::cout << "  ✓ Background logger initialized\n";

        // Initialize MQTT services
        std::shared_ptr<MQTTClient> mqtt_client;
        std::shared_ptr<DiscoveryPublisher> discovery_publisher;
        std::shared_ptr<CommandHandler> command_handler;

        try {
            // Create MQTT client
            mqtt_client = std::make_shared<MQTTClient>("hms_firetv");

            // Connect to MQTT broker
            if (mqtt_client->connect(mqtt_broker_address, mqtt_user, mqtt_password)) {
                std::cout << "  ✓ MQTT client connected\n";

                // Create discovery publisher
                discovery_publisher = std::make_shared<DiscoveryPublisher>(*mqtt_client);
                std::cout << "  ✓ DiscoveryPublisher initialized\n";

                // Create command handler
                command_handler = std::make_shared<CommandHandler>();
                std::cout << "  ✓ CommandHandler initialized\n";

                // Publish discovery for all devices FIRST
                try {
                    auto devices = DeviceRepository::getInstance().getAllDevices();
                    int published = 0;
                    for (const auto& device : devices) {
                        if (discovery_publisher->publishDevice(device)) {
                            published++;
                        }
                    }
                    std::cout << "  ✓ Published discovery for " << published << "/" << devices.size() << " devices\n";
                } catch (const std::exception& e) {
                    std::cerr << "  ⚠ Failed to publish discovery: " << e.what() << "\n";
                }

                // Register Home Assistant status callback WITHOUT subscribing
                // The batch subscription below will handle the actual MQTT subscription
                mqtt_client->registerTopicCallback("homeassistant/status",
                    [discovery_publisher](const std::string& topic, const std::string& payload) {
                        if (payload == "online") {
                            std::cout << "\n[HA_STATUS] Home Assistant restarted - republishing discovery...\n";
                            try {
                                auto devices = DeviceRepository::getInstance().getAllDevices();
                                int published = 0;
                                for (const auto& device : devices) {
                                    if (discovery_publisher->publishDevice(device)) {
                                        published++;
                                    }
                                }
                                std::cout << "[HA_STATUS] ✅ Republished " << published << "/" << devices.size() << " devices\n\n";
                            } catch (const std::exception& e) {
                                std::cerr << "[HA_STATUS] ❌ Failed to republish: " << e.what() << "\n";
                            }
                        }
                    }
                );
                std::cout << "  ✓ Registered Home Assistant status callback (batch subscription will activate it)\n";

                // CRITICAL: Subscribe to ALL topics in a SINGLE batch
                // This MUST be called LAST to include homeassistant/status in the batch
                // Calling any subscribe() after this will overwrite the batch!
                mqtt_client->subscribeToAllCommands(
                    [command_handler](const std::string& device_id, const Json::Value& payload) {
                        command_handler->handleCommand(device_id, payload);
                    }
                );
                std::cout << "  ✓ Subscribed to all command topics + Home Assistant status (batch)\n";

            } else {
                std::cerr << "  ✗ MQTT connection failed\n";
                std::cerr << "  Service will continue but MQTT features will be unavailable\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  ✗ MQTT initialization failed: " << e.what() << "\n";
            std::cerr << "  Service will continue but MQTT features will be unavailable\n";
        }

        std::cout << "Services initialized\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        // Configure Drogon HTTP server
        app().setLogLevel(trantor::Logger::LogLevel::kInfo)
            .addListener(api_host, api_port)
            .setThreadNum(thread_num)
            .setIdleConnectionTimeout(idle_timeout)
            .setMaxConnectionNum(10000)
            .setMaxConnectionNumPerIP(0)  // No limit per IP
            .setClientMaxBodySize(10 * 1024 * 1024)  // 10MB max body
            .setDocumentRoot("./static")  // Serve static files from ./static directory
            .setStaticFilesCacheTime(0);  // Disable cache during development

        // Register health check endpoint
        app().registerHandler(
            "/health",
            [mqtt_client](const HttpRequestPtr& req,
               std::function<void(const HttpResponsePtr&)>&& callback) {
                Json::Value response;
                response["service"] = "HMS FireTV";
                response["version"] = "1.0.0";

                // Check service health
                bool db_connected = DatabaseService::getInstance().isConnected();
                bool mqtt_connected = mqtt_client && mqtt_client->isConnected();

                response["database"] = db_connected ? "connected" : "disconnected";
                response["mqtt"] = mqtt_connected ? "connected" : "disconnected";

                // Overall status
                bool healthy = db_connected && mqtt_connected;
                response["status"] = healthy ? "healthy" : "degraded";

                auto resp = HttpResponse::newHttpJsonResponse(response);
                resp->setStatusCode(healthy ? k200OK : k503ServiceUnavailable);
                callback(resp);
            },
            {Get}
        );

        // Register status endpoint
        app().registerHandler(
            "/status",
            [](const HttpRequestPtr& req,
               std::function<void(const HttpResponsePtr&)>&& callback) {
                Json::Value response;
                response["service"] = "HMS FireTV";
                response["version"] = "1.0.0";
                response["status"] = "running";
                response["uptime_seconds"] = time(nullptr);  // Simplified for now

                auto resp = HttpResponse::newHttpJsonResponse(response);
                resp->setStatusCode(k200OK);
                callback(resp);
            },
            {Get}
        );

        // Note: REST API controllers with METHOD_LIST_BEGIN auto-register in Drogon
        // No need to manually call registerController() for:
        // - DeviceController, CommandController, PairingController, AppsController
        std::cout << "REST API controllers will auto-register (METHOD_LIST_BEGIN)\n";

        std::cout << "HTTP server configured\n";
        std::cout << "================================================================================\n";
        std::cout << "HMS FireTV is ready and listening on " << api_host << ":" << api_port << "\n";
        std::cout << "Health check:  http://localhost:" << api_port << "/health\n";
        std::cout << "Status check:  http://localhost:" << api_port << "/status\n";
        std::cout << "API Docs:      http://localhost:" << api_port << "/api/docs (TODO)\n";
        std::cout << "Web UI:        http://localhost:" << api_port << "/ (TODO)\n";
        std::cout << "================================================================================\n";

        // Run the application (blocks until quit signal)
        app().run();

        // Graceful shutdown: drain background logger queue
        std::cout << "Shutting down background logger...\n";
        CommandController::shutdownBackgroundLogger();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error during startup: " << e.what() << std::endl;

        // Attempt to shutdown logger even on error
        try {
            CommandController::shutdownBackgroundLogger();
        } catch (...) {}

        return 1;
    }

    std::cout << "HMS FireTV shut down successfully\n";
    return 0;
}
