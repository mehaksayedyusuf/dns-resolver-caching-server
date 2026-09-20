package resolver

import (
	"errors"
	"fmt"
	"net"
	"os"
	"time"

	"dns-resolver-caching-server/dns"
	"dns-resolver-caching-server/handler"
)

const DefaultUpstream = "8.8.8.8:53"
const DefaultTimeout = 2 * time.Second

var (
	ErrNilQuery            = errors.New("cannot resolve nil query")
	ErrUpstreamTimeout     = errors.New("upstream DNS query timed out")
	ErrTransactionMismatch = errors.New("transaction ID mismatch in upstream response")
	ErrNonSuccessRCODE     = errors.New("non-zero RCODE in upstream response")
	ErrNoARecord           = errors.New("no usable A record in upstream response")
)

// Result represents the internal outcome of a successful DNS resolution.
type Result struct {
	Domain       string        // Requested domain name
	Type         uint16        // Query type (TypeA = 1)
	Class        uint16        // Query class (ClassIN = 1)
	IP           string        // Resolved IPv4 address
	TTL          uint32        // Time-To-Live in seconds
	UpstreamAddr string        // Upstream server address used for resolution
	Duration     time.Duration // Time taken for resolution
}

// Resolver manages communication with upstream DNS servers.
type Resolver struct {
	UpstreamAddr string
	Timeout      time.Duration
}

// NewResolver creates a Resolver instance with configurable upstream address and timeout.
// If upstream is empty, checks UPSTREAM_DNS environment variable or defaults to 8.8.8.8:53.
func NewResolver(upstream string, timeout time.Duration) *Resolver {
	if upstream == "" {
		upstream = os.Getenv("UPSTREAM_DNS")
	}
	if upstream == "" {
		upstream = DefaultUpstream
	}
	if timeout <= 0 {
		timeout = DefaultTimeout
	}

	return &Resolver{
		UpstreamAddr: upstream,
		Timeout:      timeout,
	}
}

// Resolve transmits a validated query to the configured upstream DNS server via UDP
// and returns the parsed resolution result.
func (r *Resolver) Resolve(q *handler.Query) (*Result, error) {
	if q == nil {
		return nil, ErrNilQuery
	}

	startTime := time.Now()

	// 1. Dial upstream server using network timeout
	conn, err := net.DialTimeout("udp", r.UpstreamAddr, r.Timeout)
	if err != nil {
		return nil, fmt.Errorf("upstream dial error: %w", err)
	}
	defer conn.Close()

	// Set deadline for both write and read operations
	conn.SetDeadline(time.Now().Add(r.Timeout))

	// 2. Transmit raw DNS query payload to upstream
	_, err = conn.Write(q.RawPacket)
	if err != nil {
		return nil, fmt.Errorf("upstream write error: %w", err)
	}

	// 3. Receive upstream response
	buf := make([]byte, 512)
	n, err := conn.Read(buf)
	if err != nil {
		if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
			return nil, ErrUpstreamTimeout
		}
		return nil, fmt.Errorf("upstream read error: %w", err)
	}

	// 4. Parse upstream response packet
	respPkt, err := dns.ParsePacket(buf[:n])
	if err != nil {
		return nil, fmt.Errorf("malformed upstream response: %w", err)
	}

	// 5. Validate Transaction ID matches request
	if respPkt.Header.ID != q.Header.ID {
		return nil, fmt.Errorf("%w: expected 0x%04X, got 0x%04X", ErrTransactionMismatch, q.Header.ID, respPkt.Header.ID)
	}

	// 6. Check DNS RCODE (lower 4 bits of Header.Flags)
	rcode := respPkt.Header.Flags & 0x000F
	if rcode != 0 {
		return nil, fmt.Errorf("%w: RCODE %d", ErrNonSuccessRCODE, rcode)
	}

	// 7. Find matching Type A answer
	var targetAnswer *dns.ResourceRecord
	for i := range respPkt.Answers {
		if respPkt.Answers[i].Type == dns.TypeA && respPkt.Answers[i].IP != "" {
			targetAnswer = &respPkt.Answers[i]
			break
		}
	}

	if targetAnswer == nil {
		return nil, ErrNoARecord
	}

	elapsed := time.Since(startTime)

	return &Result{
		Domain:       q.Domain,
		Type:         q.Type,
		Class:        q.Class,
		IP:           targetAnswer.IP,
		TTL:          targetAnswer.TTL,
		UpstreamAddr: r.UpstreamAddr,
		Duration:     elapsed,
	}, nil
}
