#include "services/VoiceService.h"
#include "repositories/DeviceRepository.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

namespace hms_firetv {

VoiceService& VoiceService::getInstance() {
    static VoiceService instance;
    return instance;
}

Json::Value VoiceService::ok(const std::string& message) {
    Json::Value r;
    r["success"] = true;
    r["message"] = message;
    return r;
}

Json::Value VoiceService::err(const std::string& message) {
    Json::Value r;
    r["success"] = false;
    r["error"] = message;
    return r;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void VoiceService::configureTts(const std::string& host, int port, const std::string& voice) {
    std::lock_guard<std::mutex> lock(mutex_);
    tts_host_ = host;
    tts_port_ = port > 0 ? port : 10200;
    tts_voice_ = voice;

    if (tts_host_.empty()) {
        std::cout << "[VoiceService] no TTS configured (set TTS_HOST) - "
                     "voice by text is disabled, audio modes still work"
                  << std::endl;
    } else {
        std::cout << "[VoiceService] TTS at " << tts_host_ << ":" << tts_port_
                  << (tts_voice_.empty() ? "" : " voice=" + tts_voice_) << std::endl;
    }
}

Json::Value VoiceService::ttsStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value v;
    v["configured"] = !tts_host_.empty();
    v["host"] = tts_host_;
    v["port"] = tts_port_;
    v["voice"] = tts_voice_;
    v["protocol"] = "wyoming";
    return v;
}

// ============================================================================
// HELPERS
// ============================================================================

std::shared_ptr<LightningClient> VoiceService::lightningFor(const std::string& device_id,
                                                            std::string* error) {
    auto device = DeviceRepository::getInstance().getDeviceById(device_id);
    if (!device.has_value()) {
        if (error) *error = "unknown device: " + device_id;
        return nullptr;
    }
    return std::make_shared<LightningClient>(device->ip_address, device->api_key,
                                             device->client_token.value_or(""));
}

// ============================================================================
// MODE 1 - BOOKENDS
// ============================================================================

Json::Value VoiceService::startSession(const std::string& device_id) {
    std::string error;
    auto lightning = lightningFor(device_id, &error);
    if (!lightning) return err(error);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.count(device_id))
            return err("a voice session is already open for " + device_id);
    }

    auto start = lightning->voiceStart();
    if (!start.success)
        return err("voiceCommand?action=start failed (HTTP " +
                   std::to_string(start.status_code) + ")");

    auto device = DeviceRepository::getInstance().getDeviceById(device_id);
    auto voice = std::make_shared<VoiceClient>(device->ip_address, device->api_key,
                                               device->client_token.value_or(""));
    std::string ws_error;
    if (!voice->open(&ws_error)) {
        lightning->voiceStop();
        return err(ws_error);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[device_id] = voice;
        session_lightning_[device_id] = lightning;
    }

    auto r = ok("voice session open");
    r["device_id"] = device_id;
    return r;
}

Json::Value VoiceService::stopSession(const std::string& device_id) {
    std::shared_ptr<VoiceClient> voice;
    std::shared_ptr<LightningClient> lightning;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(device_id);
        if (it == sessions_.end()) return err("no voice session open for " + device_id);
        voice = it->second;
        lightning = session_lightning_[device_id];
        sessions_.erase(it);
        session_lightning_.erase(device_id);
    }

    voice->close();
    if (lightning) lightning->voiceStop();

    return ok("voice session closed");
}

// ============================================================================
// MODE 2 - SPOKEN
// ============================================================================

