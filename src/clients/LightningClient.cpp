#include "clients/LightningClient.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <json/json.h>

namespace hms_firetv {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

LightningClient::LightningClient(const std::string& ip_address,
                                   const std::string& api_key,
                                   const std::string& client_token)
    : ip_address_(ip_address),
      api_key_(api_key),
      client_token_(client_token),
      base_url_("https://" + ip_address + ":8080"),
      wake_url_("http://" + ip_address + ":8009/apps/FireTVRemote") {

    // Initialize CURL handle
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "[LightningClient] Failed to initialize CURL" << std::endl;
    }

    std::cout << "[LightningClient] Initialized for device at " << ip_address << std::endl;
}

LightningClient::~LightningClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

// ============================================================================
// PAIRING / AUTHENTICATION
// ============================================================================

bool LightningClient::wakeDevice() {
    std::cout << "[LightningClient] Waking device " << ip_address_ << std::endl;

    auto result = executePost(wake_url_, "", WAKE_TIMEOUT, false);

    // 200, 201, 204 are all success
    bool success = result.status_code == 200 ||
                   result.status_code == 201 ||
                   result.status_code == 204;

    if (success) {
        std::cout << "[LightningClient] Device wake successful ("
                  << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cout << "[LightningClient] Device wake failed or already awake" << std::endl;
    }

    return success;
}

std::string LightningClient::displayPin(const std::string& friendly_name) {
    std::cout << "[LightningClient] Displaying PIN on " << ip_address_ << std::endl;

    // Build JSON payload
    Json::Value payload;
    payload["friendlyName"] = friendly_name;

    Json::StreamWriterBuilder writer;
    std::string json_body = Json::writeString(writer, payload);

    // Send request
    std::string url = base_url_ + "/v1/FireTV/pin/display";
    auto result = executePost(url, json_body, COMMAND_TIMEOUT, false);

    if (result.success && result.status_code == 200) {
        // Extract PIN from response
        if (result.response_body.isMember("description")) {
            std::string pin = result.response_body["description"].asString();
            std::cout << "[LightningClient] PIN displayed: " << pin
                      << " (" << result.response_time_ms << "ms)" << std::endl;
            return pin;
        } else {
            std::cerr << "[LightningClient] PIN response missing 'description' field" << std::endl;
        }
    } else {
        std::cerr << "[LightningClient] Failed to display PIN: "
                  << result.status_code << std::endl;
    }

    return "";
}

std::string LightningClient::verifyPin(const std::string& pin) {
    std::cout << "[LightningClient] Verifying PIN " << pin
              << " on " << ip_address_ << std::endl;

    // Build JSON payload
    Json::Value payload;
    payload["pin"] = pin;

    Json::StreamWriterBuilder writer;
    std::string json_body = Json::writeString(writer, payload);

    // Send request
    std::string url = base_url_ + "/v1/FireTV/pin/verify";
    auto result = executePost(url, json_body, COMMAND_TIMEOUT, false);

    if (result.success && result.status_code == 200) {
        // Extract token from response
        if (result.response_body.isMember("description")) {
            std::string token = result.response_body["description"].asString();

            // Validate token (must not be "OK" or empty)
            if (!token.empty() && token != "OK") {
                client_token_ = token;  // Store token for future requests
                std::cout << "[LightningClient] PIN verified, token: " << token
                          << " (" << result.response_time_ms << "ms)" << std::endl;
                return token;
            } else {
                std::cerr << "[LightningClient] Invalid token received: " << token << std::endl;
            }
        } else {
            std::cerr << "[LightningClient] PIN verify response missing 'description' field" << std::endl;
        }
    } else {
        std::cerr << "[LightningClient] Failed to verify PIN: "
                  << result.status_code << std::endl;
    }

    return "";
}

void LightningClient::setClientToken(const std::string& token) {
    client_token_ = token;
}

std::string LightningClient::getClientToken() const {
    return client_token_;
}

// ============================================================================
// MEDIA CONTROLS
// ============================================================================

CommandResult LightningClient::sendMediaCommand(const std::string& action,
                                                  const std::string& direction) {
    std::string url = base_url_ + "/v1/media?action=" + action;

    if (action == "scan" && !direction.empty()) {
        url += "&direction=" + direction;
    }

    auto result = executePost(url);

    if (result.success) {
        std::cout << "[LightningClient] Media command '" << action
                  << "' sent (" << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[LightningClient] Media command '" << action
                  << "' failed: " << result.status_code << std::endl;
    }

    return result;
}

CommandResult LightningClient::play() {
    return sendMediaCommand("play");
}

CommandResult LightningClient::pause() {
    return sendMediaCommand("pause");
}

CommandResult LightningClient::scanForward() {
    return sendMediaCommand("scan", "forward");
}

CommandResult LightningClient::scanBackward() {
    return sendMediaCommand("scan", "back");
}

// ============================================================================
// NAVIGATION CONTROLS
// ============================================================================

CommandResult LightningClient::sendNavigationCommand(const std::string& action) {
    std::string url = base_url_ + "/v1/FireTV?action=" + action;

    auto result = executePost(url);

    if (result.success) {
        std::cout << "[LightningClient] Navigation command '" << action
                  << "' sent (" << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[LightningClient] Navigation command '" << action
                  << "' failed: " << result.status_code << std::endl;
    }

    return result;
}

CommandResult LightningClient::dpadUp() {
    return sendNavigationCommand("dpad_up");
}

CommandResult LightningClient::dpadDown() {
    return sendNavigationCommand("dpad_down");
}

