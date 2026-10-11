#include "upstream_engine.hpp"
#include <random>
#include <iostream>

bool UpstreamEngine::winsock_initialized_ = false;

void UpstreamEngine::ensure_winsock_initialized() {
    if (!winsock_initialized_) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            winsock_initialized_ = true;
        }
    }
}

UpstreamEngine::UpstreamEngine(const UpstreamConfig& config)
    : config_(config) {
    ensure_winsock_initialized();
}

UpstreamEngine::~UpstreamEngine() {
    // Sockets are closed per transaction
}

uint16_t UpstreamEngine::generate_random_txid() {
    static thread_local std::mt19937 rng(
        static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    std::uniform_int_distribution<unsigned int> dist(1, 0xFFFF);
    return static_cast<uint16_t>(dist(rng));
}

std::string UpstreamEngine::status_to_string(UpstreamStatus status) {
    switch (status) {
        case UpstreamStatus::SUCCESS:            return "SUCCESS";
        case UpstreamStatus::TIMEOUT:            return "TIMEOUT";
        case UpstreamStatus::MISMATCHED_TXID:    return "MISMATCHED_TXID (Kaminsky Spoof Blocked)";
        case UpstreamStatus::MALFORMED_RESPONSE: return "MALFORMED_RESPONSE";
        case UpstreamStatus::SOCKET_ERR:         return "SOCKET_ERROR";
        case UpstreamStatus::UPSTREAM_SERVFAIL:  return "UPSTREAM_SERVFAIL";
        default:                                 return "UNKNOWN_STATUS";
    }
}

UpstreamResult UpstreamEngine::query_host(const std::string& host, uint16_t port,
                                          const uint8_t* query_buf, size_t query_len,
                                          uint16_t expected_txid, uint32_t timeout_ms) {
    UpstreamResult result;
    result.used_upstream = host + ":" + std::to_string(port);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        result.status = UpstreamStatus::SOCKET_ERR;
        result.error_message = "Failed to create UDP socket: " + std::to_string(WSAGetLastError());
        return result;
    }

    // Set non-blocking I/O
    u_long non_blocking_mode = 1;
    ioctlsocket(sock, FIONBIO, &non_blocking_mode);

    sockaddr_in dest_addr;
    std::memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(host.c_str());

    auto t_start = std::chrono::high_resolution_clock::now();

    // Transmit query datagram
    int sent_bytes = sendto(sock, reinterpret_cast<const char*>(query_buf),
                            static_cast<int>(query_len), 0,
                            reinterpret_cast<struct sockaddr*>(&dest_addr), sizeof(dest_addr));
    if (sent_bytes == SOCKET_ERROR) {
        closesocket(sock);
        result.status = UpstreamStatus::SOCKET_ERR;
        result.error_message = "UDP sendto failed: " + std::to_string(WSAGetLastError());
        return result;
    }

    // Await response via select() with timeout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int sel = select(0, &read_fds, NULL, NULL, &tv);
    if (sel == 0) {
        closesocket(sock);
        result.status = UpstreamStatus::TIMEOUT;
        result.error_message = "Upstream server timed out after " + std::to_string(timeout_ms) + "ms";
        return result;
    } else if (sel == SOCKET_ERROR) {
        int err = WSAGetLastError();
        closesocket(sock);
        if (err == WSAECONNRESET) {
            result.status = UpstreamStatus::TIMEOUT;
            result.error_message = "Remote port unreachable (connection reset)";
        } else {
            result.status = UpstreamStatus::SOCKET_ERR;
            result.error_message = "Socket select error: " + std::to_string(err);
        }
        return result;
    }

    // Receive incoming datagram
    uint8_t recv_buffer[1024];
    sockaddr_in from_addr;
    int from_len = sizeof(from_addr);

    int recv_bytes = recvfrom(sock, reinterpret_cast<char*>(recv_buffer), sizeof(recv_buffer), 0,
                              reinterpret_cast<struct sockaddr*>(&from_addr), &from_len);
    closesocket(sock);

    auto t_end = std::chrono::high_resolution_clock::now();
    result.rtt_microseconds = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count()
    );

    if (recv_bytes == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAECONNRESET) {
            result.status = UpstreamStatus::TIMEOUT;
            result.error_message = "Port unreachable / connection reset";
        } else {
            result.status = UpstreamStatus::SOCKET_ERR;
            result.error_message = "recvfrom error: " + std::to_string(err);
        }
        return result;
    }

    if (recv_bytes < static_cast<int>(DNS_HEADER_SIZE)) {
        result.status = UpstreamStatus::MALFORMED_RESPONSE;
        result.error_message = "Upstream response smaller than DNS 12-byte header";
        return result;
    }

    // Store raw response
    result.raw_response.assign(recv_buffer, recv_buffer + recv_bytes);

    // Kaminsky Defense: Validate 16-bit Transaction ID
    uint16_t received_txid = read_uint16_be(recv_buffer);
    if (received_txid != expected_txid) {
        result.status = UpstreamStatus::MISMATCHED_TXID;
        result.error_message = "TxID mismatch! Expected 0x" + std::to_string(expected_txid) +
                               ", received 0x" + std::to_string(received_txid) +
                               " (Kaminsky Spoof Blocked)";
        return result;
    }

    // Parse response wire datagram
    std::string parse_err;
    if (!ProtocolEngine::parse(recv_buffer, recv_bytes, result.response_packet, parse_err)) {
        result.status = UpstreamStatus::MALFORMED_RESPONSE;
        result.error_message = "Failed to parse upstream response wire: " + parse_err;
        return result;
    }

    // Check upstream response code
    if (result.response_packet.header.rcode == RCODE_SERVFAIL ||
        result.response_packet.header.rcode == RCODE_REFUSED) {
        result.status = UpstreamStatus::UPSTREAM_SERVFAIL;
        result.error_message = "Upstream server returned error RCODE: " +
                               ProtocolEngine::rcode_to_string(result.response_packet.header.rcode);
        return result;
    }

    result.status = UpstreamStatus::SUCCESS;
    return result;
}

