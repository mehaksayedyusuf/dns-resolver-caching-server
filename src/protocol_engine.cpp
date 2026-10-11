#include "protocol_engine.hpp"
#include <iomanip>
#include <sstream>

// ============================================================================
// DNSHeader Member Implementations
// ============================================================================

uint16_t DNSHeader::encode_flags() const {
    uint16_t flags = 0;
    if (qr) flags |= FLAG_QR;
    flags |= static_cast<uint16_t>((opcode & 0x0F) << 11);
    if (aa) flags |= FLAG_AA;
    if (tc) flags |= FLAG_TC;
    if (rd) flags |= FLAG_RD;
    if (ra) flags |= FLAG_RA;
    flags |= static_cast<uint16_t>((z & 0x07) << 4);
    flags |= static_cast<uint16_t>(rcode & FLAG_RCODE_MASK);
    return flags;
}

void DNSHeader::decode_flags(uint16_t flags) {
    qr = (flags & FLAG_QR) != 0;
    opcode = static_cast<uint8_t>((flags >> 11) & 0x0F);
    aa = (flags & FLAG_AA) != 0;
    tc = (flags & FLAG_TC) != 0;
    rd = (flags & FLAG_RD) != 0;
    ra = (flags & FLAG_RA) != 0;
    z = static_cast<uint8_t>((flags >> 4) & 0x07);
    rcode = static_cast<uint8_t>(flags & FLAG_RCODE_MASK);
}

void DNSHeader::serialize(uint8_t* out_buf) const {
    write_uint16_be(out_buf, id);
    write_uint16_be(out_buf + 2, encode_flags());
    write_uint16_be(out_buf + 4, qdcount);
    write_uint16_be(out_buf + 6, ancount);
    write_uint16_be(out_buf + 8, nscount);
    write_uint16_be(out_buf + 10, arcount);
}

DNSHeader DNSHeader::deserialize(const uint8_t* in_buf) {
    DNSHeader h;
    h.id = read_uint16_be(in_buf);
    h.decode_flags(read_uint16_be(in_buf + 2));
    h.qdcount = read_uint16_be(in_buf + 4);
    h.ancount = read_uint16_be(in_buf + 6);
    h.nscount = read_uint16_be(in_buf + 8);
    h.arcount = read_uint16_be(in_buf + 10);
    return h;
}

// ============================================================================
// DNSResourceRecord Factory Methods
// ============================================================================

DNSResourceRecord DNSResourceRecord::create_a(const std::string& name, uint32_t ttl, const std::string& ip) {
    DNSResourceRecord rr;
    rr.name = ProtocolEngine::normalize_domain(name);
    rr.type = QTYPE_A;
    rr.rclass = QCLASS_IN;
    rr.ttl = ttl;
    ProtocolEngine::string_to_ipv4(ip, rr.rdata);
    rr.rdlength = static_cast<uint16_t>(rr.rdata.size());
    rr.readable_data = ip;
    return rr;
}

DNSResourceRecord DNSResourceRecord::create_cname(const std::string& name, uint32_t ttl, const std::string& cname) {
    DNSResourceRecord rr;
    rr.name = ProtocolEngine::normalize_domain(name);
    rr.type = QTYPE_CNAME;
    rr.rclass = QCLASS_IN;
    rr.ttl = ttl;
    rr.readable_data = ProtocolEngine::normalize_domain(cname);
    ProtocolEngine::encode_domain_name(rr.readable_data, rr.rdata);
    rr.rdlength = static_cast<uint16_t>(rr.rdata.size());
    return rr;
}

DNSResourceRecord DNSResourceRecord::create_aaaa(const std::string& name, uint32_t ttl, const std::string& ip6) {
    DNSResourceRecord rr;
    rr.name = ProtocolEngine::normalize_domain(name);
    rr.type = QTYPE_AAAA;
    rr.rclass = QCLASS_IN;
    rr.ttl = ttl;
    ProtocolEngine::string_to_ipv6(ip6, rr.rdata);
    rr.rdlength = static_cast<uint16_t>(rr.rdata.size());
    rr.readable_data = ip6;
    return rr;
}

