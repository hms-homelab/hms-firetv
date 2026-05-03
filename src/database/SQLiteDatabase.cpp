#include "database/SQLiteDatabase.h"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <ctime>
#include <sstream>

namespace hms_firetv {

// ── RAII stmt guard ───────────────────────────────────────────────────────────

struct StmtGuard {
    sqlite3_stmt* s = nullptr;
    ~StmtGuard() { if (s) sqlite3_finalize(s); }
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char* col_text(sqlite3_stmt* s, int i) {
    return reinterpret_cast<const char*>(sqlite3_column_text(s, i));
}
static std::string col_str(sqlite3_stmt* s, int i) {
    const char* t = col_text(s, i);
    return t ? t : "";
}
static bool col_is_null(sqlite3_stmt* s, int i) {
    return sqlite3_column_type(s, i) == SQLITE_NULL;
}

std::chrono::system_clock::time_point SQLiteDatabase::parseTs(const char* s) {
    if (!s || !*s) return std::chrono::system_clock::now();
    struct tm tm = {};
    strptime(s, "%Y-%m-%d %H:%M:%S", &tm);
    tm.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(mktime(&tm));
}

std::optional<std::chrono::system_clock::time_point> SQLiteDatabase::parseTsOpt(const char* s) {
    if (!s || !*s) return std::nullopt;
    return parseTs(s);
}

// ── Construction ──────────────────────────────────────────────────────────────

SQLiteDatabase::SQLiteDatabase(const std::string& db_path) : db_path_(db_path) {}

SQLiteDatabase::~SQLiteDatabase() { disconnect(); }

// ── Connection ────────────────────────────────────────────────────────────────

bool SQLiteDatabase::connect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) return true;

    if (db_path_ != ":memory:" && !db_path_.empty()) {
        auto parent = std::filesystem::path(db_path_).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
    }

    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "[SQLiteDB] Failed to open: " << sqlite3_errmsg(db_) << std::endl;
        db_ = nullptr;
        return false;
    }
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");
    exec("PRAGMA synchronous=NORMAL");
    createSchema();
    std::cout << "[SQLiteDB] Connected (" << db_path_ << ")" << std::endl;
    return true;
}

void SQLiteDatabase::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) { sqlite3_close(db_); db_ = nullptr; }
}

bool SQLiteDatabase::isConnected() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return db_ != nullptr;
}

bool SQLiteDatabase::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[SQLiteDB] exec error: " << (err ? err : "?") << " SQL: " << sql << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

void SQLiteDatabase::createSchema() {
    exec(R"(
CREATE TABLE IF NOT EXISTS fire_tv_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT UNIQUE NOT NULL,
    name TEXT NOT NULL,
    ip_address TEXT NOT NULL,
    api_key TEXT NOT NULL DEFAULT '0987654321',
    client_token TEXT,
    pin_code TEXT,
    pin_expires_at TEXT,
    status TEXT NOT NULL DEFAULT 'offline',
    adb_enabled INTEGER NOT NULL DEFAULT 0,
    last_seen_at TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT DEFAULT CURRENT_TIMESTAMP
))");
    exec("CREATE INDEX IF NOT EXISTS idx_ftd_device_id ON fire_tv_devices(device_id)");
    exec("CREATE INDEX IF NOT EXISTS idx_ftd_status ON fire_tv_devices(status)");

    exec(R"(
CREATE TABLE IF NOT EXISTS device_apps (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    package_name TEXT NOT NULL,
    app_name TEXT NOT NULL,
    icon_url TEXT DEFAULT '',
    is_favorite INTEGER NOT NULL DEFAULT 0,
    sort_order INTEGER NOT NULL DEFAULT 0,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(device_id, package_name),
    FOREIGN KEY(device_id) REFERENCES fire_tv_devices(device_id) ON DELETE CASCADE
))");
    exec("CREATE INDEX IF NOT EXISTS idx_da_device_id ON device_apps(device_id)");

    exec(R"(
CREATE TABLE IF NOT EXISTS popular_apps (
    package_name TEXT PRIMARY KEY,
    app_name TEXT NOT NULL,
    icon_url TEXT DEFAULT '',
    category TEXT DEFAULT 'general'
))");

    exec(R"(
CREATE TABLE IF NOT EXISTS command_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    command_type TEXT NOT NULL,
    command_data TEXT DEFAULT '{}',
    success INTEGER NOT NULL DEFAULT 0,
    response_time_ms INTEGER,
    error_message TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
))");
    exec("CREATE INDEX IF NOT EXISTS idx_ch_device_id ON command_history(device_id)");
    exec("CREATE INDEX IF NOT EXISTS idx_ch_created_at ON command_history(created_at)");

    // Seed popular apps (ignore duplicates)
    exec(R"(
INSERT OR IGNORE INTO popular_apps (package_name, app_name, category) VALUES
    ('com.netflix.ninja','Netflix','streaming'),
    ('com.amazon.avod.thirdpartyclient','Prime Video','streaming'),
    ('com.google.android.youtube.tv','YouTube','streaming'),
    ('com.disney.disneyplus','Disney+','streaming'),
    ('com.hulu.plus','Hulu','streaming'),
    ('com.hbo.hbonow','HBO Max','streaming'),
    ('com.spotify.tv.android','Spotify','music'),
    ('com.plexapp.android','Plex','media'),
    ('com.amazon.firetv.youtube','YouTube TV','streaming'),
    ('com.amazon.venezia','Silk Browser','utility'),
    ('com.apple.atve.androidtv.appletv','Apple TV+','streaming'),
    ('com.amazon.ssm','Settings','system')
)");
}

