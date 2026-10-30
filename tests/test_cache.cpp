#include "storage_engine.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << COLOR_RED << "  [FAIL] " << msg << " (line " << __LINE__ << ")" << COLOR_RESET << std::endl; \
        return false; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    g_tests_run++; \
    std::cout << COLOR_CYAN << "[RUN] " << #fn << COLOR_RESET << "... "; \
    if (fn()) { \
        g_tests_passed++; \
        std::cout << COLOR_GREEN << "PASSED" << COLOR_RESET << std::endl; \
    } else { \
        std::cout << COLOR_RED << "FAILED" << COLOR_RESET << std::endl; \
    } \
} while(0)

// ============================================================================
// Test 1: Basic Cache Hit, Miss, and Case Normalization
// ============================================================================
bool test_basic_hit_and_miss() {
    StorageEngine cache(100);

    std::vector<DNSResourceRecord> rrs;
    uint8_t rcode = 0;

    // Initial query on empty cache -> MISS
    CacheVerdict v = cache.get("example.com", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v == CacheVerdict::MISS, "Expected MISS on non-existent domain");

    // Insert record with mixed case "ExAmPlE.CoM"
    std::vector<DNSResourceRecord> records = {
        DNSResourceRecord::create_a("example.com", 300, "93.184.216.34")
    };
    cache.put("ExAmPlE.CoM", QTYPE_A, QCLASS_IN, records, 300);

    TEST_ASSERT(cache.size() == 1, "Cache size should be 1 after insertion");

    // Lookup lowercase "example.com" -> HIT
    v = cache.get("example.com", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v == CacheVerdict::HIT, "Expected HIT on cached record");
    TEST_ASSERT(rrs.size() == 1, "Expected 1 resource record returned");
    TEST_ASSERT(rrs[0].readable_data == "93.184.216.34", "IP address mismatch");

    CacheStats stats = cache.get_stats();
    TEST_ASSERT(stats.hits == 1, "Stats hits count should be 1");
    TEST_ASSERT(stats.misses == 1, "Stats misses count should be 1");

    return true;
}

// ============================================================================
// Test 2: O(1) LRU Capacity Eviction
// ============================================================================
bool test_lru_capacity_eviction() {
    // Capacity = 3
    StorageEngine cache(3);

    auto make_record = [](const std::string& dom, const std::string& ip) {
        return std::vector<DNSResourceRecord>{ DNSResourceRecord::create_a(dom, 300, ip) };
    };

    // Insert a.com, b.com, c.com
    cache.put("a.com", QTYPE_A, QCLASS_IN, make_record("a.com", "1.1.1.1"), 300);
    cache.put("b.com", QTYPE_A, QCLASS_IN, make_record("b.com", "2.2.2.2"), 300);
    cache.put("c.com", QTYPE_A, QCLASS_IN, make_record("c.com", "3.3.3.3"), 300);

    TEST_ASSERT(cache.size() == 3, "Cache should contain 3 entries");

    // Insert 4th domain d.com -> causes LRU tail (a.com) to be evicted
    cache.put("d.com", QTYPE_A, QCLASS_IN, make_record("d.com", "4.4.4.4"), 300);
    TEST_ASSERT(cache.size() == 3, "Cache capacity should remain 3");

    std::vector<DNSResourceRecord> rrs;
    uint8_t rcode = 0;

    // a.com should now be evicted (MISS)
    CacheVerdict v_a = cache.get("a.com", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v_a == CacheVerdict::MISS, "Oldest entry a.com should have been evicted");

    // b.com, c.com, d.com should still be present (HIT)
    TEST_ASSERT(cache.get("b.com", QTYPE_A, QCLASS_IN, rrs, rcode) == CacheVerdict::HIT, "b.com should still exist");
    TEST_ASSERT(cache.get("c.com", QTYPE_A, QCLASS_IN, rrs, rcode) == CacheVerdict::HIT, "c.com should still exist");
    TEST_ASSERT(cache.get("d.com", QTYPE_A, QCLASS_IN, rrs, rcode) == CacheVerdict::HIT, "d.com should still exist");

    TEST_ASSERT(cache.get_stats().evictions == 1, "Eviction counter must be 1");

    return true;
}

