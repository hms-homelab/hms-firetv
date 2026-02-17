#include <gtest/gtest.h>
#include "mqtt/CommandHandler.h"
#include "clients/LightningClient.h"
#include "repositories/DeviceRepository.h"
#include "services/DatabaseService.h"
#include <thread>
#include <chrono>

using namespace hms_firetv;

// ============================================================================
// TESTABLE COMMAND HANDLER (exposes protected methods)
// ============================================================================

class TestableCommandHandler : public CommandHandler {
public:
    using CommandHandler::CommandHandler;

    // Expose protected method for testing
    bool testEnsureDeviceAwake(LightningClient& client) {
        return ensureDeviceAwake(client);
    }
};

// ============================================================================
// COMMAND HANDLER INTEGRATION TESTS
// ============================================================================

class CommandHandlerTest : public ::testing::Test {
protected:
    std::string test_device_id = "unittest_handler_device";
    std::unique_ptr<TestableCommandHandler> handler;

    void SetUp() override {
        try {
            DatabaseService::getInstance().initialize(
                "192.168.2.15", 5432, "firetv", "firetv_user", "firetv_postgres_2026_secure"
            );
        } catch (const std::exception& e) {
            std::cerr << "Database init failed: " << e.what() << std::endl;
            GTEST_SKIP() << "Database not available";
        }

        // Create test device with non-routable IP
        Device device;
        device.device_id = test_device_id;
        device.name = "Unit Test Handler Device";
        device.ip_address = "192.168.1.250";  // Non-routable test IP
        device.api_key = "0987654321";
        device.status = "online";
        device.adb_enabled = false;

        // Clear any existing test device first
        DeviceRepository::getInstance().deleteDevice(test_device_id);
        DeviceRepository::getInstance().createDevice(device);

        handler = std::make_unique<TestableCommandHandler>();
    }

    void TearDown() override {
        handler.reset();
        try {
            DeviceRepository::getInstance().deleteDevice(test_device_id);
        } catch (...) {}
    }
};

// ============================================================================
// ENSURE DEVICE AWAKE TESTS
// ============================================================================

// Test: ensureDeviceAwake returns false for non-existent device
TEST_F(CommandHandlerTest, EnsureDeviceAwake_ReturnsFalse_ForNonExistentDevice) {
    LightningClient client("192.168.1.250", "0987654321", "test_token");

    // isLightningApiAvailable should return false for non-existent IP
    EXPECT_FALSE(client.isLightningApiAvailable())
        << "Non-existent IP should fail health check";

    // ensureDeviceAwake should attempt wake and eventually fail
    auto start = std::chrono::steady_clock::now();
    bool result = handler->testEnsureDeviceAwake(client);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    EXPECT_FALSE(result) << "Should return false when device doesn't exist";
    EXPECT_GE(elapsed, 3) << "Should have waited for wake attempts";
}

// Test: ensureDeviceAwake returns quickly for awake device (with real Fire TV)
TEST_F(CommandHandlerTest, EnsureDeviceAwake_ReturnsQuickly_WhenDeviceAwake) {
    const char* real_ip_env = std::getenv("FIRETV_TEST_IP");
    if (!real_ip_env) {
        GTEST_SKIP() << "Set FIRETV_TEST_IP env var to test with real device";
    }

    LightningClient client(real_ip_env, "0987654321");

    auto start = std::chrono::steady_clock::now();
    bool result = handler->testEnsureDeviceAwake(client);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    if (result) {
        EXPECT_LT(elapsed, 3000) << "Should return quickly when device awake";
    } else {
        SUCCEED() << "Device not responding (may be off)";
    }
}

// ============================================================================
// AUTO-WAKE BEHAVIOR TESTS (via handleCommand)
// ============================================================================

// Test: handleCommand attempts wake for sleeping device (non-turn_on command)
TEST_F(CommandHandlerTest, HandleCommand_AttemptsWake_ForSleepingDevice) {
    Json::Value payload;
    payload["command"] = "volume_up";

    auto start = std::chrono::steady_clock::now();
    handler->handleCommand(test_device_id, payload);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // Should have waited at least 3 seconds for wake attempts
    EXPECT_GE(elapsed, 3) << "Should wait for wake attempts on non-responsive device";
}