// ── Parsing ───────────────────────────────────────────────────────────────────

Device SQLiteDatabase::parseDevice(sqlite3_stmt* s) {
    // Columns: id, device_id, name, ip_address, api_key, client_token, pin_code,
    //          pin_expires_at, status, adb_enabled, last_seen_at, created_at, updated_at
    Device d;
    d.id           = sqlite3_column_int(s, 0);
    d.device_id    = col_str(s, 1);
    d.name         = col_str(s, 2);
    d.ip_address   = col_str(s, 3);
    d.api_key      = col_str(s, 4);
    if (!col_is_null(s, 5)) d.client_token = col_str(s, 5);
    if (!col_is_null(s, 6)) d.pin_code     = col_str(s, 6);
    d.pin_expires_at = parseTsOpt(col_text(s, 7));
    d.status        = col_str(s, 8);
    d.adb_enabled   = sqlite3_column_int(s, 9) != 0;
    d.last_seen_at  = parseTsOpt(col_text(s, 10));
    d.created_at    = parseTs(col_text(s, 11));
    d.updated_at    = parseTs(col_text(s, 12));
    return d;
}

DeviceApp SQLiteDatabase::parseApp(sqlite3_stmt* s) {
    // Columns: id, device_id, package_name, app_name, icon_url, is_favorite, sort_order,
    //          created_at, updated_at
    DeviceApp a;
    a.id           = sqlite3_column_int(s, 0);
    a.device_id    = col_str(s, 1);
    a.package_name = col_str(s, 2);
    a.app_name     = col_str(s, 3);
    a.icon_url     = col_str(s, 4);
    a.is_favorite  = sqlite3_column_int(s, 5) != 0;
    a.sort_order   = sqlite3_column_int(s, 6);
    a.created_at   = col_str(s, 7);
    a.updated_at   = col_str(s, 8);
    return a;
}

// ── Device CRUD ───────────────────────────────────────────────────────────────

std::optional<Device> SQLiteDatabase::createDevice(const Device& device) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "INSERT INTO fire_tv_devices (device_id,name,ip_address,api_key,status,adb_enabled,"
        "created_at,updated_at) VALUES (?,?,?,?,?,?,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(g.s, 1, device.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 3, device.ip_address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 4, device.api_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 5, device.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(g.s, 6, device.adb_enabled ? 1 : 0);
    if (sqlite3_step(g.s) != SQLITE_DONE) return std::nullopt;
    return getDeviceById(device.device_id);
}

std::optional<Device> SQLiteDatabase::getDeviceById(const std::string& device_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "SELECT * FROM fire_tv_devices WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(g.s) != SQLITE_ROW) return std::nullopt;
    return parseDevice(g.s);
}

std::vector<Device> SQLiteDatabase::getAllDevices() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "SELECT * FROM fire_tv_devices ORDER BY created_at DESC";
    StmtGuard g;
    std::vector<Device> out;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(g.s) == SQLITE_ROW) out.push_back(parseDevice(g.s));
    return out;
}

