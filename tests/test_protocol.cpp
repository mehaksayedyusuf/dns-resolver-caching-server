#include "protocol_engine.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cstring>

// Color formatting for console test runner
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
// Test 1: Header Bitfields, Flags, and Serialization Roundtrip
// ============================================================================
bool test_header_bitfields_and_roundtrip() {
    DNSHeader h;
    h.id = 0xBEEF;
    h.qr = true;          // Response
    h.opcode = 0;         // Standard query
    h.aa = true;          // Authoritative
    h.tc = false;         // Not truncated
    h.rd = true;          // Recursion desired
    h.ra = true;          // Recursion available
    h.z = 0;              // Reserved
    h.rcode = RCODE_NOERROR;
    h.qdcount = 1;
    h.ancount = 2;
    h.nscount = 0;
    h.arcount = 1;

    uint16_t encoded = h.encode_flags();
    // QR=1 (0x8000), AA=1 (0x0400), RD=1 (0x0100), RA=1 (0x0080) -> 0x8580
    TEST_ASSERT(encoded == 0x8580, "Header flags bitmask calculation failed");

    uint8_t buffer[DNS_HEADER_SIZE];
    h.serialize(buffer);

    DNSHeader parsed = DNSHeader::deserialize(buffer);
    TEST_ASSERT(parsed.id == 0xBEEF, "Header ID mismatch");
    TEST_ASSERT(parsed.qr == true, "Header QR mismatch");
    TEST_ASSERT(parsed.aa == true, "Header AA mismatch");
    TEST_ASSERT(parsed.rd == true, "Header RD mismatch");
    TEST_ASSERT(parsed.ra == true, "Header RA mismatch");
    TEST_ASSERT(parsed.rcode == RCODE_NOERROR, "Header RCODE mismatch");
    TEST_ASSERT(parsed.qdcount == 1, "Header QDCOUNT mismatch");
    TEST_ASSERT(parsed.ancount == 2, "Header ANCOUNT mismatch");
    TEST_ASSERT(parsed.nscount == 0, "Header NSCOUNT mismatch");
    TEST_ASSERT(parsed.arcount == 1, "Header ARCOUNT mismatch");

    return true;
}

// ============================================================================
// Test 2: Domain Name Encoding & Normalization
// ============================================================================
bool test_domain_name_encoding_and_decoding() {
    std::string domain = "ExAmPlE.CoM";
    std::vector<uint8_t> encoded;
    ProtocolEngine::encode_domain_name(domain, encoded);

    // Expected wire format: \x07example\x03com\x00
    // Total bytes: 1 + 7 + 1 + 3 + 1 = 13 bytes
    TEST_ASSERT(encoded.size() == 13, "Encoded domain size mismatch");
    TEST_ASSERT(encoded[0] == 7, "First label length byte must be 7");
    TEST_ASSERT(encoded[8] == 3, "Second label length byte must be 3");
    TEST_ASSERT(encoded[12] == 0, "Terminating byte must be 0x00");

    size_t offset = 0;
    std::string decoded;
    bool ok = ProtocolEngine::decode_domain_name(encoded.data(), encoded.size(), offset, decoded);
    TEST_ASSERT(ok, "Failed to decode domain name");
    TEST_ASSERT(decoded == "example.com", "Decoded domain should be normalized lowercase 'example.com'");
    TEST_ASSERT(offset == 13, "Stream offset should advance past null terminator");

    // Multi-level domain: cs.pes.edu
    std::vector<uint8_t> cs_encoded;
    ProtocolEngine::encode_domain_name("cs.pes.edu", cs_encoded);
    offset = 0;
    std::string cs_decoded;
    TEST_ASSERT(ProtocolEngine::decode_domain_name(cs_encoded.data(), cs_encoded.size(), offset, cs_decoded), "Failed to decode cs.pes.edu");
    TEST_ASSERT(cs_decoded == "cs.pes.edu", "Mismatch for cs.pes.edu");

    return true;
}

