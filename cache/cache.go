package cache

import (
	"strings"
	"sync"
	"time"
)

// Key uniquely identifies a DNS question section in the cache.
type Key struct {
	Domain string
	Type   uint16
	Class  uint16
}

// NormalizeDomain normalizes domain names to lowercase without trailing dots
// to ensure equivalent domain names produce identical cache keys.
func NormalizeDomain(domain string) string {
	d := strings.ToLower(strings.TrimSpace(domain))
	d = strings.TrimSuffix(d, ".")
	return d
}

// NewKey creates a normalized Key instance for cache indexing.
func NewKey(domain string, qtype uint16, qclass uint16) Key {
	return Key{
		Domain: NormalizeDomain(domain),
		Type:   qtype,
		Class:  qclass,
	}
}

// Entry contains the resolved DNS data and absolute TTL expiration timestamps.
type Entry struct {
	Domain    string
	Type      uint16
	Class     uint16
	IP        string
	TTL       uint32    // Original TTL in seconds returned by upstream resolver
	CreatedAt time.Time // Timestamp when entry was stored
	ExpiresAt time.Time // Absolute expiration timestamp (CreatedAt + TTL)
}

// RemainingTTL calculates the remaining TTL in seconds based on ExpiresAt.
func (e Entry) RemainingTTL() uint32 {
	if e.ExpiresAt.IsZero() {
		return e.TTL
	}
	remaining := time.Until(e.ExpiresAt)
	if remaining <= 0 {
		return 0
	}
	sec := uint32(remaining.Seconds())
	if sec == 0 && remaining > 0 {
		return 1 // Round up sub-second remaining time to 1 second
	}
	return sec
}

// Cache provides thread-safe in-memory storage for DNS lookup results with TTL expiration.
type Cache struct {
	mu      sync.RWMutex
	entries map[Key]Entry
}

// NewCache constructs and initializes a new thread-safe Cache.
func NewCache() *Cache {
	return &Cache{
		entries: make(map[Key]Entry),
	}
}

// Get retrieves a cached DNS entry for the given Key.
// Returns (entry, hit, expired):
// - hit=true, expired=false: CACHE HIT (valid non-expired entry)
// - hit=false, expired=true: CACHE EXPIRED (entry was present but has expired; deleted from map)
// - hit=false, expired=false: CACHE MISS (entry not found)
func (c *Cache) Get(key Key) (Entry, bool, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()

	entry, found := c.entries[key]
	if !found {
		return Entry{}, false, false
	}

	now := time.Now()
	// Check if entry has expired (TTL == 0 or now >= ExpiresAt)
	if !entry.ExpiresAt.IsZero() && (now.After(entry.ExpiresAt) || now.Equal(entry.ExpiresAt)) {
		delete(c.entries, key)
		return Entry{}, false, true
	}

	return entry, true, false
}

// Set stores or updates a DNS entry for the given Key.
// Calculates absolute ExpiresAt = CreatedAt + TTL seconds if not already set.
func (c *Cache) Set(key Key, entry Entry) {
	c.mu.Lock()
	defer c.mu.Unlock()

	now := time.Now()
	if entry.CreatedAt.IsZero() {
		entry.CreatedAt = now
	}

	if entry.ExpiresAt.IsZero() {
		entry.ExpiresAt = entry.CreatedAt.Add(time.Duration(entry.TTL) * time.Second)
	}

	c.entries[key] = entry
}

// Len returns the current total number of cached entries.
func (c *Cache) Len() int {
	c.mu.RLock()
	defer c.mu.RUnlock()

	return len(c.entries)
}
