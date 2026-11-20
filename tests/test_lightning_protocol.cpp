// Lightning protocol interpretation and token revalidation.
//
// Everything here runs without a Fire TV: the response classifiers are pure,
// TokenGuard and PairingController take an injected token checker, and the
// database is in-memory SQLite. Device addresses are from TEST-NET-1
// (192.0.2.0/24), which is never routed, so nothing can reach a real device.

#include <gtest/gtest.h>
#include "api/PairingController.h"
#include "clients/LightningClient.h"
#include "database/SQLiteDatabase.h"
#include "repositories/DeviceRepository.h"
#include "services/TextInput.h"
#include "services/TokenGuard.h"

#include <cstdio>
#include <drogon/drogon.h>
#include <sqlite3.h>
#include <json/json.h>
#include <memory>
#include <sstream>

using namespace hms_firetv;
using namespace drogon;

namespace {

CommandResult answer(int status, const std::string& json_body = "") {
    CommandResult r;
    r.status_code = status;
    r.success = status >= 200 && status < 300;
    if (!json_body.empty()) {
        Json::CharReaderBuilder b;
        std::string errs;
        std::istringstream in(json_body);
        Json::parseFromStream(b, in, &r.response_body, &errs);
    }
    return r;
}

CommandResult noAnswer() {
    CommandResult r;  // status 0, success false: socket-level failure
    r.error = "Couldn't connect to server";
    return r;
}

Json::Value parse(const std::string& s) {
    Json::Value v;
    Json::CharReaderBuilder b;
    std::string errs;
    std::istringstream in(s);
    Json::parseFromStream(b, in, &v, &errs);
    return v;
}

}  // namespace

// ============================================================================
// PURE CLASSIFIERS
// ============================================================================

TEST(LightningProtocol, AuthRejectionIsOnly401And403) {
    EXPECT_TRUE(LightningClient::isAuthRejection(401));
    EXPECT_TRUE(LightningClient::isAuthRejection(403));
    EXPECT_FALSE(LightningClient::isAuthRejection(0));
    EXPECT_FALSE(LightningClient::isAuthRejection(200));
    EXPECT_FALSE(LightningClient::isAuthRejection(400));
    EXPECT_FALSE(LightningClient::isAuthRejection(404));
    EXPECT_FALSE(LightningClient::isAuthRejection(500));
}

TEST(LightningProtocol, PinVerifyTokenIsPaired) {
    auto r = LightningClient::interpretPinVerify(answer(200, R"({"description":"AbC123xyz"})"));
    EXPECT_EQ(r.status, PinVerifyStatus::Paired);
    EXPECT_EQ(r.token, "AbC123xyz");
}

TEST(LightningProtocol, PinVerifyEmptyDescriptionIsWrongPin) {
    auto r = LightningClient::interpretPinVerify(answer(200, R"({"description":""})"));
    EXPECT_EQ(r.status, PinVerifyStatus::WrongPin);
    EXPECT_TRUE(r.token.empty());
}

TEST(LightningProtocol, PinVerifyOkIsPending) {
    auto r = LightningClient::interpretPinVerify(answer(200, R"({"description":"OK"})"));
    EXPECT_EQ(r.status, PinVerifyStatus::Pending);
    EXPECT_TRUE(r.token.empty());
}

TEST(LightningProtocol, PinVerifyFailuresAreFailed) {
    EXPECT_EQ(LightningClient::interpretPinVerify(answer(500, R"({"description":"x"})")).status,
              PinVerifyStatus::Failed);
    EXPECT_EQ(LightningClient::interpretPinVerify(answer(200, R"({"other":"x"})")).status,
              PinVerifyStatus::Failed);
    EXPECT_EQ(LightningClient::interpretPinVerify(answer(200, R"({"description":5})")).status,
              PinVerifyStatus::Failed);
    EXPECT_EQ(LightningClient::interpretPinVerify(answer(200)).status, PinVerifyStatus::Failed);
    EXPECT_EQ(LightningClient::interpretPinVerify(noAnswer()).status, PinVerifyStatus::Failed);
}