// ============================================================================
// Test 3: O(1) LRU Promotion on Hit
// ============================================================================
bool test_lru_promotion_on_hit() {
    StorageEngine cache(3);

    auto make_record = [](const std::string& dom, const std::string& ip) {
        return std::vector<DNSResourceRecord>{ DNSResourceRecord::create_a(dom, 300, ip) };
    };

    cache.put("a.com", QTYPE_A, QCLASS_IN, make_record("a.com", "1.1.1.1"), 300);
    cache.put("b.com", QTYPE_A, QCLASS_IN, make_record("b.com", "2.2.2.2"), 300);
    cache.put("c.com", QTYPE_A, QCLASS_IN, make_record("c.com", "3.3.3.3"), 300);

    // Current recency list: [c.com, b.com, a.com]
    // Access a.com -> Promotes a.com to MRU Head: [a.com, c.com, b.com]
    std::vector<DNSResourceRecord> rrs;
    uint8_t rcode = 0;
    CacheVerdict v_a = cache.get("a.com", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v_a == CacheVerdict::HIT, "a.com should hit and be promoted");

    // Insert d.com -> Tail is now b.com! b.com should be evicted, NOT a.com!
    cache.put("d.com", QTYPE_A, QCLASS_IN, make_record("d.com", "4.4.4.4"), 300);

    // Verify b.com was evicted
    TEST_ASSERT(cache.get("b.com", QTYPE_A, QCLASS_IN, rrs, rcode) == CacheVerdict::MISS, "b.com should have been evicted");

    // Verify a.com was saved by promotion
    TEST_ASSERT(cache.get("a.com", QTYPE_A, QCLASS_IN, rrs, rcode) == CacheVerdict::HIT, "a.com must still be cached after promotion");

    return true;
}

// ============================================================================
// Test 4: Dynamic TTL Aging & Recalculation
// ============================================================================
bool test_dynamic_ttl_decay() {
    StorageEngine cache(10);

    std::vector<DNSResourceRecord> records = {
        DNSResourceRecord::create_a("timer.com", 2, "10.0.0.1")
    };
    cache.put("timer.com", QTYPE_A, QCLASS_IN, records, 2); // 2 second TTL

    std::vector<DNSResourceRecord> rrs;
    uint8_t rcode = 0;

    // Immediate lookup
    CacheVerdict v = cache.get("timer.com", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v == CacheVerdict::HIT, "Initial lookup should be HIT");
    TEST_ASSERT(rrs[0].ttl <= 2 && rrs[0].ttl >= 1, "TTL should be 1-2s initially");

    // Sleep 1.2s to verify decay
    Sleep(1200);

    v = cache.get("timer.com", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v == CacheVerdict::HIT, "Lookup after 1.2s should still be HIT");
    TEST_ASSERT(rrs[0].ttl == 1, "Remaining TTL should have decayed to 1s");

    // Sleep another 1.2s (total 2.4s > 2.0s TTL)
    Sleep(1200);

    v = cache.get("timer.com", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v == CacheVerdict::EXPIRED, "Entry should now report EXPIRED");

    return true;
}

