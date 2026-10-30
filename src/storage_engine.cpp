#include "storage_engine.hpp"
#include <fstream>
#include <iostream>

// ============================================================================
// CacheEntry Member Implementations
// ============================================================================

int64_t CacheEntry::remaining_ttl_seconds() const {
    auto now = std::chrono::steady_clock::now();
    if (now >= expires_at) {
        return 0;
    }
    auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(expires_at - now).count();
    return (diff_ms + 999) / 1000; // Ceiling to full seconds
}

bool CacheEntry::is_expired() const {
    return std::chrono::steady_clock::now() >= expires_at;
}

// ============================================================================
// StorageEngine Member Implementations
// ============================================================================

StorageEngine::StorageEngine(size_t max_capacity)
    : max_capacity_(max_capacity > 0 ? max_capacity : 1000) {}

CacheVerdict StorageEngine::get(const std::string& domain, uint16_t qtype, uint16_t qclass,
                                std::vector<DNSResourceRecord>& out_records, uint8_t& out_rcode) {
    WriteLockGuard guard(lock_);

    std::string norm = ProtocolEngine::normalize_domain(domain);
    CacheKey key{norm, qtype, qclass};

    auto it = hash_index_.find(key);
    if (it == hash_index_.end()) {
        stats_.misses++;
        return CacheVerdict::MISS;
    }

    auto list_it = it->second;
    if (list_it->is_expired()) {
        stats_.expirations++;
        stats_.misses++;
        lru_list_.erase(list_it);
        hash_index_.erase(it);
        return CacheVerdict::EXPIRED;
    }

    // Dynamic TTL recalculation: TTL_rem = ExpiresAt - now
    int64_t rem_ttl = list_it->remaining_ttl_seconds();
    if (rem_ttl <= 0) {
        stats_.expirations++;
        stats_.misses++;
        lru_list_.erase(list_it);
        hash_index_.erase(it);
        return CacheVerdict::EXPIRED;
    }

    // Populate output records with decaying remaining TTL
    out_records = list_it->records;
    for (auto& rr : out_records) {
        rr.ttl = static_cast<uint32_t>(rem_ttl);
    }
    out_rcode = list_it->rcode;

    // Promote node to MRU Head of Doubly-Linked List in O(1) time
    if (list_it != lru_list_.begin()) {
        lru_list_.splice(lru_list_.begin(), lru_list_, list_it);
    }

    if (list_it->is_negative) {
        stats_.negative_hits++;
        return CacheVerdict::NEGATIVE_HIT;
    }

    stats_.hits++;
    return CacheVerdict::HIT;
}

void StorageEngine::put(const std::string& domain, uint16_t qtype, uint16_t qclass,
                        const std::vector<DNSResourceRecord>& records, uint32_t ttl,
                        bool is_negative, uint8_t rcode) {
    WriteLockGuard guard(lock_);

    // Bound minimal TTL to 1 second to avoid instant death
    if (ttl == 0) {
        ttl = 1;
    }

    std::string norm = ProtocolEngine::normalize_domain(domain);
    CacheKey key{norm, qtype, qclass};
    auto now = std::chrono::steady_clock::now();
    auto expires_at = now + std::chrono::seconds(ttl);
    uint64_t version = ++next_version_;

    auto it = hash_index_.find(key);
    if (it != hash_index_.end()) {
        // Entry exists: update in place and move to Head
        auto list_it = it->second;
        list_it->records = records;
        list_it->original_ttl = ttl;
        list_it->expires_at = expires_at;
        list_it->is_negative = is_negative;
        list_it->rcode = rcode;
        list_it->version = version;

        if (list_it != lru_list_.begin()) {
            lru_list_.splice(lru_list_.begin(), lru_list_, list_it);
        }

        min_heap_.push({expires_at, key, version});
        return;
    }

    // Proactively purge any expired records before checking capacity
    purge_expired_locked();

    // Capacity eviction: if limit reached, evict LRU tail in O(1) time
    while (hash_index_.size() >= max_capacity_ && !lru_list_.empty()) {
        evict_lru_tail_locked();
    }

    // Insert new node at Head of Doubly-Linked List (MRU)
    CacheEntry entry;
    entry.key = key;
    entry.records = records;
    entry.original_ttl = ttl;
    entry.expires_at = expires_at;
    entry.is_negative = is_negative;
    entry.rcode = rcode;
    entry.version = version;

    lru_list_.push_front(std::move(entry));
    hash_index_[key] = lru_list_.begin();

    // Push into Min-Heap for O(log N) proactive TTL sweep
    min_heap_.push({expires_at, key, version});
}

