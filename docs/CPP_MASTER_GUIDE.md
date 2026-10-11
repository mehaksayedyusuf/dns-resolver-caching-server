# C++ MASTER GUIDE & VIVA DEFENSE REPORT
## RFC 1035 DNS Resolver & In-Memory Caching Server
### Academic Theme 4: Network Systems and Tools (Team 11)

---

## 1. Executive Summary & Viva Pitch

### 30-Second Viva Pitch (Student Voice)
> *"We built an RFC 1035 compliant DNS Resolver and Caching Server from scratch in C++ (C++14) using Windows Sockets 2 (`ws2_32`) and native Windows synchronization primitives (`SRWLOCK`). The server listens on UDP port 8053, ingests raw network datagrams, parses DNS wire messages byte-by-byte with zero external libraries, and resolves domain queries concurrently using a worker thread pool.
>
> It features a dual-indexed in-memory cache combining an $O(1)$ LRU capacity eviction doubly-linked list with an $O(\log N)$ Min-Heap for proactive TTL sweeps. When records are not cached, it forwards queries upstream to recursive resolvers (8.8.8.8 / 1.1.1.1) with non-blocking timeouts, exponential retry backoff, and 16-bit cryptographic TxID randomization to defeat Dan Kaminsky cache poisoning attacks.
>
> On cache hits, it dynamically recalculates decaying remaining TTLs ($TTL_{rem} = TTL_{orig} - \Delta t$), serializes RFC-compliant response packets using `0xC00C` pointer compression, and logs microsecond-precision telemetry. In live benchmarks, in-memory cache hits resolve in ~0.85 ms compared to ~121 ms upstream resolution—a 140x acceleration."*

---

## 2. Four Core Engines Architecture (The Orbit Model)

The system decomposes into an **Orbit Model** where **Engine E4 (Query Execution Virtual Machine)** serves as the central state machine coordinating three satellite engines:

```
                      +-----------------------------+
                      |   Client (dig / nslookup)   |
                      +-----------------------------+
                                     |
                             Raw UDP Datagram (Port 8053)
                                     v
                  +-----------------------------------+
                  |  Reactor Network Listener & Loop  |
                  +-----------------------------------+
                                     |
                       Pushes Task to Thread-Safe Queue
                                     v
            +-----------------------------------------------+
            |  ENGINE E4: Query Execution Virtual Machine   |
            |     (OSPP Module - Owner: Joel Biju)          |
            |                                               |
            |  6-Stage Instruction Pipeline:                |
            |  FETCH -> DECODE -> LOOKUP -> RESOLVE ->      |
            |                 COMMIT -> EMIT                |
            +-----------------------------------------------+
               /                     |                     \
              /                      |                      \
  1. Parse / Serialize       2. Cache / Evict         3. Forward / Failover
             v                       v                       v
+-------------------------+ +-------------------------+ +-------------------------+
|      ENGINE E1:         | |      ENGINE E2:         | |      ENGINE E3:         |
| Protocol Parsing Engine | |  Storage & Cache Engine | | Upstream Network Engine |
|   (Computer Networks)   | |      (DAA & DBMS)       | |        (CN & OS)        |
|  Owner: Vageesh Mathad  | | Owner: Mehak Sayed Yusuf| |     Owner: Ishani G     |
|                         | |                         | |                         |
| • RFC 1035 Wire Parser  | | • O(1) LRU Hash + List  | | • Non-blocking UDP      |
| • 14-bit Pointer Decom. | | • O(log N) Min-Heap TTL | | • select() 2.0s Timeout |
| • Loop Guard (depth<=16)| | • Dynamic Decaying TTL  | | • Exponential Backoff   |
| • Wire Serializer 0xC00C| | • Win32 SRWLOCK Guard   | | • Kaminsky TxID Defense |
+-------------------------+ +-------------------------+ +-------------------------+
                                     |                               |
                               Disk Snapshot                 Recursive Query
                               (dns_cache.db)                (8.8.8.8 / 1.1.1.1)
```