TEST(LightningProtocol, TokenCheckClassification) {
    EXPECT_EQ(LightningClient::interpretTokenCheck(
                  answer(200, R"({"osVersion":"1","platformType":"android","turnstileVersion":"0.1"})")),
              TokenCheck::Valid);
    EXPECT_EQ(LightningClient::interpretTokenCheck(answer(403)), TokenCheck::Rejected);
    EXPECT_EQ(LightningClient::interpretTokenCheck(answer(401)), TokenCheck::Rejected);
    // Nothing that is not an explicit auth answer may count against a token.
    EXPECT_EQ(LightningClient::interpretTokenCheck(noAnswer()), TokenCheck::Inconclusive);
    EXPECT_EQ(LightningClient::interpretTokenCheck(answer(404)), TokenCheck::Inconclusive);
    EXPECT_EQ(LightningClient::interpretTokenCheck(answer(500)), TokenCheck::Inconclusive);
}

TEST(LightningProtocol, CheckTokenWithoutTokenIsInconclusiveAndOffline) {
    // No token: answered locally, nothing is sent.
    LightningClient client("192.0.2.10");
    EXPECT_EQ(client.checkToken(), TokenCheck::Inconclusive);
}

TEST(LightningProtocol, ScanBodyCarriesDirectionDurationAndSpeedAsStrings) {
    auto fwd = parse(LightningClient::buildScanBody("forward"));
    EXPECT_EQ(fwd["direction"].asString(), "forward");
    ASSERT_TRUE(fwd["durationInSeconds"].isString());
    EXPECT_EQ(fwd["durationInSeconds"].asString(), "10");
    ASSERT_TRUE(fwd["speed"].isString());
    EXPECT_EQ(fwd["speed"].asString(), "1");

    EXPECT_EQ(parse(LightningClient::buildScanBody("backward"))["direction"].asString(), "backward");
    // The old internal spelling still maps to what the device understands.
    EXPECT_EQ(parse(LightningClient::buildScanBody("back"))["direction"].asString(), "backward");
}

TEST(LightningProtocol, PauseAndStopUseTheDocumentedPlayToggle) {
    EXPECT_EQ(LightningClient::mediaWireAction("pause"), "play");
    EXPECT_EQ(LightningClient::mediaWireAction("stop"), "play");
    EXPECT_EQ(LightningClient::mediaWireAction("play_pause"), "play");
    EXPECT_EQ(LightningClient::mediaWireAction("play"), "play");
    EXPECT_EQ(LightningClient::mediaWireAction("scan"), "scan");
}

// ============================================================================
// TOKEN GUARD
// ============================================================================

class TokenGuardTest : public ::testing::Test {
protected:
    std::shared_ptr<SQLiteDatabase> db;
    const std::string id = "guard_device";
    int checks = 0;

    void SetUp() override {
        db = std::make_shared<SQLiteDatabase>(":memory:");
        db->connect();
        DeviceRepository::setDatabase(db);

        Device d;
        d.device_id = id;
        d.name = "Guard Test";
        d.ip_address = "192.0.2.10";
        d.status = "online";
        ASSERT_TRUE(DeviceRepository::getInstance().createDevice(d).has_value());
        ASSERT_TRUE(db->completePairing(id, "token-A"));
    }

    TokenGuard::Checker answering(TokenCheck verdict) {
        return [this, verdict](const Device&) { ++checks; return verdict; };
    }

    std::string tokenOnFile() {
        auto d = DeviceRepository::getInstance().getDeviceById(id);
        return d.has_value() ? d->client_token.value_or("") : "<missing>";
    }
};

