#ifndef PROTOCOL_ENGINE_HPP
#define PROTOCOL_ENGINE_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <map>

// ============================================================================
// ENGINE E1: Protocol Parsing & Wire Message Engine
// Author/Lead: Vageesh Mathad (Computer Networks Module)
// RFC 1035 Domain Names - Wire Format & Pointer Decompression Specification
// ============================================================================

// 12-Byte RFC 1035 DNS Header Structure
struct DNSHeader {
    uint16_t id = 0;          // 16-bit Identification
    bool qr = false;          // 0 = Query, 1 = Response
    uint8_t opcode = 0;       // 4 bits (0 = Standard Query)
    bool aa = false;          // Authoritative Answer
    bool tc = false;          // Truncation Flag
    bool rd = false;          // Recursion Desired
    bool ra = false;          // Recursion Available
    uint8_t z = 0;            // 3 bits (Reserved, must be 0)
    uint8_t rcode = 0;        // 4 bits (Response Code)

    uint16_t qdcount = 0;     // Number of questions
    uint16_t ancount = 0;     // Number of answers
    uint16_t nscount = 0;     // Number of authority records
    uint16_t arcount = 0;     // Number of additional records

    uint16_t encode_flags() const;
    void decode_flags(uint16_t flags);

    void serialize(uint8_t* out_buf) const;
    static DNSHeader deserialize(const uint8_t* in_buf);
};

// DNS Question Section Entry
struct DNSQuestion {
    std::string qname;            // Dot-formatted domain name (e.g. "example.com")
    uint16_t qtype = QTYPE_A;     // 16-bit Query Type (A, AAAA, CNAME, etc.)
    uint16_t qclass = QCLASS_IN;  // 16-bit Query Class (IN = 1)

    bool operator==(const DNSQuestion& other) const {
        return qtype == other.qtype && qclass == other.qclass && qname == other.qname;
    }
};

// DNS Resource Record (Answer / Authority / Additional)
struct DNSResourceRecord {
    std::string name;             // Domain name
    uint16_t type = QTYPE_A;      // 16-bit RR Type
    uint16_t rclass = QCLASS_IN;  // 16-bit RR Class (IN = 1)
    uint32_t ttl = 300;           // 32-bit Time to Live (seconds)
    uint16_t rdlength = 0;        // 16-bit Length of RDATA in bytes
    std::vector<uint8_t> rdata;   // Raw binary RDATA bytes
    std::string readable_data;    // Human-readable format (e.g. "93.184.216.34")

    static DNSResourceRecord create_a(const std::string& name, uint32_t ttl, const std::string& ip);
    static DNSResourceRecord create_cname(const std::string& name, uint32_t ttl, const std::string& cname);
    static DNSResourceRecord create_aaaa(const std::string& name, uint32_t ttl, const std::string& ip6);
};

// Complete In-Memory Representation of an RFC 1035 DNS Message
struct DNSPacket {
    DNSHeader header;
    std::vector<DNSQuestion> questions;
    std::vector<DNSResourceRecord> answers;
    std::vector<DNSResourceRecord> authorities;
    std::vector<DNSResourceRecord> additionals;

    bool is_query() const { return !header.qr; }
    bool is_response() const { return header.qr; }
};

class ProtocolEngine {
public:
    // Core Parsing and Serializing APIs
    static bool parse(const uint8_t* buffer, size_t length, DNSPacket& out_packet, std::string& out_error);
    static bool serialize(const DNSPacket& packet, std::vector<uint8_t>& out_buffer, bool use_compression = true);

    // RFC 1035 Domain Name Wire Encoding / Decoding with Pointer Decompression
    // Cycle-loop guard: recursion depth is strictly bounded by MAX_DECOMPRESSION_DEPTH (16)
    static bool decode_domain_name(const uint8_t* buffer, size_t length, size_t& offset, std::string& out_domain, size_t recursion_depth = 0);
    static void encode_domain_name(const std::string& domain, std::vector<uint8_t>& out_buffer);
    static void encode_domain_name_compressed(const std::string& domain, std::vector<uint8_t>& out_buffer, std::map<std::string, uint16_t>& compression_map);

    // Response construction helpers
    static DNSPacket create_response(const DNSPacket& query, uint8_t rcode = RCODE_NOERROR);
    static std::vector<uint8_t> create_error_response_raw(const uint8_t* query_buf, size_t query_len, uint8_t rcode);

    // IP address conversion utilities
    static std::string ipv4_to_string(const uint8_t* rdata, size_t len);
    static bool string_to_ipv4(const std::string& ip_str, std::vector<uint8_t>& out_bytes);
    static std::string ipv6_to_string(const uint8_t* rdata, size_t len);
    static bool string_to_ipv6(const std::string& ip_str, std::vector<uint8_t>& out_bytes);

    // Presentation and Diagnostic Helpers
    static std::string qtype_to_string(uint16_t qtype);
    static uint16_t string_to_qtype(const std::string& str);
    static std::string rcode_to_string(uint8_t rcode);
    static std::string dump_packet(const DNSPacket& packet);
    static std::string normalize_domain(const std::string& domain);

private:
    static bool parse_rr(const uint8_t* buffer, size_t length, size_t& offset, DNSResourceRecord& out_rr, std::string& out_error);
};

#endif // PROTOCOL_ENGINE_HPP
