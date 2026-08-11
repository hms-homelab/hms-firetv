#pragma once

#include "clients/LightningClient.h"
#include "clients/TtsClient.h"
#include "clients/VoiceClient.h"

#include <json/json.h>

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace hms_firetv {

/**
 * VoiceService - Alexa voice control for Fire TV
 *
 * Owns the three ways of driving the voice channel, which differ only in
 * where the audio comes from:
 *
 *   1. BOOKENDS   - open and close a voice session with no audio at all.
 *                   Useful for testing and for driving the session from
 *                   somewhere else.
 *   2. SPOKEN     - synthesise text with Piper, or take a WAV/URL/file, and
 *                   stream it paced at 1x. This is the one that makes
 *                   "tell the TV to play Stranger Things" work from an
 *                   automation.
 *   3. LIVE RELAY - hold a session open across requests so a caller can push
 *                   microphone audio as it is captured.
 *
 * Every mode wraps the audio in the same bookends:
 *   voiceCommand?action=start -> WebSocket -> voiceCommand?action=stop
 *
 * CONFIGURATION
 * =============
 * The TTS engine is configured on the server, never compiled in. If TTS_HOST
 * is unset, text-to-speech reports itself unavailable and the audio-based
 * modes keep working.
 */
class VoiceService {
public:
    static VoiceService& getInstance();

    /**
     * Point the service at a Wyoming TTS engine.
     *
     * @param host Hostname or IP; empty disables synthesis
     * @param port Wyoming port (wyoming-piper listens on 10200)
     * @param voice Optional voice name; empty uses the engine's default
     */
    void configureTts(const std::string& host, int port, const std::string& voice);

    /** Current TTS configuration, for the status endpoint. */
    Json::Value ttsStatus() const;

    // ========================================================================
    // MODE 1 - BOOKENDS
    // ========================================================================

    /** Open a voice session and leave it open (no audio). */
    Json::Value startSession(const std::string& device_id);

    /** Close a voice session opened by startSession(). */
    Json::Value stopSession(const std::string& device_id);

    // ========================================================================
    // MODE 2 - SPOKEN
    // ========================================================================

    /** Synthesise `text` and speak it to the device. */
    Json::Value say(const std::string& device_id, const std::string& text);

    /** Speak audio already encoded (WAV natively; anything else needs ffmpeg). */
    Json::Value speakAudio(const std::string& device_id, const std::vector<uint8_t>& audio);

    /** Fetch a URL and speak it. Home Assistant TTS proxy URLs land here. */
    Json::Value speakUrl(const std::string& device_id, const std::string& url);

    /** Read a local file and speak it. */
    Json::Value speakFile(const std::string& device_id, const std::string& path);

    // ========================================================================
    // MODE 3 - LIVE RELAY
    // ========================================================================

    /**
     * Open a relay session.
     *
     * @param device_id Target device
     * @param out_session_id Receives the session handle
     * @return Result JSON (success flag plus the session id)
     */
    Json::Value openRelay(const std::string& device_id, std::string* out_session_id = nullptr);

    /**
     * Push captured audio into an open relay.
     *
     * Audio is sent as it arrives - no pacing, because the caller's capture
     * rate is the pacing. Accepts raw 16 kHz mono s16le, or a WAV chunk.
     */
    Json::Value pushRelay(const std::string& session_id, const std::vector<uint8_t>& audio,
                          bool is_raw_pcm);

    /** Close a relay session. */
    Json::Value closeRelay(const std::string& session_id);

    /** Drop relays that were opened and never closed. */
    void reapStaleRelays();

    /** Snapshot of open relays, for the status endpoint. */
    Json::Value relayStatus() const;

private:
    VoiceService() = default;
    VoiceService(const VoiceService&) = delete;
    VoiceService& operator=(const VoiceService&) = delete;

    struct Relay {
        std::string device_id;
        std::shared_ptr<LightningClient> lightning;
        std::shared_ptr<VoiceClient> voice;
        std::chrono::steady_clock::time_point opened;
        std::chrono::steady_clock::time_point last_push;
        size_t bytes = 0;
    };

    /** Build a Lightning client for a device, or nullptr if unknown. */
    std::shared_ptr<LightningClient> lightningFor(const std::string& device_id,
                                                  std::string* error);

    /** The shared path: bookend, open socket, stream, close, bookend. */
    Json::Value speakSamples(const std::string& device_id, const std::vector<int16_t>& samples);

    static Json::Value ok(const std::string& message);
    static Json::Value err(const std::string& message);

    mutable std::mutex mutex_;

    std::string tts_host_;
    int tts_port_ = 10200;
    std::string tts_voice_;

    // Sessions opened by startSession(), keyed by device.
    std::map<std::string, std::shared_ptr<VoiceClient>> sessions_;
    std::map<std::string, std::shared_ptr<LightningClient>> session_lightning_;

    // Relay sessions, keyed by handle.
    std::map<std::string, Relay> relays_;
    uint64_t relay_counter_ = 0;

    /** A relay left open this long is assumed abandoned. */
    static constexpr int RELAY_IDLE_TIMEOUT_SEC = 60;
};

}  // namespace hms_firetv
