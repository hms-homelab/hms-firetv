# HMS-FireTV Code Review: Memory Leaks, Deadlocks, and Blocking Calls

**Review Date:** February 15, 2026
**Reviewer:** Claude (Sonnet 4.5)
**Scope:** Memory management, concurrency safety, performance bottlenecks

---

## CRITICAL ISSUES

### 1. **Database Service Mutex Deadlock Risk** ⚠️ CRITICAL
**Location:** `src/services/DatabaseService.cpp:53, 117`

**Problem:**
- Single global mutex locks ALL database operations
- Sleep operations (1-4 seconds) happen while mutex is held (lines 73, 106)
- Database reconnection happens while mutex is held (line 67)
- Blocking transactions happen while mutex is held (line 83)

**Impact:**
- Under load, ALL HTTP threads block waiting for the database mutex
- A single slow query blocks the entire application
- Retry logic can cause 7-second total blocks (1s + 2s + 4s)
- Connection pool of 1 (single conn_) creates severe bottleneck

**Example Scenario:**
```
Thread 1: Holds mutex, executes 2-second query
Thread 2-8: Blocked waiting for mutex
Result: Max throughput = 0.5 requests/second
```

**Recommendation:**
- Use connection pooling (pqxx::connection_pool or custom pool)
- Remove mutex from query execution (only protect pool access)
- Move retry logic outside of mutex-protected sections
- Consider async database library (libpq with async APIs)

---

### 2. **Triple Blocking in HTTP Request Handlers** 🐌 CRITICAL
**Location:** `src/api/CommandController.cpp:60, 90, 95`

**Problem:**
Each command request does 3 sequential blocking operations:
1. `getClient()` → `getDeviceById()` → Database query (~10-50ms)
2. `sendNavigationCommand()` → CURL HTTP call to Fire TV (~100-500ms)
3. `logCommand()` → Database INSERT (~10-50ms)

**Total blocking time:** 120-600ms per request, during which the HTTP thread cannot handle other requests.

**Impact:**
- With 8 HTTP threads and 200ms average block time → Max 40 req/sec
- Fire TV timeout (5 seconds) can block a thread for 5+ seconds
- Under load, all threads become blocked, new requests queue/timeout

**Code Path:**
```
HTTP Request → DeviceController → getClient() [DB BLOCK]
                                → sendNavigationCommand() [HTTP BLOCK]
                                → logCommand() [DB BLOCK]
                                → Response
```