---

## 3. Deep-Dive Engine Walkthroughs

### Engine E1: Protocol Parsing & Wire Message Engine
- **Lead / Owner:** Vageesh Mathad
- **Academic Subject:** Computer Networks
- **Core Files:** `include/protocol_engine.hpp`, `src/protocol_engine.cpp`
- **Key Algorithms & Mechanisms:**
  1. **12-Byte Header Wire Parsing:**
     Unpacks 16-bit Transaction ID, 16-bit Flags (`QR`, `Opcode`, `AA`, `TC`, `RD`, `RA`, `Z`, `RCODE`), and 16-bit section counts (`QDCOUNT`, `ANCOUNT`, `NSCOUNT`, `ARCOUNT`) in network byte order (big-endian).
  2. **RFC 1035 Compression Pointer Decompression:**
     Detects 2-byte compression pointers where the two highest bits are set (`0xC000`). Extracts the 14-bit byte offset (`((byte[0] & 0x3F) << 8) | byte[1]`). Jumps cursor to target offset and reads labels until null terminator.
  3. **Pointer Loop & Cycle Guard (Security Feature):**
     Neutralizes infinite loop denial-of-service attacks (e.g. self-referencing pointers $A \to A$ or mutual loops $A \to B \to A$) by tracking visited offsets and enforcing a maximum recursion depth of `MAX_DECOMPRESSION_DEPTH = 16`. Exceeding this limit immediately rejects the packet as malformed (`RCODE_FORMERR`).
  4. **Pointer Compression Response Serializer:**
     Emits compressed answers pointing to Question domain offsets (`0xC00C`), saving significant packet payload bytes.

### Engine E2: In-Memory Storage & Cache Engine
- **Lead / Owner:** Mehak Sayed Yusuf
- **Academic Subject:** Design and Analysis of Algorithms (DAA) & Database Systems (DBMS)
- **Core Files:** `include/storage_engine.hpp`, `src/storage_engine.cpp`
- **Key Algorithms & Mechanisms:**
  1. **Primary Hash Index ($O(1)$ Lookups):**
     `std::unordered_map` mapping composite primary keys `(domain, qtype, qclass)` to iterators within the LRU list.
  2. **Doubly-Linked Recency List ($O(1)$ LRU Capacity Eviction):**
     `std::list` tracking access order: MRU at Head, LRU at Tail. On a cache hit, the entry is spliced to Head in $O(1)$ time. When capacity is reached, the Tail entry is popped in $O(1)$ time.
  3. **Min-Heap Priority Queue ($O(\log N)$ Proactive TTL Sweeps):**
     `std::priority_queue` Min-Heap keyed by `expires_at`. The earliest expiring record is always at the heap root, enabling proactive sweeping without $O(N)$ full-table scans.
  4. **Dynamic TTL Aging:**
     Calculates live remaining TTLs: $TTL_{rem} = ExpiresAt - now$. Downstream clients receive strictly non-stale countdowns.
  5. **Concurrency Synchronization:**
     Multi-reader single-writer locking via native Windows `SRWLOCK` (`AcquireSRWLockShared` for non-mutating peeks, `AcquireSRWLockExclusive` for insertions, updates, and promotions).
  6. **Snapshot Durability (`dns_cache.db`):**
     Serializes active valid entries to binary disk format on shutdown, warming up cache on reboot.

### Engine E3: Upstream Resolution & Network Forwarding Engine
- **Lead / Owner:** Ishani G
- **Academic Subject:** Computer Networks & Operating Systems
- **Core Files:** `include/upstream_engine.hpp`, `src/upstream_engine.cpp`
- **Key Algorithms & Mechanisms:**
  1. **Non-blocking UDP Sockets:**
     Socket I/O governed by `select()` with 2.0s timeouts to prevent worker thread starvation.
  2. **Dan Kaminsky Cache Poisoning Defense:**
     Replaces client TxID with a cryptographically unpredictable 16-bit random integer generated via `std::mt19937`. Rejects any incoming datagram whose TxID does not match the outbound query ID (`MISMATCHED_TXID`).
  3. **Exponential Retry Backoff & Automatic Failover:**
     Retries queries with exponential timeout scaling ($T, 2T, 4T$) before failing over from primary `8.8.8.8` to secondary `1.1.1.1`.

