#ifndef TELEMETRY_ENGINE_HPP
#define TELEMETRY_ENGINE_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <chrono>

// ============================================================================
// ENGINE E4 COMPONENT: Telemetry & Performance Analytics Subsystem
// Microsecond Latency Tracking, Hit Ratios, QPS Counters, and Live Dashboard
// ============================================================================

struct TelemetrySnapshot {
    uint64_t total_queries = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t cache_expired = 0;
    uint64_t negative_hits = 0;
    uint64_t upstream_success = 0;
    uint64_t upstream_timeouts = 0;
    uint64_t formerrs = 0;
    uint64_t servfails = 0;
    uint64_t min_latency_us = 0;
    uint64_t max_latency_us = 0;
    double avg_latency_us = 0.0;
    double hit_ratio_pct = 0.0;
    double qps = 0.0;
    double uptime_seconds = 0.0;
};

class TelemetryEngine {
public:
    TelemetryEngine();
    ~TelemetryEngine() = default;

    TelemetryEngine(const TelemetryEngine&) = delete;
    TelemetryEngine& operator=(const TelemetryEngine&) = delete;

    // Metrics recording
    void record_query_completed(const std::string& condition, uint32_t latency_microseconds);
    void record_formerr();
    void record_servfail();

    // Telemetry snapshots & dashboard formatting
    TelemetrySnapshot get_snapshot() const;
    std::string generate_dashboard() const;
    void reset();

private:
    std::atomic<uint64_t> total_queries_{0};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::atomic<uint64_t> cache_expired_{0};
    std::atomic<uint64_t> negative_hits_{0};
    std::atomic<uint64_t> upstream_success_{0};
    std::atomic<uint64_t> upstream_timeouts_{0};
    std::atomic<uint64_t> formerrs_{0};
    std::atomic<uint64_t> servfails_{0};

    std::atomic<uint64_t> total_latency_us_{0};
    std::atomic<uint64_t> min_latency_us_{UINT64_MAX};
    std::atomic<uint64_t> max_latency_us_{0};

    std::chrono::steady_clock::time_point start_time_;
    mutable Mutex lock_;
};

#endif // TELEMETRY_ENGINE_HPP
