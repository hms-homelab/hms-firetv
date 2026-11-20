#include "services/TextInput.h"
#include "repositories/DeviceRepository.h"

#include <iostream>

namespace hms_firetv {

std::vector<std::string> TextInput::order(const std::optional<std::string>& learned) {
    if (learned.has_value() && *learned == "text") return {"text", "keyboard"};
    return {"keyboard", "text"};
}

bool TextInput::worthTryingOther(const CommandResult& result) {
    if (result.success) return false;
    if (result.status_code <= 0) return false;  // no answer at all
    return !LightningClient::isAuthRejection(result.status_code);
}

TextInput::Outcome TextInput::send(const std::string& device_id, const Sender& sender) {
    auto& repo = DeviceRepository::getInstance();
    auto device = repo.getDeviceById(device_id);
    std::optional<std::string> learned;
    if (device.has_value()) learned = device->text_endpoint;

    Outcome out;
    for (const auto& endpoint : order(learned)) {
        out.result = sender(endpoint);
        out.endpoint = endpoint;

        if (out.result.success) {
            if (learned != endpoint) {
                repo.setTextEndpoint(device_id, endpoint);
                std::cout << "[TextInput] " << device_id << " takes text via /" << endpoint
                          << "; remembered" << std::endl;
            }
            return out;
        }
        if (!worthTryingOther(out.result)) return out;

        std::cout << "[TextInput] " << device_id << " answered HTTP " << out.result.status_code
                  << " on /" << endpoint << ", trying the other text route" << std::endl;
    }
    return out;
}

TextInput::Outcome TextInput::send(const std::string& device_id, LightningClient& client,
                                   const std::string& text) {
    return send(device_id, [&client, &text](const std::string& endpoint) {
        return client.sendTextTo(endpoint, text);
    });
}

}  // namespace hms_firetv
