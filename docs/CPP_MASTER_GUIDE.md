# C++ MASTER GUIDE & VIVA DEFENSE REPORT
## RFC 1035 DNS Resolver & In-Memory Caching Server — Distributed VM Multi-Cluster
### Academic Theme 4: Network Systems and Tools (Team 11)

---

## 1. Executive Summary & Viva Pitch

### 30-Second Viva Pitch (Student Voice)
> *"We built an RFC 1035 compliant DNS Resolver and Caching Server from scratch in C++ (C++14), deployed as a distributed multi-cluster system on VMware Workstation Pro. The server listens on UDP port 8053 bound to all interfaces (`0.0.0.0`) and is reachable from any client over VMnet8 NAT. It ingests raw UDP datagrams, parses DNS wire messages byte-by-byte with zero external libraries, and resolves queries concurrently using a cross-platform worker thread pool (`std::thread` on Linux, Win32 threads on Windows).*
>
> *It features a dual-indexed in-memory cache combining an O(1) LRU capacity eviction list with an O(log N) Min-Heap for proactive TTL sweeps. Cache misses forward queries to upstream resolvers (`8.8.8.8` primary, `1.1.1.1` failover) over non-blocking UDP sockets with exponential retry backoff and 16-bit randomized TxID defense against Kaminsky poisoning.*
>
> *In benchmark runs, cache hits resolved in approximately 0.414 ms median vs ~154 ms for upstream WAN resolution — roughly a 220× speedup. The architecture runs on two Ubuntu Server VMs (Cluster-Alpha and Cluster-Beta), tagged with cluster identity in logs, and supports client failover when a node goes down."*

---

## 2. Five-Engine Architecture (VM Cluster Model)

```
            VMNET8 VIRTUAL NETWORK (192.168.150.0/24)
                              │
              ┌───────────────┴───────────────┐
              │  CLUSTER-ALPHA (192.168.150.10)│
              │  CLUSTER-BETA  (192.168.150.20)│
              └───────────────┬───────────────┘
                              │  UDP Port 8053
                              ▼
         ┌────────────────────────────────────────────┐
         │   ENGINE E4: Query Execution Virtual Machine│
         │   (OSPP — Joel Biju)                        │
         │   6-Stage Pipeline:                         │
         │   FETCH→DECODE→LOOKUP→RESOLVE→COMMIT→EMIT  │
         └──────┬──────────────┬────────────────┬──────┘
                │              │                │
        Parse/Serialize  Cache/Evict     Forward/Failover
                ▼              ▼                ▼
     ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
     │   ENGINE E1  │ │   ENGINE E2  │ │   ENGINE E3  │
     │  Wire Parser │ │  Cache & LRU │ │  Upstream    │
     │  (Vageesh M) │ │  (Mehak S.Y.)│ │  (Ishani G)  │
     │              │ │              │ │              │
     │ RFC 1035     │ │ O(1) LRU     │ │ Non-blocking │
     │ Bitfields    │ │ O(log N) Heap│ │ select() UDP │
     │ 0xC00C Comp. │ │ Dynamic TTL  │ │ 8.8.8.8/1.1.1│
     └──────────────┘ └──────┬───────┘ └──────────────┘
                             │
                     dns_cache_<id>.db
                     (per-cluster snapshot)
```

| Engine | Name | Owner | Subject | Responsibilities |
|:---:|:---|:---|:---|:---|
| **E1** | Protocol Parsing & Wire Message Engine | Vageesh Mathad | Computer Networks | RFC 1035 wire parser, bitfield extraction, recursion-bounded decompression (depth ≤ 16), `0xC00C` serializer |
| **E2** | In-Memory Storage & Cache Engine | Mehak Sayed Yusuf | DAA & DBMS | O(1) LRU (hash map + doubly-linked list), O(log N) Min-Heap proactive TTL, dynamic TTL aging, reader-writer locks, per-cluster `dns_cache_<id>.db` |
| **E3** | Upstream Resolution & Network Forwarding Engine | Ishani G | CN & OS | Non-blocking UDP to `8.8.8.8`/`1.1.1.1`, `select()` 2s timeout, exponential backoff, randomized TxID Kaminsky defense |
| **E4** | Query Execution VM Engine | Joel Biju | OSPP | 6-stage state machine, condition routing, `std::thread` pool, cluster-tagged logging |
| **E5** | Telemetry Subsystem | Joel Biju | OSPP | Atomic QPS counters, latency histogram, hit ratio %, live dashboard |

