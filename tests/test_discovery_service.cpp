#include <gtest/gtest.h>
#include "services/DiscoveryService.h"
#include "repositories/DeviceRepository.h"
#include "database/SQLiteDatabase.h"
#include <sqlite3.h>
#include <cstdio>
#include <memory>

using namespace hms_firetv;

namespace {

const char* kCubeUdn  = "uuid:11111111-2222-3333-4444-555555555555";
const char* kOtherUdn = "uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";

DiscoveredDevice seen(const std::string& ip, const std::string& udn) {
    return DiscoveredDevice{ip, "", true, true, udn};
}

}  // namespace

class DiscoveryServiceTest : public ::testing::Test {
protected:
    std::shared_ptr<SQLiteDatabase> db;
    std::unique_ptr<DiscoveryService> discovery;

    void SetUp() override {
        db = std::make_shared<SQLiteDatabase>(":memory:");
        db->connect();
        DeviceRepository::setDatabase(db);
        discovery = std::make_unique<DiscoveryService>("10.0.0", 300);
    }

    void addDevice(const std::string& id, const std::string& ip,
                   const std::string& udn = "") {
        Device d;
        d.device_id  = id;
        d.name       = id;
        d.ip_address = ip;
        d.status     = "offline";
        ASSERT_TRUE(DeviceRepository::getInstance().createDevice(d).has_value());
        if (!udn.empty()) {
            ASSERT_TRUE(DeviceRepository::getInstance().setDialUdn(id, udn));
        }
    }

    Device get(const std::string& id) {
        return DeviceRepository::getInstance().getDeviceById(id).value();
    }
};

TEST(DialUdnParse, ReadsUdnFromDeviceDescription) {
    std::string xml =
        "<?xml version=\"1.0\"?><root><device>"
        "<friendlyName>Living Room Fire TV</friendlyName>"
        "<UDN>uuid:11111111-2222-3333-4444-555555555555</UDN>"
        "</device></root>";
    EXPECT_EQ(DiscoveryService::parseDialUdn(xml), kCubeUdn);
}

TEST(DialUdnParse, MissingOrUnterminatedUdnIsEmpty) {
    EXPECT_EQ(DiscoveryService::parseDialUdn(""), "");
    EXPECT_EQ(DiscoveryService::parseDialUdn("<root><device/></root>"), "");
    EXPECT_EQ(DiscoveryService::parseDialUdn("<UDN>uuid:cut-off"), "");
}

TEST_F(DiscoveryServiceTest, LearnsUdnWhileAtRegisteredIp) {
    addDevice("lr", "10.0.0.47");

    discovery->matchAndUpdate({seen("10.0.0.47", kCubeUdn)});

    auto d = get("lr");
    EXPECT_EQ(d.dial_udn.value_or(""), kCubeUdn);
    EXPECT_EQ(d.status, "online");
}

TEST_F(DiscoveryServiceTest, FindsMovedDeviceByUdn) {
    addDevice("lr", "10.0.0.47", kCubeUdn);

    discovery->matchAndUpdate({seen("10.0.0.52", kCubeUdn)});

    auto d = get("lr");
    EXPECT_EQ(d.ip_address, "10.0.0.52");
    EXPECT_EQ(d.status, "online");
}

// The incident: a device with no known identity went missing, and discovery
// woke the only other Fire TV on the subnet every scan to test it, which
// turned that Fire TV's television on. It must not touch or adopt it.
TEST_F(DiscoveryServiceTest, DeviceWithoutUdnDoesNotAdoptAnotherFireTv) {
    addDevice("albin_colada", "10.0.0.64");

    discovery->matchAndUpdate({seen("10.0.0.47", kCubeUdn)});

    auto d = get("albin_colada");
    EXPECT_EQ(d.ip_address, "10.0.0.64");
    EXPECT_FALSE(d.dial_udn.has_value());
}

TEST_F(DiscoveryServiceTest, DifferentDeviceAtRegisteredIpMeansMoved) {
    addDevice("lr", "10.0.0.47", kCubeUdn);

    discovery->matchAndUpdate({seen("10.0.0.47", kOtherUdn),
                               seen("10.0.0.60", kCubeUdn)});

    EXPECT_EQ(get("lr").ip_address, "10.0.0.60");
}

TEST_F(DiscoveryServiceTest, KnownUdnIsNotOverwrittenByAnotherDevice) {
    addDevice("lr", "10.0.0.47", kCubeUdn);

    discovery->matchAndUpdate({seen("10.0.0.47", kOtherUdn)});

    auto d = get("lr");
    EXPECT_EQ(d.dial_udn.value_or(""), kCubeUdn);
    EXPECT_EQ(d.ip_address, "10.0.0.47");
}

// A database file written before dial_udn existed gains the column on connect.
TEST(SQLiteMigration, AddsDialUdnToOlderDatabase) {
    const std::string path = "/tmp/hms_firetv_test_migration.db";
    std::remove(path.c_str());

    sqlite3* raw = nullptr;
    ASSERT_EQ(sqlite3_open(path.c_str(), &raw), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(raw,
        "CREATE TABLE fire_tv_devices ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, device_id TEXT UNIQUE NOT NULL,"
        " name TEXT NOT NULL, ip_address TEXT NOT NULL,"
        " api_key TEXT NOT NULL DEFAULT '0987654321', client_token TEXT,"
        " pin_code TEXT, pin_expires_at TEXT,"
        " status TEXT NOT NULL DEFAULT 'offline', last_seen_at TEXT,"
        " created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        " updated_at TEXT DEFAULT CURRENT_TIMESTAMP);"
        "INSERT INTO fire_tv_devices (device_id,name,ip_address)"
        " VALUES ('old','Old','10.0.0.9');",
        nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(raw);

    auto db = std::make_shared<SQLiteDatabase>(path);
    ASSERT_TRUE(db->connect());

    auto before = db->getDeviceById("old");
    ASSERT_TRUE(before.has_value());
    EXPECT_FALSE(before->dial_udn.has_value());

    ASSERT_TRUE(db->setDialUdn("old", kCubeUdn));
    EXPECT_EQ(db->getDeviceById("old")->dial_udn.value_or(""), kCubeUdn);

    db->disconnect();
    std::remove(path.c_str());
}