TEST_F(TokenGuardTest, ConfirmedRejectionClearsThePairing) {
    EXPECT_TRUE(TokenGuard::handleRejection(id, "token-A", answering(TokenCheck::Rejected)));
    EXPECT_EQ(checks, 1);
    EXPECT_EQ(tokenOnFile(), "");
    auto d = DeviceRepository::getInstance().getDeviceById(id);
    ASSERT_TRUE(d.has_value());
    EXPECT_FALSE(d->isPaired());
}

TEST_F(TokenGuardTest, InconclusiveCheckKeepsTheToken) {
    // A sleeping device after a 403 must not cost a good token.
    EXPECT_FALSE(TokenGuard::handleRejection(id, "token-A", answering(TokenCheck::Inconclusive)));
    EXPECT_EQ(tokenOnFile(), "token-A");
}

TEST_F(TokenGuardTest, ValidCheckKeepsTheToken) {
    EXPECT_FALSE(TokenGuard::handleRejection(id, "token-A", answering(TokenCheck::Valid)));
    EXPECT_EQ(tokenOnFile(), "token-A");
}

TEST_F(TokenGuardTest, StaleRejectionNeverTouchesANewerPairing) {
    // A late 403 for the old token arrives after the device was re-paired.
    EXPECT_FALSE(TokenGuard::handleRejection(id, "token-OLD", answering(TokenCheck::Rejected)));
    EXPECT_EQ(checks, 0);
    EXPECT_EQ(tokenOnFile(), "token-A");
}

TEST_F(TokenGuardTest, RePairDuringTheProbeIsKept) {
    auto repairs_mid_check = [this](const Device&) {
        ++checks;
        db->completePairing(id, "token-B");
        return TokenCheck::Rejected;  // about token-A, which is gone now
    };
    EXPECT_FALSE(TokenGuard::handleRejection(id, "token-A", repairs_mid_check));
    EXPECT_EQ(tokenOnFile(), "token-B");
}

TEST_F(TokenGuardTest, EmptyOrMissingTokensAreIgnored) {
    EXPECT_FALSE(TokenGuard::handleRejection(id, "", answering(TokenCheck::Rejected)));
    EXPECT_FALSE(TokenGuard::handleRejection("no_such_device", "token-A",
                                             answering(TokenCheck::Rejected)));
    ASSERT_TRUE(db->clearPairing(id));
    EXPECT_EQ(TokenGuard::verify(id, answering(TokenCheck::Rejected)), TokenCheck::Inconclusive);
    EXPECT_EQ(checks, 0);
}

TEST_F(TokenGuardTest, VerifyReportsAndActsOnTheVerdict) {
    EXPECT_EQ(TokenGuard::verify(id, answering(TokenCheck::Valid)), TokenCheck::Valid);
    EXPECT_EQ(tokenOnFile(), "token-A");
    EXPECT_EQ(TokenGuard::verify(id, answering(TokenCheck::Rejected)), TokenCheck::Rejected);
    EXPECT_EQ(tokenOnFile(), "");
}

// ============================================================================
// PAIRING CONTROLLER: is_paired is checked, not assumed
// ============================================================================

class PairingRevalidationTest : public TokenGuardTest {
protected:
    void TearDown() override { PairingController::setTokenChecker(nullptr); }

    Json::Value call(PairingController& c, bool verify) {
        Json::Value out;
        auto req = HttpRequest::newHttpRequest();
        if (verify) req->setParameter("verify", "true");
        c.getPairingStatus(req, [&](const HttpResponsePtr& resp) {
            out = parse(std::string(resp->getBody()));
        }, id);
        return out;
    }

    std::pair<int, Json::Value> start(PairingController& c) {
        std::pair<int, Json::Value> out;
        auto req = HttpRequest::newHttpRequest();
        c.startPairing(req, [&](const HttpResponsePtr& resp) {
            out = {static_cast<int>(resp->getStatusCode()), parse(std::string(resp->getBody()))};
        }, id);
        return out;
    }
};

