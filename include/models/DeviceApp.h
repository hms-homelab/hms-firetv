#pragma once
#include <string>

namespace hms_firetv {

struct DeviceApp {
    int id = 0;
    std::string device_id;
    std::string package_name;
    std::string app_name;
    std::string icon_url;
    bool is_favorite = false;
    int sort_order = 0;
    std::string created_at;
    std::string updated_at;
};

} // namespace hms_firetv
