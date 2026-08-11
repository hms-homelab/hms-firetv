#include "api/VoiceWebSocket.h"
#include "services/VoiceService.h"

#include <json/json.h>

#include <iostream>
#include <thread>

namespace hms_firetv {

void VoiceWebSocket::sendJson(const WebSocketConnectionPtr& conn, const std::string& type,
                              const std::string& detail) {
    Json::Value v;
    v["type"] = type;
    if (!detail.empty()) v["detail"] = detail;

    Json::StreamWriterBuilder w;
    w["indentation"] = "";
    conn->send(Json::writeString(w, v), WebSocketMessageType::Text);
}

void VoiceWebSocket::handleNewConnection(const HttpRequestPtr& req,
                                         const WebSocketConnectionPtr& conn) {
    std::string device_id = req->getParameter("device");

    if (device_id.empty()) {
        sendJson(conn, "error", "missing ?device=<device_id>");
        conn->shutdown(CloseCode::kViolation, "no device");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        Session s;
        s.device_id = device_id;
        sessions_[conn.get()] = s;
    }

    std::cout << "[VoiceWebSocket] client connected for " << device_id << std::endl;

    // Connected, but not yet listening - the caller sends "start" when the
    // user actually presses to talk, so the device is not held open idle.
    sendJson(conn, "connected", device_id);
}

void VoiceWebSocket::startRelay(const WebSocketConnectionPtr& conn) {
    std::string device_id;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(conn.get());
        if (it == sessions_.end()) return;
        if (it->second.starting || !it->second.relay_id.empty()) {
            sendJson(conn, "error", "session already starting or open");
            return;
        }
        it->second.starting = true;
        device_id = it->second.device_id;
    }

    // Waking the device and completing two TLS handshakes takes seconds. Doing
    // it here would block this connection's event loop, and with it every
    // other connection sharing that loop.
    std::thread([this, conn, device_id]() {
        std::string relay_id;
        auto result = VoiceService::getInstance().openRelay(device_id, &relay_id);

        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(conn.get());
        if (it == sessions_.end()) {
            // Client vanished mid-open; do not leak the session on the device.
            if (result.get("success", false).asBool())
                VoiceService::getInstance().closeRelay(relay_id);
            return;
        }

        it->second.starting = false;

        if (!result.get("success", false).asBool()) {
            sendJson(conn, "error", result.get("error", "could not open voice session").asString());
            return;
        }

        it->second.relay_id = relay_id;
        it->second.bytes = 0;
        sendJson(conn, "ready", relay_id);
        std::cout << "[VoiceWebSocket] relay " << relay_id << " ready for " << device_id
                  << std::endl;
    }).detach();
}

void VoiceWebSocket::stopRelay(const WebSocketConnectionPtr& conn, bool notify) {
    std::string relay_id;
    size_t bytes = 0;

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(conn.get());
        if (it == sessions_.end()) return;
        relay_id = it->second.relay_id;
        bytes = it->second.bytes;
        it->second.relay_id.clear();
    }

    if (relay_id.empty()) {
        if (notify) sendJson(conn, "error", "no open session");
        return;
    }

    // Closing also does network I/O (close frame + the stop bookend), so it
    // goes off the event loop too.
    std::thread([relay_id, conn, bytes, notify]() {
        VoiceService::getInstance().closeRelay(relay_id);
        std::cout << "[VoiceWebSocket] relay " << relay_id << " closed after " << bytes
                  << " bytes" << std::endl;
        if (notify && conn->connected())
            sendJson(conn, "stopped", std::to_string(bytes) + " bytes");
    }).detach();
}

void VoiceWebSocket::handleNewMessage(const WebSocketConnectionPtr& conn, std::string&& message,
                                      const WebSocketMessageType& type) {
    if (type == WebSocketMessageType::Ping || type == WebSocketMessageType::Pong) return;

    if (type == WebSocketMessageType::Text) {
        if (message == "start") {
            startRelay(conn);
        } else if (message == "stop") {
            stopRelay(conn, true);
        } else {
            sendJson(conn, "error", "unknown command: " + message);
        }
        return;
    }

    if (type != WebSocketMessageType::Binary) return;

    // Audio. Already 16 kHz mono s16le - the browser converts before sending,
    // so this is a straight pass-through to the device.
    std::string relay_id;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(conn.get());
        if (it == sessions_.end()) return;
        relay_id = it->second.relay_id;
        it->second.bytes += message.size();
    }

    if (relay_id.empty()) {
        // Audio before "ready". Dropping it is correct: there is nowhere to
        // put it, and queueing would desynchronise the utterance.
        return;
    }

    std::vector<uint8_t> audio(message.begin(), message.end());
    auto result = VoiceService::getInstance().pushRelay(relay_id, audio, true);

    if (!result.get("success", false).asBool()) {
        sendJson(conn, "error", result.get("error", "push failed").asString());
        stopRelay(conn, false);
    }
}

void VoiceWebSocket::handleConnectionClosed(const WebSocketConnectionPtr& conn) {
    // A dropped connection must still close the session on the device,
    // otherwise the Fire TV sits with its microphone open.
    stopRelay(conn, false);

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(conn.get());
    std::cout << "[VoiceWebSocket] client disconnected" << std::endl;
}

}  // namespace hms_firetv