// ============================================================================
// Test 5: O(log N) Min-Heap Proactive TTL Purge
// ============================================================================
bool test_minheap_proactive_ttl_purge() {
    StorageEngine cache(100);

    auto make_record = [](const std::string& dom, const std::string& ip, uint32_t ttl) {
        return std::vector<DNSResourceRecord>{ DNSResourceRecord::create_a(dom, ttl, ip) };
    };

    // quick.com expires in 1s; slow.com expires in 10s
    cache.put("quick.com", QTYPE_A, QCLASS_IN, make_record("quick.com", "1.1.1.1", 1), 1);
    cache.put("slow.com", QTYPE_A, QCLASS_IN, make_record("slow.com", "2.2.2.2", 10), 10);

    TEST_ASSERT(cache.size() == 2, "Initial size should be 2");

    // Sleep 1.2s so quick.com expires
    Sleep(1200);

    // Call proactive Min-Heap sweep
    size_t purged = cache.purge_expired();
    TEST_ASSERT(purged == 1, "purge_expired should have purged exactly 1 entry");
    TEST_ASSERT(cache.size() == 1, "Cache size should be 1 after purge");

    std::vector<DNSResourceRecord> rrs;
    uint8_t rcode = 0;
    TEST_ASSERT(cache.get("quick.com", QTYPE_A, QCLASS_IN, rrs, rcode) == CacheVerdict::MISS, "quick.com was purged");
    TEST_ASSERT(cache.get("slow.com", QTYPE_A, QCLASS_IN, rrs, rcode) == CacheVerdict::HIT, "slow.com should remain");

    return true;
}

// ============================================================================
// Test 6: RFC 2308 Negative Caching (NXDOMAIN)
// ============================================================================
bool test_negative_caching_rfc2308() {
    StorageEngine cache(50);

    // Cache NXDOMAIN for "nonexistent.example" with 60s TTL
    std::vector<DNSResourceRecord> empty_rrs;
    cache.put("nonexistent.example", QTYPE_A, QCLASS_IN, empty_rrs, 60, true, RCODE_NXDOMAIN);

    std::vector<DNSResourceRecord> rrs;
    uint8_t rcode = 0;

    CacheVerdict v = cache.get("nonexistent.example", QTYPE_A, QCLASS_IN, rrs, rcode);
    TEST_ASSERT(v == CacheVerdict::NEGATIVE_HIT, "Expected NEGATIVE_HIT verdict");
    TEST_ASSERT(rcode == RCODE_NXDOMAIN, "Expected NXDOMAIN (3) response code");
    TEST_ASSERT(cache.get_stats().negative_hits == 1, "Negative hit counter should be 1");

    return true;
}

// ============================================================================
// Test 7: Multi-Threaded SRWLOCK Reader-Writer Concurrency (Win32 API)
// ============================================================================
struct ThreadContext {
    StorageEngine* cache;
    int thread_id;
    int ops;
};

static DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
    ThreadContext* ctx = static_cast<ThreadContext*>(lpParam);
    for (int i = 0; i < ctx->ops; ++i) {
        std::string dom = "domain" + std::to_string((ctx->thread_id * 20) + (i % 20)) + ".com";
        if (i % 3 == 0) {
            std::vector<DNSResourceRecord> r = {
                DNSResourceRecord::create_a(dom, 100, "192.168.1.1")
            };
            ctx->cache->put(dom, QTYPE_A, QCLASS_IN, r, 100);
        } else {
            std::vector<DNSResourceRecord> out_rrs;
            uint8_t rcode = 0;
            ctx->cache->get(dom, QTYPE_A, QCLASS_IN, out_rrs, rcode);
        }
    }
    return 0;
}

bool test_thread_concurrency() {
    StorageEngine cache(500);

    const int NUM_THREADS = 8;
    HANDLE threads[NUM_THREADS];
    ThreadContext ctxs[NUM_THREADS];

    for (int t = 0; t < NUM_THREADS; ++t) {
        ctxs[t].cache = &cache;
        ctxs[t].thread_id = t;
        ctxs[t].ops = 250;
        threads[t] = CreateThread(NULL, 0, WorkerThreadProc, &ctxs[t], 0, NULL);
        TEST_ASSERT(threads[t] != NULL, "Failed to create worker thread");
    }

    WaitForMultipleObjects(NUM_THREADS, threads, TRUE, INFINITE);

    for (int t = 0; t < NUM_THREADS; ++t) {
        CloseHandle(threads[t]);
    }

    TEST_ASSERT(cache.size() > 0, "Cache should contain entries after concurrent execution");
    return true;
}

