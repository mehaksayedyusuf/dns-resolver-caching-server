package resolver

import (
	"errors"
	"net"
	"testing"
	"time"

	"dns-resolver-caching-server/dns"
	"dns-resolver-caching-server/handler"
)

// Helper function to build a mock DNS response packet
func buildMockResponse(id uint16, rcode uint16, ancount uint16, ip []byte, ttl uint32, rdlength uint16) []byte {
	flags := uint16(0x8180) | (rcode & 0x000F)

	buf := []byte{
		byte(id >> 8), byte(id), // ID
		byte(flags >> 8), byte(flags), // Flags
		0x00, 0x01, // QDCount: 1
		byte(ancount >> 8), byte(ancount), // ANCount
		0x00, 0x00, // NSCount
		0x00, 0x00, // ARCount
	}

	// Question section: example.com (QTYPE=1, QCLASS=1)
	buf = append(buf, 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c', 'o', 'm', 0x00)
	buf = append(buf, 0x00, 0x01, 0x00, 0x01)

	// Answer section if ancount > 0
	if ancount > 0 {
		// Compression pointer to example.com at offset 12 (0xC00C)
		buf = append(buf, 0xC0, 0x0C)
		// TYPE: A (1), CLASS: IN (1)
		buf = append(buf, 0x00, 0x01, 0x00, 0x01)
		// TTL (4 bytes)
		buf = append(buf, byte(ttl>>24), byte(ttl>>16), byte(ttl>>8), byte(ttl))
		// RDLENGTH (2 bytes)
		buf = append(buf, byte(rdlength>>8), byte(rdlength))
		// RDATA
		if len(ip) > 0 {
			buf = append(buf, ip...)
		}
	}

	return buf
}

// 1. Test Valid A Response & TTL Extraction
func TestValidAResponse(t *testing.T) {
	respBytes := buildMockResponse(0x1234, 0, 1, []byte{93, 184, 215, 14}, 300, 4)

	// Start local mock UDP server to respond to resolver
	addr, err := net.ResolveUDPAddr("udp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("failed to resolve local mock addr: %v", err)
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		t.Fatalf("failed to start mock UDP server: %v", err)
	}
	defer conn.Close()

	go func() {
		buf := make([]byte, 512)
		n, clientAddr, err := conn.ReadFromUDP(buf)
		if err == nil && n > 0 {
			conn.WriteToUDP(respBytes, clientAddr)
		}
	}()

	mockQuery := &handler.Query{
		Header:    dns.Header{ID: 0x1234},
		Domain:    "example.com",
		Type:      dns.TypeA,
		Class:     dns.ClassIN,
		RawPacket: respBytes[:29], // Raw query portion
	}

	res := NewResolver(conn.LocalAddr().String(), 1*time.Second)
	result, err := res.Resolve(mockQuery)
	if err != nil {
		t.Fatalf("unexpected error resolving query: %v", err)
	}

	if result.IP != "93.184.215.14" {
		t.Errorf("expected IP 93.184.215.14, got %s", result.IP)
	}
	if result.TTL != 300 {
		t.Errorf("expected TTL 300, got %d", result.TTL)
	}
	if result.Domain != "example.com" {
		t.Errorf("expected domain example.com, got %s", result.Domain)
	}
}

// 2. Test Response with No Answer (ANCount = 0)
func TestNoAnswerResponse(t *testing.T) {
	respBytes := buildMockResponse(0x1234, 0, 0, nil, 0, 0)

	pkt, err := dns.ParsePacket(respBytes)
	if err != nil {
		t.Fatalf("unexpected error parsing packet: %v", err)
	}

	if len(pkt.Answers) != 0 {
		t.Errorf("expected 0 answers, got %d", len(pkt.Answers))
	}
}

// 3. Test Malformed Response
func TestMalformedResponse(t *testing.T) {
	malformed := []byte{0x12, 0x34, 0x81, 0x80} // Shorter than 12-byte header
	_, err := dns.ParsePacket(malformed)
	if !errors.Is(err, dns.ErrTruncatedHeader) {
		t.Errorf("expected ErrTruncatedHeader, got %v", err)
	}
}

// 4. Test Invalid RDATA Length (RDLength != 4 for A record)
func TestInvalidRDLength(t *testing.T) {
	respBytes := buildMockResponse(0x1234, 0, 1, []byte{93, 184, 215}, 300, 3) // 3 bytes instead of 4

	_, err := dns.ParsePacket(respBytes)
	if err == nil {
		t.Fatal("expected error for invalid RDLength 3, got nil")
	}
	if !errors.Is(err, dns.ErrInvalidRDLength) {
		t.Errorf("expected ErrInvalidRDLength, got %v", err)
	}
}

// 5. Test Transaction ID Mismatch
func TestTransactionIDMismatch(t *testing.T) {
	respBytes := buildMockResponse(0x9999, 0, 1, []byte{93, 184, 215, 14}, 300, 4)

	conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("failed to start mock server: %v", err)
	}
	defer conn.Close()

	go func() {
		buf := make([]byte, 512)
		n, clientAddr, err := conn.ReadFromUDP(buf)
		if err == nil && n > 0 {
			conn.WriteToUDP(respBytes, clientAddr)
		}
	}()

	q := &handler.Query{
		Header:    dns.Header{ID: 0x1234}, // Query ID is 0x1234, response ID is 0x9999
		Domain:    "example.com",
		Type:      dns.TypeA,
		Class:     dns.ClassIN,
		RawPacket: buildMockResponse(0x1234, 0, 0, nil, 0, 0)[:29],
	}

	res := NewResolver(conn.LocalAddr().String(), 1*time.Second)
	_, err = res.Resolve(q)
	if err == nil {
		t.Fatal("expected Transaction ID Mismatch error, got nil")
	}
	if !errors.Is(err, ErrTransactionMismatch) {
		t.Errorf("expected ErrTransactionMismatch, got %v", err)
	}
}

