package dns

import (
	"errors"
	"testing"
)

// Helper to construct a valid raw DNS query packet for "example.com" A-record (IN class)
func createSampleQuery() []byte {
	return []byte{
		// 12-byte DNS Header
		0x12, 0x34, // ID: 0x1234
		0x01, 0x00, // Flags: Standard Query (RD=1)
		0x00, 0x01, // QDCount: 1 question
		0x00, 0x00, // ANCount: 0
		0x00, 0x00, // NSCount: 0
		0x00, 0x00, // ARCount: 0

		// Question: example.com
		0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
		0x03, 'c', 'o', 'm',
		0x00,       // End of name
		0x00, 0x01, // QTYPE: A (1)
		0x00, 0x01, // QCLASS: IN (1)
	}
}

// 1. Test Header Parsing
func TestParseHeader(t *testing.T) {
	data := createSampleQuery()
	header, err := ParseHeader(data)
	if err != nil {
		t.Fatalf("unexpected error parsing header: %v", err)
	}

	if header.ID != 0x1234 {
		t.Errorf("expected ID 0x1234, got 0x%X", header.ID)
	}
	if header.Flags != 0x0100 {
		t.Errorf("expected Flags 0x0100, got 0x%X", header.Flags)
	}
	if header.QDCount != 1 {
		t.Errorf("expected QDCount 1, got %d", header.QDCount)
	}
	if header.ANCount != 0 || header.NSCount != 0 || header.ARCount != 0 {
		t.Errorf("expected counts (0,0,0), got (%d,%d,%d)", header.ANCount, header.NSCount, header.ARCount)
	}
}

// 2. Test QNAME Decoding
func TestDecodeName(t *testing.T) {
	data := createSampleQuery()
	name, nextOffset, err := DecodeName(data, 12)
	if err != nil {
		t.Fatalf("unexpected error decoding QNAME: %v", err)
	}

	expectedName := "example.com"
	if name != expectedName {
		t.Errorf("expected decoded name %q, got %q", expectedName, name)
	}

	// 12 + 13 bytes for "example.com\0" = 25
	if nextOffset != 25 {
		t.Errorf("expected nextOffset 25, got %d", nextOffset)
	}
}

// 3. Test Question Parsing (including QTYPE and QCLASS)
func TestParseQuestion(t *testing.T) {
	data := createSampleQuery()
	q, nextOffset, err := ParseQuestion(data, 12)
	if err != nil {
		t.Fatalf("unexpected error parsing question: %v", err)
	}

	if q.Name != "example.com" {
		t.Errorf("expected QNAME example.com, got %s", q.Name)
	}
	if q.Type != TypeA {
		t.Errorf("expected QTYPE %d (TypeA), got %d", TypeA, q.Type)
	}
	if q.Class != ClassIN {
		t.Errorf("expected QCLASS %d (ClassIN), got %d", ClassIN, q.Class)
	}
	if nextOffset != len(data) {
		t.Errorf("expected final offset %d, got %d", len(data), nextOffset)
	}
}

// 4. Test Complete Packet Parsing
func TestParsePacket(t *testing.T) {
	data := createSampleQuery()
	pkt, err := ParsePacket(data)
	if err != nil {
		t.Fatalf("unexpected error parsing packet: %v", err)
	}

	if pkt.Header.ID != 0x1234 {
		t.Errorf("expected ID 0x1234, got 0x%X", pkt.Header.ID)
	}
	if len(pkt.Questions) != 1 {
		t.Fatalf("expected 1 question, got %d", len(pkt.Questions))
	}
	if pkt.Questions[0].Name != "example.com" {
		t.Errorf("expected question name example.com, got %s", pkt.Questions[0].Name)
	}
}

// 5. Test Truncated Packets
func TestTruncatedPackets(t *testing.T) {
	// 5a. Truncated Header (< 12 bytes)
	shortHeader := []byte{0x12, 0x34, 0x01, 0x00}
	_, err := ParsePacket(shortHeader)
	if !errors.Is(err, ErrTruncatedHeader) {
		t.Errorf("expected ErrTruncatedHeader, got %v", err)
	}

	// 5b. Truncated Question (Header claims 1 question, but bytes end abruptly in QNAME)
	truncatedQNAME := []byte{
		0x12, 0x34, 0x01, 0x00,
		0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x07, 'e', 'x', // Ends before length completes
	}
	_, err = ParsePacket(truncatedQNAME)
	if !errors.Is(err, ErrTruncatedQuestion) {
		t.Errorf("expected ErrTruncatedQuestion, got %v", err)
	}

	// 5c. Truncated QTYPE/QCLASS (QNAME completes, but packet ends before 4-byte QTYPE/QCLASS)
	truncatedTypeClass := []byte{
		0x12, 0x34, 0x01, 0x00,
		0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
		0x03, 'c', 'o', 'm', 0x00,
		0x00, // Only 1 byte of QTYPE provided instead of 4 bytes
	}
	_, err = ParsePacket(truncatedTypeClass)
	if !errors.Is(err, ErrTruncatedQuestion) {
		t.Errorf("expected ErrTruncatedQuestion for missing QTYPE/QCLASS bytes, got %v", err)
	}
}

// 6. Test Malformed Packets
func TestMalformedPackets(t *testing.T) {
	// 6a. Malformed label (label length 70 > max 63)
	malformedLabelLen := []byte{
		0x12, 0x34, 0x01, 0x00,
		0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		70, 'a', 'b', 'c', // 70 is greater than 63
	}
	_, err := ParsePacket(malformedLabelLen)
	if !errors.Is(err, ErrMalformedName) {
		t.Errorf("expected ErrMalformedName for label length > 63, got %v", err)
	}

	// 6b. Pointer out of bounds
	pointerOutOfBounds := []byte{
		0x12, 0x34, 0x01, 0x00,
		0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0xC0, 0x50, // Pointer points to offset 0x50 (80), which is out of bounds
	}
	_, err = ParsePacket(pointerOutOfBounds)
	if !errors.Is(err, ErrMalformedName) {
		t.Errorf("expected ErrMalformedName for pointer out of bounds, got %v", err)
	}
}
