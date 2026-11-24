# DNS Resolver & Caching Server

A production-grade, RFC 1035 compliant DNS resolver and in-memory caching server written from scratch in C++14, deployed as a **distributed multi-cluster system** on VMware Workstation Pro. Built without any third-party networking or parsing libraries — every byte of the DNS wire format is handled manually.

**Academic context:** Theme 4 — Network Systems & Tools, Team 11, PES University.
**Faculty Guide:** Mr. Pavankumar S

---

## Table of Contents

1. [What This Project Does](#1-what-this-project-does)
2. [System Architecture](#2-system-architecture)
3. [Complete Request Lifecycle](#3-complete-request-lifecycle)
4. [Engine Internals](#4-engine-internals)
   - [E1 — Protocol Parsing & Wire Message Engine](#e1--protocol-parsing--wire-message-engine)
   - [E2 — In-Memory Storage & Cache Engine](#e2--in-memory-storage--cache-engine)
   - [E3 — Upstream Resolution & Forwarding Engine](#e3--upstream-resolution--forwarding-engine)
   - [E4 — Query Execution VM Engine](#e4--query-execution-vm-engine)
   - [E5 — Telemetry Subsystem](#e5--telemetry-subsystem)
5. [Cross-Platform Design](#5-cross-platform-design)
6. [Security Considerations](#6-security-considerations)
7. [Quick Start](#7-quick-start)
8. [Server CLI Reference](#8-server-cli-reference)
9. [Running Tests](#9-running-tests)
10. [Benchmarks](#10-benchmarks)
11. [VMware Deployment Guide](#11-vmware-deployment-guide)
12. [Project Structure](#12-project-structure)
13. [Team](#13-team)

---

## 1. What This Project Does

When you type `google.com` in a browser, your OS sends a DNS query to a resolver, which looks up the IP address. This project **is** that resolver — it implements the full DNS resolution pipeline:

- Accepts raw UDP datagrams from any client on the network.
- Parses the DNS binary wire format (RFC 1035) byte-by-byte — no libraries, no shortcuts.
- Checks an in-memory cache. If the answer is there and not expired, it responds in under 1 ms.
- If not cached, forwards the query to Google's public DNS (`8.8.8.8`) or Cloudflare (`1.1.1.1`) over non-blocking UDP, waits for the response, caches it, and returns it to the client.
- Serializes a fully compliant DNS response with pointer compression back to the original client.

It runs as **two independent server processes** (Cluster-Alpha and Cluster-Beta) on separate Ubuntu Server VMs inside VMware Workstation Pro, reachable from the host machine or any device on the virtual network. If one cluster goes down, the client retries the other — zero changes on the server side.

---

## 2. System Architecture

### Physical topology

```
  ┌─────────────────────────────────────────────────────────────────────────┐
  │  VMware Workstation Pro — Host Laptop (Windows)                         │
  │                                                                         │
  │  ┌──────────────────────────────────────────────────────────────────┐   │
  │  │  VMnet8 — NAT Virtual Switch  (192.168.150.0/24 subnet)          │   │
  │  │                                                                  │   │
  │  │   ┌─────────────────────────┐    ┌─────────────────────────┐    │   │
  │  │   │   CLUSTER-ALPHA         │    │   CLUSTER-BETA           │    │   │
  │  │   │   Ubuntu Server 22.04   │    │   Ubuntu Server 22.04    │    │   │
  │  │   │   IP: 192.168.150.10    │    │   IP: 192.168.150.20     │    │   │
  │  │   │   Port: 8053 (UDP)      │    │   Port: 8053 (UDP)       │    │   │
  │  │   │                         │    │                          │    │   │
  │  │   │  ./bin/dns_server        │    │  ./bin/dns_server         │    │   │
  │  │   │    --cluster-id ALPHA    │    │    --cluster-id BETA      │    │   │
  │  │   │    -b 0.0.0.0            │◄──►│    -b 0.0.0.0             │    │   │
  │  │   │    -t 4                  │    │    -t 4                   │    │   │
  │  │   └─────────────────────────┘    └─────────────────────────┘    │   │
  │  │                                                                  │   │
  │  │   Host OS   ──► dig @192.168.150.10 -p 8053 google.com A        │   │
  │  │   Python    ──► cluster_client.py --mode failover                │   │
  │  │   Any VM    ──► nslookup -port=8053 github.com 192.168.150.10   │   │
  │  └──────────────────────────────────────────────────────────────────┘   │
  │                            │                                             │
  │                   VMnet8 NAT Gateway ──► Internet                        │
  │                            ├──► 8.8.8.8:53  (Google Public DNS)         │
  │                            └──► 1.1.1.1:53  (Cloudflare DNS)            │
  └─────────────────────────────────────────────────────────────────────────┘
```

### Internal server structure (per cluster node)

Each cluster node is a single binary (`bin/dns_server`) that runs the following components concurrently:

```
  ┌────────────────────────────────────────────────────────────┐
  │  bin/dns_server  (single process, one per VM)              │
  │                                                            │
  │  Main Thread — Reactor                                     │
  │  ┌──────────────────────────────────────────────────────┐  │
  │  │  UDP socket bound to 0.0.0.0:8053                    │  │
  │  │  select() loop (non-blocking, 100 ms timeout)        │  │
  │  │  recvfrom() → copy buffer to heap → push to queue    │  │
  │  └──────────────────────────┬───────────────────────────┘  │
  │                             │                              │
  │  Worker Threads (std::thread × N)                          │
  │  ┌──────────────────────────▼───────────────────────────┐  │
  │  │  std::queue<Task>  protected by std::mutex            │  │
  │  │  std::condition_variable — workers sleep until work   │  │
  │  │                                                       │  │
  │  │  Worker 1  ──┐                                        │  │
  │  │  Worker 2  ──┤──► 6-stage VM pipeline per task        │  │
  │  │  Worker 3  ──┤                                        │  │
  │  │  Worker N  ──┘                                        │  │
  │  └──────────────────────────────────────────────────────┘  │
  │                                                            │
  │  Shared State (accessed by all workers)                    │
  │  ┌────────────────────────────────────────────────────┐    │
  │  │  Engine E2: In-Memory Cache                        │    │
  │  │  protected by std::shared_timed_mutex (Linux)      │    │
  │  │  or SRWLOCK (Windows)                              │    │
  │  └────────────────────────────────────────────────────┘    │
  │  ┌────────────────────────────────────────────────────┐    │
  │  │  Engine E5: Telemetry counters (std::atomic)       │    │
  │  └────────────────────────────────────────────────────┘    │
  └────────────────────────────────────────────────────────────┘
```

---

## 3. Complete Request Lifecycle

This traces every step from the moment a UDP packet hits the network adapter to the moment a response is sent back. This is exactly what happens inside the binary.

```
CLIENT                      REACTOR (main thread)              WORKER THREAD
  │                                │                                │
  │── UDP datagram ──────────────► │                                │
  │   (raw bytes, e.g. 29 bytes)   │                                │
  │                                │  select() fires                │
  │                                │  recvfrom() reads bytes        │
  │                                │  memcpy → vector<uint8_t>      │
  │                                │  record client sockaddr_in     │
  │                                │  push Task to queue            │
  │                                │  condition_variable::notify()  │
  │                                │                                │
  │                                │            worker wakes ───────┤
  │                                │                                │
  │                         ───────────────── STAGE 1: FETCH ───────┤
  │                                │            timestamp = now()   │
  │                                │            check len >= 12     │
  │                                │                                │
  │                         ───────────────── STAGE 2: DECODE ──────┤
  │                                │  E1: parse 12-byte header      │
  │                                │    TxID   = bytes[0..1] BE     │
  │                                │    QR     = bit 15 of flags    │
  │                                │    Opcode = bits 14-11         │
  │                                │    RD     = bit 8              │
  │                                │    RCODE  = bits 3-0           │
  │                                │  E1: decode QNAME              │
  │                                │    read label lengths          │
  │                                │    follow 0xC000 pointers      │
  │                                │    depth guard ≤ 16            │
  │                                │  E1: extract QTYPE, QCLASS     │
  │                                │                                │
  │                         ───────────────── STAGE 3: LOOKUP ──────┤
  │                                │  E2: shared_lock(mutex)        │
  │                                │  unordered_map::find(key)      │
  │                                │  key = {domain, qtype, qclass} │
  │                                │                                │
  │                 ┌─────────────── HIT? ────────────────────────┐ │
  │                 │ YES                               NO / EXPIRED│ │
  │                 │                                             │ │
  │        TTL_rem = ExpiresAt - now()           upgrade to       │ │
  │        if TTL_rem <= 0: EXPIRED              unique_lock      │ │
  │        else:                                 pop from heap    │ │
  │          upgrade lock → exclusive            delete from map  │ │
  │          list::splice to MRU head            ↓ STAGE 4        │ │
  │          build HIT response                                   │ │
  │          → STAGE 6 (EMIT)                                     │ │
  │                 └─────────────────────────────────────────────┘ │
  │                                                                  │
  │                         ───────────────── STAGE 4: RESOLVE ─────┤
  │                                │  E3: create UDP socket         │
  │                                │  fcntl(F_SETFL, O_NONBLOCK)    │
  │                                │  replace TxID with random u16  │
  │                                │    mt19937 seeded at boot      │
  │                                │  sendto(8.8.8.8, 53)           │
  │                                │  select(sock+1, timeout=2s)    │
  │                                │                                │
  │                                │  if timeout:                   │
  │                                │    retry × 2 (backoff 1s,2s)  │
  │                                │    failover to 1.1.1.1         │
  │                                │    if still timeout → SERVFAIL │
  │                                │                                │
  │                                │  recvfrom() upstream response  │
  │                                │  verify TxID matches           │
  │                                │  parse RCODE                   │
  │                                │    NXDOMAIN → negative cache   │
  │                                │    NOERROR  → STAGE 5          │
  │                                │                                │
  │                         ───────────────── STAGE 5: COMMIT ──────┤
  │                                │  E2: unique_lock(mutex)        │
  │                                │  insert into unordered_map     │
  │                                │  push_front into std::list     │
  │                                │  push into priority_queue      │
  │                                │    key = {expires_at, version} │
  │                                │  if size > capacity:           │
  │                                │    erase list.back() from map  │
  │                                │    list::pop_back()            │
  │                                │                                │
  │                         ───────────────── STAGE 6: EMIT ────────┤
  │                                │  E1: build response header     │
  │                                │    set QR=1, copy TxID         │
  │                                │    set RA=1, RCODE=0           │
  │                                │  E1: serialize answer RRs      │
  │                                │    write 0xC00C pointer        │
  │                                │    write TYPE, CLASS, TTL      │
  │                                │    write RDLENGTH, RDATA       │
  │                                │  sendto(client_addr)           │
  │◄─── UDP response ──────────────│                                │
  │                                │  E5: record latency            │
  │                                │    elapsed = now() - start     │
  │                                │    atomic_add(total_queries)   │
  │                                │    CAS update min/max latency  │
  │                                │                                │
```

**Condition routing matrix** — every query exits through exactly one path:

| Cache Verdict | Upstream Result | Final Action |
|:---|:---|:---|
| HIT (valid TTL) | — | Build from cache, EMIT |
| EXPIRED | — | Purge entry, go to RESOLVE |
| MISS | NOERROR | COMMIT to cache, EMIT |
| MISS | NXDOMAIN | Negative cache (RFC 2308), EMIT |
| MISS | Timeout after retries | SERVFAIL (RCODE 2), EMIT |
| MISS | TxID mismatch | Drop packet (Kaminsky), SERVFAIL |
| Packet < 12 bytes | — | FORMERR (RCODE 1), EMIT |

---

## 4. Engine Internals

### E1 — Protocol Parsing & Wire Message Engine

**Owner:** Vageesh Mathad | **Subject:** Computer Networks

This engine is responsible for two things: understanding raw bytes that arrive from the network, and producing raw bytes to send back. It never allocates memory on the heap — all parsing is done with pointer arithmetic over the pre-allocated buffer.

#### DNS Wire Format

Every DNS message starts with a fixed 12-byte header:

```
Byte offset:  0    1    2    3    4    5    6    7    8    9   10   11
              ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
              │   TxID (16-bit)   │        Flags (16-bit)      │  QDCOUNT │
              ├────┬────┼────┬────┼────┬────┼────┬────┼────┬────┼────┬────┤
              │  ANCOUNT (16-bit) │  NSCOUNT (16-bit)  │  ARCOUNT (16-bit) │
              └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘

Flags field (16 bits):
  Bit 15:    QR       — 0=Query, 1=Response
  Bits 14-11: Opcode  — 0=Standard query
  Bit 10:    AA       — Authoritative Answer
  Bit 9:     TC       — Truncated
  Bit 8:     RD       — Recursion Desired (client sets this)
  Bit 7:     RA       — Recursion Available (server sets this)
  Bits 6-4:  Z        — Reserved (must be 0)
  Bits 3-0:  RCODE    — Response code (0=NOERROR, 3=NXDOMAIN, etc.)
```

All 16-bit and 32-bit fields are in **network byte order (big-endian)**. Reading them on a little-endian x86 host requires byte-swapping, done via `ntohs()` / `ntohl()` macros.

#### Domain Name Encoding

A domain like `www.example.com` is encoded as a sequence of **length-prefixed labels**:

```
  03 'w' 'w' 'w'   07 'e' 'x' 'a' 'm' 'p' 'l' 'e'   03 'c' 'o' 'm'   00
  ↑                 ↑                                   ↑                ↑
  length=3          length=7                            length=3         null terminator
```

#### Compression Pointer Decompression

To save space in responses, RFC 1035 allows a domain name to be replaced by a 2-byte **compression pointer**. If the top 2 bits of a byte are both `1` (i.e., the byte ≥ `0xC0`), it is a pointer, not a length:

```
  High byte: 11xxxxxx   Low byte: yyyyyyyy
              ↑↑
              Compression flag

  Offset = ((high & 0x3F) << 8) | low
  → jump cursor to that byte offset in the message buffer
```

The most common pointer is `0xC00C` = offset 12, which points to the start of the Question section — where the domain name was first written.

**Security guard:** A malformed packet can craft a self-referencing pointer (`A → A`) or a cycle (`A → B → A`), causing an infinite loop. The parser tracks recursion depth and aborts with `RCODE_FORMERR` if depth exceeds 16.

#### Response Serialization

After resolution, E1 serializes the answer. It writes the domain name once and then uses `0xC00C` pointers in every answer RR to reference it, minimizing packet size. Final packet layout:

```
  [12-byte header] [Question section] [Answer RRs with 0xC00C pointers]
```

---

### E2 — In-Memory Storage & Cache Engine

**Owner:** Mehak Sayed Yusuf | **Subject:** DAA & DBMS

This engine provides O(1) lookup, O(1) eviction, and O(log N) proactive TTL expiration simultaneously using a carefully combined data structure.

#### Composite Primary Key

Every cache entry is indexed by the tuple `(domain, qtype, qclass)`. This ensures an `A` record (QTYPE=1) and an `AAAA` record (QTYPE=28) for the same domain are stored as independent entries.

#### Dual-Index Architecture

The cache combines three data structures:

```
  struct CacheEntry {
      string domain; uint16_t qtype, qclass;
      uint32_t ttl_original;
      time_t   inserted_at;          // for dynamic TTL calculation
      time_t   expires_at;           // for Min-Heap ordering
      uint32_t version;              // for lazy heap deletion
      vector<ResourceRecord> rrs;    // the answer records
      bool is_negative;              // RFC 2308 NXDOMAIN flag
  };

  unordered_map<CompositeKey, list<CacheEntry>::iterator>  index_;
  //   ↑ O(1) lookup by key                   ↑ points directly into list

  list<CacheEntry>  lru_list_;
  //   ↑ doubly-linked, MRU at front, LRU at back

  priority_queue<HeapNode, vector<HeapNode>, greater<HeapNode>>  ttl_heap_;
  //   ↑ Min-Heap, root = earliest expires_at
  //   HeapNode { expires_at, version, key }
```

**Cache hit (O(1)):**
1. `index_.find(key)` → iterator to list node.
2. Check `expires_at - now`. If ≤ 0: expired, treat as miss.
3. `list::splice(lru_list_.begin(), lru_list_, it)` — moves node to front in O(1) pointer manipulation.
4. Return reference to the entry.

**Cache miss eviction (O(1)):**
1. When `index_.size() >= capacity`, take `lru_list_.back()`.
2. `index_.erase(back_key)` — O(1) hash delete.
3. `lru_list_.pop_back()` — O(1) list delete.

**Cache insert (O(log N)):**
1. Create new entry, push to `lru_list_.front()`, insert iterator into `index_`.
2. Push `HeapNode{expires_at, version, key}` onto `ttl_heap_` — O(log N).

**Proactive TTL sweep (O(log N) per entry):**
```
while heap root exists and heap.top().expires_at <= now:
    node = heap.pop()                           // O(log N)
    entry = index_.find(node.key)
    if entry.version != node.version:
        continue                                // stale heap entry, skip (lazy delete)
    index_.erase(node.key)
    lru_list_.erase(entry->second)
```

**Dynamic TTL calculation:**
Clients must receive the remaining, not original, TTL. At response time:
```
  TTL_remaining = entry.expires_at - time(nullptr)
```
This decays in real time — a record cached 50 seconds ago with TTL=300 returns TTL=250 to the next client.

**Thread safety:**
```
  Read path  (LOOKUP):  shared_lock<shared_timed_mutex>  — multiple workers read simultaneously
  Write path (COMMIT):  unique_lock<shared_timed_mutex>  — exclusive, all readers blocked
```
On Windows, `SRWLOCK` with `AcquireSRWLockShared` / `AcquireSRWLockExclusive` provides identical semantics.

**Snapshot persistence:**
On graceful shutdown (SIGINT / Ctrl+C), all entries with `TTL_rem > 0` are serialized to `dns_cache_<CLUSTER-ID>.db`:
```
  Magic: "DNSCACHE\x01"  (8 bytes)
  Count: N               (4 bytes)
  Per entry:
    domain_len (2 bytes) + domain string
    qtype  (2 bytes), qclass (2 bytes)
    ttl_rem (4 bytes, computed at flush time)
    is_negative (1 byte), rcode (1 byte)
    rr_count (2 bytes) + serialized RRs
```
On next boot, `load_snapshot()` reads this file and discards any entry whose stored `ttl_rem` is already expired — zero stale data can enter the cache from disk.

---

### E3 — Upstream Resolution & Forwarding Engine

**Owner:** Ishani G | **Subject:** Computer Networks & OS

When the cache has no answer, E3 is responsible for asking the global DNS infrastructure.

#### Non-Blocking UDP Socket

Using a blocking socket would freeze the entire worker thread for up to several seconds if the upstream is slow. Instead:

```cpp
// Linux (POSIX)
int sock = socket(AF_INET, SOCK_DGRAM, 0);
int flags = fcntl(sock, F_GETFL, 0);
fcntl(sock, F_SETFL, flags | O_NONBLOCK);

// Windows
u_long mode = 1;
ioctlsocket(sock, FIONBIO, &mode);
```

After `sendto()`, the engine polls with `select()`:
```cpp
fd_set read_fds;
FD_ZERO(&read_fds);
FD_SET(sock, &read_fds);
struct timeval timeout = {2, 0};                 // 2 second deadline

int ready = select(sock + 1, &read_fds, NULL, NULL, &timeout);
// sock + 1 is required by POSIX; Winsock ignores the first argument
```

#### Retry and Failover

```
  Attempt 1: sendto 8.8.8.8, select(timeout=2s)
     → Timeout: Attempt 2: sendto 8.8.8.8, select(timeout=4s)  [2× backoff]
        → Timeout: Attempt 3: sendto 1.1.1.1, select(timeout=4s)  [failover]
           → Timeout: return SERVFAIL to client
           → Response: validate TxID, parse, return to pipeline
```

#### Kaminsky Cache Poisoning Defense

The Dan Kaminsky attack (2008) works by flooding a resolver with forged responses while it has an outstanding query, hoping to guess the 16-bit Transaction ID:

- There are only 65,536 possible TxID values.
- If the attacker can send ~65,536 forged responses per second, they can poison the cache with a fake IP address.

**Defense:** Before forwarding the client's query to `8.8.8.8`, E3 replaces the client's TxID with a fresh 16-bit random integer:
```cpp
std::mt19937 rng(std::random_device{}());
std::uniform_int_distribution<uint16_t> dist(0, 65535);
uint16_t outbound_txid = dist(rng);
```
When the response arrives, E3 verifies `response_txid == outbound_txid`. Any mismatch is silently dropped. The client's TxID is restored in the response before sending back.

> Note: `std::mt19937` is a pseudorandom generator, not cryptographically secure. The defense significantly raises the cost of an attack but is not equivalent to a cryptographic source. A production system would use `RAND_bytes` from OpenSSL or the OS CSPRNG.

---

### E4 — Query Execution VM Engine

**Owner:** Joel Biju | **Subject:** Operating Systems Principles & Programming

E4 is the state machine that orchestrates the other engines. It models each DNS query as a **virtual machine instruction stream** that executes through 6 deterministic stages.

#### The 6-Stage Pipeline

```
  FETCH  → DECODE → LOOKUP → RESOLVE → COMMIT → EMIT
    E4       E1       E2       E3        E2       E1
```

Each stage is a function call. The pipeline is linear — no stage is skipped (though RESOLVE and COMMIT are bypassed on cache hits). This models how real CPUs execute instructions: each stage has a defined input, defined output, and defined error conditions.

#### Worker Thread Pool

The main reactor thread cannot do the parsing and resolution itself — that would block it from accepting new UDP packets. Instead it uses the producer-consumer pattern:

```cpp
// Reactor thread (producer)
while (g_running) {
    if (select(...) > 0) {
        Task t;
        t.buffer.resize(nbytes);
        memcpy(t.buffer.data(), raw_buf, nbytes);
        t.client_addr = sender;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(std::move(t));
        }
        queue_cv_.notify_one();
    }
}

// Worker threads (consumers)
void worker_loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] {
            return !task_queue_.empty() || shutdown_;
        });
        if (shutdown_ && task_queue_.empty()) return;
        Task t = std::move(task_queue_.front());
        task_queue_.pop();
        lock.unlock();
        execute_pipeline(t);    // runs all 6 stages
    }
}
```

#### Cluster Identity & Logging

Every log line is prefixed with the cluster ID, making it easy to distinguish traffic across VMs:
```
[CLUSTER-ALPHA] [CACHE HIT]  google.com    A  from 192.168.150.1:51234   0.41 ms  TTL_rem=298s
[CLUSTER-ALPHA] [CACHE MISS] github.com    A  from 192.168.150.1:51235  24.72 ms  via 8.8.8.8
[CLUSTER-BETA]  [CACHE HIT]  example.com   A  from 192.168.150.1:51236   0.38 ms  TTL_rem=142s
```

#### Graceful Shutdown

```cpp
// POSIX
signal(SIGINT,  signal_handler);
signal(SIGTERM, signal_handler);

// Windows
SetConsoleCtrlHandler(ctrl_handler, TRUE);   // catches Ctrl+C

void signal_handler(int) {
    g_running.store(false, std::memory_order_relaxed);
}
```
When `g_running` goes false, the reactor exits its `select()` loop, the pool's condition variable is notified with `shutdown_ = true`, workers drain remaining tasks and exit, the cache is flushed to disk, and the telemetry dashboard is printed.

---

### E5 — Telemetry Subsystem

**Owner:** Joel Biju | **Subject:** OSPP

Telemetry counters are accessed from multiple worker threads simultaneously. Using a mutex around every counter would create a bottleneck on every query. Instead, all counters use `std::atomic`:

```cpp
std::atomic<uint64_t> total_queries{0};
std::atomic<uint64_t> cache_hits{0};
std::atomic<uint64_t> cache_misses{0};
std::atomic<uint64_t> upstream_success{0};
std::atomic<uint64_t> upstream_timeout{0};
std::atomic<uint64_t> min_latency_us{UINT64_MAX};
std::atomic<uint64_t> max_latency_us{0};
```

Min/max latency is updated with a **compare-and-swap loop** (lock-free):
```cpp
uint64_t current = min_latency_us.load();
while (latency < current &&
       !min_latency_us.compare_exchange_weak(current, latency)) {
    // current was reloaded by CAS; retry
}
```

Hit ratio printed on shutdown:
```
  Hit Ratio     : 94.3%  (943 hits / 57 misses out of 1000 total)
  Mean Latency  : 3.21 ms
  Min Latency   : 0.14 ms
  Max Latency   : 187.4 ms
  QPS (observed): 312 queries/sec
```

---

## 5. Cross-Platform Design

The same codebase compiles and runs correctly on both Linux (inside VMware VMs) and Windows (host). The abstraction layer lives entirely in `include/common.hpp`:

```cpp
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    // SOCKET is already defined as UINT_PTR
    // closesocket() already exists
    // WSAGetLastError() already exists
    using SharedMutex = /* SRWLOCK wrapper */;
    using Mutex       = /* CRITICAL_SECTION wrapper */;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    typedef int SOCKET;
    #define INVALID_SOCKET  (-1)
    #define closesocket(s)  close(s)
    #define WSAGetLastError() errno
    using SharedMutex = std::shared_timed_mutex;
    using Mutex       = std::mutex;
#endif
```

The Makefile detects the OS and links the correct libraries:
```makefile
ifeq ($(OS),Windows_NT)
    LDFLAGS = -lws2_32
    EXT     = .exe
else
    LDFLAGS = -lpthread
    EXT     =
endif
```

There is no Windows-specific API anywhere in the engine logic — only in `common.hpp` and the `#ifdef` guards in `main.cpp` for WSAStartup/WSACleanup and signal handling.

---

## 6. Security Considerations

| Threat | Defense |
|:-------|:--------|
| **Kaminsky cache poisoning** | Outbound TxID randomized via `std::mt19937`; mismatched responses silently dropped |
| **Pointer loop DoS** | Compression pointer recursion depth capped at 16; exceeding → FORMERR, packet dropped |
| **Buffer overflow on short packet** | FETCH stage validates `len >= 12` before any field is read |
| **Stale cache data after restart** | Snapshot loader discards any entry with `TTL_rem <= 0` at load time |
| **Worker thread starvation** | Non-blocking upstream sockets with 2 s `select()` deadline; threads never block indefinitely |
| **Cache memory exhaustion** | LRU eviction enforces hard capacity limit; oldest unused entry evicted in O(1) |

---

## 7. Quick Start

### Prerequisites (each Ubuntu Server VM)
```
sudo apt update && sudo apt install -y build-essential git python3
```

### Step 1 — Clone

```
git clone https://github.com/mehaksayedyusuf/dns-resolver-caching-server.git
cd dns-resolver-caching-server
```

### Step 2 — Build

```
make clean && make all
```

Expected output:
```
g++ -std=c++14 -O2 -Wall -Wextra -pthread -Iinclude -c src/protocol_engine.cpp -o obj/protocol_engine.o
g++ -std=c++14 -O2 -Wall -Wextra -pthread -Iinclude -c src/storage_engine.cpp   -o obj/storage_engine.o
g++ -std=c++14 -O2 -Wall -Wextra -pthread -Iinclude -c src/upstream_engine.cpp  -o obj/upstream_engine.o
g++ -std=c++14 -O2 -Wall -Wextra -pthread -Iinclude -c src/vm_engine.cpp        -o obj/vm_engine.o
g++ -std=c++14 -O2 -Wall -Wextra -pthread -Iinclude -c src/telemetry_engine.cpp -o obj/telemetry_engine.o
g++ -std=c++14 -O2 -Wall -Wextra -pthread -Iinclude -c src/main.cpp             -o obj/main.o
g++ obj/*.o -o bin/dns_server -lpthread
Build complete: bin/dns_server
```

### Step 3 — Start Cluster-Alpha (VM 1)

```
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-ALPHA -c 1000 -t 4
```

Expected output:
```
===========================================================================
        RFC 1035 DNS RESOLVER & IN-MEMORY CACHING SERVER (C++)
                  Theme 4: Network Systems & Tools
===========================================================================
  Cluster Node   : CLUSTER-ALPHA
  Listening On   : 0.0.0.0:8053  (all interfaces)
  Upstream DNS   : 8.8.8.8:53   (primary)
  Failover DNS   : 1.1.1.1:53   (secondary)
  Cache Capacity : 1000 records (LRU + Min-Heap)
  Worker Threads : 4
===========================================================================
  Server ready. Waiting for queries...
```

### Step 4 — Start Cluster-Beta (VM 2)

```
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-BETA -c 1000 -t 4
```

### Step 5 — First query: cache miss

From host machine (Windows PowerShell or Linux terminal):
```
dig @192.168.150.10 -p 8053 google.com A
```

Expected `dig` output:
```
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 7421
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

;; ANSWER SECTION:
google.com.   299  IN  A  142.250.195.46

;; Query time: 24 msec
;; SERVER: 192.168.150.10#8053(192.168.150.10)
```

VM 1 console:
```
[CLUSTER-ALPHA] [CACHE MISS]  google.com A  from 192.168.150.1:52341  resolved in 24.1 ms via 8.8.8.8
```

### Step 6 — Second query: cache hit

```
dig @192.168.150.10 -p 8053 google.com A
```

Expected:
```
;; Query time: 0 msec
```

VM 1 console:
```
[CLUSTER-ALPHA] [CACHE HIT]   google.com A  from 192.168.150.1:52342  resolved in 0.4 ms  TTL_rem=298s
```

### Step 7 — Windows nslookup

```
nslookup -port=8053 github.com 192.168.150.10
```

Expected:
```
Server:   192.168.150.10
Address:  192.168.150.10#8053

Non-authoritative answer:
Name:    github.com
Address: 140.82.112.3
```

### Step 8 — Multi-cluster automated test

```
python scripts/cluster_client.py \
  --primary   192.168.150.10:8053 \
  --secondary 192.168.150.20:8053 \
  --mode all
```

Expected output:
```
[HIT-MISS TEST]
  google.com   CLUSTER-ALPHA  MISS   23.4 ms
  google.com   CLUSTER-ALPHA  HIT     0.4 ms
  Speedup on second query: 58.5x

[LOAD BALANCE TEST]
  Distributing 10 queries across both clusters...
  CLUSTER-ALPHA: 5 queries  avg 0.5 ms
  CLUSTER-BETA:  5 queries  avg 0.6 ms

[FAILOVER TEST]
  Sending to CLUSTER-ALPHA... timeout simulated
  Retrying on  CLUSTER-BETA... SUCCESS  18.7 ms
  Zero-downtime failover: PASS

All tests passed.
```

### Step 9 — Graceful shutdown

Press `Ctrl+C` on either VM:
```
^C
[CLUSTER-ALPHA] Shutdown signal received. Draining worker threads...
[CLUSTER-ALPHA] Flushing 47 valid cache entries to dns_cache_CLUSTER-ALPHA.db
[CLUSTER-ALPHA] Telemetry Summary:
  Total Queries : 1000
  Cache Hits    : 943  (94.3%)
  Cache Misses  : 57   (5.7%)
  Mean Latency  : 3.21 ms
  Min Latency   : 0.14 ms
  Max Latency   : 187.4 ms
[CLUSTER-ALPHA] Server shut down cleanly.
```

---

## 8. Server CLI Reference

| Flag | Default | Description |
|:-----|:--------|:------------|
| `-b <ip>` / `--bind <ip>` | `0.0.0.0` | Interface to listen on. Use `0.0.0.0` to accept from all VM network interfaces |
| `-p <port>` | `8053` | UDP port number |
| `--cluster-id <name>` | `CLUSTER-ALPHA` | Node identity tag. Affects log prefixes and snapshot filename |
| `-c <n>` | `1000` | Maximum LRU cache record count. When full, LRU tail is evicted |
| `-t <n>` | `4` | Number of worker threads in the pool |
| `--peer <ip:port>` | _(none)_ | Peer cluster node for inter-cluster query forwarding |
| `--db <file>` | `dns_cache_<id>.db` | Override the cache snapshot file path |

---

## 9. Running Tests

Each engine has its own isolated test suite. Tests are compiled and run with Make:

```
make test_protocol    # E1 — 10 unit tests: header parsing, compression, loop guard, serializer
make test_cache       # E2 — 10 unit tests: LRU eviction, TTL expiry, heap sweep, thread safety
make test_upstream    # E3 — 6 unit tests:  socket setup, TxID replacement, timeout, failover
make test_vm          # E4 — 7 integration tests: full pipeline with mock upstream
```

Expected output per suite:
```
[PASS] Header parse: TxID extraction
[PASS] Header parse: QR/RD/RA flag bits
[PASS] Label parse: simple 3-label domain
[PASS] Label parse: compressed pointer 0xC00C
[PASS] Pointer loop guard: depth > 16 returns FORMERR
[PASS] Response serializer: answer section with 0xC00C
[PASS] Negative packet: len < 12 returns FORMERR
[PASS] Wire round-trip: parse then serialize produces identical header
[PASS] QTYPE extraction: A=1, AAAA=28, CNAME=5
[PASS] QCLASS extraction: IN=1
E1: 10/10 PASSED
```

Run all suites at once:
```
make test_all
```

---

## 10. Benchmarks

Stress test run: 20 concurrent client threads, 1,000 total queries against a pre-warmed Cluster-Alpha. Results are from that specific benchmark run and are not universal performance guarantees.

| Metric | Cache Hit (RAM) | Upstream Miss (WAN) | Ratio |
|:-------|:---------------:|:-------------------:|:-----:|
| Minimum latency | 0.143 ms | 103.6 ms | ~724x |
| Median latency (p50) | 0.414 ms | 146.4 ms | ~354x |
| Mean latency | 0.700 ms | 154.6 ms | ~221x |
| p95 latency | 2.38 ms | 210.0 ms | ~88x |
| Peak throughput | **6,538 QPS** | — | — |
| Error rate | 0% | 0% | — |

Reproduce:
```
python scripts/benchmark_stress.py \
  --host 192.168.150.10 --port 8053 \
  -n 1000 -w 20 --mode hit
```

Detailed report with scenario breakdowns: [`benchmarks/BENCHMARK_REPORT.md`](benchmarks/BENCHMARK_REPORT.md)

---

## 11. VMware Deployment Guide

Full step-by-step guide (VMnet8 config, VM creation, static IPs, SCP file transfer, startup, testing):

**[VM_CLUSTER_IMPLEMENTATION.md](VM_CLUSTER_IMPLEMENTATION.md)**

One-command VM deployer (run this inside each Ubuntu VM after cloning):
```
chmod +x scripts/deploy_vm_cluster.sh
./scripts/deploy_vm_cluster.sh CLUSTER-ALPHA 8053 4
```

This script installs `build-essential` if missing, runs `make clean && make all`, detects the VM's IP via `hostname -I`, and launches the server.

---

## 12. Project Structure

```
dns-resolver-caching-server/
│
├── Makefile                            Cross-platform build system
│                                       Linux: g++ ... -lpthread
│                                       Windows: g++ ... -lws2_32
│
├── README.md                           This file
├── VM_CLUSTER_IMPLEMENTATION.md        Full VMware deployment guide
├── CONTEXT.md                          Project state and decisions log
├── VERIFICATION_REPORT.txt             Output of verify_implementation.py (72/72 passed)
│
├── include/
│   ├── common.hpp                      Platform abstraction layer
│   │                                   #ifdef _WIN32 guards for sockets, mutexes, signals
│   ├── protocol_engine.hpp             E1 interface: parse_query(), build_response()
│   ├── storage_engine.hpp              E2 interface: lookup(), insert(), sweep_expired()
│   ├── upstream_engine.hpp             E3 interface: resolve_upstream()
│   ├── vm_engine.hpp                   E4 interface: execute_pipeline()
│   └── telemetry_engine.hpp            E5 interface: record_query(), print_dashboard()
│
├── src/
│   ├── main.cpp                        Entry point: CLI parsing, reactor loop, worker pool
│   ├── protocol_engine.cpp             RFC 1035 wire parse and serialize
│   ├── storage_engine.cpp              LRU + Min-Heap cache implementation
│   ├── upstream_engine.cpp             Non-blocking UDP upstream forwarder
│   ├── vm_engine.cpp                   6-stage pipeline executor
│   └── telemetry_engine.cpp            Atomic counters and dashboard
│
├── tests/
│   ├── test_protocol.cpp               10 unit tests for E1
│   ├── test_cache.cpp                  10 unit tests for E2
│   ├── test_upstream.cpp                6 unit tests for E3
│   └── test_vm_integration.cpp          7 integration tests for E4
│
├── scripts/
│   ├── cluster_client.py               Multi-cluster test client
│   │                                   Modes: hit-miss, loadbalance, failover, all
│   ├── deploy_vm_cluster.sh            One-click bash deployer for Linux VMs
│   ├── benchmark_stress.py             Concurrent UDP stress benchmark tool
│   ├── live_client.py                  Interactive single-query client for manual testing
│   └── verify_implementation.py        Static verification suite (72 checks)
│
├── benchmarks/
│   ├── BENCHMARK_REPORT.md             Detailed analysis with percentile tables
│   ├── benchmark_hit.json              Raw results — pure cache hit run
│   ├── benchmark_hit_1000.json         Raw results — 1,000 queries, 20 threads
│   └── benchmark_mixed.json            Raw results — 80/20 mixed workload
│
├── diagrams/                           Architecture diagrams (PNG, 300 DPI)
│   ├── system_orbit_diagram.png
│   ├── engine_pipeline_diagram.png
│   ├── cache_lru_minheap_diagram.png
│   ├── wire_protocol_format_diagram.png
│   ├── concurrency_threadpool_diagram.png
│   ├── dns_resolution_flowchart.png
│   ├── vm_pipeline_branching_diagram.png
│   ├── engine_e3_upstream_diagram.png
│   ├── engine_e5_telemetry_diagram.png
│   └── cache_storage_er_diagram.png
│
├── docs/
│   ├── CPP_MASTER_GUIDE.md             30 viva Q&A, engine deep-dives, course alignment
│   └── CPP_MASTER_PLAN.md              Architectural specification and phase breakdown
│
└── DOCUMENTATION/                      Official academic submissions (do not modify)
    ├── Gate_0_team11_document.docx
    ├── Gate_0_team11_ppt.pptx
    ├── Team11_Gate1.pptx
    ├── Team11_S1_Student_Handbook.docx
    ├── Team11_Sprint1.docx
    ├── Team11_Chapter4_Sprint1_Handbook_Final.pdf
    └── Team11_Chapter4_Sprint1_Handbook_Submission.docx
```

---

## 13. Team

**Team 11 — PES University**
Theme 4: Network Systems and Tools (Academic 3-Credit Mini Project)
Faculty Guide: Mr. Pavankumar S

| Name | Roll Number | Engine | Academic Subject |
|:-----|:------------|:-------|:-----------------|
| Vageesh Mathad | 01fe24bci008 | E1: Protocol Parsing & Wire Message Engine | Computer Networks |
| Mehak Sayed Yusuf | 01fe24bci012 | E2: In-Memory Storage & Cache Engine | DAA & DBMS |
| Ishani G | 01fe22bci009 | E3: Upstream Resolution & Forwarding Engine | Computer Networks & OS |
| Joel Biju | 01fe24bci021 | E4: Query Execution VM & E5: Telemetry | OSPP |
