package main

import (
	"fmt"
	"log"
	"net"
	"time"

	"dns-resolver-caching-server/handler"
	"dns-resolver-caching-server/resolver"
)

func main() {
	// Define the server address and port (localhost:8053)
	address := "127.0.0.1:8053"

	// Resolve the string address into a UDPAddr struct
	udpAddr, err := net.ResolveUDPAddr("udp", address)
	if err != nil {
		log.Fatalf("Failed to resolve UDP address %s: %v", address, err)
	}

	// Create a UDP socket and bind it to the address
	conn, err := net.ListenUDP("udp", udpAddr)
	if err != nil {
		log.Fatalf("Failed to listen on UDP socket %s: %v", address, err)
	}
	defer conn.Close()

	// Initialize DNS Resolution Engine with 2 second timeout
	dnsResolver := resolver.NewResolver("", 2*time.Second)

	fmt.Printf("DNS Resolver & Engine listening on %s (Upstream: %s)...\n", address, dnsResolver.UpstreamAddr)

	// Buffer to store incoming packet data (512 bytes standard for conventional DNS over UDP)
	buf := make([]byte, 512)

	// Infinite loop to keep the UDP server running continuously
	for {
		// Read incoming UDP packet from the connection socket
		n, clientAddr, err := conn.ReadFromUDP(buf)
		if err != nil {
			log.Printf("Error receiving UDP packet: %v\n", err)
			continue // Handle basic receive errors gracefully without exiting
		}

		fmt.Printf("\n--- Incoming Packet (%d bytes) from %s ---\n", n, clientAddr.String())

		// Step 1: Process and validate packet through Query Handling Engine
		query, err := handler.ProcessPacket(buf[:n])
		if err != nil {
			log.Printf("[Query Handling Engine] Rejected query from %s: %v\n", clientAddr.String(), err)
			continue
		}

		fmt.Printf("[Query Handling Engine] Validated Query: %s (TxID 0x%04X)\n", query.Domain, query.Header.ID)

		// Step 2: Perform Upstream DNS Resolution
		result, err := dnsResolver.Resolve(query)
		if err != nil {
			log.Printf("[DNS Resolution Engine] Resolution failed for %s: %v\n", query.Domain, err)
			continue
		}

		// Step 3: Log Successful Resolution Result
		fmt.Println("[DNS Resolution Engine] Resolution Succeeded:")
		fmt.Printf("  Domain       : %s\n", result.Domain)
		fmt.Printf("  Resolved IP  : %s\n", result.IP)
		fmt.Printf("  TTL          : %d seconds\n", result.TTL)
		fmt.Printf("  Upstream     : %s\n", result.UpstreamAddr)
		fmt.Printf("  Elapsed Time : %v\n", result.Duration)
	}
}