// ============================================================================
// Test 3: RFC 1035 Pointer Decompression (0xC00C)
// ============================================================================
bool test_pointer_decompression() {
    // Construct simulated packet:
    // Offset 0..11: 12-byte header
    // Offset 12: \x07example\x03com\x00 (13 bytes, ends at offset 24)
    // Offset 25: 5 bytes padding
    // Offset 30: Pointer 0xC00C -> points to offset 12 ("example.com")
    std::vector<uint8_t> pkt(40, 0);

    // Question name at offset 12
    std::vector<uint8_t> qname_bytes;
    ProtocolEngine::encode_domain_name("example.com", qname_bytes);
    std::memcpy(pkt.data() + 12, qname_bytes.data(), qname_bytes.size());

    // Pointer at offset 30: 0xC00C
    pkt[30] = 0xC0;
    pkt[31] = 0x0C;

    size_t offset = 30;
    std::string decompressed;
    bool ok = ProtocolEngine::decode_domain_name(pkt.data(), pkt.size(), offset, decompressed);
    TEST_ASSERT(ok, "Pointer decompression failed");
    TEST_ASSERT(decompressed == "example.com", "Decompressed domain must match 'example.com'");
    TEST_ASSERT(offset == 32, "Stream offset must advance exactly 2 bytes past compression pointer");

    return true;
}

// ============================================================================
// Test 4: Pointer Loop Cycle Detection (RFC 1035 Security Guard)
// ============================================================================
bool test_pointer_loop_attack_prevention() {
    // Scenario A: Self-referencing loop (offset 12 points to offset 12)
    std::vector<uint8_t> pkt_self(32, 0);
    pkt_self[12] = 0xC0;
    pkt_self[13] = 0x0C; // Pointer to 12

    size_t offset = 12;
    std::string domain;
    bool ok_self = ProtocolEngine::decode_domain_name(pkt_self.data(), pkt_self.size(), offset, domain);
    TEST_ASSERT(!ok_self, "Self-referencing pointer loop MUST be rejected");

    // Scenario B: Mutual loop (offset 12 points to 20, offset 20 points to 12)
    std::vector<uint8_t> pkt_mutual(32, 0);
    pkt_mutual[12] = 0xC0;
    pkt_mutual[13] = 20;
    pkt_mutual[20] = 0xC0;
    pkt_mutual[21] = 12;

    offset = 12;
    bool ok_mutual = ProtocolEngine::decode_domain_name(pkt_mutual.data(), pkt_mutual.size(), offset, domain);
    TEST_ASSERT(!ok_mutual, "Mutual pointer loop (12 <-> 20) MUST be rejected");

    // Scenario C: Exceeding MAX_DECOMPRESSION_DEPTH (17 chained pointers)
    std::vector<uint8_t> pkt_deep(100, 0);
    for (size_t i = 0; i < 18; ++i) {
        pkt_deep[i * 2] = 0xC0;
        pkt_deep[i * 2 + 1] = static_cast<uint8_t>((i + 1) * 2);
    }
    offset = 0;
    bool ok_deep = ProtocolEngine::decode_domain_name(pkt_deep.data(), pkt_deep.size(), offset, domain);
    TEST_ASSERT(!ok_deep, "Pointer chain exceeding depth limit 16 MUST be rejected");

    return true;
}

// ============================================================================
// Test 5: Out of Bounds & Malformed Pointer Handling
// ============================================================================
bool test_out_of_bounds_rejection() {
    std::vector<uint8_t> pkt(20, 0);
    // Pointer at offset 12 points to offset 250 (out of 20-byte buffer bounds)
    pkt[12] = 0xC0;
    pkt[13] = 250;

    size_t offset = 12;
    std::string domain;
    bool ok = ProtocolEngine::decode_domain_name(pkt.data(), pkt.size(), offset, domain);
    TEST_ASSERT(!ok, "Out-of-bounds pointer target must be safely rejected");

    // Truncated pointer (0xC0 as last byte with no 2nd byte)
    std::vector<uint8_t> trunc_pkt = { 0xC0 };
    offset = 0;
    bool ok_trunc = ProtocolEngine::decode_domain_name(trunc_pkt.data(), trunc_pkt.size(), offset, domain);
    TEST_ASSERT(!ok_trunc, "Truncated 1-byte pointer must be safely rejected");

    return true;
}

