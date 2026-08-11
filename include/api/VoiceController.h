#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace hms_firetv {

/**
 * VoiceController - REST API for Alexa voice control
 *
 * The Fire TV voice channel takes AUDIO, never text: raw PCM at 16 kHz mono
 * over a WebSocket on port 9090. Text is turned into audio by the Wyoming TTS
 * engine configured on the server (TTS_HOST), so /say is text in, speech out,
 * Alexa acting on the other end.
 *
 * Endpoints:
 * - POST /api/devices/:id/voice/say      - speak text (needs TTS configured)
 * - POST /api/devices/:id/voice/audio    - speak a WAV/URL/file
 * - POST /api/devices/:id/voice/start    - open a session, send nothing
 * - POST /api/devices/:id/voice/stop     - close that session
 * - POST /api/devices/:id/voice/relay    - open a live relay, returns session id
 * - POST /api/voice/relay/:sid/audio     - push captured audio into the relay
 * - POST /api/voice/relay/:sid/close     - close the relay
 * - GET  /api/voice/status               - TTS config and open sessions
 */
class VoiceController : public drogon::HttpController<VoiceController> {
public:
    METHOD_LIST_BEGIN

    ADD_METHOD_TO(VoiceController::say, "/api/devices/{1}/voice/say", Post);
    ADD_METHOD_TO(VoiceController::audio, "/api/devices/{1}/voice/audio", Post);
    ADD_METHOD_TO(VoiceController::start, "/api/devices/{1}/voice/start", Post);
    ADD_METHOD_TO(VoiceController::stop, "/api/devices/{1}/voice/stop", Post);
    ADD_METHOD_TO(VoiceController::openRelay, "/api/devices/{1}/voice/relay", Post);
    ADD_METHOD_TO(VoiceController::pushRelay, "/api/voice/relay/{1}/audio", Post);
    ADD_METHOD_TO(VoiceController::closeRelay, "/api/voice/relay/{1}/close", Post);
    ADD_METHOD_TO(VoiceController::status, "/api/voice/status", Get);

    METHOD_LIST_END

    /**
     * Speak text.
     * POST /api/devices/:id/voice/say
     * Body: {"text": "play Stranger Things"}
     */
    void say(const HttpRequestPtr& req,
             std::function<void(const HttpResponsePtr&)>&& callback,
             std::string device_id);

    /**
     * Speak pre-existing audio.
     * POST /api/devices/:id/voice/audio
     * Body: {"url": "..."} or {"file": "..."}, or the raw audio as the body.
     */
    void audio(const HttpRequestPtr& req,
               std::function<void(const HttpResponsePtr&)>&& callback,
               std::string device_id);

    /** Open a voice session with no audio. */
    void start(const HttpRequestPtr& req,
               std::function<void(const HttpResponsePtr&)>&& callback,
               std::string device_id);

    /** Close a session opened by start(). */
    void stop(const HttpRequestPtr& req,
              std::function<void(const HttpResponsePtr&)>&& callback,
              std::string device_id);

    /** Open a live relay and return its session id. */
    void openRelay(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   std::string device_id);

    /**
     * Push captured audio into an open relay.
     * The body is the audio itself. Content-Type audio/L16 (or a `raw=1`
     * query parameter) means it is already 16 kHz mono s16le and goes
     * straight through; anything else is decoded first.
     */
    void pushRelay(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   std::string session_id);

    /** Close a relay. */
    void closeRelay(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback,
                    std::string session_id);

    /** TTS configuration and currently open sessions. */
    void status(const HttpRequestPtr& req,
                std::function<void(const HttpResponsePtr&)>&& callback);

private:
    static void reply(std::function<void(const HttpResponsePtr&)>&& callback,
                      const Json::Value& result);
};

}  // namespace hms_firetv