// ============================================================================
// Test 8: Snapshot Persistence & Durability (dns_cache.db)
// ============================================================================
bool test_snapshot_persistence() {
    std::string db_file = "test_dns_cache.db";

    {
        StorageEngine cache1(100);
        std::vector<DNSResourceRecord> r1 = { DNSResourceRecord::create_a("pes.edu", 300, "10.10.10.10") };
        std::vector<DNSResourceRecord> r2 = { DNSResourceRecord::create_a("google.com", 600, "142.250.190.46") };

        cache1.put("pes.edu", QTYPE_A, QCLASS_IN, r1, 300);
        cache1.put("google.com", QTYPE_A, QCLASS_IN, r2, 600);

        bool ok_save = cache1.save_snapshot(db_file);
        TEST_ASSERT(ok_save, "Failed to save cache snapshot to disk");
    }

    // Load snapshot into a brand new cache instance
    {
        StorageEngine cache2(100);
        bool ok_load = cache2.load_snapshot(db_file);
        TEST_ASSERT(ok_load, "Failed to load cache snapshot from disk");
        TEST_ASSERT(cache2.size() == 2, "Loaded cache size should be 2");

        std::vector<DNSResourceRecord> rrs;
        uint8_t rcode = 0;

        CacheVerdict v1 = cache2.get("pes.edu", QTYPE_A, QCLASS_IN, rrs, rcode);
        TEST_ASSERT(v1 == CacheVerdict::HIT, "pes.edu should hit in restored cache");
        TEST_ASSERT(rrs[0].readable_data == "10.10.10.10", "Restored IP mismatch for pes.edu");

        CacheVerdict v2 = cache2.get("google.com", QTYPE_A, QCLASS_IN, rrs, rcode);
        TEST_ASSERT(v2 == CacheVerdict::HIT, "google.com should hit in restored cache");
        TEST_ASSERT(rrs[0].readable_data == "142.250.190.46", "Restored IP mismatch for google.com");
    }

    // Cleanup snapshot file
    std::remove(db_file.c_str());
    return true;
}

// ============================================================================
// Test 9: Domain, Record Type, and Class Cache Key Isolation
// ============================================================================
bool test_cache_type_and_class_isolation() {
    StorageEngine cache(50);

    // Cache Type A (IPv4)
    std::vector<DNSResourceRecord> r_a = { DNSResourceRecord::create_a("example.com", 300, "93.184.216.34") };
    cache.put("example.com", QTYPE_A, QCLASS_IN, r_a, 300);

    // Cache Type AAAA (IPv6)
    std::vector<DNSResourceRecord> r_aaaa = { DNSResourceRecord::create_aaaa("example.com", 300, "2606:2800:220:1:248:1893:25c8:1946") };
    cache.put("example.com", QTYPE_AAAA, QCLASS_IN, r_aaaa, 300);

    std::vector<DNSResourceRecord> out;
    uint8_t rcode = 0;

    // Type A query must return IPv4
    CacheVerdict v_a = cache.get("example.com", QTYPE_A, QCLASS_IN, out, rcode);
    TEST_ASSERT(v_a == CacheVerdict::HIT, "Type A lookup should be HIT");
    TEST_ASSERT(out.size() == 1, "Expected 1 A record");
    TEST_ASSERT(out[0].readable_data == "93.184.216.34", "IPv4 address mismatch");

    // Type AAAA query must return IPv6
    CacheVerdict v_aaaa = cache.get("example.com", QTYPE_AAAA, QCLASS_IN, out, rcode);
    TEST_ASSERT(v_aaaa == CacheVerdict::HIT, "Type AAAA lookup should be HIT");
    TEST_ASSERT(out.size() == 1, "Expected 1 AAAA record");
    TEST_ASSERT(out[0].readable_data == "2606:2800:220:1:248:1893:25c8:1946", "IPv6 address mismatch");

    // Type MX query must be a MISS (distinct key)
    CacheVerdict v_mx = cache.get("example.com", QTYPE_MX, QCLASS_IN, out, rcode);
    TEST_ASSERT(v_mx == CacheVerdict::MISS, "Uncached Type MX should be MISS");

    // Different class query must be a MISS
    CacheVerdict v_cls = cache.get("example.com", QTYPE_A, 3 /* Chaos class */, out, rcode);
    TEST_ASSERT(v_cls == CacheVerdict::MISS, "Different class should be MISS");

    return true;
}