CacheVerdict StorageEngine::peek(const std::string& domain, uint16_t qtype, uint16_t qclass,
                                 std::vector<DNSResourceRecord>& out_records, uint8_t& out_rcode) const {
    ReadLockGuard guard(lock_);

    std::string norm = ProtocolEngine::normalize_domain(domain);
    CacheKey key{norm, qtype, qclass};

    auto it = hash_index_.find(key);
    if (it == hash_index_.end()) {
        return CacheVerdict::MISS;
    }

    auto list_it = it->second;
    if (list_it->is_expired()) {
        return CacheVerdict::EXPIRED;
    }

    int64_t rem_ttl = list_it->remaining_ttl_seconds();
    if (rem_ttl <= 0) {
        return CacheVerdict::EXPIRED;
    }

    out_records = list_it->records;
    for (auto& rr : out_records) {
        rr.ttl = static_cast<uint32_t>(rem_ttl);
    }
    out_rcode = list_it->rcode;

    return list_it->is_negative ? CacheVerdict::NEGATIVE_HIT : CacheVerdict::HIT;
}

void StorageEngine::evict_lru_tail_locked() {
    if (lru_list_.empty()) return;

    CacheKey tail_key = lru_list_.back().key;
    hash_index_.erase(tail_key);
    lru_list_.pop_back();
    stats_.evictions++;
}

void StorageEngine::purge_expired_locked() {
    auto now = std::chrono::steady_clock::now();

    while (!min_heap_.empty()) {
        const auto& top = min_heap_.top();
        if (top.expires_at > now) {
            break; // Earliest record is still unexpired
        }

        // Pop expired entry from heap in O(log N)
        MinHeapNode node = min_heap_.top();
        min_heap_.pop();

        auto it = hash_index_.find(node.key);
        if (it != hash_index_.end()) {
            auto list_it = it->second;
            // Only purge if versions match (prevents purges on updated nodes)
            if (list_it->version == node.version && list_it->is_expired()) {
                lru_list_.erase(list_it);
                hash_index_.erase(it);
                stats_.expirations++;
            }
        }
    }
}

size_t StorageEngine::purge_expired() {
    WriteLockGuard guard(lock_);
    size_t before = hash_index_.size();
    purge_expired_locked();
    size_t after = hash_index_.size();
    return before - after;
}

size_t StorageEngine::size() const {
    ReadLockGuard guard(lock_);
    return hash_index_.size();
}

size_t StorageEngine::capacity() const {
    ReadLockGuard guard(lock_);
    return max_capacity_;
}

void StorageEngine::set_capacity(size_t new_capacity) {
    WriteLockGuard guard(lock_);
    max_capacity_ = new_capacity > 0 ? new_capacity : 1;
    while (hash_index_.size() > max_capacity_ && !lru_list_.empty()) {
        evict_lru_tail_locked();
    }
}

void StorageEngine::clear() {
    WriteLockGuard guard(lock_);
    hash_index_.clear();
    lru_list_.clear();
    while (!min_heap_.empty()) min_heap_.pop();
}

CacheStats StorageEngine::get_stats() const {
    ReadLockGuard guard(lock_);
    return stats_;
}

void StorageEngine::reset_stats() {
    WriteLockGuard guard(lock_);
    stats_ = CacheStats();
}

// ============================================================================
// Snapshot Durability (Persistence to disk)
// ============================================================================

static const char SNAPSHOT_MAGIC[8] = {'D', 'N', 'S', 'C', 'A', 'C', 'H', 'E'};

bool StorageEngine::save_snapshot(const std::string& filepath) const {
    ReadLockGuard guard(lock_);

    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    // Write magic header
    out.write(SNAPSHOT_MAGIC, 8);

    // Count valid records
    uint32_t count = 0;
    for (const auto& entry : lru_list_) {
        if (!entry.is_expired() && entry.remaining_ttl_seconds() > 0) {
            count++;
        }
    }
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& entry : lru_list_) {
        if (entry.is_expired()) continue;
        int64_t rem_ttl = entry.remaining_ttl_seconds();
        if (rem_ttl <= 0) continue;

        // Domain
        uint16_t dlen = static_cast<uint16_t>(entry.key.domain.size());
        out.write(reinterpret_cast<const char*>(&dlen), sizeof(dlen));
        out.write(entry.key.domain.data(), dlen);

        // QTYPE, QCLASS, remaining TTL
        out.write(reinterpret_cast<const char*>(&entry.key.qtype), sizeof(entry.key.qtype));
        out.write(reinterpret_cast<const char*>(&entry.key.qclass), sizeof(entry.key.qclass));
        uint32_t ttl_u32 = static_cast<uint32_t>(rem_ttl);
        out.write(reinterpret_cast<const char*>(&ttl_u32), sizeof(ttl_u32));

        // Negative cache flags
        uint8_t neg = entry.is_negative ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&neg), sizeof(neg));
        out.write(reinterpret_cast<const char*>(&entry.rcode), sizeof(entry.rcode));

        // RRs
        uint16_t rrcount = static_cast<uint16_t>(entry.records.size());
        out.write(reinterpret_cast<const char*>(&rrcount), sizeof(rrcount));

        for (const auto& rr : entry.records) {
            uint16_t nlen = static_cast<uint16_t>(rr.name.size());
            out.write(reinterpret_cast<const char*>(&nlen), sizeof(nlen));
            out.write(rr.name.data(), nlen);

            out.write(reinterpret_cast<const char*>(&rr.type), sizeof(rr.type));
            out.write(reinterpret_cast<const char*>(&rr.rclass), sizeof(rr.rclass));
            out.write(reinterpret_cast<const char*>(&rr.ttl), sizeof(rr.ttl));

            uint16_t rdlen = static_cast<uint16_t>(rr.rdata.size());
            out.write(reinterpret_cast<const char*>(&rdlen), sizeof(rdlen));
            if (rdlen > 0) {
                out.write(reinterpret_cast<const char*>(rr.rdata.data()), rdlen);
            }

            uint16_t rdata_str_len = static_cast<uint16_t>(rr.readable_data.size());
            out.write(reinterpret_cast<const char*>(&rdata_str_len), sizeof(rdata_str_len));
            if (rdata_str_len > 0) {
                out.write(rr.readable_data.data(), rdata_str_len);
            }
        }
    }

    return out.good();
}