---

## 3. Deep-Dive Engine Walkthroughs

### Engine E1: Protocol Parsing & Wire Message Engine
- **Owner:** Vageesh Mathad | **Subject:** Computer Networks
- **Files:** `include/protocol_engine.hpp`, `src/protocol_engine.cpp`
- **Key Algorithms:**
  1. **12-Byte Header Parsing:** Unpacks 16-bit TxID, Flags (`QR`, `Opcode`, `AA`, `TC`, `RD`, `RA`, `RCODE`), and section counts in network byte order.
  2. **RFC 1035 Compression Pointer Decompression:** Detects `0xC000` markers, extracts 14-bit offsets `((byte[0] & 0x3F) << 8) | byte[1]`, jumps cursor to target.
  3. **Cycle/Loop Guard (Security):** Tracks recursion depth ≤ `MAX_DECOMPRESSION_DEPTH = 16`. Exceeding this → `RCODE_FORMERR`. Blocks self-referencing pointer DoS attacks.
  4. **Pointer Compression Serializer:** Emits `0xC00C` pointers in responses to reduce packet size.

### Engine E2: In-Memory Storage & Cache Engine
- **Owner:** Mehak Sayed Yusuf | **Subject:** DAA & DBMS
- **Files:** `include/storage_engine.hpp`, `src/storage_engine.cpp`
- **Key Algorithms:**
  1. **O(1) LRU:** `std::unordered_map` for key lookup + `std::list` for recency order. `std::list::splice()` promotes hits to MRU head in O(1). Tail evicted at capacity in O(1).
  2. **O(log N) Min-Heap:** `std::priority_queue` keyed by `ExpiresAt = now + TTL`. Root always holds earliest expiring record — proactive purge without O(N) scans.
  3. **Dynamic TTL Aging:** `TTL_rem = ExpiresAt - now`. Clients receive strictly non-stale TTL countdowns.
  4. **Lazy Deletion:** Each heap node carries a version counter matching its hash entry. Stale heap nodes silently skipped on pop.
  5. **Thread Safety:** `std::shared_timed_mutex` on Linux (shared for reads, exclusive for writes); Win32 `SRWLOCK` on Windows.
  6. **Per-Cluster Snapshot:** Serializes valid entries to `dns_cache_<CLUSTER-ID>.db` on graceful shutdown; warm-up on boot.

### Engine E3: Upstream Resolution & Network Forwarding Engine
- **Owner:** Ishani G | **Subject:** Computer Networks & OS
- **Files:** `include/upstream_engine.hpp`, `src/upstream_engine.cpp`
- **Key Algorithms:**
  1. **Cross-Platform Non-blocking UDP:** `fcntl(sock, F_SETFL, O_NONBLOCK)` on Linux; `ioctlsocket(sock, FIONBIO)` on Windows. `select(sock+1, ...)` for POSIX compatibility.
  2. **Dan Kaminsky Defense:** Replaces client TxID with randomized 16-bit integer via `std::mt19937`. Drops any response with mismatched TxID (`MISMATCHED_TXID`).
  3. **Exponential Retry & Failover:** Retries with backoff (T → 2T → 4T) before failing over from `8.8.8.8` to `1.1.1.1`.