// ============================================================================
// Test 10: Cache Statistics and Expiration Accuracy
// ============================================================================
bool test_cache_stats_and_expiration_accuracy() {
    StorageEngine cache(10);
    cache.reset_stats();

    std::vector<DNSResourceRecord> r = { DNSResourceRecord::create_a("ephemeral.com", 1, "192.168.1.1") };
    cache.put("ephemeral.com", QTYPE_A, QCLASS_IN, r, 1);

    // Initial query -> HIT
    std::vector<DNSResourceRecord> out;
    uint8_t rcode = 0;
    TEST_ASSERT(cache.get("ephemeral.com", QTYPE_A, QCLASS_IN, out, rcode) == CacheVerdict::HIT, "Initial lookup must HIT");

    // Wait 1.2s for expiration
    Sleep(1200);

    // Query expired record -> EXPIRED (counted as miss)
    CacheVerdict v_exp = cache.get("ephemeral.com", QTYPE_A, QCLASS_IN, out, rcode);
    TEST_ASSERT(v_exp == CacheVerdict::EXPIRED, "Expired record must return EXPIRED");
    TEST_ASSERT(cache.size() == 0, "Expired entry must be erased from cache");

    CacheStats s = cache.get_stats();
    TEST_ASSERT(s.hits == 1, "Hits must be 1");
    TEST_ASSERT(s.misses == 1, "Misses must be 1 (expired lookup is a miss)");
    TEST_ASSERT(s.expirations == 1, "Expirations must be 1");
    TEST_ASSERT(s.hit_ratio() == 50.0, "Hit ratio must be exactly 50.0%");

    return true;
}

// ============================================================================
// Test Runner Main
// ============================================================================
int main() {
    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  ENGINE E2: STORAGE & CACHE ENGINE TEST SUITE (C++)    " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  Dual-Index O(1) LRU + O(log N) Min-Heap TTL & SRWLOCK " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;

    RUN_TEST(test_basic_hit_and_miss);
    RUN_TEST(test_lru_capacity_eviction);
    RUN_TEST(test_lru_promotion_on_hit);
    RUN_TEST(test_dynamic_ttl_decay);
    RUN_TEST(test_minheap_proactive_ttl_purge);
    RUN_TEST(test_negative_caching_rfc2308);
    RUN_TEST(test_thread_concurrency);
    RUN_TEST(test_snapshot_persistence);
    RUN_TEST(test_cache_type_and_class_isolation);
    RUN_TEST(test_cache_stats_and_expiration_accuracy);

    std::cout << COLOR_YELLOW << "--------------------------------------------------------" << COLOR_RESET << std::endl;
    std::cout << "Test Summary: " << g_tests_passed << "/" << g_tests_run << " passed." << std::endl;

    if (g_tests_passed == g_tests_run) {
        std::cout << COLOR_GREEN << "ALL ENGINE E2 STORAGE & CACHE TESTS PASSED EMPIRICALLY!" << COLOR_RESET << std::endl;
        return 0;
    } else {
        std::cout << COLOR_RED << "SOME ENGINE E2 TESTS FAILED!" << COLOR_RESET << std::endl;
        return 1;
    }
}
