#include "clients/VoiceClient.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

#include <curl/websockets.h>
#include <poll.h>
#include <unistd.h>

namespace hms_firetv {

namespace {

void fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    std::cerr << "[VoiceClient] " << msg << std::endl;
}

size_t discardBody(void*, size_t size, size_t nmemb, void*) { return size * nmemb; }

size_t appendBody(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::vector<uint8_t>*>(userp);
    size_t total = size * nmemb;
    const auto* p = static_cast<const uint8_t*>(contents);
    out->insert(out->end(), p, p + total);
    return total;
}

uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t rd16(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

int16_t clampToI16(float v) {
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;
    return static_cast<int16_t>(std::lround(v));
}

}  // namespace

// ============================================================================
// LIFECYCLE
// ============================================================================

VoiceClient::VoiceClient(const std::string& ip_address,
                         const std::string& api_key,
                         const std::string& client_token)
    : ip_address_(ip_address),
      api_key_(api_key),
      client_token_(client_token),
      ws_url_("wss://" + ip_address + ":9090/"),
      curl_(nullptr),
      open_(false) {}

VoiceClient::~VoiceClient() { close(); }

bool VoiceClient::isOpen() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return open_;
}

// ============================================================================
// WEBSOCKET
// ============================================================================

bool VoiceClient::open(std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (open_) return true;

    curl_ = curl_easy_init();
    if (!curl_) {
        fail(error, "curl_easy_init failed");
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("x-api-key: " + api_key_).c_str());
    if (!client_token_.empty())
        headers = curl_slist_append(headers, ("x-client-token: " + client_token_).c_str());

    curl_easy_setopt(curl_, CURLOPT_URL, ws_url_.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

    // CONNECT_ONLY=2 means "do the WebSocket handshake, then hand me the
    // socket" - curl_easy_perform returns once the device answers 101.
    curl_easy_setopt(curl_, CURLOPT_CONNECT_ONLY, 2L);

    // Self-signed cert, same as the Lightning REST endpoint.
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);

    // The device rejects an h2 upgrade with 400. WebSocket needs 1.1.
    curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, discardBody);
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(curl_);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status);
        fail(error, "voice socket to " + ws_url_ + " failed: " + curl_easy_strerror(rc) +
                        (status ? " (HTTP " + std::to_string(status) + ")" : ""));
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
        return false;
    }

    open_ = true;
    std::cout << "[VoiceClient] voice socket open to " << ws_url_ << std::endl;
    return true;
}

bool VoiceClient::waitWritable(int timeout_ms) {
    curl_socket_t sock = CURL_SOCKET_BAD;
    if (curl_easy_getinfo(curl_, CURLINFO_ACTIVESOCKET, &sock) != CURLE_OK ||
        sock == CURL_SOCKET_BAD)
        return false;

    struct pollfd pfd {};
    pfd.fd = static_cast<int>(sock);
    pfd.events = POLLOUT;
    return ::poll(&pfd, 1, timeout_ms) > 0;
}

bool VoiceClient::sendFrame(const uint8_t* data, size_t len, std::string* error) {
    size_t offset = 0;
    int stalls = 0;

    while (offset < len) {
        size_t sent = 0;

        // First call declares the whole frame length via `fragsize`; any
        // follow-up call continues the same frame and must set CURLWS_OFFSET.
        unsigned int flags = CURLWS_BINARY;
        curl_off_t fragsize = 0;
        if (offset == 0) {
            fragsize = static_cast<curl_off_t>(len);
        } else {
            flags |= CURLWS_OFFSET;
        }

        CURLcode rc = curl_ws_send(curl_, data + offset, len - offset, &sent, fragsize, flags);

        if (rc == CURLE_AGAIN) {
            if (++stalls > 100) {  // ~5s of a socket that will not drain
                fail(error, "voice socket stalled");
                return false;
            }
            waitWritable(50);
            offset += sent;
            continue;
        }
        if (rc != CURLE_OK) {
            fail(error, std::string("voice frame send failed: ") + curl_easy_strerror(rc));
            return false;
        }

        stalls = 0;
        offset += sent;
        if (sent == 0) break;  // nothing moving and no error; give up on this frame
    }

    return offset >= len;
}

bool VoiceClient::sendPcm(const int16_t* samples, size_t sample_count, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!open_) {
        fail(error, "voice socket is not open");
        return false;
    }

    const auto* bytes = reinterpret_cast<const uint8_t*>(samples);
    size_t total = sample_count * sizeof(int16_t);

    for (size_t off = 0; off < total; off += CHUNK_BYTES) {
        size_t n = std::min(CHUNK_BYTES, total - off);
        if (!sendFrame(bytes + off, n, error)) return false;
    }
    return true;
}

