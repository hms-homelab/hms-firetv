#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <curl/curl.h>

namespace hms_firetv {

/**
 * VoiceClient - Alexa voice channel for Fire TV
 *
 * PROTOCOL
 * ========
 * Voice is NOT part of the Lightning REST API. The official Fire TV remote app
 * opens a second, separate channel for it:
 *
 *   1. POST https://{ip}:8080/v1/FireTV/voiceCommand?action=start   (LightningClient)
 *   2. open  wss://{ip}:9090/                                       (this class)
 *   3. send  raw audio as binary WebSocket frames                   (this class)
 *   4. close the WebSocket                                          (this class)
 *   5. POST https://{ip}:8080/v1/FireTV/voiceCommand?action=stop    (LightningClient)
 *
 * The audio format was read out of the app itself
 * (expo.modules.feniksnetworking.voice.VoiceSearch, decompiled from
 * classes7.dex): RECORDING_RATE 16000, CHANNEL_IN_MONO, ENCODING_PCM_16BIT.
 * VoiceStreamTask reads straight from an AudioRecord into a byte[] and hands
 * each buffer to DeviceConnection.sendVoiceData(), which does
 * webSocket.send(ByteString.of(...)).
 *
 * So the wire format is RAW PCM: 16 kHz, mono, signed 16-bit little-endian.
 * No codec, no container, no length prefix, no handshake of its own. One
 * binary frame per buffer; the chunk size carries no meaning.
 *
 * The socket takes no authentication - it answers 101 to an unpaired client.
 * The Lightning headers are sent anyway, because the app sends them and there
 * is no reason to look different from the app.
 *
 * PACING
 * ======
 * The device is listening to what it believes is a live microphone, so audio
 * is streamed at 1x wall-clock rather than dumped. Sending a 5 second clip in
 * one burst gives Alexa 5 seconds of speech in ~50 ms and it does not
 * transcribe. streamRealtime() handles the pacing.
 *
 * THREAD SAFETY
 * =============
 * One instance per voice session. Guarded internally so a live relay can push
 * audio from a request thread while another thread closes the session.
 */
class VoiceClient {
public:
    /** Target format constants - these are the device's, not ours to choose. */
    static constexpr int SAMPLE_RATE = 16000;
    static constexpr int CHANNELS = 1;
    static constexpr int BITS_PER_SAMPLE = 16;

    /** 100 ms of audio. Only a pacing quantum; the device does not care. */
    static constexpr size_t CHUNK_SAMPLES = SAMPLE_RATE / 10;
    static constexpr size_t CHUNK_BYTES = CHUNK_SAMPLES * 2;

    VoiceClient(const std::string& ip_address,
                const std::string& api_key = "0987654321",
                const std::string& client_token = "");
    ~VoiceClient();

    VoiceClient(const VoiceClient&) = delete;
    VoiceClient& operator=(const VoiceClient&) = delete;

    /**
     * Open the WebSocket to wss://{ip}:9090/
     *
     * Must be HTTP/1.1 - the device answers 400 to an h2 upgrade attempt.
     * The certificate is self-signed, so verification is off as it is
     * everywhere else in this service.
     *
     * @param error Filled in on failure
     * @return true if the device answered 101
     */
    bool open(std::string* error = nullptr);

    /**
     * Send one buffer of PCM as binary frames.
     *
     * Input must already be 16 kHz mono s16le. Larger buffers are split into
     * CHUNK_BYTES frames. This does NOT pace - use it for a live relay, where
     * the caller's own capture rate is the pacing.
     */
    bool sendPcm(const int16_t* samples, size_t sample_count, std::string* error = nullptr);

    /**
     * Send PCM paced at 1x wall-clock, for pre-recorded audio.
     */
    bool streamRealtime(const std::vector<int16_t>& samples, std::string* error = nullptr);

    /** Close the WebSocket (sends a close frame). Safe to call twice. */
    void close();

    bool isOpen() const;

    // ========================================================================
    // AUDIO CONVERSION
    // ========================================================================

    /**
     * Convert an encoded audio buffer to 16 kHz mono s16le.
     *
     * Handles RIFF/WAVE natively (PCM 8/16/24/32-bit and IEEE float 32-bit,
     * any sample rate, any channel count) with an internal downmix and linear
     * resample - no external dependency, which matters because the hub has no
     * ffmpeg installed.
     *
     * Anything that is not a WAV (mp3, aac, a Home Assistant tts_proxy URL
     * that returns mp3) is handed to ffmpeg if one is on PATH, and fails with
     * a clear message if not.
     *
     * @param input Encoded bytes
     * @param out Decoded 16 kHz mono samples
     * @param error Filled in on failure
     */
    static bool decodeToPcm(const std::vector<uint8_t>& input,
                            std::vector<int16_t>& out,
                            std::string* error = nullptr);

    /** Read a local file into memory. */
    static bool readFile(const std::string& path,
                         std::vector<uint8_t>& out,
                         std::string* error = nullptr);

    /** Fetch a URL into memory (used for Home Assistant TTS proxy URLs). */
    static bool fetchUrl(const std::string& url,
                         std::vector<uint8_t>& out,
                         std::string* error = nullptr);

    /** True if an ffmpeg binary is callable. Cached after the first probe. */
    static bool ffmpegAvailable();

    /**
     * Resample mono audio to SAMPLE_RATE and append it to `out`.
     *
     * Public because TTS output arrives already decoded (Piper hands back
     * 22050 Hz PCM over Wyoming) and needs the same rate conversion without
     * going near a container format.
     */
    static void resampleMono(const std::vector<float>& in, int in_rate,
                             std::vector<int16_t>& out);

private:
    std::string ip_address_;
    std::string api_key_;
    std::string client_token_;
    std::string ws_url_;

    CURL* curl_;
    bool open_;
    mutable std::mutex mutex_;

    /** Send one complete binary frame, retrying while the socket blocks. */
    bool sendFrame(const uint8_t* data, size_t len, std::string* error);

    /** Wait until the underlying socket is writable, or time out. */
    bool waitWritable(int timeout_ms);

    // -- audio helpers --
    static bool decodeWav(const std::vector<uint8_t>& input,
                          std::vector<int16_t>& out,
                          std::string* error);
    static bool decodeWithFfmpeg(const std::vector<uint8_t>& input,
                                 std::vector<int16_t>& out,
                                 std::string* error);
};

} // namespace hms_firetv