// ============================================================================
// ProtocolEngine Implementation
// ============================================================================

std::string ProtocolEngine::normalize_domain(const std::string& domain) {
    if (domain.empty() || domain == ".") {
        return "";
    }
    std::string norm = domain;
    // Lowercase
    std::transform(norm.begin(), norm.end(), norm.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    // Remove trailing dot if present
    if (!norm.empty() && norm.back() == '.') {
        norm.pop_back();
    }
    return norm;
}

bool ProtocolEngine::decode_domain_name(const uint8_t* buffer, size_t length, size_t& offset, std::string& out_domain, size_t recursion_depth) {
    if (recursion_depth > MAX_DECOMPRESSION_DEPTH || offset >= length) {
        return false;
    }

    size_t curr = offset;
    bool jumped = false;
    size_t depth = recursion_depth;
    std::string result;
    std::vector<size_t> visited_offsets;

    while (curr < length) {
        uint8_t len = buffer[curr];

        // RFC 1035 section 4.1.4: Compression pointer has two high bits set (0b11xxxxxx)
        if ((len & 0xC0) == 0xC0) {
            if (curr + 1 >= length) {
                return false; // Truncated pointer
            }
            if (depth >= MAX_DECOMPRESSION_DEPTH) {
                return false; // Exceeded maximum recursion depth (cyclic attack)
            }
            uint16_t ptr_offset = (static_cast<uint16_t>(len & 0x3F) << 8) | buffer[curr + 1];
            if (ptr_offset >= length) {
                return false; // Pointer target out of packet bounds
            }

            // Defend against immediate pointer loops (e.g. A -> A or cycle)
            if (std::find(visited_offsets.begin(), visited_offsets.end(), ptr_offset) != visited_offsets.end()) {
                return false; // Pointer loop cycle detected
            }
            visited_offsets.push_back(ptr_offset);

            if (!jumped) {
                offset = curr + 2; // Advance calling stream past the 2-byte pointer
                jumped = true;
            }
            curr = ptr_offset;
            depth++;
        } else if (len == 0) {
            // End of domain name label sequence
            if (!jumped) {
                offset = curr + 1; // Advance calling stream past null terminator
            }
            out_domain = normalize_domain(result);
            return true;
        } else if ((len & 0xC0) != 0) {
            // Reserved label types (RFC 1035 4.1.4)
            return false;
        } else {
            // Normal uncompressed label
            curr++;
            if (curr + len > length) {
                return false; // Label length byte exceeds buffer length
            }
            if (!result.empty()) {
                result += ".";
            }
            result.append(reinterpret_cast<const char*>(buffer + curr), len);
            curr += len;
        }
    }

    return false; // Buffer exhausted without null terminator
}

void ProtocolEngine::encode_domain_name(const std::string& domain, std::vector<uint8_t>& out_buffer) {
    std::string norm = normalize_domain(domain);
    if (norm.empty()) {
        out_buffer.push_back(0);
        return;
    }

    size_t start = 0;
    while (start < norm.size()) {
        size_t dot_pos = norm.find('.', start);
        if (dot_pos == std::string::npos) {
            dot_pos = norm.size();
        }
        size_t label_len = dot_pos - start;
        if (label_len > 63) {
            label_len = 63;
        }
        out_buffer.push_back(static_cast<uint8_t>(label_len));
        for (size_t i = 0; i < label_len; ++i) {
            out_buffer.push_back(static_cast<uint8_t>(norm[start + i]));
        }
        start = dot_pos + 1;
    }
    out_buffer.push_back(0);
}

void ProtocolEngine::encode_domain_name_compressed(const std::string& domain, std::vector<uint8_t>& out_buffer, std::map<std::string, uint16_t>& compression_map) {
    std::string norm = normalize_domain(domain);
    if (norm.empty()) {
        out_buffer.push_back(0);
        return;
    }

    size_t start = 0;
    while (start < norm.size()) {
        std::string suffix = norm.substr(start);
        auto it = compression_map.find(suffix);
        if (it != compression_map.end() && it->second < 16384) {
            // Emit RFC 1035 compression pointer (0xC000 | offset)
            uint16_t ptr = 0xC000 | it->second;
            out_buffer.push_back(static_cast<uint8_t>((ptr >> 8) & 0xFF));
            out_buffer.push_back(static_cast<uint8_t>(ptr & 0xFF));
            return;
        }

        // Record suffix offset if it fits within 14-bit pointer bounds
        if (out_buffer.size() < 16384) {
            compression_map[suffix] = static_cast<uint16_t>(out_buffer.size());
        }

        size_t dot_pos = norm.find('.', start);
        if (dot_pos == std::string::npos) {
            dot_pos = norm.size();
        }
        size_t label_len = dot_pos - start;
        if (label_len > 63) {
            label_len = 63;
        }

        out_buffer.push_back(static_cast<uint8_t>(label_len));
        for (size_t i = 0; i < label_len; ++i) {
            out_buffer.push_back(static_cast<uint8_t>(norm[start + i]));
        }
        start = dot_pos + 1;
    }
    out_buffer.push_back(0);
}

bool ProtocolEngine::parse_rr(const uint8_t* buffer, size_t length, size_t& offset, DNSResourceRecord& out_rr, std::string& out_error) {
    if (!decode_domain_name(buffer, length, offset, out_rr.name)) {
        out_error = "Malformed domain name in RR";
        return false;
    }
    if (offset + 10 > length) {
        out_error = "Truncated RR header fields";
        return false;
    }

    out_rr.type = read_uint16_be(buffer + offset);
    offset += 2;
    out_rr.rclass = read_uint16_be(buffer + offset);
    offset += 2;
    out_rr.ttl = read_uint32_be(buffer + offset);
    offset += 4;
    out_rr.rdlength = read_uint16_be(buffer + offset);
    offset += 2;

    if (offset + out_rr.rdlength > length) {
        out_error = "Truncated RDATA in RR";
        return false;
    }

    out_rr.rdata.assign(buffer + offset, buffer + offset + out_rr.rdlength);

    // Decode readable data based on record type
    if (out_rr.type == QTYPE_A && out_rr.rdlength == 4) {
        out_rr.readable_data = ipv4_to_string(buffer + offset, 4);
        offset += out_rr.rdlength;
    } else if (out_rr.type == QTYPE_AAAA && out_rr.rdlength == 16) {
        out_rr.readable_data = ipv6_to_string(buffer + offset, 16);
        offset += out_rr.rdlength;
    } else if (out_rr.type == QTYPE_CNAME || out_rr.type == QTYPE_PTR || out_rr.type == QTYPE_NS) {
        size_t cname_offset = offset;
        std::string target_domain;
        if (decode_domain_name(buffer, length, cname_offset, target_domain)) {
            out_rr.readable_data = target_domain;
        } else {
            out_rr.readable_data = "<invalid-domain>";
        }
        offset += out_rr.rdlength;
    } else if (out_rr.type == QTYPE_TXT) {
        std::string txt;
        size_t p = offset;
        size_t end = offset + out_rr.rdlength;
        while (p < end) {
            uint8_t tlen = buffer[p++];
            if (p + tlen <= end) {
                if (!txt.empty()) txt += " ";
                txt.append(reinterpret_cast<const char*>(buffer + p), tlen);
                p += tlen;
            } else {
                break;
            }
        }
        out_rr.readable_data = txt;
        offset += out_rr.rdlength;
    } else {
        out_rr.readable_data = "<raw-binary>";
        offset += out_rr.rdlength;
    }

    return true;
}

bool ProtocolEngine::parse(const uint8_t* buffer, size_t length, DNSPacket& out_packet, std::string& out_error) {
    if (length < DNS_HEADER_SIZE) {
        out_error = "Packet buffer smaller than 12-byte DNS header";
        return false;
    }

    out_packet.header = DNSHeader::deserialize(buffer);
    size_t offset = DNS_HEADER_SIZE;

    // Parse Questions
    out_packet.questions.clear();
    for (uint16_t i = 0; i < out_packet.header.qdcount; ++i) {
        DNSQuestion q;
        if (!decode_domain_name(buffer, length, offset, q.qname)) {
            out_error = "Failed to decode QNAME in question " + std::to_string(i);
            return false;
        }
        if (offset + 4 > length) {
            out_error = "Truncated Question QTYPE/QCLASS";
            return false;
        }
        q.qtype = read_uint16_be(buffer + offset);
        offset += 2;
        q.qclass = read_uint16_be(buffer + offset);
        offset += 2;
        out_packet.questions.push_back(q);
    }

    // Parse Answers
    out_packet.answers.clear();
    for (uint16_t i = 0; i < out_packet.header.ancount; ++i) {
        DNSResourceRecord rr;
        if (!parse_rr(buffer, length, offset, rr, out_error)) {
            return false;
        }
        out_packet.answers.push_back(rr);
    }

    // Parse Authorities
    out_packet.authorities.clear();
    for (uint16_t i = 0; i < out_packet.header.nscount; ++i) {
        DNSResourceRecord rr;
        if (!parse_rr(buffer, length, offset, rr, out_error)) {
            return false;
        }
        out_packet.authorities.push_back(rr);
    }

    // Parse Additionals
    out_packet.additionals.clear();
    for (uint16_t i = 0; i < out_packet.header.arcount; ++i) {
        DNSResourceRecord rr;
        if (!parse_rr(buffer, length, offset, rr, out_error)) {
            return false;
        }
        out_packet.additionals.push_back(rr);
    }

    return true;
}

bool ProtocolEngine::serialize(const DNSPacket& packet, std::vector<uint8_t>& out_buffer, bool use_compression) {
    out_buffer.clear();
    out_buffer.resize(DNS_HEADER_SIZE, 0);

    std::map<std::string, uint16_t> compression_map;

    // Serialize Questions
    for (const auto& q : packet.questions) {
        if (use_compression) {
            encode_domain_name_compressed(q.qname, out_buffer, compression_map);
        } else {
            encode_domain_name(q.qname, out_buffer);
        }
        size_t curr = out_buffer.size();
        out_buffer.resize(curr + 4);
        write_uint16_be(out_buffer.data() + curr, q.qtype);
        write_uint16_be(out_buffer.data() + curr + 2, q.qclass);
    }

    // Serialize RR Section Helper
    auto serialize_rr_list = [&](const std::vector<DNSResourceRecord>& rrs) {
        for (const auto& rr : rrs) {
            if (use_compression) {
                encode_domain_name_compressed(rr.name, out_buffer, compression_map);
            } else {
                encode_domain_name(rr.name, out_buffer);
            }
            size_t curr = out_buffer.size();
            out_buffer.resize(curr + 10);
            write_uint16_be(out_buffer.data() + curr, rr.type);
            write_uint16_be(out_buffer.data() + curr + 2, rr.rclass);
            write_uint32_be(out_buffer.data() + curr + 4, rr.ttl);

            // If CNAME / PTR / NS record and readable target is available, compress it
            if (use_compression && (rr.type == QTYPE_CNAME || rr.type == QTYPE_PTR || rr.type == QTYPE_NS) && !rr.readable_data.empty()) {
                std::vector<uint8_t> encoded_target;
                encode_domain_name_compressed(rr.readable_data, encoded_target, compression_map);
                write_uint16_be(out_buffer.data() + curr + 8, static_cast<uint16_t>(encoded_target.size()));
                out_buffer.insert(out_buffer.end(), encoded_target.begin(), encoded_target.end());
            } else {
                uint16_t rdlen = static_cast<uint16_t>(rr.rdata.size());
                write_uint16_be(out_buffer.data() + curr + 8, rdlen);
                if (rdlen > 0) {
                    out_buffer.insert(out_buffer.end(), rr.rdata.begin(), rr.rdata.end());
                }
            }
        }
    };

    serialize_rr_list(packet.answers);
    serialize_rr_list(packet.authorities);
    serialize_rr_list(packet.additionals);

    // Finalize Header counts and flags
    DNSHeader h = packet.header;
    h.qdcount = static_cast<uint16_t>(packet.questions.size());
    h.ancount = static_cast<uint16_t>(packet.answers.size());
    h.nscount = static_cast<uint16_t>(packet.authorities.size());
    h.arcount = static_cast<uint16_t>(packet.additionals.size());
    h.serialize(out_buffer.data());

    return true;
}

DNSPacket ProtocolEngine::create_response(const DNSPacket& query, uint8_t rcode) {
    DNSPacket resp;
    resp.header.id = query.header.id;
    resp.header.qr = true;          // Response bit set
    resp.header.opcode = query.header.opcode;
    resp.header.aa = false;
    resp.header.tc = false;
    resp.header.rd = query.header.rd; // Echo recursion desired
    resp.header.ra = true;           // Server supports recursion
    resp.header.z = 0;
    resp.header.rcode = rcode;
    resp.questions = query.questions;
    return resp;
}

std::vector<uint8_t> ProtocolEngine::create_error_response_raw(const uint8_t* query_buf, size_t query_len, uint8_t rcode) {
    std::vector<uint8_t> resp(DNS_HEADER_SIZE, 0);
    uint16_t id = 0;
    bool rd = false;

    if (query_buf && query_len >= 2) {
        id = read_uint16_be(query_buf);
    }
    if (query_buf && query_len >= 4) {
        uint16_t flags = read_uint16_be(query_buf + 2);
        rd = (flags & FLAG_RD) != 0;
    }

    DNSHeader h;
    h.id = id;
    h.qr = true;
    h.opcode = 0;
    h.aa = false;
    h.tc = false;
    h.rd = rd;
    h.ra = true;
    h.z = 0;
    h.rcode = rcode;
    h.qdcount = 0;
    h.ancount = 0;
    h.nscount = 0;
    h.arcount = 0;
    h.serialize(resp.data());

    // If query contains a valid question section, attempt to preserve it in FORMERR/SERVFAIL
    if (query_buf && query_len > DNS_HEADER_SIZE) {
        DNSPacket parsed;
        std::string err;
        if (parse(query_buf, query_len, parsed, err) && !parsed.questions.empty()) {
            DNSPacket err_pkt = create_response(parsed, rcode);
            std::vector<uint8_t> formatted_resp;
            if (serialize(err_pkt, formatted_resp)) {
                return formatted_resp;
            }
        }
    }

    return resp;
}

std::string ProtocolEngine::ipv4_to_string(const uint8_t* rdata, size_t len) {
    if (len < 4 || !rdata) return "";
    std::ostringstream oss;
    oss << static_cast<unsigned int>(rdata[0]) << "."
        << static_cast<unsigned int>(rdata[1]) << "."
        << static_cast<unsigned int>(rdata[2]) << "."
        << static_cast<unsigned int>(rdata[3]);
    return oss.str();
}

bool ProtocolEngine::string_to_ipv4(const std::string& ip_str, std::vector<uint8_t>& out_bytes) {
    out_bytes.clear();
    std::istringstream iss(ip_str);
    std::string token;
    std::vector<int> parts;
    while (std::getline(iss, token, '.')) {
        if (token.empty()) return false;
        try {
            int val = std::stoi(token);
            if (val < 0 || val > 255) return false;
            parts.push_back(val);
        } catch (...) {
            return false;
        }
    }
    if (parts.size() != 4) return false;
    out_bytes.resize(4);
    for (size_t i = 0; i < 4; ++i) {
        out_bytes[i] = static_cast<uint8_t>(parts[i]);
    }
    return true;
}

std::string ProtocolEngine::ipv6_to_string(const uint8_t* rdata, size_t len) {
    if (len < 16 || !rdata) return "";
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 16; i += 2) {
        if (i > 0) oss << ":";
        uint16_t group = (static_cast<uint16_t>(rdata[i]) << 8) | rdata[i + 1];
        oss << group;
    }
    return oss.str();
}

