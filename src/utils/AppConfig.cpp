#include "utils/AppConfig.h"
#include "utils/ConfigManager.h"

namespace hms_firetv {

void AppConfig::applyEnvFallbacks() {
    std::string env_host = ConfigManager::getEnv("DB_HOST", "");
    std::string env_name = ConfigManager::getEnv("DB_NAME", "");
    std::string env_user = ConfigManager::getEnv("DB_USER", "");
    std::string env_pass = ConfigManager::getEnv("DB_PASSWORD", "");
    int env_port         = ConfigManager::getEnvInt("DB_PORT", 0);

    if (database.host.empty() && !env_host.empty()) database.host = env_host;
    if (database.name.empty() && !env_name.empty()) database.name = env_name;
    if (database.user.empty() && !env_user.empty()) database.user = env_user;
    if (database.password.empty() && !env_pass.empty()) database.password = env_pass;
    if (database.port == 5432 && env_port > 0) database.port = env_port;

    // Presence of host + name implies the caller wants PostgreSQL
    if (database.type == "sqlite" && !database.host.empty() && !database.name.empty())
        database.type = "postgresql";
}

} // namespace hms_firetv