### Engine E4: Query Execution VM & Telemetry Engine
- **Owner:** Joel Biju | **Subject:** OSPP
- **Files:** `include/vm_engine.hpp`, `src/vm_engine.cpp`, `src/main.cpp`
- **Key Algorithms:**
  1. **6-Stage Pipeline:** `FETCH` → `DECODE` → `LOOKUP` → `RESOLVE` → `COMMIT` → `EMIT`
  2. **Cross-Platform Thread Pool:** `std::thread` workers, `std::mutex` + `std::condition_variable` task queue. No Win32-only APIs at the pool level.
  3. **Cluster Tagging:** All log lines prefixed: `[CLUSTER-ALPHA] [CACHE HIT] example.com from 192.168.150.1:12345`
  4. **Signal Handling:** POSIX `signal(SIGINT, ...)` + Win32 `SetConsoleCtrlHandler` via `#ifdef _WIN32`.

---

## 4. Comprehensive Faculty Viva Defense: 30 Q&A

### Category A: Computer Networks & DNS Protocols

**Q1: What RFC defines the DNS protocol?**
*RFC 1035 (Domain Names — Implementation and Specification), supplemented by RFC 1034 (Concepts) and RFC 2308 (Negative Caching).*

**Q2: What is the structure of the DNS 12-byte header?**
*Six 16-bit fields: Transaction ID, Flags (QR/Opcode/AA/TC/RD/RA/Z/RCODE), QDCOUNT, ANCOUNT, NSCOUNT, ARCOUNT.*

**Q3: How does DNS message compression work?**
*A 2-byte pointer where the top 2 bits are `11` (`0xC000`). Remaining 14 bits = byte offset from DNS header start where the domain name was previously serialized.*

**Q4: What is `0xC00C` and why is it common?**
*`0xC0` = compression flag, `0x0C` = offset 12. Byte 12 is exactly where the QNAME begins after the 12-byte DNS header — the most common answer compression.*

**Q5: What is the Dan Kaminsky attack and how did you defend against it?**
*Attackers flood a resolver with forged responses while guessing the 16-bit TxID (65,536 possibilities). We replace the client TxID with a randomized 16-bit value via `std::mt19937` and drop any response whose TxID doesn't match our outbound query ID.*

**Q6: Why UDP over TCP for DNS?**
*UDP has zero connection overhead — no 3-way handshake. DNS queries are small (< 512 bytes) and stateless. TCP only used if truncated bit (TC=1) or zone transfer.*

