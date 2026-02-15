#pragma once

#include <pqxx/pqxx>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <iostream>

namespace hms_firetv {

/**
 * Thread-safe PostgreSQL connection pool
 *
 * Features:
 * - Pre-allocated connections for concurrent access
 * - Automatic connection health checking
 * - RAII-based connection management (auto-return to pool)
 * - Timeout support for connection acquisition
 * - Graceful shutdown with connection cleanup
 */
class ConnectionPool {
public:
    /**
     * RAII wrapper for pooled connections
     * Automatically returns connection to pool when destroyed
     */
    class PooledConnection {
    public:
        PooledConnection(std::unique_ptr<pqxx::connection> conn, ConnectionPool* pool)
            : conn_(std::move(conn)), pool_(pool) {}

        ~PooledConnection() {
            if (pool_ && conn_) {
                pool_->returnConnection(std::move(conn_));
            }
        }

        // Non-copyable, movable
        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;
        PooledConnection(PooledConnection&&) = default;
        PooledConnection& operator=(PooledConnection&&) = default;

        pqxx::connection* operator->() { return conn_.get(); }
        pqxx::connection& operator*() { return *conn_; }
        pqxx::connection* get() { return conn_.get(); }

        bool isValid() const {
            return conn_ && conn_->is_open();
        }

    private:
        std::unique_ptr<pqxx::connection> conn_;
        ConnectionPool* pool_;
    };

    /**
     * Constructor
     * @param connection_string PostgreSQL connection string
     * @param pool_size Number of connections in the pool (default: 8)
     * @param max_wait_ms Max time to wait for available connection (default: 5000ms)
     */
    ConnectionPool(const std::string& connection_string,
                   size_t pool_size = 8,
                   int max_wait_ms = 5000)
        : connection_string_(connection_string),
          pool_size_(pool_size),
          max_wait_ms_(max_wait_ms),
          shutdown_(false) {

        std::cout << "[ConnectionPool] Initializing pool with " << pool_size
                  << " connections..." << std::endl;

        // Pre-allocate connections
        for (size_t i = 0; i < pool_size_; ++i) {
            try {
                auto conn = std::make_unique<pqxx::connection>(connection_string_);
                if (conn->is_open()) {
                    available_connections_.push(std::move(conn));
                    std::cout << "[ConnectionPool] Connection " << (i + 1)
                              << "/" << pool_size_ << " initialized" << std::endl;
                } else {
                    std::cerr << "[ConnectionPool] Warning: Connection " << (i + 1)
                              << " failed to open" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[ConnectionPool] Error creating connection " << (i + 1)
                          << ": " << e.what() << std::endl;
            }
        }

        std::cout << "[ConnectionPool] Initialized with "
                  << available_connections_.size() << "/" << pool_size_
                  << " connections" << std::endl;
    }

    /**
     * Destructor - closes all connections
     */
    ~ConnectionPool() {
        shutdown();
    }

    /**
     * Acquire a connection from the pool
     * @return PooledConnection that auto-returns to pool when destroyed
     * @throws std::runtime_error if no connection available within timeout
     */
    PooledConnection acquire() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Wait for available connection with timeout
        auto deadline = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(max_wait_ms_);

        while (available_connections_.empty() && !shutdown_) {
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                throw std::runtime_error(
                    "Connection pool timeout: no connection available within " +
                    std::to_string(max_wait_ms_) + "ms"
                );
            }
        }

        if (shutdown_) {
            throw std::runtime_error("Connection pool is shutting down");
        }

        // Get connection from pool
        auto conn = std::move(available_connections_.front());
        available_connections_.pop();

        // Verify connection is still valid
        if (!conn || !conn->is_open()) {
            std::cerr << "[ConnectionPool] Connection invalid, attempting to recreate..."
                      << std::endl;
            try {
                conn = std::make_unique<pqxx::connection>(connection_string_);
            } catch (const std::exception& e) {
                std::cerr << "[ConnectionPool] Failed to recreate connection: "
                          << e.what() << std::endl;
                throw;
            }
        }

        return PooledConnection(std::move(conn), this);
    }

    /**
     * Get pool statistics
     */
    size_t availableCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return available_connections_.size();
    }

    size_t poolSize() const {
        return pool_size_;
    }

    size_t inUseCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_size_ - available_connections_.size();
    }

    /**
     * Shutdown the pool and close all connections
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (shutdown_) {
            return;
        }

        shutdown_ = true;
        cv_.notify_all();

        std::cout << "[ConnectionPool] Shutting down, closing "
                  << available_connections_.size() << " connections..." << std::endl;

        while (!available_connections_.empty()) {
            auto conn = std::move(available_connections_.front());
            available_connections_.pop();
            if (conn && conn->is_open()) {
                try {
                    conn->close();
                } catch (...) {
                    // Ignore errors during shutdown
                }
            }
        }

        std::cout << "[ConnectionPool] Shutdown complete" << std::endl;
    }

private:
    /**
     * Return a connection to the pool (called by PooledConnection destructor)
     */
    void returnConnection(std::unique_ptr<pqxx::connection> conn) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (shutdown_) {
            // Pool is shutting down, just close the connection
            if (conn && conn->is_open()) {
                try {
                    conn->close();
                } catch (...) {}
            }
            return;
        }

        available_connections_.push(std::move(conn));
        cv_.notify_one();
    }

    std::string connection_string_;
    size_t pool_size_;
    int max_wait_ms_;
    bool shutdown_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<pqxx::connection>> available_connections_;

    friend class PooledConnection;
};

} // namespace hms_firetv