CommandResult LightningClient::dpadLeft() {
    return sendNavigationCommand("dpad_left");
}

CommandResult LightningClient::dpadRight() {
    return sendNavigationCommand("dpad_right");
}

CommandResult LightningClient::select() {
    return sendNavigationCommand("select");
}

CommandResult LightningClient::home() {
    return sendNavigationCommand("home");
}

CommandResult LightningClient::back() {
    return sendNavigationCommand("back");
}

CommandResult LightningClient::menu() {
    return sendNavigationCommand("menu");
}

CommandResult LightningClient::sleep() {
    return sendNavigationCommand("sleep");
}

// ============================================================================
// APP LAUNCH
// ============================================================================

CommandResult LightningClient::launchApp(const std::string& package_name) {
    std::string url = base_url_ + "/v1/FireTV/app/" + package_name;

    auto result = executePost(url);

    if (result.success) {
        std::cout << "[LightningClient] Launched app '" << package_name
                  << "' (" << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[LightningClient] App launch failed: "
                  << result.status_code << std::endl;
    }

    return result;
}

// ============================================================================
// KEYBOARD INPUT
// ============================================================================

CommandResult LightningClient::sendKeyboardInput(const std::string& text) {
    // Build JSON payload
    Json::Value payload;
    payload["text"] = text;

    Json::StreamWriterBuilder writer;
    std::string json_body = Json::writeString(writer, payload);

    std::string url = base_url_ + "/v1/FireTV/keyboard";
    auto result = executePost(url, json_body);

    if (result.success) {
        std::cout << "[LightningClient] Keyboard input sent ("
                  << result.response_time_ms << "ms)" << std::endl;
    } else {
        std::cerr << "[LightningClient] Keyboard input failed: "
                  << result.status_code << std::endl;
    }

    return result;
}

// ============================================================================
// HEALTH CHECK
// ============================================================================

bool LightningClient::isLightningApiAvailable() {
    std::string url = base_url_ + "/v1/FireTV";
    auto result = executeGet(url, HEALTH_TIMEOUT);

    // Any response (even errors) means API is up
    return result.status_code > 0;
}

bool LightningClient::healthCheck() {
    auto result = executeGet(wake_url_, HEALTH_TIMEOUT);

    // 200, 204, 404 are all good (means device is reachable)
    return result.status_code == 200 ||
           result.status_code == 204 ||
           result.status_code == 404;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

size_t LightningClient::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

struct curl_slist* LightningClient::buildHeaders(bool include_token) {
    struct curl_slist* headers = nullptr;

    // Add API key
    std::string api_key_header = "X-Api-Key: " + api_key_;
    headers = curl_slist_append(headers, api_key_header.c_str());

    // Add client token if available and requested
    if (include_token && !client_token_.empty()) {
        std::string token_header = "X-Client-Token: " + client_token_;
        headers = curl_slist_append(headers, token_header.c_str());
    }

    // Add content type
    headers = curl_slist_append(headers, "Content-Type: application/json");

    return headers;
}

CommandResult LightningClient::executeGet(const std::string& url, long timeout_seconds) {
    CommandResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!curl_) {
        result.error = "CURL not initialized";
        return result;
    }

    std::string response_body;
    struct curl_slist* headers = buildHeaders(true);

    // Configure CURL
    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);  // Disable SSL verification (self-signed cert)
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects

    // Execute request
    CURLcode res = curl_easy_perform(curl_);

    // Calculate response time
    auto end_time = std::chrono::steady_clock::now();
    result.response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();

    // Get HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
    result.status_code = static_cast<int>(http_code);

    // Cleanup
    curl_slist_free_all(headers);

    // Check for errors
    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
        result.success = false;
        return result;
    }

    // Parse JSON response if available
    if (!response_body.empty()) {
        result.response_body = parseJsonResponse(response_body);
    }

    result.success = (http_code >= 200 && http_code < 300);
    return result;
}

CommandResult LightningClient::executePost(const std::string& url,
                                             const std::string& json_body,
                                             long timeout_seconds,
                                             bool include_token) {
    CommandResult result;
    auto start_time = std::chrono::steady_clock::now();

    if (!curl_) {
        result.error = "CURL not initialized";
        return result;
    }

    std::string response_body;
    struct curl_slist* headers = buildHeaders(include_token);

    // Configure CURL
    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);  // Disable SSL verification (self-signed cert)
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects

    // Set POST data if provided
    if (!json_body.empty()) {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, json_body.c_str());
    } else {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, 0L);
    }

    // Execute request
    CURLcode res = curl_easy_perform(curl_);

    // Calculate response time
    auto end_time = std::chrono::steady_clock::now();
    result.response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();

    // Get HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
    result.status_code = static_cast<int>(http_code);

    // Cleanup
    curl_slist_free_all(headers);

    // Check for errors
    if (res != CURLE_OK) {
        result.error = curl_easy_strerror(res);
        result.success = false;
        return result;
    }

    // Parse JSON response if available
    if (!response_body.empty()) {
        result.response_body = parseJsonResponse(response_body);
    }

    result.success = (http_code >= 200 && http_code < 300);
    return result;
}

Json::Value LightningClient::parseJsonResponse(const std::string& body) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;

    std::istringstream stream(body);
    if (!Json::parseFromStream(reader, stream, &root, &errors)) {
        std::cerr << "[LightningClient] JSON parse error: " << errors << std::endl;
    }

    return root;
}

} // namespace hms_firetv