// ============================================================================
// Test 6: Full Query Packet Parsing
// ============================================================================
bool test_full_query_packet_parsing() {
    // Build real DNS query wire buffer:
    // Header (12 bytes): ID=0x1234, Flags=0x0100 (RD=1), QDCOUNT=1
    // Question: "google.com" (0x06 google 0x03 com 0x00), QTYPE=1 (A), QCLASS=1 (IN)
    std::vector<uint8_t> wire;
    wire.resize(DNS_HEADER_SIZE, 0);
    write_uint16_be(wire.data(), 0x1234);
    write_uint16_be(wire.data() + 2, FLAG_RD); // RD=1
    write_uint16_be(wire.data() + 4, 1);       // QDCOUNT=1

    ProtocolEngine::encode_domain_name("google.com", wire);
    size_t qend = wire.size();
    wire.resize(qend + 4);
    write_uint16_be(wire.data() + qend, QTYPE_A);
    write_uint16_be(wire.data() + qend + 2, QCLASS_IN);

    DNSPacket pkt;
    std::string err;
    bool ok = ProtocolEngine::parse(wire.data(), wire.size(), pkt, err);
    TEST_ASSERT(ok, "Failed to parse valid query packet: " + err);
    TEST_ASSERT(pkt.header.id == 0x1234, "Query ID mismatch");
    TEST_ASSERT(pkt.header.rd == true, "RD flag mismatch");
    TEST_ASSERT(pkt.is_query(), "Packet must be identified as query");
    TEST_ASSERT(pkt.questions.size() == 1, "Question count mismatch");
    TEST_ASSERT(pkt.questions[0].qname == "google.com", "Question name mismatch");
    TEST_ASSERT(pkt.questions[0].qtype == QTYPE_A, "Question type mismatch");
    TEST_ASSERT(pkt.questions[0].qclass == QCLASS_IN, "Question class mismatch");

    return true;
}

// ============================================================================
// Test 7: Full Response Serialization with Pointer Compression & Roundtrip
// ============================================================================
bool test_response_serialization_and_roundtrip() {
    DNSPacket query;
    query.header.id = 0x4321;
    query.header.rd = true;
    DNSQuestion q;
    q.qname = "example.com";
    q.qtype = QTYPE_A;
    q.qclass = QCLASS_IN;
    query.questions.push_back(q);

    DNSPacket resp = ProtocolEngine::create_response(query, RCODE_NOERROR);
    resp.header.ra = true; // Recursion available

    // Add A record answer
    DNSResourceRecord rr = DNSResourceRecord::create_a("example.com", 300, "93.184.216.34");
    resp.answers.push_back(rr);

    std::vector<uint8_t> wire;
    bool ok_ser = ProtocolEngine::serialize(resp, wire, true);
    TEST_ASSERT(ok_ser, "Failed to serialize response packet");

    // Verify pointer compression in wire buffer:
    // Header = 12 bytes
    // Question = 13 (name) + 4 (type, class) = 17 bytes (ends at offset 29)
    // Answer starts at offset 29:
    // If compressed, bytes 29 and 30 must be 0xC0 and 0x0C (pointing to question at offset 12)!
    TEST_ASSERT(wire[29] == 0xC0, "Answer name must start with compression pointer 0xC0");
    TEST_ASSERT(wire[30] == 0x0C, "Answer pointer target must be 0x0C (offset 12)");

    // Roundtrip verification: Parse serialized wire response back
    DNSPacket parsed;
    std::string err;
    bool ok_parse = ProtocolEngine::parse(wire.data(), wire.size(), parsed, err);
    TEST_ASSERT(ok_parse, "Failed to parse serialized response wire: " + err);
    TEST_ASSERT(parsed.header.id == 0x4321, "Parsed response ID mismatch");
    TEST_ASSERT(parsed.header.qr == true, "Parsed packet must have QR=1");
    TEST_ASSERT(parsed.header.ra == true, "Parsed packet must have RA=1");
    TEST_ASSERT(parsed.answers.size() == 1, "Answer count mismatch");
    TEST_ASSERT(parsed.answers[0].name == "example.com", "Answer domain mismatch");
    TEST_ASSERT(parsed.answers[0].type == QTYPE_A, "Answer type mismatch");
    TEST_ASSERT(parsed.answers[0].ttl == 300, "Answer TTL mismatch");
    TEST_ASSERT(parsed.answers[0].readable_data == "93.184.216.34", "Answer IP mismatch");

    return true;
}

// ============================================================================
// Test 8: CNAME and IPv6 (AAAA) Records Support
// ============================================================================
bool test_cname_and_aaaa_records() {
    DNSPacket pkt;
    pkt.header.id = 0x5555;
    pkt.header.qr = true;

    // CNAME answer
    DNSResourceRecord cname_rr = DNSResourceRecord::create_cname("alias.com", 600, "target.com");
    pkt.answers.push_back(cname_rr);

    // AAAA answer
    DNSResourceRecord aaaa_rr = DNSResourceRecord::create_aaaa("ipv6.target.com", 120, "2606:2800:220:1:248:1893:25c8:1946");
    pkt.answers.push_back(aaaa_rr);

    std::vector<uint8_t> wire;
    bool ok_ser = ProtocolEngine::serialize(pkt, wire, false);
    TEST_ASSERT(ok_ser, "Failed to serialize CNAME and AAAA packet");

    DNSPacket parsed;
    std::string err;
    bool ok_parse = ProtocolEngine::parse(wire.data(), wire.size(), parsed, err);
    TEST_ASSERT(ok_parse, "Failed to parse CNAME and AAAA packet: " + err);
    TEST_ASSERT(parsed.answers.size() == 2, "Expected 2 answers");
    TEST_ASSERT(parsed.answers[0].type == QTYPE_CNAME, "Record 0 must be CNAME");
    TEST_ASSERT(parsed.answers[0].readable_data == "target.com", "CNAME target mismatch");
    TEST_ASSERT(parsed.answers[1].type == QTYPE_AAAA, "Record 1 must be AAAA");

    return true;
}

