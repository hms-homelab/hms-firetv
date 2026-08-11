#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hms_firetv {

/**
 * TtsClient - Wyoming protocol text-to-speech client
 *
 * Talks to the wyoming-piper container on the voice box (10200/tcp), the same
 * TTS engine Home Assistant uses. This is what turns "play Stranger Things"
 * into audio we can push down the Fire TV voice socket.
 *
 * WYOMING PROTOCOL
 * ================
 * Newline-delimited JSON events over a plain TCP socket. Each event is one
 * JSON line, optionally followed by binary payload:
 *
 *   {"type":"...","data":{...},"payload_length":N}\n
 *   <N bytes of payload>
 *
 * `data` may instead arrive out-of-line, as `data_length` bytes of JSON
 * immediately after the header line and before the payload. Both spellings
 * are handled, because which one you get depends on the sending library.
 *
 * A synthesis exchange is:
 *   -> {"type":"synthesize","data":{"text":"..."}}
 *   <- {"type":"audio-start","data":{"rate":22050,"width":2,"channels":1}}
 *   <- {"type":"audio-chunk", ...} + raw PCM payload   (repeated)
 *   <- {"type":"audio-stop"}
 *
 * Piper emits 22050 Hz, 16-bit, mono raw PCM - verified against the running
 * container. It is resampled to the Fire TV's 16 kHz by VoiceClient.
 */
class TtsClient {
public:
    TtsClient(const std::string& host, int port = 10200, const std::string& voice = "");

    /**
     * Synthesise text to PCM.
     *
     * @param text What to say
     * @param out Raw signed 16-bit mono samples at `out_rate`
     * @param out_rate Sample rate the engine produced (Piper: 22050)
     * @param error Filled in on failure
     * @return true on success
     */
    bool synthesize(const std::string& text,
                    std::vector<int16_t>& out,
                    int& out_rate,
                    std::string* error = nullptr);

    /** True if a host was configured. */
    bool configured() const { return !host_.empty(); }

    const std::string& host() const { return host_; }
    int port() const { return port_; }

private:
    std::string host_;
    int port_;
    std::string voice_;

    /** Connect with a timeout; returns a socket fd or -1. */
    int connectSocket(std::string* error);

    static constexpr int CONNECT_TIMEOUT_SEC = 5;
    static constexpr int IO_TIMEOUT_SEC = 30;
};

} // namespace hms_firetv
