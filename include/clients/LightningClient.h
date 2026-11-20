#pragma once

#include <string>
#include <optional>
#include <json/json.h>
#include <curl/curl.h>
#include <functional>
#include <mutex>

namespace hms_firetv {

/**
 * CommandResult - Result of a Lightning command execution
 */
struct CommandResult {
    bool success;
    int status_code;
    int response_time_ms;
    std::optional<std::string> error;
    Json::Value response_body;

    CommandResult() : success(false), status_code(0), response_time_ms(0) {}
};

/**
 * What POST /v1/FireTV/pin/verify said.
 *
 * The device answers {"description":"<token>"} for a good PIN and
 * {"description":""} for a wrong one, and the pairing session stays open so
 * the PIN can be entered again. A wrong PIN is the user's typo, not a
 * transport failure, and must not be retried with the same digits.
 */
enum class PinVerifyStatus {
    Paired,     // token returned
    WrongPin,   // empty description: the device rejected the PIN
    Pending,    // "OK": accepted, token not issued yet
    Failed      // no usable answer (HTTP error, socket error, bad body)
};

struct PinVerifyResult {
    PinVerifyStatus status = PinVerifyStatus::Failed;
    std::string token;
    int http_status = 0;
};

/**
 * Whether the device still accepts the client token we hold.
 *
 * Only an HTTP 401/403 proves a token is dead. No answer at all (asleep,
 * gone, timeout) or any other status says nothing about the token, and a
 * caller must never discard a token on that basis.
 */
enum class TokenCheck {
    Valid,        // GET /v1/FireTV/status answered 2xx with our token
    Rejected,     // the device answered 401 or 403
    Inconclusive  // no answer, or an answer that is not about auth
};

/**
 * LightningClient - Fire TV Lightning Protocol Client
 *
 * Implements the Fire TV Lightning API protocol for device control.
 * Community-discovered protocol documented at:
 * https://community.home-assistant.io/t/fire-tv-stick-1-2-3-gen-full-rest-api-no-adb
 *
 * PROTOCOL OVERVIEW:
 * ==================
 * - Base URL: https://{ip}:8080 (HTTPS with self-signed cert)
 * - Wake URL: http://{ip}:8009/apps/FireTVRemote (HTTP)
 * - Authentication: X-Api-Key + X-Client-Token headers
 * - PIN-based pairing: Display PIN → User enters → Verify → Get token
 *
 * THREAD SAFETY:
 * ==============
 * This class is NOT thread-safe. Create one instance per device or use
 * external synchronization if sharing across threads.
 */
class LightningClient {
public:
    /**
     * Constructor
     *
     * @param ip_address Fire TV device IP address
     * @param api_key Lightning API key (default: "0987654321")
     * @param client_token Authentication token from pairing (optional)
     */
    LightningClient(const std::string& ip_address,
                    const std::string& api_key = "0987654321",
                    const std::string& client_token = "");

    /**
     * Destructor - cleanup CURL handle
     */
    ~LightningClient();

    // Disable copy (CURL handles are non-copyable)
    LightningClient(const LightningClient&) = delete;
    LightningClient& operator=(const LightningClient&) = delete;

    // ========================================================================
    // PAIRING / AUTHENTICATION
    // ========================================================================

    /**
     * Wake device before pairing (optional)
     *
     * Sends HTTP POST to http://{ip}:8009/apps/FireTVRemote
     * to wake device from standby mode.
     *
     * @return true if wake successful or device already awake
     */
    bool wakeDevice();

    /**
     * Display PIN on Fire TV screen for pairing
     *
     * Sends request to display PIN on TV screen. User must then enter this PIN
     * on the Fire TV to complete pairing.
     *
     * @param friendly_name Name shown on TV screen (default: "HMS FireTV")
     * @return true if TV acknowledged the PIN display request
     */
    bool displayPin(const std::string& friendly_name = "HMS FireTV");

    /**
     * Verify PIN entered on Fire TV
     *
     * After user enters PIN on Fire TV, call this to verify and get client token.
     * The token will be automatically stored for future requests.
     *
     * @param pin PIN code to verify
     * @return Client token if successful, empty string if failed
     */
    std::string verifyPin(const std::string& pin);

    /**
     * Verify a PIN and say why it failed, if it did.
     *
     * verifyPin() collapses a wrong PIN and a dead network into the same
     * empty string; this keeps them apart so the caller can tell the user
     * to retype rather than reporting a device failure.
     */
    PinVerifyResult verifyPinDetailed(const std::string& pin);

