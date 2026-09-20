package main

import (
	"fmt"
	"log"
	"net"

	"dns-resolver-caching-server/dns"
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

	fmt.Printf("DNS Resolver listening on %s...\n", address)

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

		// Parse raw []byte packet into structured DNS packet
		packet, err := dns.ParsePacket(buf[:n])
		if err != nil {
			log.Printf("Failed to parse DNS packet: %v\n", err)
			continue
		}

		// Display parsed DNS header and question fields
		fmt.Printf("Transaction ID: 0x%04X (%d)\n", packet.Header.ID, packet.Header.ID)
		fmt.Printf("Flags         : 0x%04X\n", packet.Header.Flags)
		fmt.Printf("Questions     : %d\n", len(packet.Questions))

		for i, q := range packet.Questions {
			fmt.Printf("  Question #%d:\n", i+1)
			fmt.Printf("    Domain : %s\n", q.Name)
			fmt.Printf("    QTYPE  : %d\n", q.Type)
			fmt.Printf("    QCLASS : %d\n", q.Class)
		}
	}
}
