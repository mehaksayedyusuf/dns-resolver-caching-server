#ifndef VM_ENGINE_HPP
#define VM_ENGINE_HPP

#include "common.hpp"
#include "protocol_engine.hpp"
#include "storage_engine.hpp"
#include "upstream_engine.hpp"
#include "telemetry_engine.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <chrono>

// ============================================================================
// ENGINE E4: Query Execution Virtual Machine
// Author/Lead: Joel Biju (OSPP Module)
// 6-Stage Instruction Pipeline: FETCH -> DECODE -> LOOKUP -> RESOLVE -> COMMIT -> EMIT
// ============================================================================

enum class VMStage {
    STAGE_FETCH,    // Stage 1: Ingest raw datagram buffer & timestamp start
    STAGE_DECODE,   // Stage 2: RFC 1035 wire parser & recursion limit check
    STAGE_LOOKUP,   // Stage 3: Dual-index LRU cache lookup with dynamic TTL
    STAGE_RESOLVE,  // Stage 4: Upstream resolution forwarder (8.8.8.8)
    STAGE_COMMIT,   // Stage 5: In-memory cache commit with SRWLOCK exclusive
    STAGE_EMIT,     // Stage 6: Serialize response with 0xC00C compression & emit
    STAGE_HALT      // Transaction finished
};

enum class VMCondition {
    NONE,
    CACHE_HIT,           // Found valid non-expired record in cache
    CACHE_NEGATIVE_HIT,  // Found cached NXDOMAIN in cache
    CACHE_EXPIRED,       // Found record but TTL decayed to 0
    CACHE_MISS,          // Record not in cache
    UPSTREAM_SUCCESS,    // Successfully resolved from 8.8.8.8/1.1.1.1
    UPSTREAM_NXDOMAIN,   // Upstream returned NXDOMAIN (RCODE 3)
    UPSTREAM_TIMEOUT,    // Upstream timed out across all retries
    FORMERR_MALFORMED,   // Malformed query packet (RCODE 1)
    SERVFAIL_ERROR       // Server failure / unreachable (RCODE 2)
};

// Transaction Execution State
struct VMTransaction {
    uint64_t transaction_id = 0;
    std::string client_ip = "127.0.0.1";
    uint16_t client_port = 0;

    // Buffer states
    std::vector<uint8_t> inbound_raw;
    DNSPacket query_packet;
    DNSPacket response_packet;
    std::vector<uint8_t> outbound_raw;

    // Execution pipeline state
    VMStage current_stage = VMStage::STAGE_FETCH;
    VMCondition condition = VMCondition::NONE;
    std::chrono::high_resolution_clock::time_point start_time;
    uint32_t latency_microseconds = 0;
    std::string error_message;
    std::string execution_trace;

    void log_trace(const std::string& msg);
};

class VMEngine {
public:
    VMEngine(StorageEngine& cache_engine, UpstreamEngine& upstream_engine, TelemetryEngine& telemetry_engine);
    ~VMEngine() = default;

    // Prevent copies
    VMEngine(const VMEngine&) = delete;
    VMEngine& operator=(const VMEngine&) = delete;

    // Main Transaction Processor (Executes full 6-stage pipeline)
    bool process_transaction(VMTransaction& tx);

    // Convenience API for raw network datagram input -> raw output wire
    std::vector<uint8_t> process_raw_query(const uint8_t* raw_buf, size_t raw_len,
                                          const std::string& client_ip = "127.0.0.1",
                                          uint16_t client_port = 0);

    // Individual Stage Methods (for modular verification and step execution)
    bool stage_fetch(VMTransaction& tx);
    bool stage_decode(VMTransaction& tx);
    bool stage_lookup(VMTransaction& tx);
    bool stage_resolve(VMTransaction& tx);
    bool stage_commit(VMTransaction& tx);
    bool stage_emit(VMTransaction& tx);

    // Helpers
    static std::string stage_to_string(VMStage stage);
    static std::string condition_to_string(VMCondition cond);

private:
    StorageEngine& cache_;
    UpstreamEngine& upstream_;
    TelemetryEngine& telemetry_;
    std::atomic<uint64_t> next_tx_id_{1};
};

#endif // VM_ENGINE_HPP
