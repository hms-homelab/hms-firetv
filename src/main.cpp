#include <drogon/drogon.h>
#include <iostream>
#include <fstream>
#include <csignal>
#include <filesystem>
#include "utils/ConfigManager.h"
#include "utils/AppConfig.h"
#include "database/IDatabase.h"
#include "database/SQLiteDatabase.h"
#ifdef WITH_POSTGRESQL
#include "database/PostgresDatabase.h"
#endif
#include "repositories/DeviceRepository.h"
#include "repositories/AppsRepository.h"
#include "api/StatsController.h"
#include "mqtt/MQTTClient.h"
#include "mqtt/DiscoveryPublisher.h"
#include "mqtt/CommandHandler.h"
#include "api/DeviceController.h"
#include "api/CommandController.h"
#include "api/PairingController.h"
#include "api/AppsController.h"
#include "services/DiscoveryService.h"

using namespace drogon;
using namespace hms_firetv;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    app().quit();
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "================================================================================\n";
    std::cout << "Starting HMS FireTV v1.0.5\n";
    std::cout << "================================================================================\n";

    try {
        // Load configuration
        std::string api_host     = ConfigManager::getEnv("API_HOST", "0.0.0.0");
        int api_port             = ConfigManager::getEnvInt("API_PORT", 8888);
        int thread_num           = ConfigManager::getEnvInt("THREAD_NUM", 4);
        int idle_timeout         = ConfigManager::getEnvInt("IDLE_CONNECTION_TIMEOUT", 60);
        std::string log_level    = ConfigManager::getEnv("LOG_LEVEL", "info");
        std::string mqtt_broker  = ConfigManager::getEnv("MQTT_BROKER_HOST", "192.168.2.15");
        int mqtt_port            = ConfigManager::getEnvInt("MQTT_BROKER_PORT", 1883);
        std::string mqtt_user    = ConfigManager::getEnv("MQTT_USER", "aamat");
        std::string mqtt_pass    = ConfigManager::getEnv("MQTT_PASS", "exploracion");
        std::string mqtt_addr    = "tcp://" + mqtt_broker + ":" + std::to_string(mqtt_port);

        // Database config (SQLite by default; PostgreSQL if DB_HOST + DB_NAME set)
        AppConfig config;
        config.applyEnvFallbacks();
        if (config.database.sqlite_path.empty()) {
            const char* home = getenv("HOME");
            config.database.sqlite_path = std::string(home ? home : "/tmp") + "/.hms-firetv/firetv.db";
        }

        std::cout << "Configuration:\n";
        std::cout << "  API: " << api_host << ":" << api_port << "\n";
        std::cout << "  DB type: " << config.database.type << "\n";
        if (config.database.type == "sqlite")
            std::cout << "  DB path: " << config.database.sqlite_path << "\n";
        else
            std::cout << "  DB host: " << config.database.host << ":" << config.database.port
                      << "/" << config.database.name << "\n";
        std::cout << "  MQTT: " << mqtt_addr << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        // Create database backend
        std::cout << "Initializing services...\n";
        std::shared_ptr<IDatabase> db;
        if (config.database.type == "postgresql") {
#ifdef WITH_POSTGRESQL
            db = std::make_shared<PostgresDatabase>(config.database.host, config.database.port,
                                                    config.database.name, config.database.user,
                                                    config.database.password);
#else
            std::cerr << "  PostgreSQL requested but not compiled in — use -DBUILD_WITH_POSTGRESQL=ON\n";
            return 1;
#endif
        } else {
            std::filesystem::create_directories(
                std::filesystem::path(config.database.sqlite_path).parent_path());
            db = std::make_shared<SQLiteDatabase>(config.database.sqlite_path);
        }

        if (!db->connect()) {
            std::cerr << "  Database connection failed — service will continue degraded\n";
        } else {
            std::cout << "  ✓ Database connected (" << config.database.type << ")\n";
        }

        // Inject DB into repositories and StatsController
        DeviceRepository::setDatabase(db);
        AppsRepository::setDatabase(db);
        StatsController::setDatabase(db);

        // Initialize background logger
        CommandController::initBackgroundLogger();
        std::cout << "  ✓ Background logger initialized\n";

        // MQTT
        std::shared_ptr<MQTTClient> mqtt_client;
        std::shared_ptr<DiscoveryPublisher> discovery_publisher;
        std::shared_ptr<CommandHandler> command_handler;

        try {
            mqtt_client = std::make_shared<MQTTClient>("hms_firetv");
            if (mqtt_client->connect(mqtt_addr, mqtt_user, mqtt_pass)) {
                std::cout << "  ✓ MQTT client connected\n";
                discovery_publisher = std::make_shared<DiscoveryPublisher>(*mqtt_client);
                command_handler = std::make_shared<CommandHandler>();

                try {
                    auto devices = DeviceRepository::getInstance().getAllDevices();
                    int published = 0;
                    for (const auto& device : devices)
                        if (discovery_publisher->publishDevice(device)) published++;
                    std::cout << "  ✓ Published discovery for " << published << "/" << devices.size() << " devices\n";
                } catch (const std::exception& e) {
                    std::cerr << "  ⚠ Discovery publish failed: " << e.what() << "\n";
                }

                mqtt_client->registerTopicCallback("homeassistant/status",
                    [discovery_publisher](const std::string&, const std::string& payload) {
                        if (payload == "online") {
                            try {
                                auto devices = DeviceRepository::getInstance().getAllDevices();
                                int published = 0;
                                for (const auto& d : devices)
                                    if (discovery_publisher->publishDevice(d)) published++;
                                std::cout << "[HA_STATUS] Republished " << published << "/" << devices.size() << " devices\n";
                            } catch (...) {}
                        }
                    });

                mqtt_client->subscribeToAllCommands(
                    [command_handler](const std::string& device_id, const Json::Value& payload) {
                        command_handler->handleCommand(device_id, payload);
                    });
                std::cout << "  ✓ Subscribed to all command topics\n";
            } else {
                std::cerr << "  ✗ MQTT connection failed — MQTT features unavailable\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  ✗ MQTT init failed: " << e.what() << "\n";
        }

        // Device discovery
        std::string discovery_subnet = ConfigManager::getEnv("DISCOVERY_SUBNET", "192.168.2");
        int discovery_interval = ConfigManager::getEnvInt("DISCOVERY_INTERVAL", 300);
        DiscoveryService::initialize(discovery_subnet, discovery_interval);
        if (mqtt_client) DiscoveryService::getInstance().setMqttClient(mqtt_client);
        DiscoveryService::getInstance().start();
        std::cout << "  ✓ DiscoveryService started (every " << discovery_interval << "s)\n";

        std::cout << "Services initialized\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        // HTTP server
        app().setLogLevel(trantor::Logger::LogLevel::kInfo)
            .addListener(api_host, api_port)
            .setThreadNum(thread_num)
            .setIdleConnectionTimeout(idle_timeout)
            .setMaxConnectionNum(10000)
            .setMaxConnectionNumPerIP(0)
            .setClientMaxBodySize(10 * 1024 * 1024)
            .setDocumentRoot("./static")
            .setStaticFilesCacheTime(3600);

        // SPA fallback
        std::string spa_fallback;
        {
            std::ifstream ifs("./static/index.html");
            if (ifs.good())
                spa_fallback = std::string(std::istreambuf_iterator<char>(ifs), {});
        }
        if (!spa_fallback.empty()) {
            app().setCustomErrorHandler(
                [spa_fallback](HttpStatusCode code, const HttpRequestPtr& req) -> HttpResponsePtr {
                    if (code == k404NotFound) {
                        const auto& path = req->path();
                        if (path.find("/api/") == 0 || path == "/health" || path == "/status") {
                            Json::Value err; err["success"] = false; err["error"] = "Not found: " + path;
                            return HttpResponse::newHttpJsonResponse(err);
                        }
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setStatusCode(k200OK);
                        resp->setContentTypeCode(CT_TEXT_HTML);
                        resp->setBody(spa_fallback);
                        return resp;
                    }
                    return nullptr;
                });
        }

        // Health endpoint
        app().registerHandler("/health",
            [mqtt_client, &config, db](const HttpRequestPtr&,
                std::function<void(const HttpResponsePtr&)>&& callback) {
                Json::Value r;
                r["service"] = "HMS FireTV";
                r["version"] = "1.0.5";
                r["db_type"] = config.database.type;
                bool db_ok   = db && db->isConnected();
                bool mqtt_ok = mqtt_client && mqtt_client->isConnected();
                r["database"] = db_ok ? "connected" : "disconnected";
                r["mqtt"]     = mqtt_ok ? "connected" : "disconnected";
                r["status"]   = (db_ok && mqtt_ok) ? "healthy" : "degraded";
                auto resp = HttpResponse::newHttpJsonResponse(r);
                resp->setStatusCode((db_ok && mqtt_ok) ? k200OK : k503ServiceUnavailable);
                callback(resp);
            }, {Get});

        // Status endpoint
        app().registerHandler("/status",
            [mqtt_client, &config, db, mqtt_addr](const HttpRequestPtr&,
                std::function<void(const HttpResponsePtr&)>&& callback) {
                Json::Value r;
                r["service"] = "HMS FireTV";
                r["version"] = "1.0.5";
                r["status"]  = "running";
                bool db_ok   = db && db->isConnected();
                bool mqtt_ok = mqtt_client && mqtt_client->isConnected();
                r["connections"]["database"] = db_ok ? "connected" : "disconnected";
                r["connections"]["mqtt"]     = mqtt_ok ? "connected" : "disconnected";
                r["config"]["db_type"]       = config.database.type;
                r["config"]["mqtt_broker"]   = mqtt_addr;
                try {
                    auto devices = DeviceRepository::getInstance().getAllDevices();
                    int paired = 0, online = 0;
                    for (const auto& d : devices) {
                        if (d.isPaired()) paired++;
                        if (d.isOnline()) online++;
                    }
                    r["devices"]["total"]  = static_cast<int>(devices.size());
                    r["devices"]["paired"] = paired;
                    r["devices"]["online"] = online;
                } catch (...) {
                    r["devices"]["total"] = 0;
                }
                auto resp = HttpResponse::newHttpJsonResponse(r);
                resp->setStatusCode(k200OK);
                callback(resp);
            }, {Get});

        std::cout << "================================================================================\n";
        std::cout << "HMS FireTV ready on " << api_host << ":" << api_port << "\n";
        std::cout << "================================================================================\n";

        app().run();

        DiscoveryService::getInstance().stop();
        CommandController::shutdownBackgroundLogger();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        try { CommandController::shutdownBackgroundLogger(); } catch (...) {}
        return 1;
    }

    std::cout << "HMS FireTV shut down successfully\n";
    return 0;
}
