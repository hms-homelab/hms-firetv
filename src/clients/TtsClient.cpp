#include "clients/TtsClient.h"

#include <json/json.h>

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace hms_firetv {

namespace {

/** Read exactly n bytes, or fail. */
bool readExact(int fd, void* buf, size_t n) {
    auto* p = static_cast<uint8_t*>(buf);
    size_t got = 0;
    while (got < n) {
        ssize_t r = ::recv(fd, p + got, n - got, 0);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
    }
    return true;
}

/**
 * Read one newline-terminated header line.
 *
 * Byte at a time. The header is short and the payload that follows it is
 * length-prefixed, so a buffered reader would have to hand back its overread
 * anyway - not worth the bookkeeping.
 */
bool readLine(int fd, std::string& line) {
    line.clear();
    char c;
    while (true) {
        ssize_t r = ::recv(fd, &c, 1, 0);
        if (r <= 0) return false;
        if (c == '\n') return true;
        line.push_back(c);
        if (line.size() > 64 * 1024) return false;  // runaway guard
    }
}

bool writeAll(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t w = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (w <= 0) return false;
        sent += static_cast<size_t>(w);
    }
    return true;
}

void fail(std::string* error, const std::string& msg) {
    if (error) *error = msg;
    std::cerr << "[TtsClient] " << msg << std::endl;
}

}  // namespace

TtsClient::TtsClient(const std::string& host, int port, const std::string& voice)
    : host_(host), port_(port), voice_(voice) {}

int TtsClient::connectSocket(std::string* error) {
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port_);
    int rc = ::getaddrinfo(host_.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0 || !res) {
        fail(error, "cannot resolve TTS host " + host_ + ": " + gai_strerror(rc));
        return -1;
    }

    int fd = -1;
    for (auto* ai = res; ai; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        struct timeval tv {};
        tv.tv_sec = CONNECT_TIMEOUT_SEC;
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        tv.tv_sec = IO_TIMEOUT_SEC;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(res);

    if (fd < 0) {
        fail(error, "cannot connect to TTS at " + host_ + ":" + port_str);
        return -1;
    }
    return fd;
}

bool TtsClient::synthesize(const std::string& text,
                           std::vector<int16_t>& out,
                           int& out_rate,
                           std::string* error) {
    out.clear();
    out_rate = 0;

    if (!configured()) {
        fail(error, "no TTS host configured (set TTS_HOST)");
        return false;
    }
    if (text.empty()) {
        fail(error, "nothing to synthesise");
        return false;
    }

    int fd = connectSocket(error);
    if (fd < 0) return false;

    // -- request --
    Json::Value req;
    req["type"] = "synthesize";
    Json::Value data;
    data["text"] = text;
    if (!voice_.empty()) {
        Json::Value voice;
        voice["name"] = voice_;
        data["voice"] = voice;
    }
    req["data"] = data;

    Json::StreamWriterBuilder w;
    w["indentation"] = "";  // must stay on one line - the protocol is line based
    if (!writeAll(fd, Json::writeString(w, req) + "\n")) {
        fail(error, "failed to send synthesize request");
        ::close(fd);
        return false;
    }

    // -- response --
    int width = 2, channels = 1;
    bool got_stop = false;

    while (true) {
        std::string line;
        if (!readLine(fd, line)) break;  // clean EOF or timeout
        if (line.empty()) continue;

        Json::Value ev;
        Json::CharReaderBuilder rb;
        std::string errs;
        std::istringstream ss(line);
        if (!Json::parseFromStream(rb, ss, &ev, &errs)) {
            fail(error, "bad event from TTS: " + errs);
            ::close(fd);
            return false;
        }

        // `data` may arrive out-of-line, as its own length-prefixed blob.
        if (ev.isMember("data_length") && ev["data_length"].asInt64() > 0) {
            std::string blob(static_cast<size_t>(ev["data_length"].asInt64()), '\0');
            if (!readExact(fd, blob.data(), blob.size())) {
                fail(error, "truncated event data from TTS");
                ::close(fd);
                return false;
            }
            Json::Value parsed;
            std::istringstream ds(blob);
            if (Json::parseFromStream(rb, ds, &parsed, &errs)) ev["data"] = parsed;
        }

        std::vector<uint8_t> payload;
        if (ev.isMember("payload_length") && ev["payload_length"].asInt64() > 0) {
            payload.resize(static_cast<size_t>(ev["payload_length"].asInt64()));
            if (!readExact(fd, payload.data(), payload.size())) {
                fail(error, "truncated audio payload from TTS");
                ::close(fd);
                return false;
            }
        }

        std::string type = ev.get("type", "").asString();
        const Json::Value& d = ev["data"];

        if (type == "audio-start" || (type == "audio-chunk" && out_rate == 0)) {
            if (d.isObject()) {
                if (d.isMember("rate")) out_rate = d["rate"].asInt();
                if (d.isMember("width")) width = d["width"].asInt();
                if (d.isMember("channels")) channels = d["channels"].asInt();
            }
        }

        if (type == "audio-chunk" && !payload.empty()) {
            if (width != 2) {
                fail(error, "TTS returned " + std::to_string(width * 8) +
                                "-bit audio; only 16-bit is handled");
                ::close(fd);
                return false;
            }
            const auto* s = reinterpret_cast<const int16_t*>(payload.data());
            size_t n = payload.size() / 2;

            if (channels <= 1) {
                out.insert(out.end(), s, s + n);
            } else {
                // Downmix. Piper is mono, but a different Wyoming engine
                // could be swapped in behind the same port.
                for (size_t i = 0; i + channels <= n; i += channels) {
                    int32_t acc = 0;
                    for (int c = 0; c < channels; ++c) acc += s[i + c];
                    out.push_back(static_cast<int16_t>(acc / channels));
                }
            }
        } else if (type == "audio-stop") {
            got_stop = true;
            break;
        } else if (type == "error") {
            std::string msg = d.isObject() ? d.get("text", "unknown").asString() : "unknown";
            fail(error, "TTS reported an error: " + msg);
            ::close(fd);
            return false;
        }
    }

    ::close(fd);

    if (out.empty()) {
        fail(error, got_stop ? "TTS produced no audio" : "TTS connection closed early");
        return false;
    }
    if (out_rate <= 0) out_rate = 22050;  // Piper's default, if it never said

    std::cout << "[TtsClient] synthesised " << out.size() << " samples @ " << out_rate
              << " Hz (" << (out.size() / static_cast<double>(out_rate)) << "s)" << std::endl;
    return true;
}

}  // namespace hms_firetv
