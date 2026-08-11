#include "api/VoiceController.h"
#include "services/VoiceService.h"

#include <iostream>

using namespace drogon;

namespace hms_firetv {

void VoiceController::reply(std::function<void(const HttpResponsePtr&)>&& callback,
                            const Json::Value& result) {
    auto resp = HttpResponse::newHttpJsonResponse(result);
    if (!result.get("success", false).asBool()) resp->setStatusCode(k400BadRequest);
    callback(resp);
}

// ============================================================================
// MODE 2 - SPOKEN
// ============================================================================

void VoiceController::say(const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback,
                          std::string device_id) {
    try {
        std::string text;

        auto json = req->getJsonObject();
        if (json && json->isMember("text")) {
            text = (*json)["text"].asString();
        } else {
            // Accept a bare string body too, so an MQTT-style text payload
            // and a curl one-liner both work.
            text = std::string(req->getBody());
        }

        if (text.empty()) {
            Json::Value e;
            e["success"] = false;
            e["error"] = "no text to speak - send {\"text\": \"...\"}";
            reply(std::move(callback), e);
            return;
        }

        reply(std::move(callback), VoiceService::getInstance().say(device_id, text));

    } catch (const std::exception& e) {
        std::cerr << "[VoiceController] say failed: " << e.what() << std::endl;
        Json::Value err;
        err["success"] = false;
        err["error"] = e.what();
        reply(std::move(callback), err);
    }
}

void VoiceController::audio(const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& callback,
                            std::string device_id) {
    try {
        auto& service = VoiceService::getInstance();
        auto json = req->getJsonObject();

        if (json && json->isMember("url")) {
            reply(std::move(callback), service.speakUrl(device_id, (*json)["url"].asString()));
            return;
        }
        if (json && json->isMember("file")) {
            reply(std::move(callback), service.speakFile(device_id, (*json)["file"].asString()));
            return;
        }

        // Otherwise the body is the audio itself.
        auto body = req->getBody();
        if (body.empty()) {
            Json::Value e;
            e["success"] = false;
            e["error"] = "send {\"url\":...}, {\"file\":...}, or the audio as the body";
            reply(std::move(callback), e);
            return;
        }

        std::vector<uint8_t> audio_bytes(body.begin(), body.end());
        reply(std::move(callback), service.speakAudio(device_id, audio_bytes));

    } catch (const std::exception& e) {
        std::cerr << "[VoiceController] audio failed: " << e.what() << std::endl;
        Json::Value err;
        err["success"] = false;
        err["error"] = e.what();
        reply(std::move(callback), err);
    }
}

// ============================================================================
// MODE 1 - BOOKENDS
// ============================================================================

void VoiceController::start(const HttpRequestPtr&,
                            std::function<void(const HttpResponsePtr&)>&& callback,
                            std::string device_id) {
    reply(std::move(callback), VoiceService::getInstance().startSession(device_id));
}

void VoiceController::stop(const HttpRequestPtr&,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           std::string device_id) {
    reply(std::move(callback), VoiceService::getInstance().stopSession(device_id));
}

// ============================================================================
// MODE 3 - LIVE RELAY
// ============================================================================

void VoiceController::openRelay(const HttpRequestPtr&,
                                std::function<void(const HttpResponsePtr&)>&& callback,
                                std::string device_id) {
    reply(std::move(callback), VoiceService::getInstance().openRelay(device_id));
}

void VoiceController::pushRelay(const HttpRequestPtr& req,
                                std::function<void(const HttpResponsePtr&)>&& callback,
                                std::string session_id) {
    try {
        auto body = req->getBody();
        std::vector<uint8_t> audio(body.begin(), body.end());

        // Raw means "already 16 kHz mono s16le, do not touch it".
        std::string content_type = req->getHeader("content-type");
        bool is_raw = req->getParameter("raw") == "1" ||
                      content_type.find("audio/L16") != std::string::npos ||
                      content_type.find("application/octet-stream") != std::string::npos;

        reply(std::move(callback),
              VoiceService::getInstance().pushRelay(session_id, audio, is_raw));

    } catch (const std::exception& e) {
        std::cerr << "[VoiceController] relay push failed: " << e.what() << std::endl;
        Json::Value err;
        err["success"] = false;
        err["error"] = e.what();
        reply(std::move(callback), err);
    }
}

void VoiceController::closeRelay(const HttpRequestPtr&,
                                 std::function<void(const HttpResponsePtr&)>&& callback,
                                 std::string session_id) {
    reply(std::move(callback), VoiceService::getInstance().closeRelay(session_id));
}

// ============================================================================
// STATUS
// ============================================================================

void VoiceController::status(const HttpRequestPtr&,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
    auto& service = VoiceService::getInstance();
    service.reapStaleRelays();

    Json::Value out = service.relayStatus();
    out["success"] = true;
    out["tts"] = service.ttsStatus();

    Json::Value format;
    format["encoding"] = "pcm_s16le";
    format["sample_rate"] = VoiceClient::SAMPLE_RATE;
    format["channels"] = VoiceClient::CHANNELS;
    format["transport"] = "binary websocket frames to wss://<device>:9090/";
    out["audio_format"] = format;

    callback(HttpResponse::newHttpJsonResponse(out));
}

}  // namespace hms_firetv
