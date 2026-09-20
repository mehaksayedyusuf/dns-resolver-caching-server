package handler

import (
	"errors"
	"testing"

	"dns-resolver-caching-server/dns"
)

// Helper function to build raw DNS packets for testing
func buildRawQuery(id uint16, domain string, qtype uint16, qclass uint16, qdcount uint16) []byte {
	buf := []byte{
		byte(id >> 8), byte(id), // ID
		0x01, 0x00, // Flags
		byte(qdcount >> 8), byte(qdcount), // QDCount
		0x00, 0x00, // ANCount
		0x00, 0x00, // NSCount
		0x00, 0x00, // ARCount
	}

	if qdcount > 0 && domain != "" {
		// Encode domain labels
		for _, label := range splitDomain(domain) {
			buf = append(buf, byte(len(label)))
			buf = append(buf, []byte(label)...)
		}
		buf = append(buf, 0x00) // End of domain name

		// Append QTYPE and QCLASS
		buf = append(buf, byte(qtype>>8), byte(qtype))
		buf = append(buf, byte(qclass>>8), byte(qclass))
	} else if qdcount > 0 && domain == "" {
		// Zero-length root label "."
		buf = append(buf, 0x00)
		buf = append(buf, byte(qtype>>8), byte(qtype))
		buf = append(buf, byte(qclass>>8), byte(qclass))
	}

	return buf
}

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

// 1. Test Valid A / IN Query
func TestValidAQuery(t *testing.T) {
	raw := buildRawQuery(0x1234, "example.com", dns.TypeA, dns.ClassIN, 1)

	query, err := ProcessPacket(raw)
	if err != nil {
		t.Fatalf("unexpected error processing valid query: %v", err)
	}

	if query.Domain != "example.com" {
		t.Errorf("expected domain example.com, got %s", query.Domain)
	}
	if query.Type != dns.TypeA {
		t.Errorf("expected QTYPE %d, got %d", dns.TypeA, query.Type)
	}
	if query.Class != dns.ClassIN {
		t.Errorf("expected QCLASS %d, got %d", dns.ClassIN, query.Class)
	}
}

// 2. Test Malformed DNS Packet
func TestMalformedDNSPacket(t *testing.T) {
	malformed := []byte{0x01, 0x02, 0x03} // Shorter than 12-byte header

	_, err := ProcessPacket(malformed)
	if err == nil {
		t.Fatal("expected error for malformed packet, got nil")
	}
	if !errors.Is(err, ErrMalformedPacket) {
		t.Errorf("expected ErrMalformedPacket, got %v", err)
	}
}

// 3. Test Unsupported QTYPE
func TestUnsupportedQTYPE(t *testing.T) {
	raw := buildRawQuery(0x1234, "example.com", dns.TypeMX, dns.ClassIN, 1)

	_, err := ProcessPacket(raw)
	if err == nil {
		t.Fatal("expected error for unsupported QTYPE MX, got nil")
	}
	if !errors.Is(err, ErrUnsupportedQType) {
		t.Errorf("expected ErrUnsupportedQType, got %v", err)
	}
}

// 4. Test Unsupported QCLASS
func TestUnsupportedQCLASS(t *testing.T) {
	raw := buildRawQuery(0x1234, "example.com", dns.TypeA, 3, 1) // QCLASS 3 (CHAOS)

	_, err := ProcessPacket(raw)
	if err == nil {
		t.Fatal("expected error for unsupported QCLASS 3, got nil")
	}
	if !errors.Is(err, ErrUnsupportedQClass) {
		t.Errorf("expected ErrUnsupportedQClass, got %v", err)
	}
}

// 5. Test Invalid/Empty Domain
func TestInvalidEmptyDomain(t *testing.T) {
	raw := buildRawQuery(0x1234, "", dns.TypeA, dns.ClassIN, 1)

	_, err := ProcessPacket(raw)
	if err == nil {
		t.Fatal("expected error for empty domain, got nil")
	}
	if !errors.Is(err, ErrInvalidDomain) {
		t.Errorf("expected ErrInvalidDomain, got %v", err)
	}
}

// 6. Test Missing Question (QDCOUNT = 0)
func TestMissingQuestion(t *testing.T) {
	raw := buildRawQuery(0x1234, "example.com", dns.TypeA, dns.ClassIN, 0)

	_, err := ProcessPacket(raw)
	if err == nil {
		t.Fatal("expected error for QDCOUNT=0 missing question, got nil")
	}
	if !errors.Is(err, ErrNoQuestions) {
		t.Errorf("expected ErrNoQuestions, got %v", err)
	}
}
