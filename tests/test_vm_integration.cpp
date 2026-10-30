#include "vm_engine.hpp"
#include <iostream>
#include <cassert>
#include <vector>

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

// Helper: Mock Upstream Server
struct MockUpstreamCtx {
    uint16_t port;
    uint8_t rcode_to_return = RCODE_NOERROR;
    bool respond = true;
    volatile bool ready = false;
};

static DWORD WINAPI MockUpstreamServerThread(LPVOID param) {
    MockUpstreamCtx* ctx = static_cast<MockUpstreamCtx*>(param);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ctx->port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    ctx->ready = true;

    while (ctx->respond) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv = { 1, 0 };

        int sel = select(0, &fds, NULL, NULL, &tv);
        if (sel <= 0) continue;

        uint8_t buf[512];
        sockaddr_in client;
        int clen = sizeof(client);
        int bytes = recvfrom(sock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                             reinterpret_cast<struct sockaddr*>(&client), &clen);

        if (bytes >= 12) {
            DNSPacket q;
            std::string err;
            if (ProtocolEngine::parse(buf, bytes, q, err)) {
                DNSPacket resp = ProtocolEngine::create_response(q, ctx->rcode_to_return);
                if (ctx->rcode_to_return == RCODE_NOERROR && !q.questions.empty()) {
                    resp.answers.push_back(DNSResourceRecord::create_a(q.questions[0].qname, 300, "93.184.216.34"));
                }

                std::vector<uint8_t> wire;
                ProtocolEngine::serialize(resp, wire, true);

                sendto(sock, reinterpret_cast<const char*>(wire.data()),
                       static_cast<int>(wire.size()), 0,
                       reinterpret_cast<struct sockaddr*>(&client), clen);
            }
        }
    }

    closesocket(sock);
    return 0;
}

// Helper: Build standard raw DNS query buffer
static std::vector<uint8_t> make_query_wire(uint16_t txid, const std::string& domain) {
    DNSPacket pkt;
    pkt.header.id = txid;
    pkt.header.rd = true;
    DNSQuestion q;
    q.qname = domain;
    q.qtype = QTYPE_A;
    q.qclass = QCLASS_IN;
    pkt.questions.push_back(q);

    std::vector<uint8_t> wire;
    ProtocolEngine::serialize(pkt, wire, false);
    return wire;
}

// ============================================================================
// Test 1: Full Pipeline End-to-End Cache MISS -> Upstream RESOLVE -> Cache HIT
// ============================================================================
bool test_end_to_end_miss_then_hit() {
    uint16_t mock_port = 18060;
    MockUpstreamCtx mock_ctx;
    mock_ctx.port = mock_port;
    mock_ctx.rcode_to_return = RCODE_NOERROR;
    mock_ctx.respond = true;

    HANDLE thread = CreateThread(NULL, 0, MockUpstreamServerThread, &mock_ctx, 0, NULL);
    while (!mock_ctx.ready) Sleep(10);

    StorageEngine cache(100);
    UpstreamConfig up_cfg;
    up_cfg.primary_server = "127.0.0.1";
    up_cfg.primary_port = mock_port;
    up_cfg.fallback_server = "";
    up_cfg.timeout_ms = 1000;
    up_cfg.enable_txid_randomization = true;

    UpstreamEngine upstream(up_cfg);
    TelemetryEngine telemetry;
    VMEngine vm(cache, upstream, telemetry);

    // --- Transaction 1: Cache MISS (Must resolve upstream and commit) ---
    std::vector<uint8_t> q1 = make_query_wire(0x1001, "example.com");
    VMTransaction tx1;
    tx1.inbound_raw = q1;

    bool ok1 = vm.process_transaction(tx1);
    TEST_ASSERT(ok1, "Transaction 1 execution failed");
    TEST_ASSERT(tx1.condition == VMCondition::UPSTREAM_SUCCESS, "Tx1 expected UPSTREAM_SUCCESS");
    TEST_ASSERT(tx1.response_packet.header.rcode == RCODE_NOERROR, "Tx1 expected NOERROR");
    TEST_ASSERT(tx1.response_packet.answers.size() == 1, "Tx1 expected 1 answer");
    TEST_ASSERT(tx1.response_packet.answers[0].readable_data == "93.184.216.34", "Tx1 IP mismatch");
    TEST_ASSERT(tx1.outbound_raw.size() >= 12, "Tx1 outbound wire packet empty");

    // Verify cache now holds "example.com"
    TEST_ASSERT(cache.size() == 1, "Cache should contain 1 entry after Tx1 COMMIT");

    // --- Transaction 2: Cache HIT (Must bypass upstream and emit directly) ---
    std::vector<uint8_t> q2 = make_query_wire(0x2002, "example.com");
    VMTransaction tx2;
    tx2.inbound_raw = q2;

    bool ok2 = vm.process_transaction(tx2);
    TEST_ASSERT(ok2, "Transaction 2 execution failed");
    TEST_ASSERT(tx2.condition == VMCondition::CACHE_HIT, "Tx2 expected CACHE_HIT");
    TEST_ASSERT(tx2.response_packet.header.id == 0x2002, "Tx2 response ID must match client ID 0x2002");
    TEST_ASSERT(tx2.response_packet.answers.size() == 1, "Tx2 expected 1 cached answer");
    TEST_ASSERT(tx2.response_packet.answers[0].readable_data == "93.184.216.34", "Tx2 IP mismatch");

    // Stop mock server
    mock_ctx.respond = false;
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);

    return true;
}