bool VoiceClient::streamRealtime(const std::vector<int16_t>& samples, std::string* error) {
    if (samples.empty()) {
        fail(error, "no audio to stream");
        return false;
    }

    using clock = std::chrono::steady_clock;
    auto start = clock::now();
    size_t sent_samples = 0;

    for (size_t off = 0; off < samples.size(); off += CHUNK_SAMPLES) {
        size_t n = std::min(CHUNK_SAMPLES, samples.size() - off);
        if (!sendPcm(samples.data() + off, n, error)) return false;
        sent_samples += n;

        // Hold the send rate to 1x. The device thinks this is a live mic; a
        // burst arrives faster than Alexa will transcribe it.
        auto target = start + std::chrono::microseconds(
                                  (sent_samples * 1000000ULL) / SAMPLE_RATE);
        std::this_thread::sleep_until(target);
    }

    std::cout << "[VoiceClient] streamed " << samples.size() << " samples ("
              << (samples.size() / static_cast<double>(SAMPLE_RATE)) << "s)" << std::endl;
    return true;
}

void VoiceClient::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (curl_) {
        if (open_) {
            size_t sent = 0;
            curl_ws_send(curl_, "", 0, &sent, 0, CURLWS_CLOSE);
        }
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
    open_ = false;
}

// ============================================================================
// AUDIO CONVERSION
// ============================================================================

bool VoiceClient::readFile(const std::string& path, std::vector<uint8_t>& out,
                           std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fail(error, "cannot open " + path);
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    if (out.empty()) {
        fail(error, "empty file: " + path);
        return false;
    }
    return true;
}

bool VoiceClient::fetchUrl(const std::string& url, std::vector<uint8_t>& out,
                           std::string* error) {
    CURL* c = curl_easy_init();
    if (!c) {
        fail(error, "curl_easy_init failed");
        return false;
    }

    out.clear();
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, appendBody);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK) {
        fail(error, std::string("fetch failed: ") + curl_easy_strerror(rc));
        return false;
    }
    if (status >= 400) {
        fail(error, "fetch returned HTTP " + std::to_string(status));
        return false;
    }
    if (out.empty()) {
        fail(error, "fetch returned no data");
        return false;
    }
    return true;
}

void VoiceClient::resampleMono(const std::vector<float>& in, int in_rate,
                               std::vector<int16_t>& out) {
    if (in.empty()) return;

    if (in_rate == SAMPLE_RATE) {
        out.reserve(out.size() + in.size());
        for (float v : in) out.push_back(clampToI16(v));
        return;
    }

    // Linear interpolation. Speech into a recogniser at 16 kHz does not
    // justify a windowed-sinc; Piper's 22050 -> 16000 is a mild ratio.
    double ratio = static_cast<double>(in_rate) / SAMPLE_RATE;
    size_t n_out = static_cast<size_t>(in.size() / ratio);
    out.reserve(out.size() + n_out);

    for (size_t i = 0; i < n_out; ++i) {
        double src = i * ratio;
        size_t i0 = static_cast<size_t>(src);
        size_t i1 = std::min(i0 + 1, in.size() - 1);
        double frac = src - i0;
        out.push_back(clampToI16(static_cast<float>(in[i0] * (1.0 - frac) + in[i1] * frac)));
    }
}