Json::Value VoiceService::speakSamples(const std::string& device_id,
                                       const std::vector<int16_t>& samples) {
    if (samples.empty()) return err("no audio to speak");

    std::string error;
    auto lightning = lightningFor(device_id, &error);
    if (!lightning) return err(error);

    auto device = DeviceRepository::getInstance().getDeviceById(device_id);

    // The TV will not answer on 8080 in standby, so nothing below works
    // unless it is awake. Wake is cheap and idempotent.
    if (!lightning->isLightningApiAvailable()) {
        std::cout << "[VoiceService] device asleep, waking before voice" << std::endl;
        lightning->wakeDevice();
        for (int i = 0; i < 5 && !lightning->isLightningApiAvailable(); ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto start = lightning->voiceStart();
    if (!start.success)
        return err("voiceCommand?action=start failed (HTTP " +
                   std::to_string(start.status_code) + ")");

    VoiceClient voice(device->ip_address, device->api_key, device->client_token.value_or(""));
    std::string ws_error;
    if (!voice.open(&ws_error)) {
        lightning->voiceStop();
        return err(ws_error);
    }

    bool streamed = voice.streamRealtime(samples, &ws_error);

    // Close the socket before the stop bookend, in that order - the app does
    // the same, and the device treats the close as end-of-utterance.
    voice.close();
    lightning->voiceStop();

    if (!streamed) return err(ws_error);

    auto r = ok("spoke to device");
    r["device_id"] = device_id;
    r["samples"] = static_cast<Json::UInt64>(samples.size());
    r["duration_sec"] = samples.size() / static_cast<double>(VoiceClient::SAMPLE_RATE);
    return r;
}

Json::Value VoiceService::say(const std::string& device_id, const std::string& text) {
    std::string host, voice_name;
    int port;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        host = tts_host_;
        port = tts_port_;
        voice_name = tts_voice_;
    }

    if (host.empty())
        return err("no TTS engine configured on this server - set TTS_HOST "
                   "(and TTS_PORT / TTS_VOICE) to a Wyoming endpoint");

    TtsClient tts(host, port, voice_name);
    std::vector<int16_t> samples;
    int rate = 0;
    std::string error;
    if (!tts.synthesize(text, samples, rate, &error)) return err(error);

    // Piper speaks at 22050; the Fire TV only accepts 16000.
    if (rate != VoiceClient::SAMPLE_RATE) {
        std::vector<float> as_float(samples.begin(), samples.end());
        std::vector<int16_t> resampled;
        VoiceClient::resampleMono(as_float, rate, resampled);
        samples.swap(resampled);
    }

    auto result = speakSamples(device_id, samples);
    if (result["success"].asBool()) result["text"] = text;
    return result;
}

Json::Value VoiceService::speakAudio(const std::string& device_id,
                                     const std::vector<uint8_t>& audio) {
    std::vector<int16_t> samples;
    std::string error;
    if (!VoiceClient::decodeToPcm(audio, samples, &error)) return err(error);
    return speakSamples(device_id, samples);
}

Json::Value VoiceService::speakUrl(const std::string& device_id, const std::string& url) {
    std::vector<uint8_t> audio;
    std::string error;
    if (!VoiceClient::fetchUrl(url, audio, &error)) return err(error);
    auto result = speakAudio(device_id, audio);
    if (result["success"].asBool()) result["url"] = url;
    return result;
}

Json::Value VoiceService::speakFile(const std::string& device_id, const std::string& path) {
    std::vector<uint8_t> audio;
    std::string error;
    if (!VoiceClient::readFile(path, audio, &error)) return err(error);
    auto result = speakAudio(device_id, audio);
    if (result["success"].asBool()) result["file"] = path;
    return result;
}

// ============================================================================
// MODE 3 - LIVE RELAY
// ============================================================================

Json::Value VoiceService::openRelay(const std::string& device_id, std::string* out_session_id) {
    reapStaleRelays();

    std::string error;
    auto lightning = lightningFor(device_id, &error);
    if (!lightning) return err(error);

    auto device = DeviceRepository::getInstance().getDeviceById(device_id);

    if (!lightning->isLightningApiAvailable()) {
        lightning->wakeDevice();
        for (int i = 0; i < 5 && !lightning->isLightningApiAvailable(); ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto start = lightning->voiceStart();
    if (!start.success)
        return err("voiceCommand?action=start failed (HTTP " +
                   std::to_string(start.status_code) + ")");

    auto voice = std::make_shared<VoiceClient>(device->ip_address, device->api_key,
                                               device->client_token.value_or(""));
    std::string ws_error;
    if (!voice->open(&ws_error)) {
        lightning->voiceStop();
        return err(ws_error);
    }

    std::string sid;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sid = device_id + "-" + std::to_string(++relay_counter_);
        Relay relay;
        relay.device_id = device_id;
        relay.lightning = lightning;
        relay.voice = voice;
        relay.opened = std::chrono::steady_clock::now();
        relay.last_push = relay.opened;
        relays_[sid] = relay;
    }

    if (out_session_id) *out_session_id = sid;

    auto r = ok("relay open");
    r["session_id"] = sid;
    r["device_id"] = device_id;
    r["format"] = "pcm_s16le";
    r["sample_rate"] = VoiceClient::SAMPLE_RATE;
    r["channels"] = VoiceClient::CHANNELS;
    return r;
}

Json::Value VoiceService::pushRelay(const std::string& session_id,
                                    const std::vector<uint8_t>& audio, bool is_raw_pcm) {
    std::shared_ptr<VoiceClient> voice;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = relays_.find(session_id);
        if (it == relays_.end()) return err("unknown relay session: " + session_id);
        voice = it->second.voice;
        it->second.last_push = std::chrono::steady_clock::now();
        it->second.bytes += audio.size();
    }

    if (audio.empty()) return ok("nothing to push");

    std::string error;
    bool sent;

    if (is_raw_pcm) {
        // Already in the device's format - straight through, no pacing.
        sent = voice->sendPcm(reinterpret_cast<const int16_t*>(audio.data()),
                              audio.size() / 2, &error);
    } else {
        std::vector<int16_t> samples;
        if (!VoiceClient::decodeToPcm(audio, samples, &error)) return err(error);
        sent = voice->sendPcm(samples.data(), samples.size(), &error);
    }

    if (!sent) return err(error);

    auto r = ok("pushed");
    r["bytes"] = static_cast<Json::UInt64>(audio.size());
    return r;
}

Json::Value VoiceService::closeRelay(const std::string& session_id) {
    Relay relay;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = relays_.find(session_id);
        if (it == relays_.end()) return err("unknown relay session: " + session_id);
        relay = it->second;
        relays_.erase(it);
    }

    relay.voice->close();
    relay.lightning->voiceStop();

    auto r = ok("relay closed");
    r["session_id"] = session_id;
    r["bytes"] = static_cast<Json::UInt64>(relay.bytes);
    return r;
}