// 6. Test Unsuccessful DNS Response (RCODE = 3 NXDomain)
func TestUnsuccessfulDNSResponse(t *testing.T) {
	respBytes := buildMockResponse(0x1234, 3, 0, nil, 0, 0) // RCODE 3 = NXDomain

	conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("failed to start mock server: %v", err)
	}
	defer conn.Close()

	go func() {
		buf := make([]byte, 512)
		n, clientAddr, err := conn.ReadFromUDP(buf)
		if err == nil && n > 0 {
			conn.WriteToUDP(respBytes, clientAddr)
		}
	}()

	q := &handler.Query{
		Header:    dns.Header{ID: 0x1234},
		Domain:    "nonexistent.example.com",
		Type:      dns.TypeA,
		Class:     dns.ClassIN,
		RawPacket: buildMockResponse(0x1234, 0, 0, nil, 0, 0)[:29],
	}

	res := NewResolver(conn.LocalAddr().String(), 1*time.Second)
	_, err = res.Resolve(q)
	if err == nil {
		t.Fatal("expected RCODE error for NXDomain, got nil")
	}
	if !errors.Is(err, ErrNonSuccessRCODE) {
		t.Errorf("expected ErrNonSuccessRCODE, got %v", err)
	}
}

// 7. Test Upstream Timeout Handling
func TestUpstreamTimeout(t *testing.T) {
	// Listen on port but do NOT respond to simulate timeout
	conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("failed to start mock server: %v", err)
	}
	defer conn.Close()

	q := &handler.Query{
		Header:    dns.Header{ID: 0x1234},
		Domain:    "example.com",
		Type:      dns.TypeA,
		Class:     dns.ClassIN,
		RawPacket: buildMockResponse(0x1234, 0, 0, nil, 0, 0)[:29],
	}

	res := NewResolver(conn.LocalAddr().String(), 100*time.Millisecond) // Short timeout
	_, err = res.Resolve(q)
	if err == nil {
		t.Fatal("expected timeout error, got nil")
	}
	if !errors.Is(err, ErrUpstreamTimeout) {
		t.Errorf("expected ErrUpstreamTimeout, got %v", err)
	}
}