bool VoiceClient::decodeWav(const std::vector<uint8_t>& input, std::vector<int16_t>& out,
                            std::string* error) {
    if (input.size() < 44 || std::memcmp(input.data(), "RIFF", 4) != 0 ||
        std::memcmp(input.data() + 8, "WAVE", 4) != 0) {
        fail(error, "not a RIFF/WAVE file");
        return false;
    }

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t data_len = 0;

    // Walk the chunk list rather than assuming a canonical 44-byte header -
    // real encoders insert LIST/fact chunks before `data`.
    size_t pos = 12;
    while (pos + 8 <= input.size()) {
        const uint8_t* hdr = input.data() + pos;
        uint32_t chunk_len = rd32(hdr + 4);
        size_t body = pos + 8;
        if (body > input.size()) break;
        size_t avail = std::min(static_cast<size_t>(chunk_len), input.size() - body);

        if (std::memcmp(hdr, "fmt ", 4) == 0 && avail >= 16) {
            const uint8_t* f = input.data() + body;
            format = rd16(f);
            channels = rd16(f + 2);
            rate = rd32(f + 4);
            bits = rd16(f + 14);
            if (format == 0xFFFE && avail >= 26) {
                // WAVE_FORMAT_EXTENSIBLE - the real tag is in the GUID's head.
                format = rd16(f + 24);
            }
        } else if (std::memcmp(hdr, "data", 4) == 0) {
            data = input.data() + body;
            data_len = avail;
            break;
        }

        pos = body + chunk_len + (chunk_len & 1);  // chunks are word aligned
    }

    if (!data || !data_len || !channels || !rate) {
        fail(error, "WAV file has no usable fmt/data chunk");
        return false;
    }

    // Decode to float mono first, then resample once.
    std::vector<float> mono;
    size_t bytes_per_sample = bits / 8;
    if (bytes_per_sample == 0) {
        fail(error, "WAV declares 0 bits per sample");
        return false;
    }
    size_t frame_bytes = bytes_per_sample * channels;
    size_t frames = data_len / frame_bytes;
    mono.reserve(frames);

    for (size_t i = 0; i < frames; ++i) {
        const uint8_t* f = data + i * frame_bytes;
        float acc = 0.0f;
        for (uint16_t c = 0; c < channels; ++c) {
            const uint8_t* s = f + c * bytes_per_sample;
            float v = 0.0f;
            if (format == 3 && bits == 32) {
                float fv;
                std::memcpy(&fv, s, 4);
                v = fv * 32768.0f;
            } else if (bits == 16) {
                v = static_cast<float>(static_cast<int16_t>(rd16(s)));
            } else if (bits == 8) {
                v = (static_cast<float>(s[0]) - 128.0f) * 256.0f;  // 8-bit WAV is unsigned
            } else if (bits == 24) {
                int32_t iv = (static_cast<int32_t>(s[0]) << 8) |
                             (static_cast<int32_t>(s[1]) << 16) |
                             (static_cast<int32_t>(s[2]) << 24);
                v = static_cast<float>(iv >> 8) / 256.0f;
            } else if (bits == 32) {
                v = static_cast<float>(static_cast<int32_t>(rd32(s))) / 65536.0f;
            } else {
                fail(error, "unsupported WAV sample width: " + std::to_string(bits));
                return false;
            }
            acc += v;
        }
        mono.push_back(acc / channels);
    }

    resampleMono(mono, static_cast<int>(rate), out);
    return !out.empty();
}

bool VoiceClient::ffmpegAvailable() {
    static const bool available = [] {
        return std::system("command -v ffmpeg >/dev/null 2>&1") == 0;
    }();
    return available;
}

bool VoiceClient::decodeWithFfmpeg(const std::vector<uint8_t>& input,
                                   std::vector<int16_t>& out, std::string* error) {
    if (!ffmpegAvailable()) {
        fail(error,
             "audio is not WAV and no ffmpeg is installed on this host - "
             "supply WAV, or use text (Piper) instead");
        return false;
    }

    // popen is one-directional, so the input goes via a temp file and only
    // the decoded output comes back over the pipe.
    char in_tmpl[] = "/tmp/firetv-voice-inXXXXXX";
    int in_fd = ::mkstemp(in_tmpl);
    if (in_fd < 0) {
        fail(error, "cannot create temp file");
        return false;
    }
    if (::write(in_fd, input.data(), input.size()) < 0) {
        ::close(in_fd);
        ::unlink(in_tmpl);
        fail(error, "cannot write temp file");
        return false;
    }
    ::close(in_fd);

    std::string cmd = std::string("ffmpeg -hide_banner -loglevel error -i ") + in_tmpl +
                      " -f s16le -acodec pcm_s16le -ar 16000 -ac 1 pipe:1";
    FILE* p = ::popen(cmd.c_str(), "r");
    if (!p) {
        ::unlink(in_tmpl);
        fail(error, "cannot run ffmpeg");
        return false;
    }

    std::vector<uint8_t> raw;
    uint8_t buf[8192];
    size_t n;
    while ((n = ::fread(buf, 1, sizeof(buf), p)) > 0) raw.insert(raw.end(), buf, buf + n);
    int rc = ::pclose(p);
    ::unlink(in_tmpl);

    if (rc != 0 || raw.empty()) {
        fail(error, "ffmpeg could not decode the audio");
        return false;
    }

    out.resize(raw.size() / 2);
    std::memcpy(out.data(), raw.data(), out.size() * 2);
    return true;
}

bool VoiceClient::decodeToPcm(const std::vector<uint8_t>& input, std::vector<int16_t>& out,
                              std::string* error) {
    out.clear();
    if (input.size() < 12) {
        fail(error, "audio is too short to identify");
        return false;
    }

    if (std::memcmp(input.data(), "RIFF", 4) == 0)
        return decodeWav(input, out, error);

    return decodeWithFfmpeg(input, out, error);
}

}  // namespace hms_firetv