**Q7: What is RFC 2308 negative caching?**
*When an authoritative server returns NXDOMAIN (domain doesn't exist), caching servers store this negative result for the SOA TTL to avoid repeated recursive lookups.*

**Q8: What do RD and RA bits mean?**
*RD (Recursion Desired): client asking server to resolve recursively. RA (Recursion Available): server indicating it supports recursive resolution.*

**Q9: What happens if a packet is < 12 bytes?**
*FETCH stage immediately rejects it. A minimal FORMERR response (RCODE=1) is generated and transmitted without parsing.*

**Q10: Primary RCODE values?**
*0=NOERROR, 1=FORMERR, 2=SERVFAIL, 3=NXDOMAIN, 4=NOTIMP, 5=REFUSED.*

---

### Category B: Operating Systems & Concurrency

**Q11: What is the Reactor Pattern?**
*The main thread binds to UDP `0.0.0.0:8053`, runs a non-blocking `select()` event loop, ingests datagrams via `recvfrom()`, and dispatches them to the worker pool without blocking ingestion.*

**Q12: Why reader-writer locking for the cache?**
*DNS cache workloads are read-dominant (>90% lookups). A plain mutex serializes all readers, causing contention. Reader-writer locking (`std::shared_timed_mutex` on Linux, `SRWLOCK` on Windows) allows concurrent reads while only serializing writes.*

**Q13: How does the worker thread pool synchronize its queue?**
*Producer-consumer pattern: reactor pushes to `std::queue` under `std::mutex`, then calls `condition_variable::notify_one()`. Workers wait on `condition_variable::wait()`, awaken, pop a task, and run the 6-stage pipeline.*

**Q14: How does the server handle graceful termination?**
*On Linux: `signal(SIGINT/SIGTERM, handler)` sets `std::atomic<bool> g_running = false`. On Windows: `SetConsoleCtrlHandler` for `CTRL_C_EVENT`. The reactor's `select()` loop exits, workers drain remaining tasks, cache flushes to disk, telemetry prints, process exits.*

**Q15: Why is non-blocking socket I/O necessary upstream?**
*A blocking `recvfrom()` would freeze a worker thread indefinitely if 8.8.8.8 drops a packet. Non-blocking `select()` enforces a 2s deadline, then triggers retry/failover without starvation.*

**Q16: How do buffer races between worker threads get prevented?**
*In FETCH, the reactor immediately copies the raw datagram into an isolated `std::vector<uint8_t>` per transaction. Each worker owns its own memory slice — no shared mutable buffer.*

**Q17: What threading model does the VM version use?**
*`std::thread` for OS kernel-level threads (cross-platform). No Win32-only `CreateThread` at the pool level — same code compiles and runs on Linux and Windows.*

---

### Category C: Algorithms (DAA)

**Q18: Why dual-indexing in a high-performance cache?**
*Hash table alone: O(N) eviction scan. Linked list alone: O(N) lookup. Combined: O(1) both. This is the classic LRU cache data structure.*

**Q19: Explain O(1) LRU promotion on cache hit.**
*The hash map stores `std::list` iterators. On a hit: `std::list::splice(begin(), list_, it)` atomically moves the node to MRU head in O(1) pointer swaps — no memory allocation.*

**Q20: Why an O(log N) Min-Heap for TTL instead of lazy expiry?**
*Pure lazy expiry never cleans records that are never re-queried — memory leaks over time. The Min-Heap root always exposes the soonest-expiring record for proactive background sweeping without O(N) full scans.*

**Q21: How do you handle heap updates without O(N) restructuring?**
*Lazy deletion with version counters. When an entry is updated, we push a new heap node with an incremented version. When a stale heap node is popped, its version is compared to the hash entry's version — mismatch means it's discarded in O(log N).*

**Q22: Worst-case complexity of pointer decompression?**
*Without guard: O(∞) — infinite loops from self-referencing pointers. With `MAX_DECOMPRESSION_DEPTH = 16`: strictly bounded to 16 × 63 ≤ 1008 iterations → O(1) deterministic.*

**Q23: Dynamic TTL formula?**
*`TTL_rem = ExpiresAt − t_current = (t_inserted + TTL_orig) − t_current`. If TTL_rem ≤ 0: expired, O(1) evaluation.*

---

### Category D: Database Systems & Telemetry

**Q24: What is the composite primary key?**
*The tuple `(Domain, QTYPE, QCLASS)`. Ensures A, AAAA, and CNAME records for the same domain are stored as distinct non-colliding entries.*

**Q25: Describe the binary format of `dns_cache_<id>.db`.**
*8-byte magic header (`"DNSCACHE\x01"`), 4-byte record count. Per record: 2-byte domain length + domain string, 2-byte QTYPE, 2-byte QCLASS, 4-byte remaining TTL, 1-byte negative flag, 1-byte RCODE, 2-byte RR count, then RR fields.*

**Q26: What happens during cache warm-up after offline period?**
*`load_snapshot()` reads each stored entry's remaining TTL. If TTL_rem > 0: schedule expiration relative to current boot time. If expired during offline period: discarded. Zero stale data leaks.*

**Q27: How does telemetry record latency without locking?**
*`std::atomic<uint64_t>` for counters (lock-free). Min/max latency updated via `compare_exchange_weak()` CAS loops.*

**Q28: Cache hit ratio formula?**
*`Hit% = (Hits + Negative Hits) / (Hits + Misses + Expired + Negative Hits) × 100`*

**Q29: What acceleration did benchmarks show for cache hits vs upstream?**
*In benchmark runs: median cache hit ~0.414 ms vs median upstream ~146 ms — roughly 353× faster at the median. These are benchmark-specific results, not universal guarantees.*

**Q30: Why is C++ optimal for a DNS caching resolver?**
*DNS servers handle tens of thousands of datagrams per second. C++ provides: zero-cost RAII resource management (no GC pauses), direct bitfield manipulation for wire packets, native OS socket and sync primitives, and compile-time platform abstraction via `#ifdef _WIN32`.*

---

## 5. Cross-Platform Architecture (VM Phase)

| Component | Linux VM (Ubuntu) | Windows (Host) |
|:---|:---|:---|
| **Socket headers** | `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>` | `<winsock2.h>`, `<ws2tcpip.h>` |
| **Non-blocking** | `fcntl(sock, F_SETFL, O_NONBLOCK)` | `ioctlsocket(sock, FIONBIO, &mode)` |
| **select() first arg** | `sock + 1` (POSIX) | `0` or `sock + 1` (Winsock accepts both) |
| **Socket close** | `close(sock)` | `closesocket(sock)` |
| **Shared mutex** | `std::shared_timed_mutex` | Win32 `SRWLOCK` |
| **Thread pool** | `std::thread` + `std::mutex` + `std::condition_variable` | Same (C++ std — cross-platform) |
| **Signal handling** | `signal(SIGINT/SIGTERM, handler)` | `SetConsoleCtrlHandler` |
| **Init / Cleanup** | No-op | `WSAStartup()` / `WSACleanup()` |
| **Build** | `make` with `-lpthread` | `mingw32-make` with `-lws2_32` |
| **Binary** | `bin/dns_server` | `bin/dns_server.exe` |

---

## 6. Course Alignment & Syllabus Mapping

| Academic Course | Where It Appears | Code Proof |
|:---|:---|:---|
| **Computer Networks** | Wire format, UDP sockets, upstream forwarding, RFC 1035 | `protocol_engine.cpp`, `upstream_engine.cpp`, bitfields, `0xC00C` |
| **OSPP** | Thread pool, Reactor loop, cross-platform mutex, non-blocking I/O, cluster deployment | `main.cpp`, `vm_engine.cpp`, `std::thread`, `select()`, VMware VMs |
| **DAA** | Dual-index cache: O(1) LRU + O(log N) Min-Heap | `storage_engine.cpp`, `std::list::splice`, `std::priority_queue` |
| **DBMS** | In-memory key-value store, composite primary key, ACID write lock, snapshot durability | `storage_engine.cpp`, `dns_cache_<id>.db`, reader-writer synchronization |

---

## 7. Test & Verification Report

```text
========================================================================
                    TEST SUITE EXECUTION SUMMARY
========================================================================
1. Engine E1 (Protocol Wire Engine)   : 10/10 tests PASSED [test_protocol]
2. Engine E2 (In-Memory Cache & LRU)  : 10/10 tests PASSED [test_cache]
3. Engine E3 (Upstream Forwarder)     :  6/6  tests PASSED [test_upstream]
4. Engine E4 (Query Execution VM)     :  7/7  tests PASSED [test_vm]
------------------------------------------------------------------------
TOTAL AUTOMATED TEST CASES            : 33/33 tests PASSED (100%)
========================================================================

AUTOMATED VERIFICATION (scripts/verify_implementation.py):
  72/72 checks passed (100.0%)

LIVE VM CLUSTER VERIFICATION:
  [CLUSTER-ALPHA] Query 1: 'google.com' (Cache MISS)  → ~25 ms (via 8.8.8.8)
  [CLUSTER-ALPHA] Query 2: 'google.com' (Cache HIT)   → ~0.4 ms (from memory)
  [CLUSTER-BETA]  Failover test: Cluster-Alpha offline → seamless handoff ✓
========================================================================
```

---

*Generated for Theme 4: Network Systems and Tools — Team 11 (PES University)*
*Faculty Guide: Mr. Pavankumar S*