// ============================================================================
// Test 2: FORMERR Generation on Truncated and Malformed Packets
// ============================================================================
bool test_pipeline_formerr_conditions() {
    StorageEngine cache(50);
    UpstreamConfig up_cfg;
    up_cfg.fallback_server = "";
    UpstreamEngine upstream(up_cfg);
    TelemetryEngine telemetry;
    VMEngine vm(cache, upstream, telemetry);

    // Scenario A: Under 12 bytes
    std::vector<uint8_t> short_buf = { 0x12, 0x34, 0x01, 0x00, 0x00 };
    VMTransaction tx_short;
    tx_short.inbound_raw = short_buf;

    vm.process_transaction(tx_short);
    TEST_ASSERT(tx_short.condition == VMCondition::FORMERR_MALFORMED, "Expected FORMERR for <12 bytes");
    TEST_ASSERT(tx_short.outbound_raw.size() >= 12, "FORMERR wire response must be at least 12 bytes");
    DNSHeader h1 = DNSHeader::deserialize(tx_short.outbound_raw.data());
    TEST_ASSERT(h1.rcode == RCODE_FORMERR, "Header RCODE must be 1 (FORMERR)");

    // Scenario B: Valid header, zero questions
    std::vector<uint8_t> no_qd_buf(12, 0);
    write_uint16_be(no_qd_buf.data(), 0x5555); // ID
    VMTransaction tx_no_qd;
    tx_no_qd.inbound_raw = no_qd_buf;

    vm.process_transaction(tx_no_qd);
    TEST_ASSERT(tx_no_qd.condition == VMCondition::FORMERR_MALFORMED, "Expected FORMERR for zero questions");
    DNSHeader h2 = DNSHeader::deserialize(tx_no_qd.outbound_raw.data());
    TEST_ASSERT(h2.rcode == RCODE_FORMERR, "Header RCODE must be 1 (FORMERR)");

    return true;
}

// ============================================================================
// Test 3: RFC 2308 Negative Caching (NXDOMAIN Resolution & Cache Hit)
// ============================================================================
bool test_pipeline_nxdomain_negative_caching() {
    uint16_t mock_port = 18061;
    MockUpstreamCtx mock_ctx;
    mock_ctx.port = mock_port;
    mock_ctx.rcode_to_return = RCODE_NXDOMAIN; // Upstream reports domain does not exist!
    mock_ctx.respond = true;

    HANDLE thread = CreateThread(NULL, 0, MockUpstreamServerThread, &mock_ctx, 0, NULL);
    while (!mock_ctx.ready) Sleep(10);

    StorageEngine cache(50);
    UpstreamConfig up_cfg;
    up_cfg.primary_server = "127.0.0.1";
    up_cfg.primary_port = mock_port;
    up_cfg.fallback_server = "";
    up_cfg.timeout_ms = 1000;

    UpstreamEngine upstream(up_cfg);
    TelemetryEngine telemetry;
    VMEngine vm(cache, upstream, telemetry);

    // Query 1: Upstream returns NXDOMAIN -> Commits negative cache
    std::vector<uint8_t> q1 = make_query_wire(0x3003, "nonexistent.pes.edu");
    VMTransaction tx1;
    tx1.inbound_raw = q1;

    vm.process_transaction(tx1);
    TEST_ASSERT(tx1.condition == VMCondition::UPSTREAM_NXDOMAIN, "Expected UPSTREAM_NXDOMAIN");
    TEST_ASSERT(tx1.response_packet.header.rcode == RCODE_NXDOMAIN, "Expected NXDOMAIN RCODE");

    // Query 2: Cache lookup should now HIT negatively!
    std::vector<uint8_t> q2 = make_query_wire(0x4004, "nonexistent.pes.edu");
    VMTransaction tx2;
    tx2.inbound_raw = q2;

    vm.process_transaction(tx2);
    TEST_ASSERT(tx2.condition == VMCondition::CACHE_NEGATIVE_HIT, "Expected CACHE_NEGATIVE_HIT");
    TEST_ASSERT(tx2.response_packet.header.rcode == RCODE_NXDOMAIN, "Expected cached NXDOMAIN");

    mock_ctx.respond = false;
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);

    return true;
}

