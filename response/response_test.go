package response

import (
	"errors"
	"net"
	"testing"

	"dns-resolver-caching-server/dns"
	"dns-resolver-caching-server/handler"
	"dns-resolver-caching-server/resolver"
)

// 1. Test Successful Response Generation with RD=1
func TestBuildSuccessResponse_RDSet(t *testing.T) {
	q := &handler.Query{
		Header: dns.Header{
			ID:    0xABCD,
			Flags: 0x0100, // RD = 1
		},
		Domain: "example.com",
		Type:   dns.TypeA,
		Class:  dns.ClassIN,
	}

	res := &resolver.Result{
		Domain:       "example.com",
		Type:         dns.TypeA,
		Class:        dns.ClassIN,
		IP:           "93.184.216.34",
		TTL:          3600,
		UpstreamAddr: "8.8.8.8:53",
	}

	respBytes, err := BuildSuccessResponse(q, res)
	if err != nil {
		t.Fatalf("unexpected error building response: %v", err)
	}

	// Parse generated response using existing dns parser to verify wire correctness
	pkt, err := dns.ParsePacket(respBytes)
	if err != nil {
		t.Fatalf("failed to parse generated DNS response packet: %v", err)
	}

	// 1. Transaction ID preservation
	if pkt.Header.ID != 0xABCD {
		t.Errorf("expected Transaction ID 0xABCD, got 0x%04X", pkt.Header.ID)
	}

	// 2. QR bit set (Bit 15 = 1)
	if (pkt.Header.Flags & 0x8000) == 0 {
		t.Errorf("expected QR bit (0x8000) to be set, got Flags 0x%04X", pkt.Header.Flags)
	}

	// 3. RD bit preserved (Bit 8 = 1)
	if (pkt.Header.Flags & 0x0100) == 0 {
		t.Errorf("expected RD bit (0x0100) to be set, got Flags 0x%04X", pkt.Header.Flags)
	}

	// 4. RA bit set (Bit 7 = 1)
	if (pkt.Header.Flags & 0x0080) == 0 {
		t.Errorf("expected RA bit (0x0080) to be set, got Flags 0x%04X", pkt.Header.Flags)
	}

	// 5. RCODE is 0 (Bits 0-3 = 0 NoError)
	if (pkt.Header.Flags & 0x000F) != 0 {
		t.Errorf("expected NoError RCODE 0, got %d", pkt.Header.Flags&0x000F)
	}

	// 6. QDCOUNT = 1
	if pkt.Header.QDCount != 1 {
		t.Errorf("expected QDCount 1, got %d", pkt.Header.QDCount)
	}

	// 7. ANCOUNT = 1
	if pkt.Header.ANCount != 1 {
		t.Errorf("expected ANCount 1, got %d", pkt.Header.ANCount)
	}

	// 8. Question domain / type / class correctness
	if len(pkt.Questions) != 1 {
		t.Fatalf("expected 1 question section, got %d", len(pkt.Questions))
	}
	qAns := pkt.Questions[0]
	if qAns.Name != "example.com" {
		t.Errorf("expected question domain example.com, got %s", qAns.Name)
	}
	if qAns.Type != dns.TypeA {
		t.Errorf("expected question QTYPE A (1), got %d", qAns.Type)
	}
	if qAns.Class != dns.ClassIN {
		t.Errorf("expected question QCLASS IN (1), got %d", qAns.Class)
	}

	// 9. Answer TYPE A / CLASS IN / TTL / IP correctness
	if len(pkt.Answers) != 1 {
		t.Fatalf("expected 1 answer section, got %d", len(pkt.Answers))
	}
	ans := pkt.Answers[0]
	if ans.Type != dns.TypeA {
		t.Errorf("expected answer TYPE A (1), got %d", ans.Type)
	}
	if ans.Class != dns.ClassIN {
		t.Errorf("expected answer CLASS IN (1), got %d", ans.Class)
	}
	if ans.TTL != 3600 {
		t.Errorf("expected TTL 3600, got %d", ans.TTL)
	}
	if ans.IP != "93.184.216.34" {
		t.Errorf("expected IPv4 address 93.184.216.34, got %s", ans.IP)
	}

	// 10. IPv4 RDATA byte slice correctness
	if len(ans.RData) != 4 {
		t.Fatalf("expected 4 bytes RDATA, got %d", len(ans.RData))
	}
	expectedIP := net.ParseIP("93.184.216.34").To4()
	for i := 0; i < 4; i++ {
		if ans.RData[i] != expectedIP[i] {
			t.Errorf("RDATA byte %d mismatch: expected %d, got %d", i, expectedIP[i], ans.RData[i])
		}
	}
}