bool ProtocolEngine::string_to_ipv6(const std::string& ip_str, std::vector<uint8_t>& out_bytes) {
    out_bytes.assign(16, 0);
    // Simple hex colon parser for standard 8 groups
    std::istringstream iss(ip_str);
    std::string group;
    std::vector<uint16_t> groups;
    while (std::getline(iss, group, ':')) {
        if (group.empty()) continue;
        try {
            unsigned long val = std::stoul(group, nullptr, 16);
            if (val > 0xFFFF) return false;
            groups.push_back(static_cast<uint16_t>(val));
        } catch (...) {
            return false;
        }
    }
    if (groups.size() != 8) return false;
    for (size_t i = 0; i < 8; ++i) {
        out_bytes[i * 2] = static_cast<uint8_t>((groups[i] >> 8) & 0xFF);
        out_bytes[i * 2 + 1] = static_cast<uint8_t>(groups[i] & 0xFF);
    }
    return true;
}

std::string ProtocolEngine::qtype_to_string(uint16_t qtype) {
    switch (qtype) {
        case QTYPE_A:     return "A";
        case QTYPE_NS:    return "NS";
        case QTYPE_CNAME: return "CNAME";
        case QTYPE_SOA:   return "SOA";
        case QTYPE_PTR:   return "PTR";
        case QTYPE_MX:    return "MX";
        case QTYPE_TXT:   return "TXT";
        case QTYPE_AAAA:  return "AAAA";
        default:          return "TYPE" + std::to_string(qtype);
    }
}