void VoiceService::reapStaleRelays() {
    std::vector<Relay> dead;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = relays_.begin(); it != relays_.end();) {
            auto idle = std::chrono::duration_cast<std::chrono::seconds>(
                            now - it->second.last_push)
                            .count();
            if (idle > RELAY_IDLE_TIMEOUT_SEC) {
                std::cout << "[VoiceService] reaping abandoned relay " << it->first << " ("
                          << idle << "s idle)" << std::endl;
                dead.push_back(it->second);
                it = relays_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Close outside the lock - both calls do network I/O.
    for (auto& relay : dead) {
        relay.voice->close();
        relay.lightning->voiceStop();
    }
}

Json::Value VoiceService::relayStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value list(Json::arrayValue);
    auto now = std::chrono::steady_clock::now();

    for (const auto& [sid, relay] : relays_) {
        Json::Value v;
        v["session_id"] = sid;
        v["device_id"] = relay.device_id;
        v["bytes"] = static_cast<Json::UInt64>(relay.bytes);
        v["open_sec"] = static_cast<Json::Int64>(
            std::chrono::duration_cast<std::chrono::seconds>(now - relay.opened).count());
        list.append(v);
    }

    Json::Value out;
    out["relays"] = list;

    Json::Value sessions(Json::arrayValue);
    for (const auto& [device_id, _] : sessions_) sessions.append(device_id);
    out["open_sessions"] = sessions;

    return out;
}

}  // namespace hms_firetv
