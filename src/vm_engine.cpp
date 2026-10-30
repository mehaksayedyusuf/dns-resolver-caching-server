#include "vm_engine.hpp"
#include <iostream>

void VMTransaction::log_trace(const std::string& msg) {
    if (!execution_trace.empty()) {
        execution_trace += " -> ";
    }
    execution_trace += msg;
}

VMEngine::VMEngine(StorageEngine& cache_engine, UpstreamEngine& upstream_engine, TelemetryEngine& telemetry_engine)
    : cache_(cache_engine), upstream_(upstream_engine), telemetry_(telemetry_engine) {}

std::string VMEngine::stage_to_string(VMStage stage) {
    switch (stage) {
        case VMStage::STAGE_FETCH:   return "FETCH";
        case VMStage::STAGE_DECODE:  return "DECODE";
        case VMStage::STAGE_LOOKUP:  return "LOOKUP";
        case VMStage::STAGE_RESOLVE: return "RESOLVE";
        case VMStage::STAGE_COMMIT:  return "COMMIT";
        case VMStage::STAGE_EMIT:    return "EMIT";
        case VMStage::STAGE_HALT:    return "HALT";
        default:                     return "UNKNOWN_STAGE";
    }
}

std::string VMEngine::condition_to_string(VMCondition cond) {
    switch (cond) {
        case VMCondition::CACHE_HIT:          return "CACHE_HIT";
        case VMCondition::CACHE_NEGATIVE_HIT: return "CACHE_NEGATIVE_HIT";
        case VMCondition::CACHE_EXPIRED:      return "CACHE_EXPIRED";
        case VMCondition::CACHE_MISS:         return "CACHE_MISS";
        case VMCondition::UPSTREAM_SUCCESS:   return "UPSTREAM_SUCCESS";
        case VMCondition::UPSTREAM_NXDOMAIN:  return "UPSTREAM_NXDOMAIN";
        case VMCondition::UPSTREAM_TIMEOUT:   return "UPSTREAM_TIMEOUT";
        case VMCondition::FORMERR_MALFORMED:  return "FORMERR_MALFORMED";
        case VMCondition::SERVFAIL_ERROR:     return "SERVFAIL_ERROR";
        default:                              return "NONE";
    }
}

bool VMEngine::stage_fetch(VMTransaction& tx) {
    tx.transaction_id = next_tx_id_++;
    tx.start_time = std::chrono::high_resolution_clock::now();

    if (tx.inbound_raw.size() < DNS_HEADER_SIZE) {
        tx.condition = VMCondition::FORMERR_MALFORMED;
        tx.error_message = "Packet smaller than 12-byte header";
        tx.log_trace("FETCH: FormErr (length < 12)");
        tx.current_stage = VMStage::STAGE_EMIT;
        return false;
    }

    tx.log_trace("FETCH: Ingested " + std::to_string(tx.inbound_raw.size()) + "B");
    tx.current_stage = VMStage::STAGE_DECODE;
    return true;
}

bool VMEngine::stage_decode(VMTransaction& tx) {
    std::string err;
    if (!ProtocolEngine::parse(tx.inbound_raw.data(), tx.inbound_raw.size(), tx.query_packet, err)) {
        tx.condition = VMCondition::FORMERR_MALFORMED;
        tx.error_message = err;
        tx.log_trace("DECODE: FormErr (" + err + ")");
        tx.current_stage = VMStage::STAGE_EMIT;
        return false;
    }

    if (tx.query_packet.questions.empty()) {
        tx.condition = VMCondition::FORMERR_MALFORMED;
        tx.error_message = "Packet has zero questions";
        tx.log_trace("DECODE: FormErr (QDCOUNT=0)");
        tx.current_stage = VMStage::STAGE_EMIT;
        return false;
    }

    tx.log_trace("DECODE: QNAME='" + tx.query_packet.questions[0].qname + "'");
    tx.current_stage = VMStage::STAGE_LOOKUP;
    return true;
}

bool VMEngine::stage_lookup(VMTransaction& tx) {
    const auto& q = tx.query_packet.questions[0];
    std::vector<DNSResourceRecord> cached_rrs;
    uint8_t cached_rcode = 0;

    CacheVerdict v = cache_.get(q.qname, q.qtype, q.qclass, cached_rrs, cached_rcode);

    if (v == CacheVerdict::HIT) {
        tx.condition = VMCondition::CACHE_HIT;
        tx.response_packet = ProtocolEngine::create_response(tx.query_packet, RCODE_NOERROR);
        tx.response_packet.answers = cached_rrs;
        tx.log_trace("LOOKUP: Cache HIT (" + std::to_string(cached_rrs.size()) + " RRs)");
        tx.current_stage = VMStage::STAGE_EMIT;
        return true;
    } else if (v == CacheVerdict::NEGATIVE_HIT) {
        tx.condition = VMCondition::CACHE_NEGATIVE_HIT;
        tx.response_packet = ProtocolEngine::create_response(tx.query_packet, RCODE_NXDOMAIN);
        tx.log_trace("LOOKUP: Cache NEGATIVE_HIT (NXDOMAIN)");
        tx.current_stage = VMStage::STAGE_EMIT;
        return true;
    } else if (v == CacheVerdict::EXPIRED) {
        tx.condition = VMCondition::CACHE_EXPIRED;
        tx.log_trace("LOOKUP: Cache EXPIRED");
        tx.current_stage = VMStage::STAGE_RESOLVE;
        return true;
    } else {
        tx.condition = VMCondition::CACHE_MISS;
        tx.log_trace("LOOKUP: Cache MISS");
        tx.current_stage = VMStage::STAGE_RESOLVE;
        return true;
    }
}