// 2. Test Response Generation preserving RD=0 when client query has RD=0
func TestBuildSuccessResponse_RDCleared(t *testing.T) {
	q := &handler.Query{
		Header: dns.Header{
			ID:    0x1234,
			Flags: 0x0000, // RD = 0
		},
		Domain: "example.com",
		Type:   dns.TypeA,
		Class:  dns.ClassIN,
	}

	res := &resolver.Result{
		Domain:       "example.com",
		Type:         dns.TypeA,
		Class:        dns.ClassIN,
		IP:           "1.1.1.1",
		TTL:          120,
		UpstreamAddr: "8.8.8.8:53",
	}

	respBytes, err := BuildSuccessResponse(q, res)
	if err != nil {
		t.Fatalf("unexpected error building response: %v", err)
	}

	pkt, err := dns.ParsePacket(respBytes)
	if err != nil {
		t.Fatalf("failed to parse response: %v", err)
	}

	// RD bit must be 0
	if (pkt.Header.Flags & 0x0100) != 0 {
		t.Errorf("expected RD bit to be 0, got Flags 0x%04X", pkt.Header.Flags)
	}

	// QR bit must be 1 (0x8000) and RA bit must be 1 (0x0080)
	if (pkt.Header.Flags & 0x8000) == 0 {
		t.Errorf("expected QR bit to be 1, got Flags 0x%04X", pkt.Header.Flags)
	}
	if (pkt.Header.Flags & 0x0080) == 0 {
		t.Errorf("expected RA bit to be 1, got Flags 0x%04X", pkt.Header.Flags)
	}
}

// 3. Test Missing Query Information (Nil Query / Nil Result / Domain Mismatch)
func TestMissingQueryInformation(t *testing.T) {
	q := &handler.Query{Domain: "example.com"}
	r := &resolver.Result{Domain: "example.com", IP: "1.1.1.1"}

	// Nil Query
	_, err := BuildSuccessResponse(nil, r)
	if !errors.Is(err, ErrNilQuery) {
		t.Errorf("expected ErrNilQuery, got %v", err)
	}

	// Nil Result
	_, err = BuildSuccessResponse(q, nil)
	if !errors.Is(err, ErrNilResult) {
		t.Errorf("expected ErrNilResult, got %v", err)
	}

	// Domain Mismatch
	mismatchResult := &resolver.Result{Domain: "other.com", IP: "1.1.1.1"}
	_, err = BuildSuccessResponse(q, mismatchResult)
	if !errors.Is(err, ErrDomainMismatch) {
		t.Errorf("expected ErrDomainMismatch, got %v", err)
	}
}

// 4. Test Invalid IPv4 Address Rejection
func TestInvalidIPv4Address(t *testing.T) {
	q := &handler.Query{Domain: "example.com"}
	invalidResult := &resolver.Result{
		Domain: "example.com",
		IP:     "invalid-ip-string",
	}

	_, err := BuildSuccessResponse(q, invalidResult)
	if !errors.Is(err, ErrInvalidIPv4Address) {
		t.Errorf("expected ErrInvalidIPv4Address, got %v", err)
	}
}
