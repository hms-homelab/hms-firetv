#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <string>
#include "services/ConnectionPool.h"

namespace hms_firetv {

/**
 * DatabaseService - Thread-safe PostgreSQL client with connection pooling
 *
 * DESIGN PHILOSOPHY:
 * =================
 * This service NEVER crashes the application, even if PostgreSQL is completely
 * unavailable. All errors are caught, logged, and handled gracefully.
 *
 * RESILIENCE FEATURES:
 * ===================
 * 1. Connection Pool: Multiple concurrent database connections (no mutex serialization)
 * 2. Auto-Recovery: Pool validates connections and recreates failed ones
 * 3. Transaction Safety: Uses RAII transactions (auto-rollback on exception)
 * 4. Thread Safety: Connection pool with internal locking (queries can run in parallel)
 * 5. Graceful Degradation: Returns empty results on failure, never throws to caller
 *
 * PERFORMANCE:
 * ===========
 * - Pool size: 8 connections (configurable)
 * - No global mutex on queries (concurrent execution)
 * - RAII connection management (auto-return to pool)
 * - Connection timeout: 5 seconds
 *
 * USAGE PATTERN:
 * =============
 * ```cpp
 * DatabaseService::getInstance().initialize(host, port, db, user, pass);
 *
 * bool success = DatabaseService::getInstance().executeQuery("SELECT...");
 * if (!success) {
 *     // Log warning, continue operation
 *     // Service will auto-recover when DB comes back online
 * }
 * ```
 *
 * DATABASE SCHEMA:
 * ===============
 * Database: firetv
 * Main Table: fire_tv_devices
 * User: maestro
 * Password: maestro_postgres_2026_secure
 */
class DatabaseService {
public:
    /**
     * Get singleton instance (thread-safe)
     */
    static DatabaseService& getInstance();

    // Disable copy/move to ensure singleton integrity
    DatabaseService(const DatabaseService&) = delete;
    DatabaseService& operator=(const DatabaseService&) = delete;
    DatabaseService(DatabaseService&&) = delete;
    DatabaseService& operator=(DatabaseService&&) = delete;

    /**
     * Initialize PostgreSQL connection
     *
     * Creates initial connection to database. Must be called before any
     * database operations.
     *
     * @param host Database hostname (e.g., "192.168.2.15")
     * @param port Database port (default: 5432)
     * @param dbname Database name ("firetv")
     * @param user Database username ("firetv_user")
     * @param password Database password
     * @throws std::runtime_error if initial connection fails
     */
    void initialize(const std::string& host, int port, const std::string& dbname,
                    const std::string& user, const std::string& password);

    /**
     * Execute query and return result
     *
     * Generic query execution with retry logic.
     * Thread-safe, auto-reconnects on failure.
     *
     * @param query SQL query string (parameterized)
     * @return pqxx::result object (empty if failed)
     */
    pqxx::result executeQuery(const std::string& query);

    /**
     * Execute query with parameters
     *
     * Safe parameterized query execution to prevent SQL injection.
     *
     * @param query SQL query with $1, $2, etc placeholders
     * @param params Vector of parameter values
     * @return pqxx::result object (empty if failed)
     */
    pqxx::result executeQueryParams(const std::string& query,
                                     const std::vector<std::string>& params);

    /**
     * Execute command (INSERT/UPDATE/DELETE)
     *
     * @param command SQL command string
     * @return true if successful, false if failed
     */
    bool executeCommand(const std::string& command);

    /**
     * Check if database connection pool is available
     *
     * @return true if pool initialized with at least one connection
     */
    bool isConnected() const;

    /**
     * Get pool statistics
     */
    size_t availableConnections() const;
    size_t totalConnections() const;
    size_t inUseConnections() const;

private:
    /**
     * Private constructor for singleton pattern
     */
    DatabaseService() = default;

    /**
     * Build connection string from parameters
     */
    std::string buildConnectionString(const std::string& host, int port,
                                       const std::string& dbname,
                                       const std::string& user,
                                       const std::string& password);

    // Connection pool (replaces single connection + mutex)
    std::unique_ptr<ConnectionPool> pool_;
    std::string connection_string_;

    // Pool configuration
    static constexpr size_t DEFAULT_POOL_SIZE = 8;
    static constexpr int DEFAULT_CONNECTION_TIMEOUT_MS = 5000;
};

} // namespace hms_firetv
