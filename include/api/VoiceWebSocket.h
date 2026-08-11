#pragma once

#include <drogon/WebSocketController.h>

#include <mutex>
#include <string>
#include <unordered_map>

using namespace drogon;

namespace hms_firetv {

/**
 * VoiceWebSocket - live microphone relay into the Fire TV voice channel
 *
 *   browser / HA companion mic  --wss-->  this service  --wss-->  Fire TV:9090
 *
 * The device wants raw PCM at 16 kHz mono s16le (see VoiceClient). The browser
 * does the capture and rate conversion, so frames arriving here are already in
 * that format and are forwarded untouched - no transcode, no buffering beyond
 * the socket, which keeps the relay close to real time.
 *
 * PROTOCOL (between browser and this service)
 * ===========================================
 *   client connects to  /ws/voice?device=<device_id>
 *   -> text "start"     open the voice session on the device
 *   <- text {"type":"ready"}         audio may now be sent
 *   -> binary frames    raw PCM s16le 16 kHz mono
 *   -> text "stop"      close the session; Alexa acts on what it heard
 *   <- text {"type":"stopped"}
 *
 * Opening a session wakes the device, does the voiceCommand bookend and a TLS
 * handshake, so it can take a couple of seconds. That work happens on its own
 * thread rather than in the connection callback, which would stall a Drogon
 * event loop and every other request on it. The client waits for "ready".
 *
 * SECURE CONTEXT
 * ==============
 * Browsers only expose a microphone to a secure context, so the page and this
 * socket must be reached over TLS (https:// and wss://). Set API_SSL_PORT to
 * enable the service's TLS listener; see main.cpp.
 */
class VoiceWebSocket : public drogon::WebSocketController<VoiceWebSocket> {
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/voice", Get);
    WS_PATH_LIST_END

    void handleNewConnection(const HttpRequestPtr& req,
                             const WebSocketConnectionPtr& conn) override;

    void handleNewMessage(const WebSocketConnectionPtr& conn, std::string&& message,
                          const WebSocketMessageType& type) override;

    void handleConnectionClosed(const WebSocketConnectionPtr& conn) override;

private:
    struct Session {
        std::string device_id;
        std::string relay_id;   // empty until "start" completes
        bool starting = false;
        size_t bytes = 0;
    };

    /** Per-connection state, keyed by the connection pointer. */
    std::unordered_map<const WebSocketConnection*, Session> sessions_;
    std::mutex sessions_mutex_;

    void startRelay(const WebSocketConnectionPtr& conn);
    void stopRelay(const WebSocketConnectionPtr& conn, bool notify);

    static void sendJson(const WebSocketConnectionPtr& conn, const std::string& type,
                         const std::string& detail = "");
};

}  // namespace hms_firetv