### Engine E4: Query Execution Virtual Machine & Telemetry
- **Lead / Owner:** Joel Biju
- **Academic Subject:** Operating Systems Principles & Programming (OSPP)
- **Core Files:** `include/vm_engine.hpp`, `src/vm_engine.cpp`, `include/telemetry_engine.hpp`, `src/telemetry_engine.cpp`, `src/main.cpp`
- **Key Algorithms & Mechanisms:**
  1. **6-Stage Instruction Pipeline:**
     - `STAGE 1 (FETCH)`: Ingests raw datagram slice ($\ge 12$ bytes), timestamps start.
     - `STAGE 2 (DECODE)`: Extracts question and bitfields; routes malformed packets to `FORMERR`.
     - `STAGE 3 (LOOKUP)`: Queries cache. If `HIT`/`NEGATIVE_HIT`, routes straight to `EMIT`. If `EXPIRED`/`MISS`, routes to `RESOLVE`.
     - `STAGE 4 (RESOLVE)`: Outbound forwarder to upstreams. If timeout, routes to `SERVFAIL`. If `NXDOMAIN`, routes to negative cache.
     - `STAGE 5 (COMMIT)`: Inserts upstream records into in-memory cache and min-heap.
     - `STAGE 6 (EMIT)`: Serializes compressed wire response, records microsecond latency in telemetry, and transmits datagram back to client.
  2. **Multi-Threaded Worker Pool:**
     Main Reactor thread enqueues tasks to a thread-safe task queue synchronized via Win32 `CRITICAL_SECTION` and `CONDITION_VARIABLE`. Multiple OS worker threads process transactions in parallel.
  3. **Telemetry & Live Performance Dashboard:**
     Atomic counters track hit ratio %, QPS, and min/avg/max latency in microseconds.

---

## 4. Comprehensive Faculty Viva Defense: 30 Q&A

### Category A: Computer Networks & DNS Protocols

**Q1: What RFC defines the DNS protocol?**  
*Answer:* RFC 1035 (*Domain Names - Implementation and Specification*), supplemented by RFC 1034 (Concepts and Facilities) and RFC 2308 (Negative Caching).

**Q2: What is the structure of the DNS 12-byte header?**  
*Answer:* It consists of six 16-bit fields: Identification (16 bits), Parameter Flags (16 bits: QR, Opcode, AA, TC, RD, RA, Z, RCODE), QDCOUNT (Questions), ANCOUNT (Answers), NSCOUNT (Authorities), and ARCOUNT (Additionals).

**Q3: How does DNS message compression work in RFC 1035?**  
*Answer:* A domain name label sequence can be replaced by a 2-byte compression pointer where the two highest bits are set to `11` (`0xC000`). The remaining 14 bits represent the byte offset from the start of the DNS header where the target domain name was previously serialized.

**Q4: What is `0xC00C` and why does it appear frequently in DNS responses?**  
*Answer:* `0xC00C` is a 16-bit pointer where `0xC0` indicates compression and `0x0C` (decimal 12) points to byte offset 12—which is exactly where the Question section's QNAME begins immediately after the 12-byte DNS header.

**Q5: What is the Dan Kaminsky DNS cache poisoning attack and how did your team prevent it?**  
*Answer:* Attackers flood a recursive resolver with forged responses containing malicious IP mappings while sending random subdomain queries to guess the 16-bit Transaction ID (65,536 possibilities). We neutralize this by generating cryptographically random 16-bit TxIDs using `std::mt19937` and dropping any incoming packet where the received TxID does not strictly match the outbound TxID.

