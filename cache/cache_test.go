package cache

import (
	"sync"
	"testing"
)

// 1. Empty cache returns MISS
func TestEmptyCacheReturnsMiss(t *testing.T) {
	c := NewCache()
	key := NewKey("example.com", 1, 1)

	_, hit := c.Get(key)
	if hit {
		t.Error("expected CACHE MISS on empty cache, got HIT")
	}
}

// 2. Set then Get returns HIT, same domain, IPv4, and TTL
func TestSetAndGet(t *testing.T) {
	c := NewCache()
	key := NewKey("example.com", 1, 1)
	entry := Entry{
		Domain: "example.com",
		Type:   1,
		Class:  1,
		IP:     "93.184.216.34",
		TTL:    3600,
	}

	c.Set(key, entry)

	retrieved, hit := c.Get(key)
	if !hit {
		t.Fatal("expected CACHE HIT after Set, got MISS")
	}

	// 3. Domain match
	if retrieved.Domain != "example.com" {
		t.Errorf("expected domain example.com, got %s", retrieved.Domain)
	}

	// 4. IPv4 match
	if retrieved.IP != "93.184.216.34" {
		t.Errorf("expected IP 93.184.216.34, got %s", retrieved.IP)
	}

	// 5. TTL preserved
	if retrieved.TTL != 3600 {
		t.Errorf("expected TTL 3600, got %d", retrieved.TTL)
	}
}

// 6. Different domains create different entries
func TestDifferentDomains(t *testing.T) {
	c := NewCache()
	key1 := NewKey("example.com", 1, 1)
	key2 := NewKey("google.com", 1, 1)

	c.Set(key1, Entry{Domain: "example.com", IP: "1.1.1.1", TTL: 100})

	_, hit1 := c.Get(key1)
	if !hit1 {
		t.Error("expected HIT for example.com")
	}

	_, hit2 := c.Get(key2)
	if hit2 {
		t.Error("expected MISS for google.com")
	}
}

// 7. Different QTYPE values create different keys
func TestDifferentQType(t *testing.T) {
	c := NewCache()
	keyA := NewKey("example.com", 1, 1)     // Type A
	keyAAAA := NewKey("example.com", 28, 1) // Type AAAA

	c.Set(keyA, Entry{Domain: "example.com", Type: 1, Class: 1, IP: "1.1.1.1", TTL: 100})

	_, hitA := c.Get(keyA)
	if !hitA {
		t.Error("expected HIT for QTYPE 1")
	}

	_, hitAAAA := c.Get(keyAAAA)
	if hitAAAA {
		t.Error("expected MISS for QTYPE 28")
	}
}

// 8. Different QCLASS values create different keys
func TestDifferentQClass(t *testing.T) {
	c := NewCache()
	keyIN := NewKey("example.com", 1, 1) // Class IN
	keyCH := NewKey("example.com", 1, 3) // Class CHAOS

	c.Set(keyIN, Entry{Domain: "example.com", Type: 1, Class: 1, IP: "1.1.1.1", TTL: 100})

	_, hitIN := c.Get(keyIN)
	if !hitIN {
		t.Error("expected HIT for QCLASS IN")
	}

	_, hitCH := c.Get(keyCH)
	if hitCH {
		t.Error("expected MISS for QCLASS CHAOS")
	}
}

// 9. Updating an existing key replaces the stored entry
func TestUpdateExistingKey(t *testing.T) {
	c := NewCache()
	key := NewKey("example.com", 1, 1)

	c.Set(key, Entry{Domain: "example.com", IP: "1.1.1.1", TTL: 100})
	c.Set(key, Entry{Domain: "example.com", IP: "2.2.2.2", TTL: 200})

	retrieved, hit := c.Get(key)
	if !hit {
		t.Fatal("expected HIT after update")
	}
	if retrieved.IP != "2.2.2.2" {
		t.Errorf("expected updated IP 2.2.2.2, got %s", retrieved.IP)
	}
	if retrieved.TTL != 200 {
		t.Errorf("expected updated TTL 200, got %d", retrieved.TTL)
	}
}

// 10. Multiple cache entries can coexist
func TestMultipleEntriesCoexist(t *testing.T) {
	c := NewCache()
	k1 := NewKey("example.com", 1, 1)
	k2 := NewKey("google.com", 1, 1)
	k3 := NewKey("cloudflare.com", 1, 1)

	c.Set(k1, Entry{Domain: "example.com", IP: "1.1.1.1", TTL: 100})
	c.Set(k2, Entry{Domain: "google.com", IP: "8.8.8.8", TTL: 200})
	c.Set(k3, Entry{Domain: "cloudflare.com", IP: "1.0.0.1", TTL: 300})

	if c.Len() != 3 {
		t.Errorf("expected 3 entries in cache, got %d", c.Len())
	}
}

// 11. Concurrent Get/Set safety
func TestConcurrentGetSet(t *testing.T) {
	c := NewCache()
	var wg sync.WaitGroup

	for i := 0; i < 50; i++ {
		wg.Add(2)

		// Writer goroutine
		go func(id int) {
			defer wg.Done()
			key := NewKey("example.com", 1, 1)
			c.Set(key, Entry{Domain: "example.com", IP: "1.1.1.1", TTL: uint32(id)})
		}(i)

		// Reader goroutine
		go func() {
			defer wg.Done()
			key := NewKey("example.com", 1, 1)
			c.Get(key)
		}()
	}

	wg.Wait()
}

// 12. Domain normalization test
func TestDomainNormalization(t *testing.T) {
	c := NewCache()
	keyUpper := NewKey("EXAMPLE.COM.", 1, 1)
	keyLower := NewKey("example.com", 1, 1)

	c.Set(keyUpper, Entry{Domain: "example.com", IP: "93.184.216.34", TTL: 300})

	retrieved, hit := c.Get(keyLower)
	if !hit {
		t.Fatal("expected CACHE HIT for normalized domain, got MISS")
	}
	if retrieved.IP != "93.184.216.34" {
		t.Errorf("expected IP 93.184.216.34, got %s", retrieved.IP)
	}
}