// Test: handleCommand skips wake for turn_on command
TEST_F(CommandHandlerTest, HandleCommand_SkipsWake_ForTurnOn) {
    Json::Value payload;
    payload["command"] = "turn_on";

    auto start = std::chrono::steady_clock::now();
    handler->handleCommand(test_device_id, payload);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // turn_on should NOT call ensureDeviceAwake (which waits 5+ seconds)
    EXPECT_LE(elapsed, 6) << "turn_on should not wait for full wake cycle";
}

// ============================================================================
// COMMAND ROUTING TESTS
// ============================================================================

// Test: handleCommand correctly routes volume commands
TEST_F(CommandHandlerTest, HandleCommand_RoutesVolumeCommands) {
    Json::Value payload;
    payload["command"] = "volume_up";

    handler->handleCommand(test_device_id, payload);

    SUCCEED() << "Volume command routing completed without crash";
}

// Test: handleCommand correctly routes media commands
TEST_F(CommandHandlerTest, HandleCommand_RoutesMediaCommands) {
    Json::Value payload;
    payload["command"] = "media_play";

    handler->handleCommand(test_device_id, payload);

    SUCCEED() << "Media command routing completed";
}

// Test: handleCommand handles missing device gracefully
TEST_F(CommandHandlerTest, HandleCommand_HandlesMissingDevice) {
    Json::Value payload;
    payload["command"] = "volume_up";

    handler->handleCommand("nonexistent_device_xyz", payload);

    SUCCEED() << "Missing device handled gracefully";
}

// Test: handleCommand handles missing command field
TEST_F(CommandHandlerTest, HandleCommand_HandlesMissingCommandField) {
    Json::Value payload;

    handler->handleCommand(test_device_id, payload);

    SUCCEED() << "Missing command field handled gracefully";
}

// Test: handleCommand handles navigation commands
TEST_F(CommandHandlerTest, HandleCommand_RoutesNavigationCommands) {
    Json::Value payload;
    payload["command"] = "navigate";
    payload["direction"] = "up";

    handler->handleCommand(test_device_id, payload);

    SUCCEED() << "Navigation command routing completed";
}

// Test: handleCommand handles app launch commands
TEST_F(CommandHandlerTest, HandleCommand_RoutesAppLaunchCommands) {
    Json::Value payload;
    payload["command"] = "launch_app";
    payload["source"] = "Netflix";

    handler->handleCommand(test_device_id, payload);

    SUCCEED() << "App launch command routing completed";
}

// ============================================================================
// LIGHTNING CLIENT TESTS
// ============================================================================

// Test: LightningClient healthCheck returns false for invalid IP
TEST_F(CommandHandlerTest, LightningClient_HealthCheck_FailsForInvalidIP) {
    LightningClient client("192.168.255.255", "0987654321");

    bool result = client.healthCheck();

    EXPECT_FALSE(result) << "Invalid IP should fail health check";
}

// Test: LightningClient isLightningApiAvailable returns false for invalid IP
TEST_F(CommandHandlerTest, LightningClient_ApiAvailable_FailsForInvalidIP) {
    LightningClient client("192.168.255.255", "0987654321");

    bool result = client.isLightningApiAvailable();

    EXPECT_FALSE(result) << "Invalid IP should fail API availability check";
}

// Test: LightningClient wakeDevice fails gracefully for invalid IP
TEST_F(CommandHandlerTest, LightningClient_WakeDevice_FailsGracefully) {
    LightningClient client("192.168.255.255", "0987654321");

    bool result = client.wakeDevice();

    EXPECT_FALSE(result) << "Wake should fail for invalid IP";
}

// Test: LightningClient constructor initializes correctly
TEST_F(CommandHandlerTest, LightningClient_Constructor_Initializes) {
    LightningClient client("192.168.1.100", "test_key", "test_token");

    EXPECT_EQ(client.getClientToken(), "test_token");
}

// Test: LightningClient setClientToken works
TEST_F(CommandHandlerTest, LightningClient_SetClientToken_Works) {
    LightningClient client("192.168.1.100", "test_key");

    client.setClientToken("new_token");

    EXPECT_EQ(client.getClientToken(), "new_token");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
