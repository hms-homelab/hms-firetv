#pragma once
#include <string>

namespace hms_firetv {

struct AppConfig {
    struct Database {
        std::string type = "sqlite";   // "sqlite" | "postgresql"
        std::string sqlite_path;       // default: ~/.hms-firetv/firetv.db
        std::string host;
        int port = 5432;
        std::string name;
        std::string user;
        std::string password;
    } database;

    // Reads DB_HOST/DB_PORT/DB_NAME/DB_USER/DB_PASSWORD from env.
    // Auto-switches type to "postgresql" when host + name are both set.
    void applyEnvFallbacks();
};

} // namespace hms_firetv
