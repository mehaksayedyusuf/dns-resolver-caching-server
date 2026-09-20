package main

import (
	"fmt"
	"log"
	"net"
	"time"

	"dns-resolver-caching-server/cache"
	"dns-resolver-caching-server/handler"
	"dns-resolver-caching-server/resolver"
	"dns-resolver-caching-server/response"
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

	// Initialize DNS Resolution Engine & In-Memory Cache
	dnsResolver := resolver.NewResolver("", 2*time.Second)
	dnsCache := cache.NewCache()

	fmt.Printf("DNS Server & Cache listening on %s (Upstream: %s)...\n", address, dnsResolver.UpstreamAddr)

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

		// Step 2: Check Local In-Memory DNS Cache
		cacheKey := cache.NewKey(query.Domain, query.Type, query.Class)
		cachedEntry, hit, expired := dnsCache.Get(cacheKey)

		var result *resolver.Result

		if hit {
			remTTL := cachedEntry.RemainingTTL()
			fmt.Printf("[CACHE] HIT: %s (Remaining TTL: %ds)\n", query.Domain, remTTL)

			// Reconstruct resolver.Result from cached entry with remaining TTL
			result = &resolver.Result{
				Domain:       cachedEntry.Domain,
				Type:         cachedEntry.Type,
				Class:        cachedEntry.Class,
				IP:           cachedEntry.IP,
				TTL:          remTTL,
				UpstreamAddr: "CACHE",
			}
		} else {
			if expired {
				fmt.Printf("[CACHE] EXPIRED: %s\n", query.Domain)
			} else {
				fmt.Printf("[CACHE] MISS: %s\n", query.Domain)
			}

			// Step 3: Perform Upstream DNS Resolution on Cache Miss / Expired
			res, err := dnsResolver.Resolve(query)
			if err != nil {
				log.Printf("[DNS Resolution Engine] Resolution failed for %s: %v\n", query.Domain, err)
				continue
			}

			result = res
			fmt.Printf("[DNS Resolution Engine] Resolved %s -> %s (TTL: %ds)\n", result.Domain, result.IP, result.TTL)

			// Step 4: Store Resolution Result in Cache with actual upstream TTL & ExpiresAt
			now := time.Now()
			dnsCache.Set(cacheKey, cache.Entry{
				Domain:    result.Domain,
				Type:      result.Type,
				Class:     result.Class,
				IP:        result.IP,
				TTL:       result.TTL,
				CreatedAt: now,
				ExpiresAt: now.Add(time.Duration(result.TTL) * time.Second),
			})
			fmt.Printf("[CACHE] STORED: %s (IP: %s, TTL: %ds)\n", result.Domain, result.IP, result.TTL)
		}

		// Step 5: Generate Binary DNS Response Packet
		respBytes, err := response.BuildSuccessResponse(query, result)
		if err != nil {
			log.Printf("[Response Generation Engine] Failed to build response for %s: %v\n", query.Domain, err)
			continue
		}

		// Step 6: Transmit DNS Response back to original client via UDP
		_, err = conn.WriteToUDP(respBytes, clientAddr)
		if err != nil {
			log.Printf("[Response Generation Engine] Failed to send UDP response to %s: %v\n", clientAddr.String(), err)
			continue
		}

		fmt.Printf("[Response Generation Engine] Transmitted DNS Response (%d bytes) to %s\n", len(respBytes), clientAddr.String())
	}
}
