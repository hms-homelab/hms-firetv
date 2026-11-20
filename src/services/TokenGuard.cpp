#include "services/TokenGuard.h"
#include "repositories/DeviceRepository.h"

#include <iostream>
#include <mutex>

namespace hms_firetv {

namespace {

// One revalidation at a time. A burst of presses against a dead token would
// otherwise send one status probe per press; serialised, the first clears the
// pairing and the rest find the token gone and stop.
std::mutex& guardMutex() {
    static std::mutex m;
    return m;
}

bool hasToken(const Device& device) {
    return device.client_token.has_value() && !device.client_token->empty();
}

TokenCheck verifyLocked(const std::string& device_id, const TokenGuard::Checker& checker,
                        bool* cleared = nullptr) {
    if (cleared) *cleared = false;
    auto& repo = DeviceRepository::getInstance();
    auto device = repo.getDeviceById(device_id);
    if (!device.has_value() || !hasToken(*device)) return TokenCheck::Inconclusive;

    TokenCheck check = checker(*device);
    if (check == TokenCheck::Rejected) {
        // Re-read: the device may have been re-paired while the probe was in
        // flight, and that newer token is not the one the device rejected.
        auto now = repo.getDeviceById(device_id);
        if (now.has_value() && now->client_token == device->client_token) {
            bool ok = repo.clearPairing(device_id);
            if (cleared) *cleared = ok;
            std::cerr << "[TokenGuard] " << device_id
                      << " no longer accepts its client token; pairing cleared, pair again"
                      << std::endl;
        }
    }
    return check;
}

}  // namespace

TokenCheck TokenGuard::checkWithDevice(const Device& device) {
    LightningClient probe(device.ip_address, device.api_key, device.client_token.value_or(""));
    return probe.checkToken();
}

TokenCheck TokenGuard::verify(const std::string& device_id, const Checker& checker) {
    std::lock_guard<std::mutex> lock(guardMutex());
    return verifyLocked(device_id, checker);
}

bool TokenGuard::handleRejection(const std::string& device_id,
                                 const std::string& rejected_token,
                                 const Checker& checker) {
    if (rejected_token.empty()) return false;

    std::lock_guard<std::mutex> lock(guardMutex());

    auto device = DeviceRepository::getInstance().getDeviceById(device_id);
    if (!device.has_value() || !hasToken(*device)) return false;
    if (*device->client_token != rejected_token) {
        // Stale: the request was sent with a token that has since been
        // replaced. The current one has not been rejected by anything.
        return false;
    }

    bool cleared = false;
    verifyLocked(device_id, checker, &cleared);
    return cleared;
}

void TokenGuard::attach(LightningClient& client, const std::string& device_id) {
    client.setAuthRejectedHandler([device_id](const std::string& token) {
        TokenGuard::handleRejection(device_id, token);
    });
}

}  // namespace hms_firetv