std::vector<Device> SQLiteDatabase::getDevicesByStatus(const std::string& status) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "SELECT * FROM fire_tv_devices WHERE status=? ORDER BY created_at DESC";
    StmtGuard g;
    std::vector<Device> out;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(g.s, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(g.s) == SQLITE_ROW) out.push_back(parseDevice(g.s));
    return out;
}

bool SQLiteDatabase::updateDevice(const Device& device) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "UPDATE fire_tv_devices SET name=?,ip_address=?,status=?,adb_enabled=?,"
        "updated_at=CURRENT_TIMESTAMP WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, device.ip_address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 3, device.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(g.s, 4, device.adb_enabled ? 1 : 0);
    sqlite3_bind_text(g.s, 5, device.device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::deleteDevice(const std::string& device_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "DELETE FROM fire_tv_devices WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::deviceExists(const std::string& device_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "SELECT COUNT(*) FROM fire_tv_devices WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_ROW && sqlite3_column_int(g.s, 0) > 0;
}

bool SQLiteDatabase::updateLastSeen(const std::string& device_id, const std::string& status) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "UPDATE fire_tv_devices SET last_seen_at=CURRENT_TIMESTAMP,status=?,"
        "updated_at=CURRENT_TIMESTAMP WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::setPairingPin(const std::string& device_id, const std::string& pin_code,
                                   int expires_secs) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // datetime('now', '+N seconds')
    std::string expires_modifier = "+" + std::to_string(expires_secs) + " seconds";
    const char* sql =
        "UPDATE fire_tv_devices SET pin_code=?,pin_expires_at=datetime('now',?),"
        "status='pairing',updated_at=CURRENT_TIMESTAMP WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, pin_code.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, expires_modifier.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 3, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::verifyPinAndSetToken(const std::string& device_id,
                                          const std::string& pin_code,
                                          const std::string& client_token) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // Read pin and expiry
    {
        const char* sql =
            "SELECT pin_code, pin_expires_at FROM fire_tv_devices WHERE device_id=?";
        StmtGuard g;
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(g.s) != SQLITE_ROW) return false;
        if (col_is_null(g.s, 0)) return false;
        if (col_str(g.s, 0) != pin_code) return false;
        // Check expiry via SQLite
        const char* expires = col_text(g.s, 1);
        if (expires && *expires) {
            const char* check_sql =
                "SELECT datetime('now') > ?";
            StmtGuard g2;
            if (sqlite3_prepare_v2(db_, check_sql, -1, &g2.s, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(g2.s, 1, expires, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(g2.s) == SQLITE_ROW && sqlite3_column_int(g2.s, 0)) {
                    return false; // expired
                }
            }
        }
    }
    // Set token
    const char* sql =
        "UPDATE fire_tv_devices SET client_token=?,pin_code=NULL,pin_expires_at=NULL,"
        "status='online',updated_at=CURRENT_TIMESTAMP WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, client_token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::completePairing(const std::string& device_id,
                                     const std::string& client_token) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "UPDATE fire_tv_devices SET client_token=?,pin_code=NULL,pin_expires_at=NULL,"
        "status='online',updated_at=CURRENT_TIMESTAMP WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, client_token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::clearPairing(const std::string& device_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "UPDATE fire_tv_devices SET client_token=NULL,pin_code=NULL,pin_expires_at=NULL,"
        "status='offline',updated_at=CURRENT_TIMESTAMP WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

// ── Apps CRUD ─────────────────────────────────────────────────────────────────

std::vector<DeviceApp> SQLiteDatabase::getAppsForDevice(const std::string& device_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "SELECT id,device_id,package_name,app_name,icon_url,is_favorite,sort_order,"
        "created_at,updated_at FROM device_apps WHERE device_id=? "
        "ORDER BY is_favorite DESC, app_name";
    StmtGuard g;
    std::vector<DeviceApp> out;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(g.s) == SQLITE_ROW) out.push_back(parseApp(g.s));
    return out;
}