**Q6: Why is UDP preferred over TCP for DNS queries?**  
*Answer:* UDP is connectionless and incurs zero round-trip handshake overhead (no 3-way SYN-SYN/ACK-ACK), which is ideal for small, stateless queries (< 512 bytes). TCP is only used if the response exceeds 512 bytes (truncated bit `TC=1`) or during zone transfers.

**Q7: What is negative caching according to RFC 2308?**  
*Answer:* When an authoritative server responds that a domain does not exist (`NXDOMAIN`, RCODE 3), caching servers cache this negative result for a duration (specified in the SOA record TTL) to prevent repeated recursive lookups for non-existent domains.

**Q8: What does the Recursion Desired (RD) bit mean versus Recursion Available (RA)?**  
*Answer:* `RD` is set by the client in queries to ask the server to recursively resolve the domain. `RA` is set by the server in responses to indicate that recursive resolution is supported.

**Q9: What happens when an incoming packet has less than 12 bytes?**  
*Answer:* The packet cannot even contain a valid DNS header. Our Stage 1 (FETCH) immediately traps this condition, generates a minimal header with `QR=1` and `RCODE=1` (`FORMERR`), and terminates without allocating question records.

**Q10: What are the primary RCODE values in DNS?**  
*Answer:* `0` = NOERROR (Success), `1` = FORMERR (Format Error), `2` = SERVFAIL (Server Failure), `3` = NXDOMAIN (Non-Existent Domain), `4` = NOTIMP (Not Implemented), `5` = REFUSED (Query Refused).

---

### Category B: Operating Systems & Concurrency

**Q11: Explain the Reactor Pattern used in your server.**  
*Answer:* The main thread acts as the Reactor: it binds to UDP port 8053, calls non-blocking `select()` in an event loop, ingests raw datagrams via `recvfrom()`, and dispatches discrete task items to the Worker Thread Pool queue without blocking network ingestion.

**Q12: Why did your team use Win32 `SRWLOCK` instead of standard `CRITICAL_SECTION` or `std::mutex` for the cache?**  
*Answer:* DNS caching workloads are heavily read-dominant (>90% lookups, <10% writes). A standard mutex serializes all readers, creating a thread contention bottleneck. Win32 `SRWLOCK` allows multiple worker threads to execute concurrent read lookups simultaneously (`AcquireSRWLockShared`), only locking exclusively during insertions and evictions (`AcquireSRWLockExclusive`).

**Q13: How does the Worker Thread Pool synchronize its task queue?**  
*Answer:* We use a producer-consumer model: the Reactor thread enters a `CRITICAL_SECTION`, pushes tasks to a `std::queue`, and calls `WakeConditionVariable()`. Worker threads wait using `SleepConditionVariableCS()`, awaken when work arrives, pop tasks, and process the 6-stage VM pipeline.

**Q14: How does the server handle graceful termination?**  
*Answer:* We register a Windows Console Control Handler (`SetConsoleCtrlHandler`) catching `CTRL_C_EVENT`. It sets a volatile flag `g_running = false`. The Reactor loop exits its 100ms `select()` timeout, signals the worker pool to finish pending tasks, flushes active valid cache entries to `dns_cache.db`, prints the telemetry dashboard, and exits cleanly.

**Q15: Why is non-blocking socket I/O necessary when communicating with upstream servers?**  
*Answer:* If an upstream server (e.g. 8.8.8.8) is temporarily unreachable or drops a UDP packet, a blocking `recvfrom()` would freeze the worker thread indefinitely. Non-blocking `select()` enforces a strict 2.0s deadline, allowing the engine to retry or failover to `1.1.1.1` without thread starvation.

**Q16: How do you prevent buffer race conditions between concurrent worker threads?**  
*Answer:* In Stage 1 (FETCH), the reactor thread immediately allocates an isolated, self-contained buffer copy (`std::vector<uint8_t>`) for each incoming datagram. Each worker operates entirely on its own transaction memory slice.

**Q17: What is the difference between user space threads and OS worker threads in this project?**  
*Answer:* Our worker pool creates true OS kernel-level threads via Win32 `CreateThread()`. They are scheduled preemptively by the Windows NT kernel across multiple physical CPU cores.

