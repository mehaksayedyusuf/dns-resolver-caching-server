#ifndef STORAGE_ENGINE_HPP
#define STORAGE_ENGINE_HPP

#include "common.hpp"
#include "protocol_engine.hpp"
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <queue>
#include <chrono>
#include <memory>

// ============================================================================
// ENGINE E2: In-Memory Storage & Cache Engine
// Author/Lead: Mehak Sayed Yusuf (DAA & DBMS Modules)
// Dual-Index Architecture: O(1) LRU Eviction + O(log N) Min-Heap TTL Purge
// ============================================================================

// Composite Primary Key for DNS Cache
struct CacheKey {
    std::string domain;           // Normalized lowercase domain name (e.g. "example.com")
    uint16_t qtype = QTYPE_A;     // DNS query type (A, AAAA, CNAME, etc.)
    uint16_t qclass = QCLASS_IN;  // DNS query class (IN = 1)

    bool operator==(const CacheKey& other) const {
        return qtype == other.qtype && qclass == other.qclass && domain == other.domain;
    }
};

// Hash Functor for CacheKey
struct CacheKeyHash {
    std::size_t operator()(const CacheKey& k) const {
        std::size_t h1 = std::hash<std::string>{}(k.domain);
        std::size_t h2 = std::hash<uint16_t>{}(k.qtype);
        std::size_t h3 = std::hash<uint16_t>{}(k.qclass);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// Cache Lookup Verdict
enum class CacheVerdict {
    HIT,          // Positive cache hit with active, valid records
    MISS,         // Record not found in cache
    EXPIRED,      // Record was present but TTL has expired
    NEGATIVE_HIT  // RFC 2308 negative cache hit (e.g. NXDOMAIN)
};

// Cache Node Stored in Doubly-Linked LRU List
struct CacheEntry {
    CacheKey key;
    std::vector<DNSResourceRecord> records;
    uint32_t original_ttl = 0;                     // Original TTL from upstream in seconds
    std::chrono::steady_clock::time_point expires_at; // Absolute expiration timestamp
    bool is_negative = false;                       // True if NXDOMAIN / negative response
    uint8_t rcode = RCODE_NOERROR;                 // Response code (e.g. NXDOMAIN = 3)
    uint64_t version = 0;                          // Monotonic version counter for heap sync

    int64_t remaining_ttl_seconds() const;
    bool is_expired() const;
};

// Min-Heap Node for Proactive O(log N) TTL Expiration Sweeps
struct MinHeapNode {
    std::chrono::steady_clock::time_point expires_at;
    CacheKey key;
    uint64_t version = 0;

    bool operator>(const MinHeapNode& other) const {
        return expires_at > other.expires_at; // Min-Heap: earliest expiration on top
    }
};

// Live Telemetry Counters for Storage Engine
struct CacheStats {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t expirations = 0;
    uint64_t evictions = 0;
    uint64_t negative_hits = 0;

    double hit_ratio() const {
        uint64_t total = hits + misses + negative_hits;
        if (total == 0) return 0.0;
        return (static_cast<double>(hits + negative_hits) / static_cast<double>(total)) * 100.0;
    }
};

class StorageEngine {
public:
    explicit StorageEngine(size_t max_capacity = 1000);
    ~StorageEngine() = default;

    // Prevent copies (RAII lock safety)
    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    // Core Cache Operations
    CacheVerdict get(const std::string& domain, uint16_t qtype, uint16_t qclass,
                     std::vector<DNSResourceRecord>& out_records, uint8_t& out_rcode);

    void put(const std::string& domain, uint16_t qtype, uint16_t qclass,
             const std::vector<DNSResourceRecord>& records, uint32_t ttl,
             bool is_negative = false, uint8_t rcode = RCODE_NOERROR);

    // Non-mutating peek for telemetry / diagnostics (does not alter LRU recency)
    CacheVerdict peek(const std::string& domain, uint16_t qtype, uint16_t qclass,
                      std::vector<DNSResourceRecord>& out_records, uint8_t& out_rcode) const;

    // Proactive Min-Heap TTL sweep: purges all entries with expires_at <= now
    size_t purge_expired();

    // Capacity & Management
    size_t size() const;
    size_t capacity() const;
    void set_capacity(size_t new_capacity);
    void clear();

    // Snapshot Durability (Persistence to dns_cache.db)
    bool save_snapshot(const std::string& filepath) const;
    bool load_snapshot(const std::string& filepath);

    // Metrics & Diagnostics
    CacheStats get_stats() const;
    void reset_stats();

private:
    size_t max_capacity_;
    uint64_t next_version_ = 1;

    // Dual-Index Structures:
    // 1. Doubly-linked list for O(1) LRU recency tracking (Head = MRU, Tail = LRU)
    std::list<CacheEntry> lru_list_;

    // 2. Hash map for O(1) key-to-iterator lookups
    std::unordered_map<CacheKey, std::list<CacheEntry>::iterator, CacheKeyHash> hash_index_;

    // 3. Min-Heap priority queue for O(log N) proactive TTL expiration
    std::priority_queue<MinHeapNode, std::vector<MinHeapNode>, std::greater<MinHeapNode>> min_heap_;

    // Concurrency: Win32 SRWLOCK (Multi-reader single-writer)
    mutable SharedMutex lock_;

    // Internal Telemetry
    CacheStats stats_;

    // Internal helpers (called under lock)
    void evict_lru_tail_locked();
    void purge_expired_locked();
};

#endif // STORAGE_ENGINE_HPP
