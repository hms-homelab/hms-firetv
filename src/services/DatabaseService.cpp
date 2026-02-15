#include "services/DatabaseService.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace hms_firetv {

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

DatabaseService& DatabaseService::getInstance() {
    // Thread-safe in C++11+: Static local variables are initialized once
    static DatabaseService instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void DatabaseService::initialize(const std::string& host, int port, const std::string& dbname,
                                  const std::string& user, const std::string& password) {
    connection_string_ = buildConnectionString(host, port, dbname, user, password);

    try {
        // Create connection pool with DEFAULT_POOL_SIZE connections
        pool_ = std::make_unique<ConnectionPool>(
            connection_string_,
            DEFAULT_POOL_SIZE,
            DEFAULT_CONNECTION_TIMEOUT_MS
        );

        std::cout << "[DatabaseService] ✅ Connected to PostgreSQL: "
                  << dbname << "@" << host << ":" << port << std::endl;
        std::cout << "[DatabaseService] Pool initialized with "
                  << pool_->poolSize() << " connections" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[DatabaseService] ❌ Failed to initialize connection pool for "
                  << host << ":" << port << "/" << dbname << std::endl;
        std::cerr << "[DatabaseService] Error: " << e.what() << std::endl;
        throw;  // Fail-fast during initialization
    }
}

// ============================================================================
// QUERY EXECUTION
// ============================================================================

pqxx::result DatabaseService::executeQuery(const std::string& query) {
    if (!pool_) {
        std::cerr << "[DatabaseService] ❌ Connection pool not initialized" << std::endl;
        return pqxx::result{};
    }

    // Retry logic: 3 attempts with exponential backoff (1s, 2s, 4s)
    const int MAX_RETRIES = 3;
    const int BACKOFF_MS[] = {1000, 2000, 4000};

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        try {
            // Acquire connection from pool (thread-safe, blocks if all connections busy)
            auto conn = pool_->acquire();

            // Execute query
            pqxx::work txn(*conn);
            pqxx::result result = txn.exec(query);
            txn.commit();

            // Success! Log if we recovered from previous failures
            if (attempt > 0) {
                std::cout << "[DatabaseService] ✅ Query succeeded after "
                          << (attempt + 1) << " attempts" << std::endl;
            }
            return result;

        } catch (const std::exception& e) {
            std::cerr << "[DatabaseService] ❌ Query failed (attempt "
                      << (attempt + 1) << "/" << MAX_RETRIES << "): "
                      << e.what() << std::endl;

            // Sleep before retry (except on last attempt)
            if (attempt < MAX_RETRIES - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(BACKOFF_MS[attempt]));
            }
        }
    }

    std::cerr << "[DatabaseService] ❌ Query failed after " << MAX_RETRIES << " attempts" << std::endl;
    return pqxx::result{};
}

pqxx::result DatabaseService::executeQueryParams(const std::string& query,
                                                  const std::vector<std::string>& params) {
    if (!pool_) {
        std::cerr << "[DatabaseService] ❌ Connection pool not initialized" << std::endl;
        return pqxx::result{};
    }

    // Retry logic: 3 attempts with exponential backoff (1s, 2s, 4s)
    const int MAX_RETRIES = 3;
    const int BACKOFF_MS[] = {1000, 2000, 4000};

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        try {
            // Acquire connection from pool (thread-safe, auto-returns on scope exit)
            auto conn = pool_->acquire();

            // Execute parameterized query
            pqxx::work txn(*conn);
            pqxx::result result;

            // Expand parameters manually based on count
            switch (params.size()) {
                case 0:
                    result = txn.exec(query);
                    break;
                case 1:
                    result = txn.exec_params(query, params[0]);
                    break;
                case 2:
                    result = txn.exec_params(query, params[0], params[1]);
                    break;
                case 3:
                    result = txn.exec_params(query, params[0], params[1], params[2]);
                    break;
                case 4:
                    result = txn.exec_params(query, params[0], params[1], params[2], params[3]);
                    break;
                case 5:
                    result = txn.exec_params(query, params[0], params[1], params[2], params[3], params[4]);
                    break;
                case 6:
                    result = txn.exec_params(query, params[0], params[1], params[2], params[3], params[4], params[5]);
                    break;
                case 7:
                    result = txn.exec_params(query, params[0], params[1], params[2], params[3], params[4], params[5], params[6]);
                    break;
                case 8:
                    result = txn.exec_params(query, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]);
                    break;
                default:
                    // For more than 8 params, fall back to non-parameterized (not ideal but rare)
                    std::cerr << "[DatabaseService] Warning: More than 8 parameters, using non-parameterized query" << std::endl;
                    result = txn.exec(query);
            }

            txn.commit();

            // Success! Log if we recovered from previous failures
            if (attempt > 0) {
                std::cout << "[DatabaseService] ✅ Parameterized query succeeded after "
                          << (attempt + 1) << " attempts" << std::endl;
            }
            return result;

        } catch (const std::exception& e) {
            std::cerr << "[DatabaseService] ❌ Parameterized query failed (attempt "
                      << (attempt + 1) << "/" << MAX_RETRIES << "): "
                      << e.what() << std::endl;

            // Sleep before retry (except on last attempt)
            if (attempt < MAX_RETRIES - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(BACKOFF_MS[attempt]));
            }
        }
    }

    std::cerr << "[DatabaseService] ❌ Parameterized query failed after "
              << MAX_RETRIES << " attempts" << std::endl;
    return pqxx::result{};
}

bool DatabaseService::executeCommand(const std::string& command) {
    pqxx::result result = executeQuery(command);
    return !result.empty() || result.affected_rows() >= 0;
}

// ============================================================================
// CONNECTION STATUS
// ============================================================================

bool DatabaseService::isConnected() const {
    return pool_ && pool_->availableCount() > 0;
}

size_t DatabaseService::availableConnections() const {
    return pool_ ? pool_->availableCount() : 0;
}

size_t DatabaseService::totalConnections() const {
    return pool_ ? pool_->poolSize() : 0;
}

size_t DatabaseService::inUseConnections() const {
    return pool_ ? pool_->inUseCount() : 0;
}

// ============================================================================
// UTILITY METHODS
// ============================================================================

std::string DatabaseService::buildConnectionString(const std::string& host, int port,
                                                     const std::string& dbname,
                                                     const std::string& user,
                                                     const std::string& password) {
    // Build PostgreSQL connection string
    // Format: "host=... port=... dbname=... user=... password=..."
    std::string conn_str = "host=" + host +
                          " port=" + std::to_string(port) +
                          " dbname=" + dbname +
                          " user=" + user +
                          " password=" + password +
                          " connect_timeout=10";
    return conn_str;
}

} // namespace hms_firetv