---

### Category C: Design and Analysis of Algorithms (DAA)

**Q18: Why is dual-indexing necessary in a high-performance cache?**  
*Answer:* A hash table alone cannot maintain access recency order without an $O(N)$ scan during evictions. A linked list alone provides $O(N)$ lookup times. By combining a hash table (`std::unordered_map`) with a doubly-linked list (`std::list`), we achieve both $O(1)$ lookups and $O(1)$ LRU capacity evictions.

**Q19: Explain the $O(1)$ LRU promotion algorithm upon a cache hit.**  
*Answer:* When a record hits in the hash map, its iterator points directly to its node in `std::list`. Using `std::list::splice(lru_list_.begin(), lru_list_, it)`, we detach the node from its current position and re-insert it at the MRU Head in $O(1)$ pointer operations without any memory reallocation.

**Q20: Why did your team implement an $O(\log N)$ Min-Heap for TTL expiration instead of relying only on lazy expiration?**  
*Answer:* Pure lazy expiration only checks a record when queried. Inactive records that are never queried again linger forever in memory. The Min-Heap priority queue stores entries keyed by absolute `expires_at` timestamp. The earliest expiring entry is always at the root ($O(1)$ inspection), allowing proactive background purging of expired batches in $O(\log N)$ time per record.

**Q21: How do you handle record updates in the Min-Heap without expensive $O(N)$ heap modifications?**  
*Answer:* We use lazy deletion with a monotonic version counter. Each cache entry has a version counter. When an entry is updated, a new heap node with the incremented version is pushed. When the heap root pops an expired node, it verifies whether the heap version matches the active entry version; stale versions are discarded in $O(\log N)$ time.

**Q22: What is the worst-case time complexity of domain name decompression?**  
*Answer:* With an unbounded pointer loop, complexity could be infinite $O(\infty)$ causing server hang. With our depth guard `MAX_DECOMPRESSION_DEPTH = 16`, each label traversal is strictly bounded to $16 \times 63$ bytes $\le 1008$ iterations, guaranteeing deterministic $O(1)$ time complexity.

**Q23: What is dynamic TTL calculation and what formula is applied?**  
*Answer:* Cached records must decay in real time. The formula is:
$$TTL_{rem} = TTL_{orig} - (t_{current} - t_{inserted}) = ExpiresAt - t_{current}$$
If $TTL_{rem} \le 0$, the record is deemed expired ($O(1)$ evaluation).

---

### Category D: Database Systems & Telemetry

**Q24: What constitutes the composite primary key in your cache engine?**  
*Answer:* The tuple `(Domain, QTYPE, QCLASS)`. This ensures that an `A` record (IPv4), `AAAA` record (IPv6), and `CNAME` record for `example.com` are stored as distinct, non-colliding entities.

**Q25: Describe the binary format of `dns_cache.db`.**  
*Answer:*
- 8-Byte Magic Header (`"DNSCACHE\x01"`)
- 4-Byte Record Count ($N$)
- For each record:
  - 2-byte domain length + domain string
  - 2-byte QTYPE + 2-byte QCLASS
  - 4-byte remaining TTL
  - 1-byte negative flag + 1-byte RCODE
  - 2-byte RR count + RR serialized fields (Type, Class, TTL, RDLENGTH, RDATA bytes)

**Q26: What happens during cache warm-up on boot if the server was offline for 1 hour?**  
*Answer:* On boot, `load_snapshot()` inspects each stored entry's remaining TTL. If the remaining TTL is non-zero, it schedules expiration relative to current boot time. Any record that expired while the server was offline is discarded, ensuring zero stale data leaks.

**Q27: How does your telemetry engine record microsecond latency without locking bottlenecks?**  
*Answer:* Telemetry counters (`total_queries`, `cache_hits`, `cache_misses`, `upstream_success`) use lock-free C++11 atomic integers (`std::atomic<uint64_t>`). Latency minimums and maximums are updated using atomic compare-and-swap (`compare_exchange_weak`) loops.

