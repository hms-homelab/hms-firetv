#pragma once

#include "clients/LightningClient.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace hms_firetv {

/**
 * TextInput - send text through whichever route this device accepts
 *
 * Fire TV firmware disagrees on the text route. On current Fire OS 8 builds
 * POST /v1/FireTV/keyboard {"text"} types the whole string and the older
 * POST /v1/FireTV/text answers 200 without typing anything; other firmware
 * rejects /keyboard and needs /text. So: try the route this device used last
 * (keyboard when nothing is known), fall back to the other when the device
 * answers with an error status, and remember per device which one worked.
 *
 * The fallback runs only on an HTTP error answer. Not on 401/403 (the token
 * is dead, the other route would be refused too) and not on a socket failure
 * (an asleep or absent device will not answer the other route either).
 */
class TextInput {
public:
    /** Sends the text to one route ("keyboard" | "text"). */
    using Sender = std::function<CommandResult(const std::string& endpoint)>;

    struct Outcome {
        CommandResult result;
        std::string endpoint;  // the route of the answer in result
    };

    /** Routes to try, the learned one first. Anything unknown means keyboard first. */
    static std::vector<std::string> order(const std::optional<std::string>& learned);

    /** Whether a failed answer justifies trying the other route. */
    static bool worthTryingOther(const CommandResult& result);

    /** Send through sender, learning the route for device_id. */
    static Outcome send(const std::string& device_id, const Sender& sender);

    /** Send text through a real client. */
    static Outcome send(const std::string& device_id, LightningClient& client,
                        const std::string& text);
};

} // namespace hms_firetv
