#include "database/PostgresDatabase.h"
#include "services/DatabaseService.h"
#include <pqxx/pqxx>
#include <iostream>
#include <sstream>

namespace hms_firetv {

PostgresDatabase::PostgresDatabase(const std::string& host, int port, const std::string& name,
                                   const std::string& user, const std::string& password)
    : host_(host), port_(port), name_(name), user_(user), password_(password) {}

bool PostgresDatabase::connect() {
    try {
        DatabaseService::getInstance().initialize(host_, port_, name_, user_, password_);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[PostgresDB] connect failed: " << e.what() << std::endl;
        return false;
    }
}

void PostgresDatabase::disconnect() {}

bool PostgresDatabase::isConnected() const {
    return DatabaseService::getInstance().isConnected();
}

// ── Parsing ───────────────────────────────────────────────────────────────────

static std::chrono::system_clock::time_point pgTs(const std::string& s) {
    if (s.empty()) return std::chrono::system_clock::now();
    struct tm tm = {};
    strptime(s.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
    tm.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(mktime(&tm));
}

Device PostgresDatabase::parseDevice(const pqxx::row& row) {
    Device d;
    d.id         = row["id"].as<int>();
    d.device_id  = row["device_id"].as<std::string>();
    d.name       = row["name"].as<std::string>();
    d.ip_address = row["ip_address"].as<std::string>();
    d.api_key    = row["api_key"].as<std::string>();
    d.status     = row["status"].as<std::string>();
    d.adb_enabled = row["adb_enabled"].as<bool>();
    if (!row["client_token"].is_null()) d.client_token = row["client_token"].as<std::string>();
    if (!row["pin_code"].is_null()) d.pin_code = row["pin_code"].as<std::string>();
    if (!row["created_at"].is_null()) d.created_at = pgTs(row["created_at"].as<std::string>());
    if (!row["updated_at"].is_null()) d.updated_at = pgTs(row["updated_at"].as<std::string>());
    return d;
}

DeviceApp PostgresDatabase::parseApp(const pqxx::row& row) {
    DeviceApp a;
    a.id           = row["id"].as<int>();
    a.device_id    = row["device_id"].as<std::string>();
    a.package_name = row["package_name"].as<std::string>();
    a.app_name     = row["app_name"].as<std::string>();
    a.icon_url     = row["icon_url"].is_null() ? "" : row["icon_url"].as<std::string>();
    a.is_favorite  = row["is_favorite"].as<bool>();
    a.sort_order   = 0;
    if (!row["created_at"].is_null()) a.created_at = row["created_at"].as<std::string>();
    if (!row["updated_at"].is_null()) a.updated_at = row["updated_at"].as<std::string>();
    return a;
}

// ── Devices ───────────────────────────────────────────────────────────────────

std::optional<Device> PostgresDatabase::createDevice(const Device& device) {
    auto r = DatabaseService::getInstance().executeQueryParams(
        "INSERT INTO fire_tv_devices (device_id,name,ip_address,api_key,status,adb_enabled,"
        "created_at,updated_at) VALUES ($1,$2,$3,$4,$5,$6,NOW(),NOW()) RETURNING id",
        {device.device_id, device.name, device.ip_address, device.api_key,
         device.status, device.adb_enabled ? "true" : "false"});
    if (r.empty()) return std::nullopt;
    return getDeviceById(device.device_id);
}

std::optional<Device> PostgresDatabase::getDeviceById(const std::string& device_id) {
    auto r = DatabaseService::getInstance().executeQueryParams(
        "SELECT * FROM fire_tv_devices WHERE device_id=$1", {device_id});
    if (r.empty()) return std::nullopt;
    return parseDevice(r[0]);
}

std::vector<Device> PostgresDatabase::getAllDevices() {
    auto r = DatabaseService::getInstance().executeQuery(
        "SELECT * FROM fire_tv_devices ORDER BY created_at DESC");
    std::vector<Device> out;
    for (const auto& row : r) out.push_back(parseDevice(row));
    return out;
}

std::vector<Device> PostgresDatabase::getDevicesByStatus(const std::string& status) {
    auto r = DatabaseService::getInstance().executeQueryParams(
        "SELECT * FROM fire_tv_devices WHERE status=$1 ORDER BY created_at DESC", {status});
    std::vector<Device> out;
    for (const auto& row : r) out.push_back(parseDevice(row));
    return out;
}

bool PostgresDatabase::updateDevice(const Device& device) {
    return DatabaseService::getInstance().executeQueryParams(
        "UPDATE fire_tv_devices SET name=$1,ip_address=$2,status=$3,adb_enabled=$4,"
        "updated_at=NOW() WHERE device_id=$5",
        {device.name, device.ip_address, device.status,
         device.adb_enabled ? "true" : "false", device.device_id}).empty()
        ? DatabaseService::getInstance().isConnected() : true;
}

bool PostgresDatabase::deleteDevice(const std::string& device_id) {
    return DatabaseService::getInstance().executeCommand(
        "DELETE FROM fire_tv_devices WHERE device_id='" + device_id + "'");
}

bool PostgresDatabase::deviceExists(const std::string& device_id) {
    auto r = DatabaseService::getInstance().executeQueryParams(
        "SELECT COUNT(*) FROM fire_tv_devices WHERE device_id=$1", {device_id});
    return !r.empty() && r[0][0].as<int>() > 0;
}

bool PostgresDatabase::updateLastSeen(const std::string& device_id, const std::string& status) {
    return DatabaseService::getInstance().executeQueryParams(
        "UPDATE fire_tv_devices SET last_seen_at=NOW(),status=$1,updated_at=NOW() "
        "WHERE device_id=$2", {status, device_id}).empty()
        ? DatabaseService::getInstance().isConnected() : true;
}

bool PostgresDatabase::setPairingPin(const std::string& device_id, const std::string& pin_code,
                                     int expires_secs) {
    return DatabaseService::getInstance().executeQueryParams(
        "UPDATE fire_tv_devices SET pin_code=$1,"
        "pin_expires_at=NOW()+INTERVAL '1 second'*$2,status='pairing',updated_at=NOW() "
        "WHERE device_id=$3",
        {pin_code, std::to_string(expires_secs), device_id}).empty()
        ? DatabaseService::getInstance().isConnected() : true;
}

bool PostgresDatabase::verifyPinAndSetToken(const std::string& device_id,
                                             const std::string& pin_code,
                                             const std::string& client_token) {
    auto r = DatabaseService::getInstance().executeQueryParams(
        "SELECT pin_code, pin_expires_at FROM fire_tv_devices WHERE device_id=$1", {device_id});
    if (r.empty() || r[0]["pin_code"].is_null()) return false;
    if (r[0]["pin_code"].as<std::string>() != pin_code) return false;
    DatabaseService::getInstance().executeQueryParams(
        "UPDATE fire_tv_devices SET client_token=$1,pin_code=NULL,pin_expires_at=NULL,"
        "status='online',updated_at=NOW() WHERE device_id=$2",
        {client_token, device_id});
    return true;
}

bool PostgresDatabase::completePairing(const std::string& device_id,
                                       const std::string& client_token) {
    return DatabaseService::getInstance().executeQueryParams(
        "UPDATE fire_tv_devices SET client_token=$1,pin_code=NULL,pin_expires_at=NULL,"
        "status='online',updated_at=NOW() WHERE device_id=$2", {client_token, device_id}).empty()
        ? DatabaseService::getInstance().isConnected() : true;
}

bool PostgresDatabase::clearPairing(const std::string& device_id) {
    return DatabaseService::getInstance().executeQueryParams(
        "UPDATE fire_tv_devices SET client_token=NULL,pin_code=NULL,pin_expires_at=NULL,"
        "status='offline',updated_at=NOW() WHERE device_id=$1", {device_id}).empty()
        ? DatabaseService::getInstance().isConnected() : true;
}

// ── Apps ──────────────────────────────────────────────────────────────────────

std::vector<DeviceApp> PostgresDatabase::getAppsForDevice(const std::string& device_id) {
    auto r = DatabaseService::getInstance().executeQueryParams(
        "SELECT id,device_id,package_name,app_name,icon_url,is_favorite,sort_order,"
        "created_at::text,updated_at::text FROM device_apps WHERE device_id=$1 "
        "ORDER BY is_favorite DESC, app_name", {device_id});
    std::vector<DeviceApp> out;
    for (const auto& row : r) out.push_back(parseApp(row));
    return out;
}

std::optional<DeviceApp> PostgresDatabase::getApp(const std::string& device_id,
                                                    const std::string& package_name) {
    auto r = DatabaseService::getInstance().executeQueryParams(
        "SELECT id,device_id,package_name,app_name,icon_url,is_favorite,sort_order,"
        "created_at::text,updated_at::text FROM device_apps "
        "WHERE device_id=$1 AND package_name=$2", {device_id, package_name});
    if (r.empty()) return std::nullopt;
    return parseApp(r[0]);
}

bool PostgresDatabase::addApp(const DeviceApp& app) {
    DatabaseService::getInstance().executeQueryParams(
        "INSERT INTO device_apps (device_id,package_name,app_name,icon_url,is_favorite) "
        "VALUES ($1,$2,$3,$4,$5) ON CONFLICT (device_id,package_name) DO NOTHING",
        {app.device_id, app.package_name, app.app_name, app.icon_url,
         app.is_favorite ? "true" : "false"});
    return true;
}

bool PostgresDatabase::updateApp(const DeviceApp& app) {
    DatabaseService::getInstance().executeQueryParams(
        "UPDATE device_apps SET app_name=$1,icon_url=$2,is_favorite=$3 "
        "WHERE device_id=$4 AND package_name=$5",
        {app.app_name, app.icon_url, app.is_favorite ? "true" : "false",
         app.device_id, app.package_name});
    return true;
}

bool PostgresDatabase::deleteApp(const std::string& device_id, const std::string& package_name) {
    DatabaseService::getInstance().executeQueryParams(
        "DELETE FROM device_apps WHERE device_id=$1 AND package_name=$2",
        {device_id, package_name});
    return true;
}

bool PostgresDatabase::deleteAllApps(const std::string& device_id) {
    DatabaseService::getInstance().executeQueryParams(
        "DELETE FROM device_apps WHERE device_id=$1", {device_id});
    return true;
}

bool PostgresDatabase::setFavorite(const std::string& device_id, const std::string& package_name,
                                    bool is_favorite) {
    DatabaseService::getInstance().executeQueryParams(
        "UPDATE device_apps SET is_favorite=$1 WHERE device_id=$2 AND package_name=$3",
        {is_favorite ? "true" : "false", device_id, package_name});
    return true;
}

bool PostgresDatabase::updateSortOrder(const std::string& device_id,
                                        const std::string& package_name, int sort_order) {
    DatabaseService::getInstance().executeQueryParams(
        "UPDATE device_apps SET sort_order=$1 WHERE device_id=$2 AND package_name=$3",
        {std::to_string(sort_order), device_id, package_name});
    return true;
}

std::vector<DeviceApp> PostgresDatabase::getPopularApps(const std::string& category) {
    pqxx::result r;
    if (category.empty()) {
        r = DatabaseService::getInstance().executeQuery(
            "SELECT 0 AS id,'' AS device_id,package_name,app_name,icon_url,"
            "false AS is_favorite,0 AS sort_order,NOW()::text AS created_at,NOW()::text AS updated_at "
            "FROM popular_apps ORDER BY app_name");
    } else {
        r = DatabaseService::getInstance().executeQueryParams(
            "SELECT 0 AS id,'' AS device_id,package_name,app_name,icon_url,"
            "false AS is_favorite,0 AS sort_order,NOW()::text AS created_at,NOW()::text AS updated_at "
            "FROM popular_apps WHERE category=$1 ORDER BY app_name", {category});
    }
    std::vector<DeviceApp> out;
    for (const auto& row : r) out.push_back(parseApp(row));
    return out;
}

bool PostgresDatabase::addPopularAppsToDevice(const std::string& device_id,
                                               const std::string& category) {
    DatabaseService::getInstance().executeQueryParams(
        "INSERT INTO device_apps (device_id,package_name,app_name,icon_url) "
        "SELECT $1,package_name,app_name,icon_url FROM popular_apps WHERE category=$2 "
        "ON CONFLICT (device_id,package_name) DO NOTHING", {device_id, category});
    return true;
}

// ── Stats ─────────────────────────────────────────────────────────────────────

Json::Value PostgresDatabase::getOverallStats() {
    Json::Value r;
    r["success"] = true;

    auto device_counts = DatabaseService::getInstance().executeQuery(
        "SELECT status, COUNT(*) as count FROM fire_tv_devices GROUP BY status");
    int total = 0, online = 0, offline = 0, pairing = 0;
    for (const auto& row : device_counts) {
        std::string st = row["status"].as<std::string>();
        int cnt = row["count"].as<int>();
        total += cnt;
        if (st == "online") online = cnt;
        else if (st == "offline") offline = cnt;
        else if (st == "pairing") pairing = cnt;
    }
    auto paired_r = DatabaseService::getInstance().executeQuery(
        "SELECT COUNT(*) FROM fire_tv_devices WHERE client_token IS NOT NULL");
    int paired = paired_r.empty() ? 0 : paired_r[0][0].as<int>();
    Json::Value devices;
    devices["total"] = total; devices["online"] = online;
    devices["offline"] = offline; devices["pairing"] = pairing; devices["paired"] = paired;
    r["devices"] = devices;

    auto app_r = DatabaseService::getInstance().executeQuery(
        "SELECT COUNT(*) FROM device_apps");
    Json::Value apps; apps["total"] = app_r.empty() ? 0 : app_r[0][0].as<int>();
    r["apps"] = apps;

    auto cmd_r = DatabaseService::getInstance().executeQuery(
        "SELECT COUNT(*) as total,"
        " SUM(CASE WHEN success=true THEN 1 ELSE 0 END) as succ,"
        " AVG(response_time_ms) as avg_rt "
        "FROM command_history WHERE created_at > NOW()-INTERVAL '24 hours'");
    int total_cmds = 0, succ_cmds = 0;
    double avg_rt = 0.0;
    if (!cmd_r.empty()) {
        total_cmds = cmd_r[0]["total"].is_null() ? 0 : cmd_r[0]["total"].as<int>();
        succ_cmds  = cmd_r[0]["succ"].is_null()  ? 0 : cmd_r[0]["succ"].as<int>();
        avg_rt     = cmd_r[0]["avg_rt"].is_null() ? 0.0 : cmd_r[0]["avg_rt"].as<double>();
    }
    double rate = total_cmds > 0 ? (double)succ_cmds / total_cmds * 100.0 : 0.0;
    Json::Value cmds;
    cmds["last_24h"] = total_cmds; cmds["successful_24h"] = succ_cmds;
    cmds["success_rate"] = rate; cmds["avg_response_time_ms"] = avg_rt;
    r["commands"] = cmds;

    return r;
}

Json::Value PostgresDatabase::getAllDeviceStats() {
    auto result = DatabaseService::getInstance().executeQuery(
        "SELECT * FROM device_stats ORDER BY name");
    Json::Value arr = Json::arrayValue;
    for (const auto& row : result) {
        Json::Value d;
        d["device_id"] = row["device_id"].as<std::string>();
        d["name"]      = row["name"].as<std::string>();
        d["status"]    = row["status"].as<std::string>();
        if (!row["last_seen_at"].is_null()) d["last_seen_at"] = row["last_seen_at"].as<std::string>();
        int app_count = row["app_count"].is_null() ? 0 : row["app_count"].as<int>();
        int cmds_24h  = row["commands_24h"].is_null() ? 0 : row["commands_24h"].as<int>();
        int succ_24h  = row["successful_commands_24h"].is_null() ? 0 : row["successful_commands_24h"].as<int>();
        double avg_rt = row["avg_response_time_ms_24h"].is_null() ? 0.0 : row["avg_response_time_ms_24h"].as<double>();
        d["app_count"] = app_count; d["commands_24h"] = cmds_24h;
        d["successful_commands_24h"] = succ_24h; d["avg_response_time_ms_24h"] = avg_rt;
        if (!row["last_command_at"].is_null()) d["last_command_at"] = row["last_command_at"].as<std::string>();
        d["success_rate_24h"] = cmds_24h > 0 ? (double)succ_24h / cmds_24h * 100.0 : 0.0;
        arr.append(d);
    }
    return arr;
}

} // namespace hms_firetv
