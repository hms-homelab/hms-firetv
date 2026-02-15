#pragma once

#include <string>
#include <optional>
#include <json/json.h>
#include <curl/curl.h>
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
     * @return PIN code if successful, empty string if failed
     */
    std::string displayPin(const std::string& friendly_name = "HMS FireTV");

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
     * @param action Media action ("play", "pause", "scan")
     * @param direction Scan direction ("forward", "back") - required for scan
     * @return Command result with success status and timing
     */
    CommandResult sendMediaCommand(const std::string& action,
                                     const std::string& direction = "");

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
     *               "select", "home", "back", "menu", "sleep"
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

    // ========================================================================
    // APP LAUNCH
    // ========================================================================

    /**
     * Launch app by package name
     *
     * @param package_name Android package name (e.g., "com.netflix.ninja")
     * @return Command result with success status and timing
     */
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
     * @return Command result
     */
    CommandResult executeGet(const std::string& url, long timeout_seconds = COMMAND_TIMEOUT);

    /**
     * Execute HTTP POST request
     *
     * @param url Request URL
     * @param json_body JSON body (empty for no body)
     * @param timeout_seconds Request timeout
     * @param include_token Include client token in headers
     * @return Command result
     */
    CommandResult executePost(const std::string& url,
                                const std::string& json_body = "",
                                long timeout_seconds = COMMAND_TIMEOUT,
                                bool include_token = true);

    /**
     * Parse JSON response body
     *
     * @param body Response body string
     * @return Parsed JSON value
     */
    Json::Value parseJsonResponse(const std::string& body);
};

} // namespace hms_firetv