// ============================================================================
// Test 4: SERVFAIL Emission on Upstream Timeout
// ============================================================================
bool test_pipeline_servfail_on_timeout() {
    StorageEngine cache(50);
    UpstreamConfig up_cfg;
    up_cfg.primary_server = "192.0.2.1"; // Silent blackhole IP
    up_cfg.fallback_server = "";
    up_cfg.timeout_ms = 100;
    up_cfg.max_retries = 0;

    UpstreamEngine upstream(up_cfg);
    TelemetryEngine telemetry;
    VMEngine vm(cache, upstream, telemetry);

    std::vector<uint8_t> q = make_query_wire(0x7777, "timeout.domain");
    VMTransaction tx;
    tx.inbound_raw = q;

    vm.process_transaction(tx);
    TEST_ASSERT(tx.condition == VMCondition::UPSTREAM_TIMEOUT, "Expected UPSTREAM_TIMEOUT");
    TEST_ASSERT(tx.response_packet.header.rcode == RCODE_SERVFAIL, "Expected SERVFAIL (RCODE 2)");

    return true;
}

// ============================================================================
// Test 5: Pipeline Execution Trace Logging
// ============================================================================
bool test_pipeline_execution_trace() {
    StorageEngine cache(50);
    std::vector<DNSResourceRecord> r = { DNSResourceRecord::create_a("cached.com", 300, "1.2.3.4") };
    cache.put("cached.com", QTYPE_A, QCLASS_IN, r, 300);

    UpstreamConfig up_cfg;
    up_cfg.fallback_server = "";
    UpstreamEngine upstream(up_cfg);
    TelemetryEngine telemetry;
    VMEngine vm(cache, upstream, telemetry);

    std::vector<uint8_t> q = make_query_wire(0x8888, "cached.com");
    VMTransaction tx;
    tx.inbound_raw = q;

    vm.process_transaction(tx);
    TEST_ASSERT(!tx.execution_trace.empty(), "Execution trace should not be empty");
    TEST_ASSERT(tx.execution_trace.find("FETCH") != std::string::npos, "Trace missing FETCH");
    TEST_ASSERT(tx.execution_trace.find("DECODE") != std::string::npos, "Trace missing DECODE");
    TEST_ASSERT(tx.execution_trace.find("LOOKUP") != std::string::npos, "Trace missing LOOKUP");
    TEST_ASSERT(tx.execution_trace.find("EMIT") != std::string::npos, "Trace missing EMIT");

    return true;
}

// ============================================================================
// Test 6: Telemetry Snapshot Metrics Accumulation
// ============================================================================
bool test_telemetry_metrics_accumulation() {
    StorageEngine cache(50);
    UpstreamConfig up_cfg;
    up_cfg.fallback_server = "";
    UpstreamEngine upstream(up_cfg);
    TelemetryEngine telemetry;
    VMEngine vm(cache, upstream, telemetry);

    std::vector<DNSResourceRecord> r = { DNSResourceRecord::create_a("fast.com", 300, "1.1.1.1") };
    cache.put("fast.com", QTYPE_A, QCLASS_IN, r, 300);

    // Process 5 cache hits
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> q = make_query_wire(0x9000 + i, "fast.com");
        VMTransaction tx;
        tx.inbound_raw = q;
        vm.process_transaction(tx);
    }

    TelemetrySnapshot snap = telemetry.get_snapshot();
    TEST_ASSERT(snap.total_queries >= 5, "Total queries should be at least 5");
    TEST_ASSERT(snap.cache_hits >= 5, "Cache hits should be at least 5");
    TEST_ASSERT(snap.avg_latency_us >= 0.0, "Avg latency should be non-negative");
    TEST_ASSERT(!telemetry.generate_dashboard().empty(), "Dashboard should be generated cleanly");

    return true;
}