**Recommendation:**
- Move Fire TV HTTP calls to async (use Drogon's HttpClient for async)
- Queue command logging to background thread
- Cache device info to avoid repeated DB queries
- Consider request timeouts to prevent thread starvation

---

### 3. **Memory Leak in Client Cache** 💧 HIGH
**Location:** `src/api/CommandController.cpp:447`

**Problem:**
```cpp
clients_[device_id] = client;  // Line 447
```
- LightningClient instances are cached indefinitely
- No eviction, no TTL, no cleanup
- Deleted devices leave orphaned clients in memory
- IP address changes create duplicate clients

**Impact:**
- Memory grows linearly with unique device_ids ever accessed
- Each LightningClient holds a CURL handle (~4KB + buffers)
- 1000 device accesses = ~4-10 MB leaked

**Recommendation:**
- Implement LRU cache with size limit (e.g., 100 entries)
- Add TTL-based eviction (e.g., 1 hour)
- Clear cache on device deletion/update
- Use std::weak_ptr to allow GC when not in use

---

## HIGH PRIORITY ISSUES

### 4. **Synchronous CURL Blocks HTTP Threads** 🐌 HIGH
**Location:** `src/clients/LightningClient.cpp:427`

**Problem:**
```cpp
CURLcode res = curl_easy_perform(curl_);  // BLOCKING
```
- All Fire TV API calls use blocking synchronous CURL
- Default timeout: 5 seconds
- Network issues can block threads for full timeout duration

**Recommendation:**
- Use Drogon's async HttpClient instead of raw CURL
- Implement request timeout monitoring
- Consider circuit breaker pattern for failing devices

---

### 5. **No Connection Pool for Database** 📊 HIGH
**Location:** `src/services/DatabaseService.cpp:36`

**Problem:**
```cpp
conn_ = std::make_unique<pqxx::connection>(connection_string_);
```
- Single PostgreSQL connection shared across all threads
- Must be protected by mutex (Issue #1)
- No parallelism for database operations

**Recommendation:**
- Implement connection pool (minimum: thread_count connections)
- Use pqxx's built-in connection pooling or custom implementation
- Allow concurrent queries from multiple threads

---

## MEDIUM PRIORITY ISSUES

### 6. **Database Reconnection Blocks All Operations**
**Location:** `src/services/DatabaseService.cpp:67`

Reconnection happens while holding the global mutex, blocking ALL database access during recovery.

**Recommendation:**
- Reconnect on a separate connection object
- Swap in atomically once ready
- Allow other queries to fail fast during reconnection

---

### 7. **No Request Cancellation**
All blocking operations (DB, HTTP) cannot be cancelled. If a Fire TV device is unreachable, the thread blocks for the full 5-second timeout.

**Recommendation:**
- Implement request cancellation tokens
- Add circuit breaker for repeatedly failing devices
- Use shorter timeouts (1-2 seconds) for initial attempts

---

### 8. **Missing RAII for Database Transactions**
**Location:** Multiple locations in repositories

Transactions are manually committed but not automatically rolled back on exception.

**Recommendation:**
- Use RAII wrappers for transactions
- Ensure rollback on scope exit if not committed

---

## MQTT ISSUES (SEPARATE BUG)

### 9. **MQTT Callbacks Not Firing**
**Status:** Under investigation

**Problem:**
- Paho async_client subscribes successfully
- TCP connection is ESTAB
- But `set_message_callback()` lambda never invokes
- Messages published by broker are not received

**Possible Causes:**
- Drogon's event loop may interfere with Paho's callback thread
- Paho C++ async client may require explicit consumer start
- Message delivery thread may not be running

**Recommendation:**
- File as separate bug for investigation
- Consider switching to mosquitto C library with manual event loop
- Test with standalone Paho example to isolate issue

---

## PERFORMANCE IMPACT SUMMARY

### Current Architecture Limitations:

| Scenario | Current Performance | Bottleneck |
|----------|-------------------|------------|
| Single request | 120-600ms | Triple blocking (DB + HTTP + DB) |
| Concurrent load | ~40 req/sec | Database mutex serialization |
| Fire TV timeout | 5+ second block | Synchronous CURL with long timeout |
| Memory growth | Unbounded | Client cache never evicts |

### With Fixes Applied:

| Improvement | Expected Gain |
|-------------|--------------|
| Async Fire TV calls | 10-20x throughput |
| Connection pooling | 5-10x DB throughput |
| Remove mutex from queries | Linear scaling with threads |
| Background logging | 20-50ms latency reduction |

**Estimated Total Improvement:** 50-100x throughput under concurrent load

---

## RECOMMENDED ACTION PLAN

### Phase 1: Critical Fixes (1-2 days)
1. Implement database connection pool
2. Remove mutex from query execution
3. Make Fire TV API calls async (use Drogon HttpClient)
4. Move command logging to background thread

### Phase 2: High Priority (2-3 days)
5. Implement LRU cache with eviction for clients
6. Add request timeouts and cancellation
7. Improve error handling and circuit breaker logic

### Phase 3: Medium Priority (1-2 days)
8. RAII wrappers for transactions
9. Metrics and monitoring for blocking operations
10. Investigate and fix MQTT callback issue

**Total Estimated Effort:** 4-7 days of development

---

## TESTING RECOMMENDATIONS

1. **Load Testing:** Use `wrk` or `ab` to test concurrent requests
2. **Timeout Testing:** Simulate slow Fire TV devices
3. **Memory Profiling:** Run with valgrind/heaptrack for 24+ hours
4. **Mutex Analysis:** Use ThreadSanitizer to detect deadlocks

---

**Conclusion:** The codebase has several critical performance and correctness issues stemming from synchronous blocking operations and lack of connection pooling. With the recommended fixes, the service could handle 50-100x more concurrent load with lower latency.
