package main

import (
	"fmt"
	"log"
	"net"

	"dns-resolver-caching-server/handler"
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

	fmt.Printf("DNS Query Handling Engine listening on %s...\n", address)

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

		// Process and validate packet through the Query Handling Engine
		query, err := handler.ProcessPacket(buf[:n])
		if err != nil {
			log.Printf("[Query Handling Engine] Rejected query from %s: %v\n", clientAddr.String(), err)
			continue
		}

		// Log structured, validated query representation ready for resolution/cache
		fmt.Println("[Query Handling Engine] Validated Internal Query:")
		fmt.Printf("  Tx ID   : 0x%04X (%d)\n", query.Header.ID, query.Header.ID)
		fmt.Printf("  Domain  : %s\n", query.Domain)
		fmt.Printf("  QTYPE   : %d (A)\n", query.Type)
		fmt.Printf("  QCLASS  : %d (IN)\n", query.Class)
	}
}
