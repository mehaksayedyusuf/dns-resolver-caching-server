#ifndef UPSTREAM_ENGINE_HPP
#define UPSTREAM_ENGINE_HPP

#include "common.hpp"
#include "protocol_engine.hpp"
#include <string>
#include <vector>
#include <chrono>

// ============================================================================
// ENGINE E3: Upstream Resolution & Network Forwarding Engine
// Author/Lead: Ishani G (Computer Networks & OS Modules)
// Non-blocking UDP Forwarder, Retry Backoff, and Kaminsky TxID Spoofing Defense
// ============================================================================

struct UpstreamConfig {
    std::string primary_server = "8.8.8.8";      // Google Public DNS
    uint16_t primary_port = 53;
    std::string fallback_server = "1.1.1.1";     // Cloudflare Public DNS
    uint16_t fallback_port = 53;
    uint32_t timeout_ms = 2000;                  // 2.0s select() polling timeout
    uint32_t max_retries = 2;                    // Max retry attempts before failover
    bool enable_txid_randomization = true;       // Defeats Kaminsky cache poisoning
};

enum class UpstreamStatus {
    SUCCESS,              // Resolution succeeded with valid RFC 1035 packet
    TIMEOUT,              // Socket select() timed out across all retries and failovers
    MISMATCHED_TXID,      // Received packet with wrong TxID (Kaminsky spoof attempt)
    MALFORMED_RESPONSE,   // Response wire bytes could not be decoded
    SOCKET_ERR,           // Failed to open, bind, or transmit via UDP socket
    UPSTREAM_SERVFAIL     // Upstream returned RCODE 2 (SERVFAIL) or RCODE 5 (REFUSED)
};

struct UpstreamResult {
    UpstreamStatus status = UpstreamStatus::SUCCESS;
    DNSPacket response_packet;
    std::vector<uint8_t> raw_response;
    std::string used_upstream;
    uint32_t rtt_microseconds = 0;
    std::string error_message;

    bool is_success() const { return status == UpstreamStatus::SUCCESS; }
};

class UpstreamEngine {
public:
    explicit UpstreamEngine(const UpstreamConfig& config = UpstreamConfig());
    ~UpstreamEngine();

    // Prevent copies (socket ownership)
    UpstreamEngine(const UpstreamEngine&) = delete;
    UpstreamEngine& operator=(const UpstreamEngine&) = delete;

    // High-Level Resolution APIs
    UpstreamResult resolve(const DNSPacket& query_packet);
    UpstreamResult resolve_raw(const uint8_t* query_wire, size_t query_len);

    // Direct Single-Host Resolution
    UpstreamResult query_host(const std::string& host, uint16_t port,
                              const uint8_t* query_buf, size_t query_len,
                              uint16_t original_txid, uint32_t timeout_ms);

    // Configuration
    const UpstreamConfig& config() const { return config_; }
    void set_config(const UpstreamConfig& config) { config_ = config; }

    // Security: Cryptographic 16-bit Transaction ID Generator
    static uint16_t generate_random_txid();

    // String Presentation Helper
    static std::string status_to_string(UpstreamStatus status);

private:
    UpstreamConfig config_;
    static bool winsock_initialized_;
    static void ensure_winsock_initialized();
};

#endif // UPSTREAM_ENGINE_HPP
