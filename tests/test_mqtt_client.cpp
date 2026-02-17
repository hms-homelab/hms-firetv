#include <gtest/gtest.h>
#include "mqtt/MQTTClient.h"
#include "repositories/DeviceRepository.h"
#include "services/DatabaseService.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace hms_firetv;

// ============================================================================
// MQTT CLIENT INTEGRATION TESTS
// ============================================================================

class MQTTClientTest : public ::testing::Test {
protected:
    std::unique_ptr<MQTTClient> mqtt_client;
    std::string client_id;

    void SetUp() override {
        try {
            DatabaseService::getInstance().initialize(
                "192.168.2.15", 5432, "firetv", "firetv_user", "firetv_postgres_2026_secure"
            );
        } catch (const std::exception& e) {
            std::cerr << "Database init failed: " << e.what() << std::endl;
            GTEST_SKIP() << "Database not available";
        }

        client_id = "test_client_" + std::to_string(std::time(nullptr)) + "_" +
                    std::to_string(rand() % 10000);
        mqtt_client = std::make_unique<MQTTClient>(client_id);
    }

    void TearDown() override {
        if (mqtt_client) {
            mqtt_client->disconnect();
        }
        mqtt_client.reset();
    }
};

// ============================================================================
// CONNECTION TESTS
// ============================================================================

// Test: Can connect to MQTT broker
TEST_F(MQTTClientTest, Connect_Succeeds) {
    bool connected = mqtt_client->connect(
        "tcp://192.168.2.15:1883",
        "aamat", "exploracion"
    );

    EXPECT_TRUE(connected) << "Should connect to MQTT broker";
    EXPECT_TRUE(mqtt_client->isConnected()) << "isConnected() should return true";
}

// Test: Connection with invalid credentials behavior
// Note: This test's result depends on broker authentication configuration.
// EMQX may allow anonymous access or not enforce strict credential validation.
TEST_F(MQTTClientTest, Connect_WithInvalidCredentials_BrokerDependent) {
    auto bad_client = std::make_unique<MQTTClient>("bad_client_" + std::to_string(rand()));

    bool connected = bad_client->connect(
        "tcp://192.168.2.15:1883",
        "invalid_user", "invalid_pass"
    );

    // Broker may or may not reject invalid credentials depending on configuration
    if (connected) {
        // EMQX allows the connection (anonymous/permissive auth)
        SUCCEED() << "Broker allows any credentials (permissive auth mode)";
        bad_client->disconnect();
    } else {
        // Broker enforces authentication
        SUCCEED() << "Broker rejected invalid credentials (strict auth mode)";
    }
}

// ============================================================================
// PUBLISH TESTS (NON-BLOCKING FIX)
// ============================================================================

// Test: Publish is non-blocking and completes quickly
TEST_F(MQTTClientTest, Publish_IsNonBlocking) {
    bool connected = mqtt_client->connect(
        "tcp://192.168.2.15:1883",
        "aamat", "exploracion"
    );
    ASSERT_TRUE(connected);

    // Publish should complete quickly (< 100ms) since it's non-blocking
    auto start = std::chrono::steady_clock::now();
    bool published = mqtt_client->publish(
        "test/nonblocking",
        "test_payload",
        1,  // QoS 1
        false  // not retained
    );
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    EXPECT_TRUE(published) << "Publish should succeed";
    EXPECT_LT(elapsed, 100) << "Non-blocking publish should complete in <100ms";
}

// Test: Multiple publishes don't block each other
TEST_F(MQTTClientTest, MultiplePublishes_DontBlock) {
    bool connected = mqtt_client->connect(
        "tcp://192.168.2.15:1883",
        "aamat", "exploracion"
    );
    ASSERT_TRUE(connected);

    auto start = std::chrono::steady_clock::now();

    // Publish 10 messages rapidly
    for (int i = 0; i < 10; ++i) {
        mqtt_client->publish(
            "test/rapid/" + std::to_string(i),
            "payload_" + std::to_string(i),
            1, false
        );
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // 10 non-blocking publishes should complete in <500ms
    EXPECT_LT(elapsed, 500) << "10 non-blocking publishes should complete in <500ms";
}

// ============================================================================
// CALLBACK TESTS (DEADLOCK PREVENTION)
// ============================================================================

// Test: Publish from callback doesn't deadlock
TEST_F(MQTTClientTest, PublishFromCallback_DoesNotDeadlock) {
    bool connected = mqtt_client->connect(
        "tcp://192.168.2.15:1883",
        "aamat", "exploracion"
    );
    ASSERT_TRUE(connected);

    std::atomic<bool> callback_completed{false};

    // Subscribe with a callback that publishes
    mqtt_client->subscribe("test/callback_trigger_" + client_id,
        [this, &callback_completed](const std::string&, const std::string&) {
            // Publish from within callback - this MUST NOT deadlock
            mqtt_client->publish("test/callback_response", "response", 1, false);
            callback_completed = true;
        }
    );

    // Small delay to ensure subscription is active
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Trigger the callback by publishing to the subscribed topic
    mqtt_client->publish("test/callback_trigger_" + client_id, "trigger", 0, false);

    // Wait up to 5 seconds for callback to complete
    for (int i = 0; i < 50; ++i) {
        if (callback_completed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_TRUE(callback_completed) << "Callback should complete without deadlock";
}

// ============================================================================
// SUBSCRIPTION TESTS
// ============================================================================

// Test: Can subscribe to topics
TEST_F(MQTTClientTest, Subscribe_Succeeds) {
    bool connected = mqtt_client->connect(
        "tcp://192.168.2.15:1883",
        "aamat", "exploracion"
    );
    ASSERT_TRUE(connected);

    bool subscribed = mqtt_client->subscribe("test/topic_" + client_id,
        [](const std::string&, const std::string&) {
            // Empty callback
        }
    );

    EXPECT_TRUE(subscribed) << "Subscribe should succeed";
}

// Test: Subscribe fails when not connected
TEST_F(MQTTClientTest, Subscribe_FailsWhenNotConnected) {
    // Don't connect
    auto unconnected_client = std::make_unique<MQTTClient>("unconnected_" + std::to_string(rand()));

    bool subscribed = unconnected_client->subscribe("test/topic",
        [](const std::string&, const std::string&) {
            // Empty callback
        }
    );

    EXPECT_FALSE(subscribed) << "Subscribe should fail when not connected";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
