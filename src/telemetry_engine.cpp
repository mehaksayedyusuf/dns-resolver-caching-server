#include "telemetry_engine.hpp"
#include <iomanip>
#include <sstream>

TelemetryEngine::TelemetryEngine()
    : start_time_(std::chrono::steady_clock::now()) {}

void TelemetryEngine::record_query_completed(const std::string& condition, uint32_t latency_microseconds) {
    total_queries_++;
    total_latency_us_ += latency_microseconds;

    // Update min / max latency
    uint64_t current_min = min_latency_us_.load();
    while (latency_microseconds < current_min &&
           !min_latency_us_.compare_exchange_weak(current_min, latency_microseconds)) {}

    uint64_t current_max = max_latency_us_.load();
    while (latency_microseconds > current_max &&
           !max_latency_us_.compare_exchange_weak(current_max, latency_microseconds)) {}

    if (condition == "CACHE_HIT") {
        cache_hits_++;
    } else if (condition == "CACHE_NEGATIVE_HIT") {
        negative_hits_++;
    } else if (condition == "CACHE_MISS") {
        cache_misses_++;
    } else if (condition == "CACHE_EXPIRED") {
        cache_expired_++;
    } else if (condition == "UPSTREAM_SUCCESS") {
        upstream_success_++;
    } else if (condition == "UPSTREAM_TIMEOUT") {
        upstream_timeouts_++;
        servfails_++;
    } else if (condition == "FORMERR_MALFORMED") {
        formerrs_++;
    } else if (condition == "SERVFAIL_ERROR") {
        servfails_++;
    }
}

void TelemetryEngine::record_formerr() {
    total_queries_++;
    formerrs_++;
}

void TelemetryEngine::record_servfail() {
    total_queries_++;
    servfails_++;
}

TelemetrySnapshot TelemetryEngine::get_snapshot() const {
    TelemetrySnapshot snap;
    snap.total_queries = total_queries_.load();
    snap.cache_hits = cache_hits_.load();
    snap.cache_misses = cache_misses_.load();
    snap.cache_expired = cache_expired_.load();
    snap.negative_hits = negative_hits_.load();
    snap.upstream_success = upstream_success_.load();
    snap.upstream_timeouts = upstream_timeouts_.load();
    snap.formerrs = formerrs_.load();
    snap.servfails = servfails_.load();

    uint64_t raw_min = min_latency_us_.load();
    snap.min_latency_us = (raw_min == UINT64_MAX) ? 0 : raw_min;
    snap.max_latency_us = max_latency_us_.load();

    uint64_t tot_lat = total_latency_us_.load();
    snap.avg_latency_us = (snap.total_queries > 0) ? (static_cast<double>(tot_lat) / snap.total_queries) : 0.0;

    uint64_t total_lookups = snap.cache_hits + snap.cache_misses + snap.cache_expired + snap.negative_hits;
    if (total_lookups > 0) {
        snap.hit_ratio_pct = (static_cast<double>(snap.cache_hits + snap.negative_hits) / total_lookups) * 100.0;
    } else {
        snap.hit_ratio_pct = 0.0;
    }

    auto now = std::chrono::steady_clock::now();
    double uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count() / 1000.0;
    snap.uptime_seconds = uptime > 0 ? uptime : 0.001;
    snap.qps = snap.total_queries / snap.uptime_seconds;

    return snap;
}

std::string TelemetryEngine::generate_dashboard() const {
    TelemetrySnapshot s = get_snapshot();
    std::ostringstream oss;
    oss << "\n==================== DNS SERVER TELEMETRY DASHBOARD ====================\n";
    oss << "  Uptime: " << std::fixed << std::setprecision(1) << s.uptime_seconds << "s"
        << " | Total Queries: " << s.total_queries
        << " | Current QPS: " << std::setprecision(2) << s.qps << "\n";
    oss << "------------------------------------------------------------------------\n";
    oss << "  Cache Performance:\n";
    oss << "    Hits: " << s.cache_hits
        << " | Misses: " << s.cache_misses
        << " | Expired: " << s.cache_expired
        << " | Negative Hits: " << s.negative_hits << "\n";
    oss << "    Hit Ratio: " << std::setprecision(2) << s.hit_ratio_pct << "%\n";
    oss << "------------------------------------------------------------------------\n";
    oss << "  Upstream & Errors:\n";
    oss << "    Upstream Success: " << s.upstream_success
        << " | Timeouts: " << s.upstream_timeouts
        << " | FORMERR: " << s.formerrs
        << " | SERVFAIL: " << s.servfails << "\n";
    oss << "------------------------------------------------------------------------\n";
    oss << "  Latency Profile:\n";
    oss << "    Min: " << s.min_latency_us << " us"
        << " | Avg: " << std::setprecision(1) << s.avg_latency_us << " us"
        << " | Max: " << s.max_latency_us << " us\n";
    oss << "========================================================================\n";
    return oss.str();
}

void TelemetryEngine::reset() {
    total_queries_ = 0;
    cache_hits_ = 0;
    cache_misses_ = 0;
    cache_expired_ = 0;
    negative_hits_ = 0;
    upstream_success_ = 0;
    upstream_timeouts_ = 0;
    formerrs_ = 0;
    servfails_ = 0;
    total_latency_us_ = 0;
    min_latency_us_ = UINT64_MAX;
    max_latency_us_ = 0;
    start_time_ = std::chrono::steady_clock::now();
}
