package response

import (
	"encoding/binary"
	"errors"
	"fmt"
	"net"

	"dns-resolver-caching-server/dns"
	"dns-resolver-caching-server/handler"
	"dns-resolver-caching-server/resolver"
)

var (
	ErrNilQuery           = errors.New("cannot generate response for nil query")
	ErrNilResult          = errors.New("cannot generate response for nil result")
	ErrInvalidIPv4Address = errors.New("invalid IPv4 address in resolution result")
	ErrDomainMismatch     = errors.New("domain mismatch between query and resolution result")
)

// BuildSuccessResponse constructs a raw binary DNS response packet (RFC 1035 wire format)
// from a validated handler.Query and resolver.Result.
func BuildSuccessResponse(q *handler.Query, r *resolver.Result) ([]byte, error) {
	if q == nil {
		return nil, ErrNilQuery
	}
	if r == nil {
		return nil, ErrNilResult
	}
	if q.Domain != r.Domain {
		return nil, fmt.Errorf("%w: query domain %q vs result domain %q", ErrDomainMismatch, q.Domain, r.Domain)
	}

	// Parse IPv4 address into 4 bytes
	ip := net.ParseIP(r.IP).To4()
	if ip == nil {
		return nil, fmt.Errorf("%w: %q", ErrInvalidIPv4Address, r.IP)
	}

	// Construct DNS response flags explicitly:
	// - Bit 15 (QR): 1 (Response) -> 0x8000
	// - Bits 11-14 (Opcode): 0 (Standard Query)
	// - Bit 10 (AA): 0 (Non-Authoritative Recursive Resolver)
	// - Bit 9 (TC): 0 (Not Truncated)
	// - Bit 8 (RD): Preserve client's Recursion Desired bit -> (q.Header.Flags & 0x0100)
	// - Bit 7 (RA): 1 (Recursion Available) -> 0x0080
	// - Bits 4-6 (Z): 0 (Reserved)
	// - Bits 0-3 (RCODE): 0 (NoError)
	flags := uint16(0x8000) | (q.Header.Flags & 0x0100) | uint16(0x0080)

	qdcount := uint16(1)
	ancount := uint16(1)
	nscount := uint16(0)
	arcount := uint16(0)

	buf := make([]byte, 0, 512)

	// Transaction ID (preserved from original client query)
	buf = append(buf, byte(q.Header.ID>>8), byte(q.Header.ID))
	// Flags
	buf = append(buf, byte(flags>>8), byte(flags))
	// Counts
	buf = append(buf, byte(qdcount>>8), byte(qdcount))
	buf = append(buf, byte(ancount>>8), byte(ancount))
	buf = append(buf, byte(nscount>>8), byte(nscount))
	buf = append(buf, byte(arcount>>8), byte(arcount))

	// 2. Question Section
	// QNAME label encoding
	for _, label := range splitDomain(q.Domain) {
		buf = append(buf, byte(len(label)))
		buf = append(buf, []byte(label)...)
	}
	buf = append(buf, 0x00) // End of QNAME

	// QTYPE (2 bytes) & QCLASS (2 bytes)
	buf = append(buf, byte(q.Type>>8), byte(q.Type))
	buf = append(buf, byte(q.Class>>8), byte(q.Class))

	// 3. Answer Section
	// Compression pointer 0xC00C (points to offset 12 in packet where QNAME starts)
	buf = append(buf, 0xC0, 0x0C)

	// TYPE: A (1), CLASS: IN (1)
	buf = append(buf, byte(dns.TypeA>>8), byte(dns.TypeA))
	buf = append(buf, byte(dns.ClassIN>>8), byte(dns.ClassIN))

	// TTL (4 bytes, big endian)
	ttlBytes := make([]byte, 4)
	binary.BigEndian.PutUint32(ttlBytes, r.TTL)
	buf = append(buf, ttlBytes...)

	// RDLENGTH: 4 bytes for IPv4 address
	buf = append(buf, 0x00, 0x04)

	// RDATA: 4-byte IPv4 address
	buf = append(buf, ip...)

	return buf, nil
}

// Helper to split domain name into component labels
func splitDomain(domain string) []string {
	if domain == "" {
		return nil
	}
	var labels []string
	start := 0
	for i := 0; i < len(domain); i++ {
		if domain[i] == '.' {
			if i > start {
				labels = append(labels, domain[start:i])
			}
			start = i + 1
		}
	}
	if start < len(domain) {
		labels = append(labels, domain[start:])
	}
	return labels
}
