#pragma once

#include "clients/LightningClient.h"
#include "models/Device.h"

#include <functional>
#include <string>

namespace hms_firetv {

/**
 * TokenGuard - keep is_paired honest against the device
 *
 * is_paired used to mean only "a client token is on file". A Fire TV can drop
 * a token on its own (a factory reset, the user removing the remote in
 * Settings, an update that resets the remote service), and from then on every
 * command is answered 401/403 while hms-firetv still reported the device as
 * paired, and /pair/start refused with 409 until someone knew to call
 * /pair/reset first.
 *
 * The guard runs on events only, never on a timer:
 *   - an authenticated command is answered 401/403 (handleRejection), or
 *   - a caller asks (GET /pair/status?verify=true, POST /pair/start).
 *
 * A rejection is CONFIRMED with GET /v1/FireTV/status before anything is
 * cleared, and only the token that was actually rejected can be cleared: a
 * late 403 from a client still holding an old token must never wipe a newer
 * pairing. A sleeping or absent device is Inconclusive and keeps its token -
 * a timeout says nothing about the token.
 */
class TokenGuard {
public:
    /** Asks the device about the token on a device record. */
    using Checker = std::function<TokenCheck(const Device&)>;

    /** The production checker: GET /v1/FireTV/status, never waking. */
    static TokenCheck checkWithDevice(const Device& device);

    /**
     * Revalidate the token on file for device_id.
     *
     * Rejected clears the pairing (token, PIN, status offline) so the device
     * reads as unpaired and /pair/start works directly. No token on file is
     * Inconclusive.
     */
    static TokenCheck verify(const std::string& device_id,
                             const Checker& checker = checkWithDevice);

    /**
     * An authenticated request carrying rejected_token was answered 401/403.
     *
     * Ignored when the token on file is no longer rejected_token (the device
     * was re-paired since). Otherwise confirmed with verify().
     *
     * @return true if the pairing was cleared
     */
    static bool handleRejection(const std::string& device_id,
                                const std::string& rejected_token,
                                const Checker& checker = checkWithDevice);

    /**
     * Wire handleRejection() into a client for device_id, so a 401/403 on any
     * of its commands revalidates the pairing.
     */
    static void attach(LightningClient& client, const std::string& device_id);
};

} // namespace hms_firetv