TEST_F(PairingRevalidationTest, StatusWithoutVerifyDoesNotProbe) {
    PairingController::setTokenChecker(answering(TokenCheck::Rejected));
    PairingController c;
    auto r = call(c, false);
    EXPECT_TRUE(r["is_paired"].asBool());
    EXPECT_FALSE(r.isMember("token_check"));
    EXPECT_EQ(checks, 0);
}

TEST_F(PairingRevalidationTest, StatusVerifyRejectedReportsUnpaired) {
    PairingController::setTokenChecker(answering(TokenCheck::Rejected));
    PairingController c;
    auto r = call(c, true);
    EXPECT_EQ(r["token_check"].asString(), "rejected");
    EXPECT_FALSE(r["is_paired"].asBool());
    EXPECT_EQ(tokenOnFile(), "");
}

TEST_F(PairingRevalidationTest, StatusVerifyInconclusiveKeepsPaired) {
    PairingController::setTokenChecker(answering(TokenCheck::Inconclusive));
    PairingController c;
    auto r = call(c, true);
    EXPECT_EQ(r["token_check"].asString(), "inconclusive");
    EXPECT_TRUE(r["is_paired"].asBool());
}

TEST_F(PairingRevalidationTest, StatusVerifyValid) {
    PairingController::setTokenChecker(answering(TokenCheck::Valid));
    PairingController c;
    auto r = call(c, true);
    EXPECT_EQ(r["token_check"].asString(), "valid");
    EXPECT_TRUE(r["is_paired"].asBool());
}

TEST_F(PairingRevalidationTest, StartRefusesWhileTheTokenIsGoodOrUnchecked) {
    PairingController c;

    PairingController::setTokenChecker(answering(TokenCheck::Valid));
    auto valid = start(c);
    EXPECT_EQ(valid.first, 409);

    PairingController::setTokenChecker(answering(TokenCheck::Inconclusive));
    auto asleep = start(c);
    EXPECT_EQ(asleep.first, 409);

    EXPECT_EQ(checks, 2);
    EXPECT_EQ(tokenOnFile(), "token-A");
}

// ============================================================================
// TEXT INPUT: keyboard first, fall back to text, remember per device
// ============================================================================

class TextInputTest : public TokenGuardTest {
protected:
    std::vector<std::string> tried;

    // A device that answers each route with the given status.
    TextInput::Sender device(int keyboard_status, int text_status) {
        return [this, keyboard_status, text_status](const std::string& endpoint) {
            tried.push_back(endpoint);
            if (endpoint == "keyboard") return keyboard_status ? answer(keyboard_status) : noAnswer();
            return text_status ? answer(text_status) : noAnswer();
        };
    }

    std::optional<std::string> learned() {
        auto d = DeviceRepository::getInstance().getDeviceById(id);
        return d.has_value() ? d->text_endpoint : std::nullopt;
    }
};

TEST_F(TextInputTest, OrderPutsTheLearnedRouteFirst) {
    EXPECT_EQ(TextInput::order(std::nullopt), (std::vector<std::string>{"keyboard", "text"}));
    EXPECT_EQ(TextInput::order(std::string("keyboard")), (std::vector<std::string>{"keyboard", "text"}));
    EXPECT_EQ(TextInput::order(std::string("text")), (std::vector<std::string>{"text", "keyboard"}));
    EXPECT_EQ(TextInput::order(std::string("garbage")), (std::vector<std::string>{"keyboard", "text"}));
}

TEST_F(TextInputTest, UnknownStartsWithKeyboardAndRemembersIt) {
    EXPECT_FALSE(learned().has_value());
    auto out = TextInput::send(id, device(200, 200));
    EXPECT_TRUE(out.result.success);
    EXPECT_EQ(out.endpoint, "keyboard");
    EXPECT_EQ(tried, (std::vector<std::string>{"keyboard"}));
    EXPECT_EQ(learned(), std::optional<std::string>("keyboard"));
}