UpstreamResult UpstreamEngine::resolve_raw(const uint8_t* query_wire, size_t query_len) {
    UpstreamResult last_result;
    if (!query_wire || query_len < DNS_HEADER_SIZE) {
        last_result.status = UpstreamStatus::MALFORMED_RESPONSE;
        last_result.error_message = "Query wire buffer shorter than 12-byte header";
        return last_result;
    }

    uint16_t client_txid = read_uint16_be(query_wire);
    uint16_t outbound_txid = client_txid;

    // Create mutable copy of query buffer for randomized TxID
    std::vector<uint8_t> wire_copy(query_wire, query_wire + query_len);
    if (config_.enable_txid_randomization) {
        outbound_txid = generate_random_txid();
        write_uint16_be(wire_copy.data(), outbound_txid);
    }

    // 1. Try Primary Upstream with Exponential Retry Backoff
    for (uint32_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
        uint32_t current_timeout = config_.timeout_ms * (1 << attempt);
        last_result = query_host(config_.primary_server, config_.primary_port,
                                 wire_copy.data(), wire_copy.size(),
                                 outbound_txid, current_timeout);

        if (last_result.is_success()) {
            // Restore client original TxID in raw buffer and parsed header
            if (config_.enable_txid_randomization && !last_result.raw_response.empty()) {
                write_uint16_be(last_result.raw_response.data(), client_txid);
                last_result.response_packet.header.id = client_txid;
            }
            return last_result;
        }

        // If spoofing detected, do not immediately accept, but retry with fresh TxID
        if (last_result.status == UpstreamStatus::MISMATCHED_TXID && config_.enable_txid_randomization) {
            outbound_txid = generate_random_txid();
            write_uint16_be(wire_copy.data(), outbound_txid);
        }
    }

    // 2. Failover to Fallback Upstream (1.1.1.1)
    if (!config_.fallback_server.empty()) {
        for (uint32_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
            uint32_t current_timeout = config_.timeout_ms * (1 << attempt);
            last_result = query_host(config_.fallback_server, config_.fallback_port,
                                     wire_copy.data(), wire_copy.size(),
                                     outbound_txid, current_timeout);

            if (last_result.is_success()) {
                if (config_.enable_txid_randomization && !last_result.raw_response.empty()) {
                    write_uint16_be(last_result.raw_response.data(), client_txid);
                    last_result.response_packet.header.id = client_txid;
                }
                return last_result;
            }
        }
    }

    return last_result;
}

UpstreamResult UpstreamEngine::resolve(const DNSPacket& query_packet) {
    std::vector<uint8_t> wire;
    if (!ProtocolEngine::serialize(query_packet, wire)) {
        UpstreamResult res;
        res.status = UpstreamStatus::MALFORMED_RESPONSE;
        res.error_message = "Failed to serialize query packet to wire format";
        return res;
    }
    return resolve_raw(wire.data(), wire.size());
}