std::optional<DeviceApp> SQLiteDatabase::getApp(const std::string& device_id,
                                                  const std::string& package_name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "SELECT id,device_id,package_name,app_name,icon_url,is_favorite,sort_order,"
        "created_at,updated_at FROM device_apps WHERE device_id=? AND package_name=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, package_name.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(g.s) != SQLITE_ROW) return std::nullopt;
    return parseApp(g.s);
}

bool SQLiteDatabase::addApp(const DeviceApp& app) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "INSERT OR IGNORE INTO device_apps "
        "(device_id,package_name,app_name,icon_url,is_favorite,sort_order,created_at,updated_at)"
        " VALUES (?,?,?,?,?,0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, app.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, app.package_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 3, app.app_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 4, app.icon_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(g.s, 5, app.is_favorite ? 1 : 0);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::updateApp(const DeviceApp& app) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "UPDATE device_apps SET app_name=?,icon_url=?,is_favorite=?,"
        "updated_at=CURRENT_TIMESTAMP WHERE device_id=? AND package_name=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, app.app_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, app.icon_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(g.s, 3, app.is_favorite ? 1 : 0);
    sqlite3_bind_text(g.s, 4, app.device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 5, app.package_name.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::deleteApp(const std::string& device_id, const std::string& package_name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "DELETE FROM device_apps WHERE device_id=? AND package_name=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, package_name.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::deleteAllApps(const std::string& device_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = "DELETE FROM device_apps WHERE device_id=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::setFavorite(const std::string& device_id, const std::string& package_name,
                                  bool is_favorite) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "UPDATE device_apps SET is_favorite=?,updated_at=CURRENT_TIMESTAMP "
        "WHERE device_id=? AND package_name=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(g.s, 1, is_favorite ? 1 : 0);
    sqlite3_bind_text(g.s, 2, device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 3, package_name.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

bool SQLiteDatabase::updateSortOrder(const std::string& device_id,
                                      const std::string& package_name, int sort_order) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "UPDATE device_apps SET sort_order=?,updated_at=CURRENT_TIMESTAMP "
        "WHERE device_id=? AND package_name=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(g.s, 1, sort_order);
    sqlite3_bind_text(g.s, 2, device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 3, package_name.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

std::vector<DeviceApp> SQLiteDatabase::getPopularApps(const std::string& category) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::string sql =
        "SELECT 0,'' AS device_id,package_name,app_name,icon_url,0,0,'' AS created_at,'' AS updated_at "
        "FROM popular_apps";
    if (!category.empty()) sql += " WHERE category=?";
    sql += " ORDER BY app_name";
    StmtGuard g;
    std::vector<DeviceApp> out;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &g.s, nullptr) != SQLITE_OK) return out;
    if (!category.empty()) sqlite3_bind_text(g.s, 1, category.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(g.s) == SQLITE_ROW) out.push_back(parseApp(g.s));
    return out;
}

bool SQLiteDatabase::addPopularAppsToDevice(const std::string& device_id,
                                             const std::string& category) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql =
        "INSERT OR IGNORE INTO device_apps "
        "(device_id,package_name,app_name,icon_url,created_at,updated_at) "
        "SELECT ?,package_name,app_name,icon_url,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP "
        "FROM popular_apps WHERE category=?";
    StmtGuard g;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(g.s, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(g.s, 2, category.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(g.s) == SQLITE_DONE;
}

// ── Stats ─────────────────────────────────────────────────────────────────────

Json::Value SQLiteDatabase::getOverallStats() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Json::Value r;
    r["success"] = true;

    // Device counts by status
    int total = 0, online = 0, offline = 0, pairing = 0, paired = 0;
    {
        const char* sql = "SELECT status, COUNT(*) FROM fire_tv_devices GROUP BY status";
        StmtGuard g;
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) == SQLITE_OK) {
            while (sqlite3_step(g.s) == SQLITE_ROW) {
                std::string st = col_str(g.s, 0);
                int cnt = sqlite3_column_int(g.s, 1);
                total += cnt;
                if (st == "online") online = cnt;
                else if (st == "offline") offline = cnt;
                else if (st == "pairing") pairing = cnt;
            }
        }
        const char* psql = "SELECT COUNT(*) FROM fire_tv_devices WHERE client_token IS NOT NULL";
        StmtGuard g2;
        if (sqlite3_prepare_v2(db_, psql, -1, &g2.s, nullptr) == SQLITE_OK &&
            sqlite3_step(g2.s) == SQLITE_ROW)
            paired = sqlite3_column_int(g2.s, 0);
    }
    Json::Value devices;
    devices["total"] = total; devices["online"] = online;
    devices["offline"] = offline; devices["pairing"] = pairing; devices["paired"] = paired;
    r["devices"] = devices;

    // Total apps
    int total_apps = 0;
    {
        const char* sql = "SELECT COUNT(*) FROM device_apps";
        StmtGuard g;
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) == SQLITE_OK &&
            sqlite3_step(g.s) == SQLITE_ROW)
            total_apps = sqlite3_column_int(g.s, 0);
    }
    Json::Value apps; apps["total"] = total_apps; r["apps"] = apps;

    // Commands last 24h
    int total_cmds = 0, successful_cmds = 0;
    double avg_rt = 0.0;
    {
        const char* sql =
            "SELECT COUNT(*), SUM(success), AVG(response_time_ms) "
            "FROM command_history WHERE created_at > datetime('now','-24 hours')";
        StmtGuard g;
        if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) == SQLITE_OK &&
            sqlite3_step(g.s) == SQLITE_ROW) {
            total_cmds     = sqlite3_column_int(g.s, 0);
            successful_cmds = col_is_null(g.s, 1) ? 0 : sqlite3_column_int(g.s, 1);
            avg_rt          = col_is_null(g.s, 2) ? 0.0 : sqlite3_column_double(g.s, 2);
        }
    }
    double success_rate = total_cmds > 0 ? (double)successful_cmds / total_cmds * 100.0 : 0.0;
    Json::Value cmds;
    cmds["last_24h"] = total_cmds; cmds["successful_24h"] = successful_cmds;
    cmds["success_rate"] = success_rate; cmds["avg_response_time_ms"] = avg_rt;
    r["commands"] = cmds;

    return r;
}