// ============================================================================
// Test 7: Repeated Queries for Same Domain & Accurate Telemetry
// ============================================================================
bool test_repeated_domain_resolution_and_telemetry() {
    uint16_t mock_port = 18062;
    MockUpstreamCtx mock_ctx;
    mock_ctx.port = mock_port;
    mock_ctx.rcode_to_return = RCODE_NOERROR;
    mock_ctx.respond = true;

    HANDLE thread = CreateThread(NULL, 0, MockUpstreamServerThread, &mock_ctx, 0, NULL);
    while (!mock_ctx.ready) Sleep(10);

    StorageEngine cache(50);
    UpstreamConfig up_cfg;
    up_cfg.primary_server = "127.0.0.1";
    up_cfg.primary_port = mock_port;
    up_cfg.fallback_server = "";
    up_cfg.timeout_ms = 1000;

    UpstreamEngine upstream(up_cfg);
    TelemetryEngine telemetry;
    VMEngine vm(cache, upstream, telemetry);

    // Query 1: Initial query -> Cache MISS -> Upstream resolution -> Commit
    std::vector<uint8_t> q1 = make_query_wire(0xA001, "repeat.com");
    VMTransaction tx1;
    tx1.inbound_raw = q1;
    bool ok1 = vm.process_transaction(tx1);
    TEST_ASSERT(ok1, "Tx1 failed");
    TEST_ASSERT(tx1.condition == VMCondition::UPSTREAM_SUCCESS, "Tx1 expected UPSTREAM_SUCCESS");

    // Query 2: Repeated query for same domain -> Cache HIT
    std::vector<uint8_t> q2 = make_query_wire(0xA002, "repeat.com");
    VMTransaction tx2;
    tx2.inbound_raw = q2;
    bool ok2 = vm.process_transaction(tx2);
    TEST_ASSERT(ok2, "Tx2 failed");
    TEST_ASSERT(tx2.condition == VMCondition::CACHE_HIT, "Tx2 expected CACHE_HIT");
    TEST_ASSERT(tx2.response_packet.answers.size() == 1, "Tx2 expected 1 cached answer");

    // Query 3: Another repeated query -> Cache HIT
    std::vector<uint8_t> q3 = make_query_wire(0xA003, "repeat.com");
    VMTransaction tx3;
    tx3.inbound_raw = q3;
    bool ok3 = vm.process_transaction(tx3);
    TEST_ASSERT(ok3, "Tx3 failed");
    TEST_ASSERT(tx3.condition == VMCondition::CACHE_HIT, "Tx3 expected CACHE_HIT");

    // Verify Telemetry Snapshot accuracy
    TelemetrySnapshot snap = telemetry.get_snapshot();
    TEST_ASSERT(snap.total_queries == 3, "Total queries should be exactly 3");
    TEST_ASSERT(snap.cache_hits == 2, "Cache hits should be exactly 2");
    TEST_ASSERT(snap.cache_misses == 1, "Cache misses should be exactly 1");
    TEST_ASSERT(snap.upstream_success == 1, "Upstream success should be exactly 1");
    TEST_ASSERT(snap.hit_ratio_pct >= 66.0 && snap.hit_ratio_pct <= 67.0, "Hit ratio should be 66.67%");

    mock_ctx.respond = false;
    WaitForSingleObject(thread, 2000);
    CloseHandle(thread);

    return true;
}

// ============================================================================
// Test Runner Main
// ============================================================================
int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  ENGINE E4: QUERY EXECUTION VM INTEGRATION TESTS (C++) " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  6-Stage Instruction Pipeline & Condition Matrix       " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;

    RUN_TEST(test_end_to_end_miss_then_hit);
    RUN_TEST(test_pipeline_formerr_conditions);
    RUN_TEST(test_pipeline_nxdomain_negative_caching);
    RUN_TEST(test_pipeline_servfail_on_timeout);
    RUN_TEST(test_pipeline_execution_trace);
    RUN_TEST(test_telemetry_metrics_accumulation);
    RUN_TEST(test_repeated_domain_resolution_and_telemetry);

    std::cout << COLOR_YELLOW << "--------------------------------------------------------" << COLOR_RESET << std::endl;
    std::cout << "Test Summary: " << g_tests_passed << "/" << g_tests_run << " passed." << std::endl;

    WSACleanup();

    if (g_tests_passed == g_tests_run) {
        std::cout << COLOR_GREEN << "ALL ENGINE E4 VM INTEGRATION TESTS PASSED EMPIRICALLY!" << COLOR_RESET << std::endl;
        return 0;
    } else {
        std::cout << COLOR_RED << "SOME ENGINE E4 VM TESTS FAILED!" << COLOR_RESET << std::endl;
        return 1;
    }
}
