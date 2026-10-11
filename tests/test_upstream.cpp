#include "upstream_engine.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <set>

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

// Helper: Run a simple mock UDP server thread
struct MockServerContext {
    uint16_t port;
    bool should_spoof_txid = false;
    uint16_t spoofed_txid = 0xDEAD;
    bool respond = true;
    volatile bool ready = false;
    SOCKET sock = INVALID_SOCKET;
};

static DWORD WINAPI MockDnsServerThread(LPVOID param) {
    MockServerContext* ctx = static_cast<MockServerContext*>(param);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ctx->sock = sock;

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ctx->port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    ctx->ready = true;

    // Wait for incoming packet
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int sel = select(0, &read_fds, NULL, NULL, &tv);
    if (sel > 0 && ctx->respond) {
        uint8_t buffer[512];
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int bytes = recvfrom(sock, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                             reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

        if (bytes >= 12) {
            DNSPacket query;
            std::string err;
            if (ProtocolEngine::parse(buffer, bytes, query, err)) {
                DNSPacket resp = ProtocolEngine::create_response(query, RCODE_NOERROR);
                resp.answers.push_back(DNSResourceRecord::create_a("example.com", 300, "93.184.216.34"));

                if (ctx->should_spoof_txid) {
                    resp.header.id = ctx->spoofed_txid; // Spoofed TxID!
                }

                std::vector<uint8_t> resp_wire;
                ProtocolEngine::serialize(resp, resp_wire, true);

                sendto(sock, reinterpret_cast<const char*>(resp_wire.data()),
                       static_cast<int>(resp_wire.size()), 0,
                       reinterpret_cast<struct sockaddr*>(&client_addr), client_len);
            }
        }
    }

    closesocket(sock);
    return 0;
}

// ============================================================================
// Test 1: Random 16-Bit TxID Generator (Kaminsky Poisoning Defense)
// ============================================================================
bool test_random_txid_generation() {
    std::set<uint16_t> seen;
    const int SAMPLE_SIZE = 500;

    for (int i = 0; i < SAMPLE_SIZE; ++i) {
        uint16_t txid = UpstreamEngine::generate_random_txid();
        TEST_ASSERT(txid != 0, "TxID should be non-zero");
        seen.insert(txid);
    }

    // High entropy check: in 500 random 16-bit integers, duplicates should be very rare (>480 unique)
    TEST_ASSERT(seen.size() >= 480, "TxID generator failed uniqueness entropy check");
    return true;
}

// ============================================================================
// Test 2: Mock Upstream Successful Query & RTT Measurement
// ============================================================================
bool test_mock_upstream_successful_resolution() {
    uint16_t mock_port = 18053;
    MockServerContext ctx;
    ctx.port = mock_port;
    ctx.should_spoof_txid = false;
    ctx.respond = true;

    HANDLE thread = CreateThread(NULL, 0, MockDnsServerThread, &ctx, 0, NULL);
    while (!ctx.ready) {
        Sleep(10);
    }

    UpstreamConfig cfg;
    cfg.primary_server = "127.0.0.1";
    cfg.primary_port = mock_port;
    cfg.fallback_server = ""; // No fallback
    cfg.timeout_ms = 1000;
    cfg.max_retries = 0;
    cfg.enable_txid_randomization = false; // deterministic for direct test

    UpstreamEngine engine(cfg);

    DNSPacket query;
    query.header.id = 0x5678;
    DNSQuestion q;
    q.qname = "example.com";
    q.qtype = QTYPE_A;
    q.qclass = QCLASS_IN;
    query.questions.push_back(q);

    UpstreamResult res = engine.resolve(query);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    TEST_ASSERT(res.is_success(), "Mock resolution failed: " + res.error_message);
    TEST_ASSERT(res.response_packet.answers.size() == 1, "Expected 1 answer");
    TEST_ASSERT(res.response_packet.answers[0].readable_data == "93.184.216.34", "Answer IP mismatch");
    TEST_ASSERT(res.error_message.empty(), "Error message should be empty on success");

    return true;
}

// ============================================================================
// Test 3: Kaminsky Spoofing Defense (Mismatched TxID Rejection)
// ============================================================================
bool test_kaminsky_spoofing_defense() {
    uint16_t mock_port = 18054;
    MockServerContext ctx;
    ctx.port = mock_port;
    ctx.should_spoof_txid = true; // Injects mismatched TxID!
    ctx.spoofed_txid = 0x9999;
    ctx.respond = true;

    HANDLE thread = CreateThread(NULL, 0, MockDnsServerThread, &ctx, 0, NULL);
    while (!ctx.ready) {
        Sleep(10);
    }

    UpstreamConfig cfg;
    cfg.primary_server = "127.0.0.1";
    cfg.primary_port = mock_port;
    cfg.fallback_server = ""; // No fallback
    cfg.timeout_ms = 500;
    cfg.max_retries = 0;
    cfg.enable_txid_randomization = false;

    UpstreamEngine engine(cfg);

    DNSPacket query;
    query.header.id = 0x1111; // We expect 0x1111, mock sends 0x9999
    DNSQuestion q;
    q.qname = "example.com";
    query.questions.push_back(q);

    UpstreamResult res = engine.resolve(query);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    TEST_ASSERT(res.status == UpstreamStatus::MISMATCHED_TXID, "Kaminsky attack was NOT blocked");
    TEST_ASSERT(res.error_message.find("Kaminsky Spoof Blocked") != std::string::npos, "Expected spoof block message");

    return true;
}

// ============================================================================
// Test 4: Socket select() Non-Blocking Timeout Handling
// ============================================================================
bool test_timeout_handling() {
    UpstreamConfig cfg;
    cfg.primary_server = "192.0.2.1"; // RFC 5737 TEST-NET-1 non-routable blackhole IP
    cfg.primary_port = 53;
    cfg.fallback_server = ""; // No fallback
    cfg.timeout_ms = 150;     // Fast 150ms timeout
    cfg.max_retries = 0;

    UpstreamEngine engine(cfg);

    DNSPacket query;
    query.header.id = 0x2222;
    DNSQuestion q;
    q.qname = "timeout.test";
    query.questions.push_back(q);

    auto t1 = std::chrono::steady_clock::now();
    UpstreamResult res = engine.resolve(query);
    auto t2 = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    TEST_ASSERT(res.status == UpstreamStatus::TIMEOUT, "Expected TIMEOUT status on silent port");
    TEST_ASSERT(elapsed_ms < 600, "Timeout should fire promptly without blocking");

    return true;
}

// ============================================================================
// Test 5: Exponential Retry and Failover to Fallback Server
// ============================================================================
bool test_failover_to_fallback() {
    uint16_t fallback_port = 18055;
    MockServerContext fallback_ctx;
    fallback_ctx.port = fallback_port;
    fallback_ctx.should_spoof_txid = false;
    fallback_ctx.respond = true;

    HANDLE thread = CreateThread(NULL, 0, MockDnsServerThread, &fallback_ctx, 0, NULL);
    while (!fallback_ctx.ready) {
        Sleep(10);
    }

    UpstreamConfig cfg;
    cfg.primary_server = "127.0.0.1";
    cfg.primary_port = 19991; // Silent primary port (will time out)
    cfg.fallback_server = "127.0.0.1";
    cfg.fallback_port = fallback_port; // Active mock fallback server
    cfg.timeout_ms = 100;
    cfg.max_retries = 1;
    cfg.enable_txid_randomization = true;

    UpstreamEngine engine(cfg);

    DNSPacket query;
    query.header.id = 0x3333;
    DNSQuestion q;
    q.qname = "example.com";
    query.questions.push_back(q);

    UpstreamResult res = engine.resolve(query);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    TEST_ASSERT(res.is_success(), "Failover to fallback server failed: " + res.error_message);
    TEST_ASSERT(res.used_upstream == "127.0.0.1:18055", "Response must come from fallback upstream server");
    TEST_ASSERT(res.response_packet.header.id == 0x3333, "Original client TxID must be restored");

    return true;
}

// ============================================================================
// Test 6: Status Code String Formatting
// ============================================================================
bool test_status_strings() {
    TEST_ASSERT(UpstreamEngine::status_to_string(UpstreamStatus::SUCCESS) == "SUCCESS", "Status string mismatch");
    TEST_ASSERT(UpstreamEngine::status_to_string(UpstreamStatus::TIMEOUT) == "TIMEOUT", "Status string mismatch");
    TEST_ASSERT(UpstreamEngine::status_to_string(UpstreamStatus::MISMATCHED_TXID).find("Kaminsky") != std::string::npos, "Status string mismatch");
    return true;
}

// ============================================================================
// Test Runner Main
// ============================================================================
int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  ENGINE E3: UPSTREAM RESOLUTION TEST SUITE (C++)       " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  Non-blocking UDP, Retry, Failover & Kaminsky Defense   " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;

    RUN_TEST(test_random_txid_generation);
    RUN_TEST(test_mock_upstream_successful_resolution);
    RUN_TEST(test_kaminsky_spoofing_defense);
    RUN_TEST(test_timeout_handling);
    RUN_TEST(test_failover_to_fallback);
    RUN_TEST(test_status_strings);

    std::cout << COLOR_YELLOW << "--------------------------------------------------------" << COLOR_RESET << std::endl;
    std::cout << "Test Summary: " << g_tests_passed << "/" << g_tests_run << " passed." << std::endl;

    WSACleanup();

    if (g_tests_passed == g_tests_run) {
        std::cout << COLOR_GREEN << "ALL ENGINE E3 UPSTREAM TESTS PASSED EMPIRICALLY!" << COLOR_RESET << std::endl;
        return 0;
    } else {
        std::cout << COLOR_RED << "SOME ENGINE E3 UPSTREAM TESTS FAILED!" << COLOR_RESET << std::endl;
        return 1;
    }
}
