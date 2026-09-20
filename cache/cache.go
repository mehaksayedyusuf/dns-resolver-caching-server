package cache

import (
	"strings"
	"sync"
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

// Entry contains the resolved DNS data required to build a DNS response.
type Entry struct {
	Domain string
	Type   uint16
	Class  uint16
	IP     string
	TTL    uint32
}

// Cache provides thread-safe in-memory storage for DNS lookup results.
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
// Returns the Entry and true on CACHE HIT, or zero-value Entry and false on CACHE MISS.
func (c *Cache) Get(key Key) (Entry, bool) {
	c.mu.RLock()
	defer c.mu.RUnlock()

	entry, found := c.entries[key]
	if !found {
		return Entry{}, false
	}
	return entry, true
}

// Set stores or updates a DNS entry for the given Key.
func (c *Cache) Set(key Key, entry Entry) {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.entries[key] = entry
}

// Len returns the current total number of cached entries.
func (c *Cache) Len() int {
	c.mu.RLock()
	defer c.mu.RUnlock()

	return len(c.entries)
}
