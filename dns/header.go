package dns

import (
	"encoding/binary"
	"errors"
	"fmt"
)

// HeaderLen is the fixed size of a DNS message header in bytes.
const HeaderLen = 12

// Header represents the 12-byte DNS header section.
type Header struct {
	ID      uint16 // Transaction ID
	Flags   uint16 // Flags (QR, Opcode, AA, TC, RD, RA, Z, RCODE)
	QDCount uint16 // Question Count
	ANCount uint16 // Answer Count
	NSCount uint16 // Authority Record Count
	ARCount uint16 // Additional Record Count
}

var (
	// ErrTruncatedHeader is returned when the packet is smaller than the 12-byte DNS header.
	ErrTruncatedHeader = errors.New("truncated packet: header length less than 12 bytes")
)

// ParseHeader parses the 12-byte header from raw DNS packet data.
// DNS headers use Big Endian (network byte order).
func ParseHeader(data []byte) (Header, error) {
	if len(data) < HeaderLen {
		return Header{}, fmt.Errorf("%w: got %d bytes", ErrTruncatedHeader, len(data))
	}

	h := Header{
		ID:      binary.BigEndian.Uint16(data[0:2]),
		Flags:   binary.BigEndian.Uint16(data[2:4]),
		QDCount: binary.BigEndian.Uint16(data[4:6]),
		ANCount: binary.BigEndian.Uint16(data[6:8]),
		NSCount: binary.BigEndian.Uint16(data[8:10]),
		ARCount: binary.BigEndian.Uint16(data[10:12]),
	}

	return h, nil
}
