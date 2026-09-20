package dns

import (
	"encoding/binary"
	"errors"
	"fmt"
	"net"
)

var (
	ErrTruncatedAnswer = errors.New("truncated answer section")
	ErrInvalidRDLength = errors.New("invalid RDATA length for A record (expected 4 bytes)")
)

// ResourceRecord represents a parsed DNS answer / authority / additional record.
type ResourceRecord struct {
	Name     string // Domain name
	Type     uint16 // Record Type (e.g., TypeA = 1)
	Class    uint16 // Record Class (e.g., ClassIN = 1)
	TTL      uint32 // Time to Live in seconds
	RDLength uint16 // Length of RDATA in bytes
	RData    []byte // Raw RDATA bytes
	IP       string // Parsed IPv4 address string for A records
}

// ParseAnswer parses a single Resource Record from raw packet data starting at offset.
func ParseAnswer(data []byte, offset int) (ResourceRecord, int, error) {
	name, newOffset, err := DecodeName(data, offset)
	if err != nil {
		return ResourceRecord{}, offset, fmt.Errorf("failed to decode answer name: %w", err)
	}

	// TYPE (2B) + CLASS (2B) + TTL (4B) + RDLENGTH (2B) = 10 bytes minimum header
	if newOffset+10 > len(data) {
		return ResourceRecord{}, newOffset, fmt.Errorf("%w: offset %d beyond data length %d", ErrTruncatedAnswer, newOffset, len(data))
	}

	rtype := binary.BigEndian.Uint16(data[newOffset : newOffset+2])
	rclass := binary.BigEndian.Uint16(data[newOffset+2 : newOffset+4])
	ttl := binary.BigEndian.Uint32(data[newOffset+4 : newOffset+8])
	rdlength := binary.BigEndian.Uint16(data[newOffset+8 : newOffset+10])

	rdataStart := newOffset + 10
	rdataEnd := rdataStart + int(rdlength)

	if rdataEnd > len(data) {
		return ResourceRecord{}, newOffset, fmt.Errorf("%w: RDATA extends beyond packet length", ErrTruncatedAnswer)
	}

	rdata := data[rdataStart:rdataEnd]
	var ipStr string

	// For Type A records, parse the 4-byte IPv4 address
	if rtype == TypeA {
		if rdlength != 4 {
			return ResourceRecord{}, newOffset, fmt.Errorf("%w: got %d bytes", ErrInvalidRDLength, rdlength)
		}
		ipStr = net.IP(rdata).String()
	}

	rr := ResourceRecord{
		Name:     name,
		Type:     rtype,
		Class:    rclass,
		TTL:      ttl,
		RDLength: rdlength,
		RData:    rdata,
		IP:       ipStr,
	}

	return rr, rdataEnd, nil
}
