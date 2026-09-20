package handler

import (
	"errors"
	"fmt"
	"strings"

	"dns-resolver-caching-server/dns"
)

var (
	ErrNoQuestions       = errors.New("no question section in DNS packet")
	ErrInvalidDomain     = errors.New("invalid or empty domain name")
	ErrUnsupportedQType  = errors.New("unsupported query type (only A supported)")
	ErrUnsupportedQClass = errors.New("unsupported query class (only IN supported)")
	ErrMalformedPacket   = errors.New("malformed DNS packet")
)

// Query represents a validated internal DNS query ready for the resolution/cache pipeline.
type Query struct {
	Header    dns.Header
	Domain    string
	Type      uint16
	Class     uint16
	RawPacket []byte
}

// ProcessPacket receives raw UDP packet bytes, parses them using the dns package,
// and validates the DNS request to produce a clean internal Query structure.
func ProcessPacket(raw []byte) (*Query, error) {
	pkt, err := dns.ParsePacket(raw)
	if err != nil {
		return nil, fmt.Errorf("%w: %v", ErrMalformedPacket, err)
	}

	return ValidateQuery(pkt, raw)
}

// ValidateQuery validates a parsed DNS packet against supported rules:
// 1. Must contain at least one question.
// 2. Domain name must be non-empty and within valid length boundaries.
// 3. QTYPE must be TypeA (1).
// 4. QCLASS must be ClassIN (1).
func ValidateQuery(pkt *dns.Packet, raw []byte) (*Query, error) {
	if len(pkt.Questions) == 0 {
		return nil, ErrNoQuestions
	}

	// Extract primary question
	q := pkt.Questions[0]

	// Domain validation
	domain := strings.TrimSpace(q.Name)
	if domain == "" || domain == "." {
		return nil, ErrInvalidDomain
	}

	// Standard DNS max domain length is 253 characters
	if len(domain) > 253 {
		return nil, fmt.Errorf("%w: domain length %d exceeds max 253 characters", ErrInvalidDomain, len(domain))
	}

	// QTYPE validation (initially support QTYPE A = 1)
	if q.Type != dns.TypeA {
		return nil, fmt.Errorf("%w: QTYPE %d", ErrUnsupportedQType, q.Type)
	}

	// QCLASS validation (initially support QCLASS IN = 1)
	if q.Class != dns.ClassIN {
		return nil, fmt.Errorf("%w: QCLASS %d", ErrUnsupportedQClass, q.Class)
	}

	return &Query{
		Header:    pkt.Header,
		Domain:    domain,
		Type:      q.Type,
		Class:     q.Class,
		RawPacket: raw,
	}, nil
}