bool StorageEngine::load_snapshot(const std::string& filepath) {
    WriteLockGuard guard(lock_);

    std::ifstream in(filepath, std::ios::binary);
    if (!in.is_open()) return false;

    char magic[8];
    in.read(magic, 8);
    if (!in.good() || std::memcmp(magic, SNAPSHOT_MAGIC, 8) != 0) {
        return false;
    }

    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in.good()) return false;

    for (uint32_t i = 0; i < count; ++i) {
        uint16_t dlen = 0;
        in.read(reinterpret_cast<char*>(&dlen), sizeof(dlen));
        std::string domain(dlen, '\0');
        in.read(&domain[0], dlen);

        uint16_t qtype = 0, qclass = 0;
        uint32_t ttl = 0;
        in.read(reinterpret_cast<char*>(&qtype), sizeof(qtype));
        in.read(reinterpret_cast<char*>(&qclass), sizeof(qclass));
        in.read(reinterpret_cast<char*>(&ttl), sizeof(ttl));

        uint8_t neg = 0, rcode = 0;
        in.read(reinterpret_cast<char*>(&neg), sizeof(neg));
        in.read(reinterpret_cast<char*>(&rcode), sizeof(rcode));

        uint16_t rrcount = 0;
        in.read(reinterpret_cast<char*>(&rrcount), sizeof(rrcount));

        std::vector<DNSResourceRecord> rrs;
        for (uint16_t j = 0; j < rrcount; ++j) {
            DNSResourceRecord rr;
            uint16_t nlen = 0;
            in.read(reinterpret_cast<char*>(&nlen), sizeof(nlen));
            rr.name.resize(nlen);
            in.read(&rr.name[0], nlen);

            in.read(reinterpret_cast<char*>(&rr.type), sizeof(rr.type));
            in.read(reinterpret_cast<char*>(&rr.rclass), sizeof(rr.rclass));
            in.read(reinterpret_cast<char*>(&rr.ttl), sizeof(rr.ttl));

            uint16_t rdlen = 0;
            in.read(reinterpret_cast<char*>(&rdlen), sizeof(rdlen));
            rr.rdlength = rdlen;
            if (rdlen > 0) {
                rr.rdata.resize(rdlen);
                in.read(reinterpret_cast<char*>(rr.rdata.data()), rdlen);
            }

            uint16_t rdata_str_len = 0;
            in.read(reinterpret_cast<char*>(&rdata_str_len), sizeof(rdata_str_len));
            if (rdata_str_len > 0) {
                rr.readable_data.resize(rdata_str_len);
                in.read(&rr.readable_data[0], rdata_str_len);
            }

            rrs.push_back(rr);
        }

        if (ttl > 0 && in.good()) {
            CacheKey key{domain, qtype, qclass};
            auto now = std::chrono::steady_clock::now();
            auto expires_at = now + std::chrono::seconds(ttl);
            uint64_t version = ++next_version_;

            CacheEntry entry;
            entry.key = key;
            entry.records = rrs;
            entry.original_ttl = ttl;
            entry.expires_at = expires_at;
            entry.is_negative = (neg != 0);
            entry.rcode = rcode;
            entry.version = version;

            lru_list_.push_front(std::move(entry));
            hash_index_[key] = lru_list_.begin();
            min_heap_.push({expires_at, key, version});
        }
    }

    // Ensure capacity bound is strictly enforced after loading snapshot
    while (hash_index_.size() > max_capacity_ && !lru_list_.empty()) {
        evict_lru_tail_locked();
    }

    return true;
}