Json::Value SQLiteDatabase::getAllDeviceStats() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const char* sql = R"(
SELECT d.device_id, d.name, d.status, d.last_seen_at,
  COUNT(DISTINCT a.id) AS app_count,
  COUNT(DISTINCT CASE WHEN h.created_at > datetime('now','-24 hours') THEN h.id END) AS commands_24h,
  COUNT(DISTINCT CASE WHEN h.success=1 AND h.created_at > datetime('now','-24 hours') THEN h.id END) AS successful_commands_24h,
  AVG(CASE WHEN h.created_at > datetime('now','-24 hours') THEN h.response_time_ms END) AS avg_rt_24h,
  MAX(h.created_at) AS last_command_at
FROM fire_tv_devices d
LEFT JOIN device_apps a ON d.device_id = a.device_id
LEFT JOIN command_history h ON d.device_id = h.device_id
GROUP BY d.device_id, d.name, d.status, d.last_seen_at
ORDER BY d.name
)";
    StmtGuard g;
    Json::Value arr = Json::arrayValue;
    if (sqlite3_prepare_v2(db_, sql, -1, &g.s, nullptr) != SQLITE_OK) return arr;
    while (sqlite3_step(g.s) == SQLITE_ROW) {
        Json::Value d;
        d["device_id"] = col_str(g.s, 0);
        d["name"]      = col_str(g.s, 1);
        d["status"]    = col_str(g.s, 2);
        if (!col_is_null(g.s, 3)) d["last_seen_at"] = col_str(g.s, 3);
        int app_count   = sqlite3_column_int(g.s, 4);
        int cmds_24h    = sqlite3_column_int(g.s, 5);
        int succ_24h    = sqlite3_column_int(g.s, 6);
        double avg_rt   = col_is_null(g.s, 7) ? 0.0 : sqlite3_column_double(g.s, 7);
        d["app_count"]  = app_count;
        d["commands_24h"] = cmds_24h;
        d["successful_commands_24h"] = succ_24h;
        d["avg_response_time_ms_24h"] = avg_rt;
        if (!col_is_null(g.s, 8)) d["last_command_at"] = col_str(g.s, 8);
        double rate = cmds_24h > 0 ? (double)succ_24h / cmds_24h * 100.0 : 0.0;
        d["success_rate_24h"] = rate;
        arr.append(d);
    }
    return arr;
}

} // namespace hms_firetv
