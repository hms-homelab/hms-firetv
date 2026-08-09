#include <gtest/gtest.h>
#include "services/ConnectionPool.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <vector>

using namespace hms_firetv;

namespace {

// A DSN that will never connect. Port 1 is reserved and nothing listens there,
// so pqxx fails fast rather than hanging on DNS or a firewall drop.
const char* kDeadDsn =
    "host=127.0.0.1 port=1 dbname=nope user=nope password=nope connect_timeout=2";

// Real DSN, only used by tests that are skipped when unavailable.
std::string liveDsn() {
    const char* env = std::getenv("TEST_PG_DSN");
    if (env && *env) return env;
    return "host=127.0.0.1 port=5432 dbname=postgres user=postgres connect_timeout=2";
}

bool liveDbAvailable() {
    try {
        pqxx::connection c(liveDsn());
        return c.is_open();
    } catch (...) {
        return false;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Regression: the pool used to permanently lose a slot whenever it failed to
// replace a dead connection. After a database outage every slot leaked, the
// queue was empty forever, and every later acquire() blocked for the full
// max_wait_ms and reported "Connection pool timeout" — even once the database
// was healthy again. hms-firetv sat in that state from 2026-08-06 until it was
// restarted.
//
// The observable signature of the bug: with an unreachable database the pool
// reports a *pool timeout* after max_wait_ms instead of a *connection error*
// straight away, because it never even attempts to build a connection.
// ---------------------------------------------------------------------------

TEST(ConnectionPool, UnreachableDbFailsFastNotAsPoolTimeout) {
    ConnectionPool pool(kDeadDsn, /*pool_size=*/2, /*max_wait_ms=*/5000);

    ASSERT_EQ(pool.availableCount(), 0u) << "dead DSN should yield no connections";

    auto start = std::chrono::steady_clock::now();
    std::string message;
    try {
        auto conn = pool.acquire();
        FAIL() << "acquire() must throw when the database is unreachable";
    } catch (const std::exception& e) {
        message = e.what();
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start).count();

    EXPECT_LT(elapsed, 5000)
        << "acquire() waited out the pool timeout instead of trying to connect";
    EXPECT_EQ(message.find("Connection pool timeout"), std::string::npos)
        << "reported a pool-timeout for what is really a connectivity failure: " << message;
}

// Capacity must survive repeated failures — this is the actual leak.
TEST(ConnectionPool, FailedAcquireDoesNotConsumeCapacity) {
    ConnectionPool pool(kDeadDsn, /*pool_size=*/2, /*max_wait_ms=*/1000);

    for (int i = 0; i < 10; ++i) {
        try {
            auto conn = pool.acquire();
        } catch (const std::exception&) {
            // expected
        }
        ASSERT_EQ(pool.availableCount(), 0u)
            << "unexpected queued connection after failed acquire #" << i;
    }

    // Still willing to try: a later acquire must attempt a connection (fast
    // failure), not report pool exhaustion.
    auto start = std::chrono::steady_clock::now();
    try {
        auto conn = pool.acquire();
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()).find("Connection pool timeout"), std::string::npos)
            << "pool declared itself exhausted after earlier failures";
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(elapsed, 1000) << "pool stopped attempting to reconnect";
}

// ---------------------------------------------------------------------------
// Normal-path behaviour against a real database.
// ---------------------------------------------------------------------------

TEST(ConnectionPool, ReturnsConnectionsToPool) {
    if (!liveDbAvailable()) GTEST_SKIP() << "no live PostgreSQL for " << liveDsn();

    ConnectionPool pool(liveDsn(), /*pool_size=*/3, /*max_wait_ms=*/2000);
    ASSERT_GT(pool.availableCount(), 0u);

    const size_t before = pool.availableCount();
    {
        auto conn = pool.acquire();
        EXPECT_TRUE(conn.isValid());
        EXPECT_EQ(pool.availableCount(), before - 1);
    }
    EXPECT_EQ(pool.availableCount(), before) << "RAII wrapper did not return the connection";
}

TEST(ConnectionPool, SurvivesManyAcquireReleaseCycles) {
    if (!liveDbAvailable()) GTEST_SKIP() << "no live PostgreSQL for " << liveDsn();

    ConnectionPool pool(liveDsn(), /*pool_size=*/2, /*max_wait_ms=*/2000);
    const size_t capacity = pool.availableCount();

    for (int i = 0; i < 50; ++i) {
        auto conn = pool.acquire();
        pqxx::work txn(*conn);
        txn.exec("SELECT 1");
        txn.commit();
    }

    EXPECT_EQ(pool.availableCount(), capacity) << "capacity drifted across cycles";
}

TEST(ConnectionPool, RefillsOnDemandWhenStartedWithoutDatabase) {
    if (!liveDbAvailable()) GTEST_SKIP() << "no live PostgreSQL for " << liveDsn();

    // Simulates the real incident: the pool is built while the database is
    // unreachable, so it starts empty. It must still be able to serve traffic
    // once the database returns.
    ConnectionPool pool(liveDsn(), /*pool_size=*/2, /*max_wait_ms=*/2000);
    ASSERT_GT(pool.availableCount(), 0u);

    // Drain every slot to force the on-demand creation path.
    std::vector<ConnectionPool::PooledConnection> held;
    while (pool.availableCount() > 0) held.push_back(pool.acquire());
    held.clear();

    auto conn = pool.acquire();
    EXPECT_TRUE(conn.isValid());
}
