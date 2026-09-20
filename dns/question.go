package dns

import (
	"encoding/binary"
	"errors"
	"fmt"
	"strings"
)

// Common DNS Resource Record Types
const (
	TypeA     uint16 = 1  // IPv4 Host Address
	TypeNS    uint16 = 2  // Authoritative Name Server
	TypeCNAME uint16 = 5  // Canonical Name
	TypeSOA   uint16 = 6  // Start of Authority
	TypePTR   uint16 = 12 // Domain Name Pointer
	TypeMX    uint16 = 15 // Mail Exchange
	TypeTXT   uint16 = 16 // Text Strings
	TypeAAAA  uint16 = 28 // IPv6 Address
)

// Common DNS Resource Record Classes
const (
	ClassIN uint16 = 1 // Internet
)

var (
	ErrMalformedName     = errors.New("malformed DNS name")
	ErrTruncatedQuestion = errors.New("truncated question section")
)

// Question represents a DNS question entry.
type Question struct {
	Name  string // Decoded domain name (e.g., "example.com")
	Type  uint16 // Query Type (QTYPE)
	Class uint16 // Query Class (QCLASS)
}

// DecodeName decodes a label-encoded domain name starting at offset.
// It returns the decoded string, the new offset in data after the name section, and any error.
func DecodeName(data []byte, offset int) (string, int, error) {
	var labels []string
	curr := offset
	nextOffset := -1 // Offset after the name, tracking original path before pointer jumps
	jumped := false
	maxJumps := 5 // Prevent compression pointer infinite loops
	jumps := 0

	for {
		if curr >= len(data) {
			return "", 0, fmt.Errorf("%w: offset %d beyond packet length %d", ErrTruncatedQuestion, curr, len(data))
		}

		length := int(data[curr])

		// Check for zero-length label (end of name)
		if length == 0 {
			if !jumped {
				nextOffset = curr + 1
			}
			break
		}

		// Check for DNS pointer compression (top 2 bits are 11 -> 0xC0)
		if (length & 0xC0) == 0xC0 {
			if curr+1 >= len(data) {
				return "", 0, fmt.Errorf("%w: truncated compression pointer at offset %d", ErrTruncatedQuestion, curr)
			}
			if !jumped {
				nextOffset = curr + 2
			}
			jumped = true

			// Extract 14-bit pointer offset
			ptr := int(binary.BigEndian.Uint16(data[curr:curr+2]) & 0x3FFF)
			if ptr >= len(data) {
				return "", 0, fmt.Errorf("%w: compression pointer %d points outside packet", ErrMalformedName, ptr)
			}
			jumps++
			if jumps > maxJumps {
				return "", 0, fmt.Errorf("%w: too many compression pointers (loop detected)", ErrMalformedName)
			}
			curr = ptr
			continue
		}

		// Standard label length checks (max label length is 63 bytes)
		if (length & 0xC0) != 0 {
			return "", 0, fmt.Errorf("%w: invalid label length prefix 0x%X at offset %d", ErrMalformedName, length, curr)
		}
		if length > 63 {
			return "", 0, fmt.Errorf("%w: label length %d exceeds max 63 bytes", ErrMalformedName, length)
		}

		curr++
		if curr+length > len(data) {
			return "", 0, fmt.Errorf("%w: label extending beyond packet length", ErrTruncatedQuestion)
		}

		labels = append(labels, string(data[curr:curr+length]))
		curr += length
	}

	if len(labels) == 0 {
		return ".", nextOffset, nil
	}

	domain := strings.Join(labels, ".")
	return domain, nextOffset, nil
}

// ParseQuestion parses a single DNS question from data starting at offset.
func ParseQuestion(data []byte, offset int) (Question, int, error) {
	name, newOffset, err := DecodeName(data, offset)
	if err != nil {
		return Question{}, offset, err
	}

	// QTYPE (2 bytes) + QCLASS (2 bytes) = 4 bytes
	if newOffset+4 > len(data) {
		return Question{}, newOffset, fmt.Errorf("%w: insufficient bytes for QTYPE and QCLASS", ErrTruncatedQuestion)
	}

	qtype := binary.BigEndian.Uint16(data[newOffset : newOffset+2])
	qclass := binary.BigEndian.Uint16(data[newOffset+2 : newOffset+4])

	q := Question{
		Name:  name,
		Type:  qtype,
		Class: qclass,
	}

	return q, newOffset + 4, nil
}