**Q28: How is the cache hit ratio calculated?**  
*Answer:*
$$\text{Hit Ratio } (\%) = \frac{\text{Hits} + \text{Negative Hits}}{\text{Hits} + \text{Misses} + \text{Expired} + \text{Negative Hits}} \times 100$$

**Q29: What is the observed performance gain of cache hits versus upstream recursive lookups?**  
*Answer:* Live tests prove that an upstream recursive query takes ~121 ms (network roundtrip to Google 8.8.8.8), whereas an in-memory cache hit executes in ~0.85 ms—yielding a **140x speedup**.

**Q30: Why is C++ (C++14) an optimal language for building a DNS caching resolver?**  
*Answer:* DNS servers process tens of thousands of datagrams per second. C++ provides direct zero-cost memory mapping, deterministic RAII resource management (no garbage collection latency spikes), bitfield manipulation for wire packets, and direct access to native OS socket and synchronization primitives (`ws2_32`, `SRWLOCK`).

---

## 5. Course Alignment & Syllabus Mapping

| Academic Course | Where it Appears in our Codebase | Concrete Code Proof & Artifacts |
| :--- | :--- | :--- |
| **Computer Networks** | Wire format parsing, UDP sockets, upstream recursive forwarding, RFC 1035 compliance | [`protocol_engine.cpp`](file:///a:/dns-resolver-caching-server/src/protocol_engine.cpp), [`upstream_engine.cpp`](file:///a:/dns-resolver-caching-server/src/upstream_engine.cpp), big-endian bitfields, `0xC00C` pointer compression. |
| **Operating Systems (OSPP)** | Multi-threaded worker pool, Reactor socket event loop, Win32 `SRWLOCK` & `CRITICAL_SECTION`, non-blocking I/O | [`main.cpp`](file:///a:/dns-resolver-caching-server/src/main.cpp), [`vm_engine.cpp`](file:///a:/dns-resolver-caching-server/src/vm_engine.cpp), worker task queue, non-blocking `select()` polling. |
| **Design & Analysis of Algorithms (DAA)** | Dual-index caching: $O(1)$ LRU doubly-linked list & $O(\log N)$ Min-Heap priority queue | [`storage_engine.cpp`](file:///a:/dns-resolver-caching-server/src/storage_engine.cpp), hash indexing, `std::list::splice` recency promotion, min-heap root popping. |
| **Database Systems (DBMS)** | In-memory key-value store, composite primary key, ACID write locking, snapshot durability | [`storage_engine.cpp`](file:///a:/dns-resolver-caching-server/src/storage_engine.cpp), `dns_cache.db` binary persistence, multi-reader single-writer locking. |

---

## 6. Empirical Verification & Test Report

All engines have been verified with automated unit and integration suites (30/30 tests passing):

```text
========================================================================
                      TEST SUITE EXECUTION SUMMARY                      
========================================================================
1. Engine E1 (Protocol Wire Engine)   : 10/10 tests PASSED [test_protocol.exe]
2. Engine E2 (In-Memory Cache & LRU)  :  8/8  tests PASSED [test_cache.exe]
3. Engine E3 (Upstream Forwarder)     :  6/6  tests PASSED [test_upstream.exe]
4. Engine E4 (Query Execution VM)     :  6/6  tests PASSED [test_vm.exe]
------------------------------------------------------------------------
TOTAL AUTOMATED TEST CASES            : 30/30 tests PASSED (100% Success)
========================================================================

LIVE SERVER RUNTIME VERIFICATION (127.0.0.1:8053):
  [*] Sending Query 1: 'example.com' (Cache MISS) -> 121.26 ms (Resolved via 8.8.8.8)
  [*] Sending Query 2: 'example.com' (Cache HIT)  ->   0.85 ms (Resolved from memory)
  [+] Speedup Factor: 142.6x faster on in-memory cache hits!
========================================================================
```

---
*Generated for Theme 4: Network Systems and Tools — Team 11 (PES University)*