uint16_t ProtocolEngine::string_to_qtype(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    if (s == "A")     return QTYPE_A;
    if (s == "NS")    return QTYPE_NS;
    if (s == "CNAME") return QTYPE_CNAME;
    if (s == "SOA")   return QTYPE_SOA;
    if (s == "PTR")   return QTYPE_PTR;
    if (s == "MX")    return QTYPE_MX;
    if (s == "TXT")   return QTYPE_TXT;
    if (s == "AAAA")  return QTYPE_AAAA;
    return QTYPE_A;
}

std::string ProtocolEngine::rcode_to_string(uint8_t rcode) {
    switch (rcode) {
        case RCODE_NOERROR:  return "NOERROR";
        case RCODE_FORMERR:  return "FORMERR";
        case RCODE_SERVFAIL: return "SERVFAIL";
        case RCODE_NXDOMAIN: return "NXDOMAIN";
        case RCODE_NOTIMP:   return "NOTIMP";
        case RCODE_REFUSED:  return "REFUSED";
        default:             return "RCODE" + std::to_string(rcode);
    }
}

std::string ProtocolEngine::dump_packet(const DNSPacket& packet) {
    std::ostringstream oss;
    oss << "=== DNS PACKET (ID: 0x" << std::hex << packet.header.id << std::dec << ") ===\n";
    oss << "  Flags: " << (packet.header.qr ? "RESPONSE" : "QUERY")
        << ", Opcode: " << static_cast<int>(packet.header.opcode)
        << ", AA: " << packet.header.aa
        << ", TC: " << packet.header.tc
        << ", RD: " << packet.header.rd
        << ", RA: " << packet.header.ra
        << ", RCODE: " << rcode_to_string(packet.header.rcode) << "\n";
    oss << "  Counts: Questions=" << packet.questions.size()
        << ", Answers=" << packet.answers.size()
        << ", Authorities=" << packet.authorities.size()
        << ", Additionals=" << packet.additionals.size() << "\n";

    for (size_t i = 0; i < packet.questions.size(); ++i) {
        const auto& q = packet.questions[i];
        oss << "  [QD " << i << "] Name: '" << q.qname << "' Type: " << qtype_to_string(q.qtype) << " Class: IN\n";
    }

    for (size_t i = 0; i < packet.answers.size(); ++i) {
        const auto& an = packet.answers[i];
        oss << "  [AN " << i << "] Name: '" << an.name << "' Type: " << qtype_to_string(an.type)
            << " TTL: " << an.ttl << "s Data: " << an.readable_data << "\n";
    }

    for (size_t i = 0; i < packet.authorities.size(); ++i) {
        const auto& ns = packet.authorities[i];
        oss << "  [NS " << i << "] Name: '" << ns.name << "' Type: " << qtype_to_string(ns.type)
            << " TTL: " << ns.ttl << "s Data: " << ns.readable_data << "\n";
    }

    return oss.str();
}
