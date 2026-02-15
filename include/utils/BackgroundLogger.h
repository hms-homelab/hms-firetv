#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <iostream>

namespace hms_firetv {

/**
 * Thread-safe background logger for asynchronous command history logging
 *
 * Features:
 * - Non-blocking enqueue operation (O(1))
 * - Dedicated worker thread for database writes
 * - Graceful shutdown with queue drainage
 * - Exception safety (logging failures don't crash the worker)
 * - Bounded queue size to prevent memory exhaustion
 */
class BackgroundLogger {
public:
    using LogTask = std::function<void()>;

    /**
     * Constructor
     * @param max_queue_size Maximum number of pending log entries (default: 1000)
     */
    explicit BackgroundLogger(size_t max_queue_size = 1000)
        : max_queue_size_(max_queue_size), running_(false), dropped_count_(0) {}

    /**
     * Destructor - ensures worker thread is stopped
     */
    ~BackgroundLogger() {
        stop();
    }

    // Non-copyable, non-movable
    BackgroundLogger(const BackgroundLogger&) = delete;
    BackgroundLogger& operator=(const BackgroundLogger&) = delete;

    /**
     * Start the background logging thread
     */
    void start() {
        if (running_.load()) {
            return;  // Already running
        }

        running_ = true;
        worker_thread_ = std::thread(&BackgroundLogger::workerLoop, this);
        std::cout << "[BackgroundLogger] Started worker thread" << std::endl;
    }

    /**
     * Stop the background logging thread
     * Waits for all pending logs to be processed
     */
    void stop() {
        if (!running_.load()) {
            return;  // Already stopped
        }

        running_ = false;
        cv_.notify_one();

        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }

        size_t dropped = dropped_count_.load();
        if (dropped > 0) {
            std::cerr << "[BackgroundLogger] Warning: Dropped " << dropped
                      << " log entries due to queue overflow" << std::endl;
        }

        std::cout << "[BackgroundLogger] Stopped worker thread" << std::endl;
    }

    /**
     * Enqueue a log task to be executed asynchronously
     * @param task Function to execute in the background thread
     * @return true if enqueued, false if queue is full (task dropped)
     */
    bool enqueue(LogTask task) {
        std::unique_lock<std::mutex> lock(mutex_);

        // Check if queue is full
        if (queue_.size() >= max_queue_size_) {
            dropped_count_++;
            return false;  // Drop the log entry
        }

        queue_.push(std::move(task));
        lock.unlock();
        cv_.notify_one();

        return true;
    }

    /**
     * Get the current queue size
     */
    size_t queueSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * Get the number of dropped log entries
     */
    size_t droppedCount() const {
        return dropped_count_.load();
    }

private:
    /**
     * Worker thread loop - processes log tasks from the queue
     */
    void workerLoop() {
        std::cout << "[BackgroundLogger] Worker thread started (ID: "
                  << std::this_thread::get_id() << ")" << std::endl;

        while (running_.load() || !queue_.empty()) {
            std::unique_lock<std::mutex> lock(mutex_);

            // Wait for tasks or shutdown signal
            cv_.wait(lock, [this] {
                return !queue_.empty() || !running_.load();
            });

            // Process all available tasks
            while (!queue_.empty()) {
                LogTask task = std::move(queue_.front());
                queue_.pop();
                lock.unlock();

                // Execute task with exception safety
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "[BackgroundLogger] Task failed: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[BackgroundLogger] Task failed with unknown exception" << std::endl;
                }

                lock.lock();
            }
        }

        std::cout << "[BackgroundLogger] Worker thread stopped" << std::endl;
    }

    size_t max_queue_size_;
    std::atomic<bool> running_;
    std::atomic<size_t> dropped_count_;
    std::thread worker_thread_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<LogTask> queue_;
};

} // namespace hms_firetv