bool VMEngine::stage_resolve(VMTransaction& tx) {
    tx.log_trace("RESOLVE: Forwarding Upstream");

    UpstreamResult up_res = upstream_.resolve_raw(tx.inbound_raw.data(), tx.inbound_raw.size());

    if (!up_res.is_success()) {
        if (up_res.status == UpstreamStatus::TIMEOUT) {
            tx.condition = VMCondition::UPSTREAM_TIMEOUT;
        } else {
            tx.condition = VMCondition::SERVFAIL_ERROR;
        }
        tx.error_message = up_res.error_message;
        tx.response_packet = ProtocolEngine::create_response(tx.query_packet, RCODE_SERVFAIL);
        tx.log_trace("RESOLVE: Failure (" + up_res.error_message + ")");
        tx.current_stage = VMStage::STAGE_EMIT;
        return false;
    }

    if (up_res.response_packet.header.rcode == RCODE_NXDOMAIN) {
        tx.condition = VMCondition::UPSTREAM_NXDOMAIN;
        tx.response_packet = up_res.response_packet;
        const auto& q = tx.query_packet.questions[0];
        uint32_t neg_ttl = 60;
        if (!up_res.response_packet.authorities.empty() && up_res.response_packet.authorities[0].ttl > 0) {
            neg_ttl = std::min(up_res.response_packet.authorities[0].ttl, 300u);
        }
        cache_.put(q.qname, q.qtype, q.qclass, {}, neg_ttl, true, RCODE_NXDOMAIN);
        tx.log_trace("RESOLVE: NXDOMAIN (Negative Cache " + std::to_string(neg_ttl) + "s)");
        tx.current_stage = VMStage::STAGE_EMIT;
        return true;
    }

    tx.condition = VMCondition::UPSTREAM_SUCCESS;
    tx.response_packet = up_res.response_packet;
    tx.log_trace("RESOLVE: Upstream NOERROR (" + std::to_string(tx.response_packet.answers.size()) + " RRs)");
    tx.current_stage = VMStage::STAGE_COMMIT;
    return true;
}

bool VMEngine::stage_commit(VMTransaction& tx) {
    if (!tx.response_packet.answers.empty()) {
        const auto& q = tx.query_packet.questions[0];
        uint32_t min_ttl = tx.response_packet.answers[0].ttl;
        for (const auto& rr : tx.response_packet.answers) {
            if (rr.ttl < min_ttl) {
                min_ttl = rr.ttl;
            }
        }
        if (min_ttl == 0) min_ttl = 1;

        cache_.put(q.qname, q.qtype, q.qclass, tx.response_packet.answers, min_ttl);
        tx.log_trace("COMMIT: Saved to cache (TTL: " + std::to_string(min_ttl) + "s)");
    } else {
        tx.log_trace("COMMIT: No answers to cache");
    }

    tx.current_stage = VMStage::STAGE_EMIT;
    return true;
}

bool VMEngine::stage_emit(VMTransaction& tx) {
    if (tx.condition == VMCondition::FORMERR_MALFORMED && tx.query_packet.questions.empty()) {
        tx.outbound_raw = ProtocolEngine::create_error_response_raw(
            tx.inbound_raw.data(), tx.inbound_raw.size(), RCODE_FORMERR
        );
    } else {
        ProtocolEngine::serialize(tx.response_packet, tx.outbound_raw, true);
    }

    auto now = std::chrono::high_resolution_clock::now();
    tx.latency_microseconds = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - tx.start_time).count()
    );

    telemetry_.record_query_completed(condition_to_string(tx.condition), tx.latency_microseconds);

    tx.log_trace("EMIT: " + std::to_string(tx.outbound_raw.size()) + "B emitted (" +
                 std::to_string(tx.latency_microseconds) + "us)");
    tx.current_stage = VMStage::STAGE_HALT;
    return true;
}

bool VMEngine::process_transaction(VMTransaction& tx) {
    while (tx.current_stage != VMStage::STAGE_HALT) {
        switch (tx.current_stage) {
            case VMStage::STAGE_FETCH:
                stage_fetch(tx);
                break;
            case VMStage::STAGE_DECODE:
                stage_decode(tx);
                break;
            case VMStage::STAGE_LOOKUP:
                stage_lookup(tx);
                break;
            case VMStage::STAGE_RESOLVE:
                stage_resolve(tx);
                break;
            case VMStage::STAGE_COMMIT:
                stage_commit(tx);
                break;
            case VMStage::STAGE_EMIT:
                stage_emit(tx);
                break;
            case VMStage::STAGE_HALT:
                break;
        }
    }
    return true;
}

std::vector<uint8_t> VMEngine::process_raw_query(const uint8_t* raw_buf, size_t raw_len,
                                                const std::string& client_ip, uint16_t client_port) {
    VMTransaction tx;
    tx.client_ip = client_ip;
    tx.client_port = client_port;
    if (raw_buf && raw_len > 0) {
        tx.inbound_raw.assign(raw_buf, raw_buf + raw_len);
    }
    process_transaction(tx);
    return tx.outbound_raw;
}
