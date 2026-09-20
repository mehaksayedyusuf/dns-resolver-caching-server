package cache

import (
	"sync"
	"testing"
	"time"
)

// 1. Entry with future expiration -> HIT
func TestFutureExpirationReturnsHit(t *testing.T) {
	c := NewCache()
	key := NewKey("example.com", 1, 1)

	now := time.Now()
	entry := Entry{
		Domain:    "example.com",
		Type:      1,
		Class:     1,
		IP:        "93.184.216.34",
		TTL:       300,
		CreatedAt: now,
		ExpiresAt: now.Add(10 * time.Second), // 10 seconds in future
	}

	c.Set(key, entry)

	retrieved, hit, expired := c.Get(key)
	if !hit || expired {
		t.Fatalf("expected CACHE HIT for future expiration, got hit=%v, expired=%v", hit, expired)
	}

	if retrieved.IP != "93.184.216.34" {
		t.Errorf("expected IP 93.184.216.34, got %s", retrieved.IP)
	}
}

// 2. Entry with past expiration -> MISS and expired == true
func TestPastExpirationReturnsMiss(t *testing.T) {
	c := NewCache()
	key := NewKey("example.com", 1, 1)

	now := time.Now()
	entry := Entry{
		Domain:    "example.com",
		Type:      1,
		Class:     1,
		IP:        "93.184.216.34",
		TTL:       10,
		CreatedAt: now.Add(-20 * time.Second),
		ExpiresAt: now.Add(-10 * time.Second), // 10 seconds in past
	}

	c.Set(key, entry)

	_, hit, expired := c.Get(key)
	if hit {
		t.Error("expected CACHE MISS for expired entry, got HIT")
	}
	if !expired {
		t.Error("expected expired == true for past expiration, got false")
	}

	// Verify entry was lazily deleted from map
	if c.Len() != 0 {
		t.Errorf("expected cache len 0 after lazy delete of expired entry, got %d", c.Len())
	}
}

// 3. TTL = 0 -> immediately expired / MISS
func TestTTLZeroImmediatelyExpired(t *testing.T) {
	c := NewCache()
	key := NewKey("example.com", 1, 1)

	now := time.Now()
	entry := Entry{
		Domain:    "example.com",
		Type:      1,
		Class:     1,
		IP:        "1.1.1.1",
		TTL:       0, // TTL 0
		CreatedAt: now,
		ExpiresAt: now, // Expired immediately
	}

	c.Set(key, entry)

	_, hit, expired := c.Get(key)
	if hit {
		t.Error("expected CACHE MISS for TTL 0, got HIT")
	}
	if !expired {
		t.Error("expected expired == true for TTL 0, got false")
	}
}

// 4. Remaining TTL decreases over time & 7. Valid cached entry returns remaining TTL
func TestRemainingTTLDecreases(t *testing.T) {
	now := time.Now()
	entry := Entry{
		Domain:    "example.com",
		TTL:       100,
		CreatedAt: now.Add(-30 * time.Second), // 30 seconds ago
		ExpiresAt: now.Add(70 * time.Second),  // 70 seconds remaining
	}

	rem := entry.RemainingTTL()
	if rem < 68 || rem > 71 {
		t.Errorf("expected remaining TTL ~70s, got %d", rem)
	}
}

// 5. Expired entry is not returned
func TestExpiredEntryNotReturned(t *testing.T) {
	c := NewCache()
	key := NewKey("expired.com", 1, 1)

	c.Set(key, Entry{
		Domain:    "expired.com",
		Type:      1,
		Class:     1,
		IP:        "1.2.3.4",
		TTL:       1,
		CreatedAt: time.Now().Add(-5 * time.Second),
		ExpiresAt: time.Now().Add(-1 * time.Second),
	})

	entry, hit, _ := c.Get(key)
	if hit {
		t.Errorf("expected expired entry not to be returned, got entry: %+v", entry)
	}
}

// 6. Resolver result's actual TTL is used to calculate expiration
func TestResolverActualTTLUsed(t *testing.T) {
	c := NewCache()
	key := NewKey("upstream.com", 1, 1)

	actualUpstreamTTL := uint32(189)
	before := time.Now()

	c.Set(key, Entry{
		Domain: "upstream.com",
		Type:   1,
		Class:  1,
		IP:     "104.20.23.154",
		TTL:    actualUpstreamTTL,
	})

	after := time.Now()

	retrieved, hit, _ := c.Get(key)
	if !hit {
		t.Fatal("expected CACHE HIT for upstream.com")
	}

	if retrieved.TTL != 189 {
		t.Errorf("expected original TTL 189, got %d", retrieved.TTL)
	}

	// Verify ExpiresAt is between before+189s and after+189s
	expectedMin := before.Add(189 * time.Second)
	expectedMax := after.Add(189 * time.Second)

	if retrieved.ExpiresAt.Before(expectedMin) || retrieved.ExpiresAt.After(expectedMax) {
		t.Errorf("ExpiresAt %v outside expected range [%v, %v]", retrieved.ExpiresAt, expectedMin, expectedMax)
	}
}

// 8. Existing Phase 8 key isolation tests still pass
func TestKeyIsolation(t *testing.T) {
	c := NewCache()
	k1 := NewKey("example.com", 1, 1)
	k2 := NewKey("google.com", 1, 1)
	k3 := NewKey("example.com", 28, 1) // AAAA

	c.Set(k1, Entry{Domain: "example.com", Type: 1, Class: 1, IP: "1.1.1.1", TTL: 300})

	_, hit1, _ := c.Get(k1)
	if !hit1 {
		t.Error("expected HIT for k1")
	}

	_, hit2, _ := c.Get(k2)
	if hit2 {
		t.Error("expected MISS for k2")
	}

	_, hit3, _ := c.Get(k3)
	if hit3 {
		t.Error("expected MISS for k3")
	}
}

// 9. Existing concurrent Get/Set tests still pass
func TestConcurrentGetSet(t *testing.T) {
	c := NewCache()
	var wg sync.WaitGroup

	for i := 0; i < 50; i++ {
		wg.Add(2)

		go func(id int) {
			defer wg.Done()
			key := NewKey("example.com", 1, 1)
			c.Set(key, Entry{Domain: "example.com", Type: 1, Class: 1, IP: "1.1.1.1", TTL: uint32(id + 100)})
		}(i)

		go func() {
			defer wg.Done()
			key := NewKey("example.com", 1, 1)
			c.Get(key)
		}()
	}

	wg.Wait()
}
