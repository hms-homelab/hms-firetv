#pragma once

#include <list>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>
#include <optional>

namespace hms_firetv {

/**
 * Thread-safe LRU Cache with TTL support
 *
 * Features:
 * - Fixed maximum size with LRU eviction
 * - Time-to-live (TTL) for entries
 * - Thread-safe operations
 * - O(1) get and put operations
 */
template<typename K, typename V>
class LRUCache {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    /**
     * Constructor
     * @param max_size Maximum number of entries (default: 100)
     * @param ttl_seconds Time-to-live in seconds (default: 3600 = 1 hour)
     */
    explicit LRUCache(size_t max_size = 100, int ttl_seconds = 3600)
        : max_size_(max_size), ttl_(std::chrono::seconds(ttl_seconds)) {}

    /**
     * Get value from cache
     * @param key The key to look up
     * @return Optional value if found and not expired
     */
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return std::nullopt;
        }

        // Check if expired
        if (isExpired(it->second.expiry_time)) {
            evict(key);
            return std::nullopt;
        }

        // Move to front (most recently used)
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.list_it);

        return it->second.value;
    }

    /**
     * Put value into cache
     * @param key The key
     * @param value The value to store
     */
    void put(const K& key, const V& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(key);

        if (it != cache_map_.end()) {
            // Update existing entry
            it->second.value = value;
            it->second.expiry_time = Clock::now() + ttl_;
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second.list_it);
        } else {
            // Evict if at capacity
            if (cache_map_.size() >= max_size_) {
                evictLRU();
            }

            // Insert new entry at front
            lru_list_.push_front(key);
            CacheEntry entry{
                value,
                Clock::now() + ttl_,
                lru_list_.begin()
            };
            cache_map_.emplace(key, std::move(entry));
        }
    }

    /**
     * Remove entry from cache
     * @param key The key to remove
     */
    void remove(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        evict(key);
    }

    /**
     * Clear all entries
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_map_.clear();
        lru_list_.clear();
    }

    /**
     * Get current size
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_map_.size();
    }

    /**
     * Check if key exists and is not expired
     */
    bool contains(const K& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        return !isExpired(it->second.expiry_time);
    }

    /**
     * Clean up expired entries
     * Call periodically from a background thread
     */
    size_t cleanupExpired() {
        std::lock_guard<std::mutex> lock(mutex_);

        size_t removed = 0;
        auto now = Clock::now();

        // Iterate from LRU (back) to MRU (front)
        auto it = lru_list_.rbegin();
        while (it != lru_list_.rend()) {
            const K& key = *it;
            auto cache_it = cache_map_.find(key);

            if (cache_it != cache_map_.end() && isExpired(cache_it->second.expiry_time)) {
                // Get forward iterator from reverse iterator
                auto forward_it = std::next(it).base();
                evictNoLock(key);
                removed++;
                // Continue from the same position (which is now the next element)
                it = std::reverse_iterator<typename std::list<K>::iterator>(forward_it);
            } else {
                ++it;
            }
        }

        return removed;
    }

private:
    struct CacheEntry {
        V value;
        TimePoint expiry_time;
        typename std::list<K>::iterator list_it;
    };

    bool isExpired(const TimePoint& expiry_time) const {
        return Clock::now() >= expiry_time;
    }

    void evictLRU() {
        if (lru_list_.empty()) return;

        const K& key = lru_list_.back();
        cache_map_.erase(key);
        lru_list_.pop_back();
    }

    void evict(const K& key) {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            lru_list_.erase(it->second.list_it);
            cache_map_.erase(it);
        }
    }

    void evictNoLock(const K& key) {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            lru_list_.erase(it->second.list_it);
            cache_map_.erase(it);
        }
    }

    size_t max_size_;
    std::chrono::seconds ttl_;
    mutable std::mutex mutex_;
    std::list<K> lru_list_;  // Front = MRU, Back = LRU
    std::unordered_map<K, CacheEntry> cache_map_;
};

} // namespace hms_firetv