// ============================================================================
// Test 9: Raw Error Response Generator (FORMERR, SERVFAIL)
// ============================================================================
bool test_error_response_generation() {
    // Malformed raw buffer (only 14 bytes, truncated question)
    uint8_t malformed_query[14] = {
        0x77, 0x88,             // ID
        0x01, 0x00,             // Flags (RD=1)
        0x00, 0x01,             // QDCOUNT = 1
        0x00, 0x00, 0x00, 0x00, // AN, NS, AR
        0x05, 't', 'e', 's'     // Truncated label: says 5 bytes, only 3 provided
    };

    std::vector<uint8_t> err_resp = ProtocolEngine::create_error_response_raw(malformed_query, 14, RCODE_FORMERR);
    TEST_ASSERT(err_resp.size() >= DNS_HEADER_SIZE, "Error response too short");

    DNSHeader h = DNSHeader::deserialize(err_resp.data());
    TEST_ASSERT(h.id == 0x7788, "Error response must preserve query ID");
    TEST_ASSERT(h.qr == true, "Error response must be QR=1");
    TEST_ASSERT(h.rcode == RCODE_FORMERR, "Error response must have RCODE=FORMERR (1)");

    return true;
}

// ============================================================================
// Test 10: Packet Dump Output Formatting
// ============================================================================
bool test_dump_packet() {
    DNSPacket pkt;
    pkt.header.id = 0x9999;
    pkt.header.qr = true;
    pkt.header.rcode = RCODE_NOERROR;
    DNSQuestion q;
    q.qname = "pes.edu";
    q.qtype = QTYPE_A;
    pkt.questions.push_back(q);
    pkt.answers.push_back(DNSResourceRecord::create_a("pes.edu", 3600, "10.0.0.1"));

    std::string dump = ProtocolEngine::dump_packet(pkt);
    TEST_ASSERT(!dump.empty(), "Dump output must not be empty");
    TEST_ASSERT(dump.find("pes.edu") != std::string::npos, "Dump must contain domain name");
    TEST_ASSERT(dump.find("10.0.0.1") != std::string::npos, "Dump must contain IP address");

    return true;
}

// ============================================================================
// Test Runner Main
// ============================================================================
int main() {
    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  ENGINE E1: PROTOCOL PARSING ENGINE TEST SUITE (C++)   " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "  RFC 1035 Wire Unpacker, Decompressor & Serializer      " << COLOR_RESET << std::endl;
    std::cout << COLOR_YELLOW << "========================================================" << COLOR_RESET << std::endl;

    RUN_TEST(test_header_bitfields_and_roundtrip);
    RUN_TEST(test_domain_name_encoding_and_decoding);
    RUN_TEST(test_pointer_decompression);
    RUN_TEST(test_pointer_loop_attack_prevention);
    RUN_TEST(test_out_of_bounds_rejection);
    RUN_TEST(test_full_query_packet_parsing);
    RUN_TEST(test_response_serialization_and_roundtrip);
    RUN_TEST(test_cname_and_aaaa_records);
    RUN_TEST(test_error_response_generation);
    RUN_TEST(test_dump_packet);

    std::cout << COLOR_YELLOW << "--------------------------------------------------------" << COLOR_RESET << std::endl;
    std::cout << "Test Summary: " << g_tests_passed << "/" << g_tests_run << " passed." << std::endl;

    if (g_tests_passed == g_tests_run) {
        std::cout << COLOR_GREEN << "ALL ENGINE E1 PROTOCOL TESTS PASSED EMPIRICALLY!" << COLOR_RESET << std::endl;
        return 0;
    } else {
        std::cout << COLOR_RED << "SOME ENGINE E1 PROTOCOL TESTS FAILED!" << COLOR_RESET << std::endl;
        return 1;
    }
}
