#include <gtest/gtest.h>
#include "utils/BackgroundLogger.h"
#include <thread>
#include <chrono>
#include <atomic>

using namespace hms_firetv;

// ============================================================================
// TEST FIXTURE
// ============================================================================

class BackgroundLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Each test gets a fresh logger
    }

    void TearDown() override {
        // Cleanup
    }
};

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

TEST_F(BackgroundLoggerTest, StartAndStopLogger) {
    BackgroundLogger logger;

    logger.start();
    EXPECT_EQ(logger.queueSize(), 0);

    logger.stop();
    EXPECT_EQ(logger.queueSize(), 0);
}

TEST_F(BackgroundLoggerTest, EnqueueAndExecuteTask) {
    BackgroundLogger logger;
    logger.start();

    std::atomic<int> counter{0};

    // Enqueue a task
    bool enqueued = logger.enqueue([&counter]() {
        counter++;
    });

    EXPECT_TRUE(enqueued);

    // Wait for task to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(counter.load(), 1);

    logger.stop();
}

TEST_F(BackgroundLoggerTest, EnqueueMultipleTasks) {
    BackgroundLogger logger;
    logger.start();

    std::atomic<int> counter{0};
    const int num_tasks = 10;

    // Enqueue multiple tasks
    for (int i = 0; i < num_tasks; i++) {
        bool enqueued = logger.enqueue([&counter]() {
            counter++;
        });
        EXPECT_TRUE(enqueued);
    }

    // Wait for all tasks to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(counter.load(), num_tasks);

    logger.stop();
}

TEST_F(BackgroundLoggerTest, TasksExecuteInOrder) {
    BackgroundLogger logger;
    logger.start();

    std::vector<int> executed_order;
    std::mutex order_mutex;

    const int num_tasks = 5;

    // Enqueue tasks with delays to ensure ordering
    for (int i = 0; i < num_tasks; i++) {
        logger.enqueue([&executed_order, &order_mutex, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::lock_guard<std::mutex> lock(order_mutex);
            executed_order.push_back(i);
        });
    }

    // Wait for all tasks to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify order
    ASSERT_EQ(executed_order.size(), num_tasks);
    for (int i = 0; i < num_tasks; i++) {
        EXPECT_EQ(executed_order[i], i);
    }

    logger.stop();
}

// ============================================================================
// EXCEPTION SAFETY TESTS
// ============================================================================

TEST_F(BackgroundLoggerTest, TaskExceptionDoesNotCrashWorker) {
    BackgroundLogger logger;
    logger.start();

    std::atomic<int> counter{0};

    // Enqueue task that throws exception
    logger.enqueue([]() {
        throw std::runtime_error("Test exception");
    });

    // Enqueue task after exception
    logger.enqueue([&counter]() {
        counter++;
    });

    // Wait for both tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second task should still execute
    EXPECT_EQ(counter.load(), 1);

    logger.stop();
}

// ============================================================================
// QUEUE SIZE LIMIT TESTS
// ============================================================================

TEST_F(BackgroundLoggerTest, QueueOverflowDropsTasks) {
    // Create logger with small queue size
    BackgroundLogger logger(5);  // Max 5 entries
    logger.start();

    std::atomic<int> executed{0};

    // Enqueue tasks that take time to execute
    for (int i = 0; i < 10; i++) {
        bool enqueued = logger.enqueue([&executed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            executed++;
        });

        // First 5 should succeed, remaining should fail
        if (i < 5) {
            EXPECT_TRUE(enqueued);
        }
    }

    // Check dropped count
    size_t dropped = logger.droppedCount();
    EXPECT_GT(dropped, 0);

    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Should execute exactly 5 tasks
    EXPECT_EQ(executed.load(), 5);

    logger.stop();
}

TEST_F(BackgroundLoggerTest, QueueSizeTracking) {
    BackgroundLogger logger;
    logger.start();

    // Enqueue tasks that take time
    for (int i = 0; i < 5; i++) {
        logger.enqueue([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }

    // Queue should have pending tasks
    size_t queue_size = logger.queueSize();
    EXPECT_GT(queue_size, 0);

    // Wait for all tasks to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Queue should be empty
    EXPECT_EQ(logger.queueSize(), 0);

    logger.stop();
}

// ============================================================================
// GRACEFUL SHUTDOWN TESTS
// ============================================================================

TEST_F(BackgroundLoggerTest, StopDrainsPendingTasks) {
    BackgroundLogger logger;
    logger.start();

    std::atomic<int> counter{0};

    // Enqueue multiple tasks
    for (int i = 0; i < 5; i++) {
        logger.enqueue([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            counter++;
        });
    }

    // Stop should wait for all tasks to complete
    logger.stop();

    // All tasks should have executed
    EXPECT_EQ(counter.load(), 5);
    EXPECT_EQ(logger.queueSize(), 0);
}

TEST_F(BackgroundLoggerTest, MultipleStartStopCycles) {
    BackgroundLogger logger;
    std::atomic<int> counter{0};

    // First cycle
    logger.start();
    logger.enqueue([&counter]() { counter++; });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger.stop();

    int count_after_first = counter.load();
    EXPECT_EQ(count_after_first, 1);

    // Second cycle
    logger.start();
    logger.enqueue([&counter]() { counter++; });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    logger.stop();

    EXPECT_EQ(counter.load(), 2);
}

// ============================================================================
// THREAD SAFETY TESTS
// ============================================================================

TEST_F(BackgroundLoggerTest, ConcurrentEnqueue) {
    BackgroundLogger logger;
    logger.start();

    std::atomic<int> counter{0};
    const int threads = 4;
    const int tasks_per_thread = 25;

    std::vector<std::thread> enqueuers;

    // Spawn multiple threads enqueuing tasks
    for (int t = 0; t < threads; t++) {
        enqueuers.emplace_back([&logger, &counter, tasks_per_thread]() {
            for (int i = 0; i < tasks_per_thread; i++) {
                logger.enqueue([&counter]() {
                    counter++;
                });
            }
        });
    }

    // Wait for all enqueuers
    for (auto& thread : enqueuers) {
        thread.join();
    }

    // Wait for all tasks to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Should execute all tasks
    EXPECT_EQ(counter.load(), threads * tasks_per_thread);

    logger.stop();
}

// ============================================================================
// PERFORMANCE TEST
// ============================================================================

TEST_F(BackgroundLoggerTest, EnqueuePerformance) {
    BackgroundLogger logger;
    logger.start();

    const int num_tasks = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_tasks; i++) {
        logger.enqueue([]() {
            // Minimal work
        });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Enqueuing 1000 tasks should be fast (< 100ms)
    EXPECT_LT(duration.count(), 100);

    std::cout << "Enqueued " << num_tasks << " tasks in " << duration.count() << "ms" << std::endl;

    logger.stop();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