    /** Classify a pin/verify response. Pure, so it is unit-testable. */
    static PinVerifyResult interpretPinVerify(const CommandResult& result);

    /**
     * Ask the device whether our client token is still good.
     *
     * GET /v1/FireTV/status with the token. Does NOT wake a sleeping device:
     * waking powers the TV on over HDMI-CEC, and a check must never do that.
     * A sleeping device therefore answers Inconclusive.
     */
    TokenCheck checkToken();

    /** Classify a /v1/FireTV/status response. Pure, so it is unit-testable. */
    static TokenCheck interpretTokenCheck(const CommandResult& result);

    /** 401 and 403 are how the device says "I do not know this token". */
    static bool isAuthRejection(int status_code);

    /**
     * Called with the token that was sent whenever an authenticated request
     * is answered 401/403. Owners use it to revalidate and, if the device
     * really has dropped the token, clear the pairing (see TokenGuard).
     * Health probes and the token check itself never fire it.
     */
    void setAuthRejectedHandler(std::function<void(const std::string& token)> handler);

    /**
     * Set client token (for already-paired devices)
     *
     * @param token Client token from previous pairing
     */
    void setClientToken(const std::string& token);

    /**
     * Get current client token
     *
     * @return Current client token
     */
    std::string getClientToken() const;

    // ========================================================================
    // MEDIA CONTROLS
    // ========================================================================

    /**
     * Send media control command
     *
     * @param action Media action ("play", "scan"; "pause"/"stop" go out as "play")
     * @param direction Scan direction ("forward", "backward"; "back" is
     *                  accepted and sent as "backward") - required for scan
     * @return Command result with success status and timing
     */
    CommandResult sendMediaCommand(const std::string& action,
                                     const std::string& direction = "");

    /**
     * JSON body for POST /v1/media?action=scan.
     *
     * The device takes the direction in the body, not the query string:
     * {"direction":"forward"|"backward","durationInSeconds":"10","speed":"1"}
     * with the numbers as strings. Without it the device answers
     * "Bad arguments supplied".
     */
    static std::string buildScanBody(const std::string& direction);

    /**
     * The media action actually sent. The device documents only "play"
     * (a play/pause toggle) and "scan"; "pause", "stop" and "play_pause"
     * are sent as "play". There is no discrete pause or stop.
     */
    static std::string mediaWireAction(const std::string& action);

    /**
     * Convenience methods for common media commands
     */
    CommandResult play();
    CommandResult pause();
    CommandResult scanForward();
    CommandResult scanBackward();

    // ========================================================================
    // NAVIGATION CONTROLS
    // ========================================================================

    /**
     * Send navigation command
     *
     * @param action Navigation action:
     *               "dpad_up", "dpad_down", "dpad_left", "dpad_right",
     *               "select", "home", "back", "menu", "sleep", "power"
     * @return Command result with success status and timing
     */
    CommandResult sendNavigationCommand(const std::string& action);

    /**
     * Convenience methods for common navigation commands
     */
    CommandResult dpadUp();
    CommandResult dpadDown();
    CommandResult dpadLeft();
    CommandResult dpadRight();
    CommandResult select();
    CommandResult home();
    CommandResult back();
    CommandResult menu();
    CommandResult sleep();

    /** action=power: a TOGGLE of the connected TV over HDMI-CEC, not a
     * discrete on or off. The device does not report TV power state, so a
     * caller must not retry it to "make sure". */
    CommandResult powerToggle();

    // ========================================================================
    // APP LAUNCH
    // ========================================================================

    /**
     * Launch app by package name
     *
     * @param package_name Android package name (e.g., "com.netflix.ninja")
     * @return Command result with success status and timing
     */
    /* Press-and-hold. The official app sends navigation as TWO requests,
     * {"keyActionType":"keyDown"} then {"keyActionType":"keyUp"}, which is what
     * produces key repeat on the device. A single body-less POST (what
     * sendNavigationCommand does) is also valid and the app uses it too, but it
     * can only ever be one discrete press. Captured from the app's own log. */
    CommandResult sendNavigationCommand(const std::string& action,
                                        const std::string& key_action_type);
    CommandResult holdKey(const std::string& action);
    CommandResult releaseKey(const std::string& action);

    /* Installed apps. GET /v1/FireTV/appsV2 returns the list the official app
     * shows in its Apps panel; launchApp() then takes one of those packages. */
    CommandResult listApps();