TEST_F(TextInputTest, KeyboardErrorFallsBackToTextAndRemembersText) {
    auto out = TextInput::send(id, device(404, 200));
    EXPECT_TRUE(out.result.success);
    EXPECT_EQ(out.endpoint, "text");
    EXPECT_EQ(tried, (std::vector<std::string>{"keyboard", "text"}));
    EXPECT_EQ(learned(), std::optional<std::string>("text"));

    // Next time text goes first and keyboard is not tried at all.
    tried.clear();
    auto again = TextInput::send(id, device(404, 200));
    EXPECT_TRUE(again.result.success);
    EXPECT_EQ(tried, (std::vector<std::string>{"text"}));
}

TEST_F(TextInputTest, LearnedRouteThatStopsWorkingIsReplaced) {
    ASSERT_TRUE(db->setTextEndpoint(id, "text"));
    auto out = TextInput::send(id, device(200, 400));
    EXPECT_TRUE(out.result.success);
    EXPECT_EQ(tried, (std::vector<std::string>{"text", "keyboard"}));
    EXPECT_EQ(learned(), std::optional<std::string>("keyboard"));
}

TEST_F(TextInputTest, BothFailingLeavesWhatWasLearned) {
    ASSERT_TRUE(db->setTextEndpoint(id, "keyboard"));
    auto out = TextInput::send(id, device(500, 500));
    EXPECT_FALSE(out.result.success);
    EXPECT_EQ(out.result.status_code, 500);
    EXPECT_EQ(tried.size(), 2u);
    EXPECT_EQ(learned(), std::optional<std::string>("keyboard"));
}

TEST_F(TextInputTest, NoFallbackOnAuthRejectionOrNoAnswer) {
    auto rejected = TextInput::send(id, device(403, 200));
    EXPECT_FALSE(rejected.result.success);
    EXPECT_EQ(tried, (std::vector<std::string>{"keyboard"}));

    tried.clear();
    auto asleep = TextInput::send(id, device(0, 200));
    EXPECT_FALSE(asleep.result.success);
    EXPECT_EQ(tried, (std::vector<std::string>{"keyboard"}));

    EXPECT_FALSE(learned().has_value());
}

TEST(SQLiteMigration, OldFileGainsTextEndpointAndStillReads) {
    // A database file from before dial_udn and text_endpoint existed.
    std::string path = ::testing::TempDir() + "hms_firetv_migration_test.db";
    std::remove(path.c_str());
    {
        sqlite3* raw = nullptr;
        ASSERT_EQ(sqlite3_open(path.c_str(), &raw), SQLITE_OK);
        const char* old_schema =
            "CREATE TABLE fire_tv_devices ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT, device_id TEXT UNIQUE NOT NULL,"
            " name TEXT NOT NULL, ip_address TEXT NOT NULL,"
            " api_key TEXT NOT NULL DEFAULT '0987654321', client_token TEXT, pin_code TEXT,"
            " pin_expires_at TEXT, status TEXT NOT NULL DEFAULT 'offline', last_seen_at TEXT,"
            " created_at TEXT DEFAULT CURRENT_TIMESTAMP, updated_at TEXT DEFAULT CURRENT_TIMESTAMP);"
            "INSERT INTO fire_tv_devices (device_id,name,ip_address,client_token)"
            " VALUES ('old_dev','Old','192.0.2.30','tok');";
        ASSERT_EQ(sqlite3_exec(raw, old_schema, nullptr, nullptr, nullptr), SQLITE_OK);
        sqlite3_close(raw);
    }

    auto migrated = std::make_shared<SQLiteDatabase>(path);
    ASSERT_TRUE(migrated->connect());
    auto before = migrated->getDeviceById("old_dev");
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before->name, "Old");
    EXPECT_EQ(before->client_token.value_or(""), "tok");
    EXPECT_FALSE(before->text_endpoint.has_value());

    ASSERT_TRUE(migrated->setTextEndpoint("old_dev", "text"));
    auto after = migrated->getDeviceById("old_dev");
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->text_endpoint.value_or(""), "text");

    migrated.reset();
    std::remove(path.c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