    /* Voice. These are only the BOOKENDS - the audio itself is binary frames on
     * a WebSocket at wss://<ip>:9090/ (see DeviceConnection.sendVoiceData in the
     * Fire TV app). Sending start/stop without streaming audio opens and closes
     * the voice session on the device but says nothing. */
    CommandResult voiceStart();
    CommandResult voiceStop();

    CommandResult launchApp(const std::string& package_name);

    // ========================================================================
    // KEYBOARD INPUT (EXPERIMENTAL)
    // ========================================================================

    /**
     * Send keyboard input (experimental feature)
     *
     * @param text Text to type
     * @return Command result with success status and timing
     */
    CommandResult sendKeyboardInput(const std::string& text);

    // ========================================================================
    // HEALTH CHECK
    // ========================================================================

    /**
     * Check if Lightning API (port 8080) is responding
     *
     * This is used to determine if device needs to be woken up.
     * Lightning API stops responding when Fire TV is in standby.
     *
     * @return true if Lightning API responds, false otherwise
     */
    bool isLightningApiAvailable();

    /**
     * Check if device is reachable (wake endpoint)
     *
     * @return true if device responds, false otherwise
     */
    bool healthCheck();

private:
    // Device information
    std::string ip_address_;
    std::string api_key_;
    std::string client_token_;

    // URLs
    std::string base_url_;        // https://{ip}:8080
    std::string wake_url_;        // http://{ip}:8009/apps/FireTVRemote

    // CURL handle (reused for all requests)
    CURL* curl_;

    // Request timeout (seconds)
    static constexpr long WAKE_TIMEOUT = 5L;
    static constexpr long HEALTH_TIMEOUT = 2L;
    static constexpr long COMMAND_TIMEOUT = 10L;
    /* A device that is present answers the TCP connect immediately, asleep
     * or not. Anything slower is absent, and waiting the full command
     * timeout on it only delays the failure. */
    static constexpr long CONNECT_TIMEOUT = 2L;

    /**
     * CURL write callback for response data
     */
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    /**
     * Build request headers with authentication
     *
     * @param include_token Whether to include X-Client-Token header
     * @return curl_slist with headers
     */
    struct curl_slist* buildHeaders(bool include_token = true);

    /**
     * Execute HTTP GET request
     *
     * @param url Request URL
     * @param timeout_seconds Request timeout
     * @param allow_wake Wake a sleeping device and retry once on a refused connect
     * @param report_auth Fire the auth-rejected handler on 401/403
     * @return Command result
     */
    CommandResult executeGet(const std::string& url, long timeout_seconds = COMMAND_TIMEOUT,
                             bool allow_wake = true, bool report_auth = true);

    /**
     * Execute HTTP POST request
     *
     * @param url Request URL
     * @param json_body JSON body (empty for no body)
     * @param timeout_seconds Request timeout
     * @param include_token Include client token in headers
     * @param allow_wake Wake a sleeping device and retry once on a refused
     *                   connect. False for the wake request itself, which
     *                   would otherwise recurse into wakeAndWait().
     * @return Command result
     */
    CommandResult executePost(const std::string& url,
                                const std::string& json_body = "",
                                long timeout_seconds = COMMAND_TIMEOUT,
                                bool include_token = true,
                                bool allow_wake = true);

    /** Fire auth_rejected_handler_ if this answer rejected the token sent. */
    void reportAuthResult(const CommandResult& result, const std::string& token_sent);

    std::function<void(const std::string&)> auth_rejected_handler_;

    /**
     * Parse JSON response body
     *
     * @param body Response body string
     * @return Parsed JSON value
     */
    Json::Value parseJsonResponse(const std::string& body);

    /**
     * Wake the device and wait for the Lightning API to answer.
     *
     * A sleeping Fire TV does not listen on 8080 at all, so a command sent to
     * one fails at the socket with no HTTP status. Callers that went through
     * MQTT used to be woken by CommandHandler, but a REST caller was not, so
     * pressing a button on a sleeping device silently did nothing. Recovering
     * here means every path gets it.
     *
     * @return true if the API answered within the wait
     */
    bool wakeAndWait();

    /**
     * Guards the wake-and-retry so a retried request cannot recurse.
     * Not atomic because a LightningClient is single-threaded by contract.
     */
    bool retrying_after_wake_ = false;
};

} // namespace hms_firetv
