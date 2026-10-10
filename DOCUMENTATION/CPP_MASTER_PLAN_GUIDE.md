# C++ MASTER PLAN & ARCHITECTURAL SPECIFICATION GUIDE
## DNS Resolver and Caching Server (Academic Theme 4: Network Systems & Tools)

> **CRITICAL LIVING DOCUMENT RULE:**  
> This file is the authoritative Master Plan Guide for the project.  
> Whenever any design decision, interface, algorithm, engine boundary, or implementation step is modified or added during development, **this document MUST be updated immediately** so that it remains 100% synchronized with the active C++ codebase.

---

## 1. 30-Second Viva Pitch (Student Voice)

> *"We built an RFC 1035 compliant DNS Resolver and Caching Server from scratch in C++. It listens on UDP port 8053, ingests raw network datagrams, parses DNS wire messages byte-by-byte with zero external libraries, and resolves domain queries concurrently using a worker thread pool. It features a dual-indexed in-memory cache combining an O(1) LRU eviction doubly-linked list with an O(log N) Min-Heap for proactive TTL expiration. When records are not cached, it forwards queries upstream to recursive resolvers like Google's 8.8.8.8, dynamically calculates remaining TTLs on hits, and serializes RFC-compliant response packets using pointer compression. The whole system is governed by a 6-stage Virtual Machine instruction pipeline and tracked with microsecond-precision telemetry."*

---

## 2. Complete Journey of One DNS Request (Trace)

```
[ Client: dig @127.0.0.1 -p 8053 example.com A ]
       │
       │  (1) Raw UDP Datagram (29 bytes)
       ▼
┌────────────────────────────────────────────────────────────────────────┐
│  Server Network Reactor (src/main.cpp)                                 │
│  • recvfrom() ingests 512-byte buffer on UDP 127.0.0.1:8053            │
│  • Allocates isolated packet buffer                                    │
│  • Enqueues transaction into Worker Thread Pool                        │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│  Engine E4: Query Execution Virtual Machine (src/vm_engine.cpp)        │
│                                                                        │
│  [STAGE 1: FETCH]                                                      │
│  Extract client sockaddr_in and byte length; timestamp request start. │
│                                                                        │
│  [STAGE 2: DECODE — Engine E1]                                         │
│  • Parse 12-byte Header bitfields (TxID, Flags, QDCount)               │
│  • Decode Question: QNAME ("example.com"), QTYPE (1=A), QCLASS (1=IN)  │
│  • Validate bounds & check recursion loop depth (max 16 pointers)      │
│  • Error handling: If malformed -> Generate FORMERR (RCODE 1)          │
│                                                                        │
│  [STAGE 3: LOOKUP — Engine E2]                                         │
│  • Query In-Memory Cache via Key: (Domain, QTYPE, QCLASS)              │
│  • Acquire SRWLOCK Shared (Read Lock)                                  │
│  • Case HIT:                                                           │
│      - Calculate dynamic TTL: rem_ttl = orig_ttl - elapsed_seconds     │
│      - Splice Node to Head of LRU List (Promote Recency)               │
│      - Skip upstream resolution -> Jump to STAGE 6 (EMIT)              │
│  • Case MISS / EXPIRED:                                                │
│      - Remove expired entry via Min-Heap / Lazy sweep                  │
│      - Proceed to STAGE 4 (RESOLVE)                                    │
│                                                                        │
│  [STAGE 4: RESOLVE — Engine E3]                                        │
│  • Dispatch query to upstream resolver (8.8.8.8:53) over UDP           │
│  • Socket polling with non-blocking select() and 2.0s timeout          │
│  • Validate response TxID (Kaminsky cache poisoning defense)           │
│  • On Upstream Timeout -> Retry failover (1.1.1.1) -> or SERVFAIL     │
│  • Parse Answer RRSet (IP: 93.184.216.34, Authoritative TTL: 300s)     │
│                                                                        │
│  [STAGE 5: COMMIT — Engine E2]                                         │
│  • Acquire SRWLOCK Exclusive (Write Lock)                              │
│  • Insert into Hash Index + Prepend to LRU Doubly-Linked List          │
│  • Push Expiration Node into Min-Heap (ExpiresAt = now + TTL)          │
│  • If Cache Size > MaxCapacity: Evict Tail Node in O(1) time           │
│                                                                        │
│  [STAGE 6: EMIT — Engine E1]                                           │
│  • Build binary DNS response wire format                               │
│  • Set Header: QR=1, AA=0, RA=1, RD preserved, RCODE=0                 │
│  • Question Section copied verbatim                                    │
│  • Answer Section: 0xC00C compression pointer + Type + Class + TTL + IP│
│  • sendto() transmits response datagram back to client                 │
│  • Record end timestamp -> Update Engine E4 Telemetry Metrics          │
└────────────────────────────────────────────────────────────────────────┘
       │
       │  (2) Binary DNS Response Datagram (45 bytes)
       ▼
[ Client receives answer: example.com -> 93.184.216.34 (TTL: 300s) ]
```

---

## 3. Four Core Engines Decomposition (S1 Aligned)

### Engine E1: Protocol Parsing & Wire Message Engine
- **Primary Owner:** Vageesh Mathad (01fe24bci008)
- **Academic Domain:** Computer Networks
- **Core Responsibilities:**
  1. **Binary Wire Ingestion:** Deserializes raw RFC 1035 network buffers into structured C++ types (`Header`, `Question`, `ResourceRecord`, `Message`).
  2. **Domain Label Pointer Decompression:** RFC 1035 compression uses `0xC0` high bits to signal a 14-bit pointer offset. E1 includes a strict dereference depth counter (`depth <= 16`) to neutralize cyclic decompression attacks.
  3. **Endianness Transformation:** Safe network-to-host (`ntohs`, `ntohl`) and host-to-network (`htons`, `htonl`) bitfield packing.
  4. **Wire Serializer:** Manages byte-buffer cursors to serialize response packets with pointer compression (`0xC00C`).

### Engine E2: In-Memory DNS Storage & Cache Engine
- **Primary Owner:** Mehak Sayed Yusuf (01fe24bci012)
- **Academic Domain:** Design & Analysis of Algorithms / DBMS
- **Core Responsibilities:**
  1. **Dual-Indexed Cache:**
     - **$O(1)$ LRU Eviction Subsystem:** `std::unordered_map<CompositeKey, LRUNode*>` paired with an intrusive doubly-linked list (`std::list` / raw pointer node chain). Hit nodes splice to head in $O(1)$; tail node evicts in $O(1)$ at cache ceiling.
     - **$O(\log N)$ Proactive TTL Purge:** Min-Heap priority queue (`std::priority_queue`) ordered by absolute expiration timestamp (`ExpiresAt`). Allows immediate identification and removal of expired records without scanning the entire cache.
  2. **Dynamic TTL Aging:** Decrements TTL dynamically on cache hits ($TTL_{rem} = TTL_{orig} - \Delta t$). Clients receive valid, non-stale countdowns.
  3. **Multi-Threaded Concurrency:** Protected by native Windows `SRWLOCK` supporting concurrent reads (`AcquireSRWLockShared`) and atomic single-writer mutations (`AcquireSRWLockExclusive`).
  4. **Durability & Persistence:** Dumps active cache state to disk binary snapshot (`dns_cache.db`) on graceful shutdown (`SIGINT`), and performs cache warm-up on boot.

### Engine E3: Upstream Resolution & Network Forwarding Engine
- **Primary Owner:** Ishani G (01fe22bci009)
- **Academic Domain:** Computer Networks / Operating Systems
- **Core Responsibilities:**
  1. **Recursive Query Forwarding:** Dispatches uncached queries to external upstream servers (Primary: `8.8.8.8`, Fallback: `1.1.1.1`) over non-blocking UDP sockets.
  2. **Non-Blocking I/O & Timers:** Utilizes `select()` with `struct timeval` for microsecond-precision timeout detection (2.0s timeout cap).
  3. **Exponential Retry & Failover:** Retries dropped packets with backoff before falling back to secondary upstream. Returns `SERVFAIL` (RCODE 2) if all upstreams fail.
  4. **Kaminsky Spoofing Defense:** Validates that incoming upstream responses match the exact 16-bit Transaction ID and expected source IP.

### Engine E4: Query Execution Virtual Machine & Telemetry Engine
- **Primary Owner:** Joel Biju (01fe24bci021)
- **Academic Domain:** Operating Systems Principles & Programming
- **Core Responsibilities:**
  1. **VM Pipeline State Machine:** Orchestrates the 6-stage lifecycle (`FETCH ➔ DECODE ➔ LOOKUP ➔ RESOLVE ➔ COMMIT ➔ EMIT`).
  2. **Condition Handling Matrix:** Deterministically routes all query conditions:
     - `HIT` -> dynamic TTL decrement & immediate emit.
     - `MISS` -> upstream resolution & cache commit.
     - `EXPIRED` -> lazy purge & upstream re-resolve.
     - `NXDOMAIN` -> RFC 2308 negative caching using SOA minimum TTL.
     - `SERVFAIL` -> upstream timeout / network outage response.
     - `FORMERR` -> syntax / malformed packet response.
  3. **Worker Thread Pool:** Thread pool with thread-safe task queue to handle high-concurrency client load (>= 1,000 QPS) without socket buffer drops.
  4. **Telemetry & Benchmarking:** Real-time tracking of hit ratio %, average latency in microseconds (cache hit < 1 ms vs upstream ~30 ms), throughput QPS, and live console stats.

---

## 4. Implementation Phases & Milestones

```
PHASE 0: Clean Slate & Build Toolchain       [COMPLETED]
PHASE 1: Architectural & Orbit Diagrams      [COMPLETED]
PHASE 2: Engine E1 (Protocol Parsing)        [CURRENT TARGET]
PHASE 3: Engine E2 (In-Memory Cache & LRU)   [PENDING]
PHASE 4: Engine E3 (Upstream Forwarding)     [PENDING]
PHASE 5: Engine E4 (Virtual Machine Pipeline)[PENDING]
PHASE 6: Telemetry & KPI Benchmarking        [PENDING]
PHASE 7: Main Server Loop & Multi-threading  [PENDING]
PHASE 8: Complete C++ Viva Guide & Docs      [PENDING]
```

### Detailed Phase Plan

#### Phase 2 — Engine E1: Protocol Parsing & Wire Message Engine
- **Files:** `include/protocol_engine.hpp`, `src/protocol_engine.cpp`, `tests/test_protocol.cpp`
- **Key Tasks:**
  - Implement `DNSHeader` struct with bitfield manipulation (Flags, QR, Opcode, AA, TC, RD, RA, RCODE).
  - Implement `DNSQuestion` struct with domain label decoder (e.g. `\x07example\x03com\x00` -> `example.com`).
  - Implement `LabelDecompressor` with recursion depth counter (max 16) and cyclic pointer check.
  - Implement `WireSerializer` to construct binary DNS response packets.
  - Build automated unit tests in `tests/test_protocol.cpp`.
  - Validate with `make test_protocol`, update `CONTEXT.md`, update this Master Plan Guide, and git commit.

#### Phase 3 — Engine E2: In-Memory DNS Storage & Cache Engine
- **Files:** `include/storage_engine.hpp`, `src/storage_engine.cpp`, `tests/test_cache.cpp`
- **Key Tasks:**
  - Define `CompositeKey` (`domain`, `qtype`, `qclass`) with custom `std::hash`.
  - Implement Doubly-Linked Recency List for $O(1)$ LRU cache capacity eviction.
  - Implement Min-Heap priority queue for $O(\log N)$ proactive TTL expiration.
  - Implement dynamic remaining TTL recalculation ($TTL_{rem} = TTL_{orig} - \Delta t$).
  - Implement Reader-Writer locking via `SharedMutex` (`SRWLOCK`).
  - Implement `SaveSnapshot()` and `LoadSnapshot()` for `dns_cache.db` disk persistence.
  - Build automated unit tests in `tests/test_cache.cpp`.
  - Validate with `make test_cache`, update `CONTEXT.md`, update this Master Plan Guide, and git commit.

#### Phase 4 — Engine E3: Upstream Resolution & Network Forwarding Engine
- **Files:** `include/upstream_engine.hpp`, `src/upstream_engine.cpp`, `tests/test_upstream.cpp`
- **Key Tasks:**
  - Implement non-blocking UDP socket wrapper with Winsock2.
  - Implement `ResolveUpstream()` with 2.0s `select()` timeout and exponential retry backoff.
  - Implement Kaminsky 16-bit TxID validation.
  - Implement primary (8.8.8.8) and secondary (1.1.1.1) server failover.
  - Build automated unit tests in `tests/test_upstream.cpp`.
  - Validate with `make test_upstream`, update `CONTEXT.md`, update this Master Plan Guide, and git commit.

#### Phase 5 — Engine E4: Query Execution Virtual Machine
- **Files:** `include/vm_engine.hpp`, `src/vm_engine.cpp`, `tests/test_vm_integration.cpp`
- **Key Tasks:**
  - Implement the 6-stage pipeline: `FETCH -> DECODE -> LOOKUP -> RESOLVE -> COMMIT -> EMIT`.
  - Implement condition handling matrix (`HIT`, `MISS`, `EXPIRED`, `NXDOMAIN`, `SERVFAIL`, `FORMERR`).
  - Implement RFC 2308 negative caching for non-existent domains.
  - Build end-to-end integration tests simulating complete client transactions.
  - Validate with `make test_vm`, update `CONTEXT.md`, update this Master Plan Guide, and git commit.

#### Phase 6 — Telemetry & KPI Benchmarking Subsystem
- **Files:** `include/telemetry_engine.hpp`, `src/telemetry_engine.cpp`
- **Key Tasks:**
  - Implement atomic counters for total queries, cache hits, cache misses, and hit ratio %.
  - Implement high-resolution microsecond latency timers (`std::chrono::high_resolution_clock`).
  - Create live console telemetry dashboard renderer.
  - Update `CONTEXT.md`, update this Master Plan Guide, and git commit.

#### Phase 7 — Server Main Entry Loop, Concurrency & Live Testing
- **Files:** `src/main.cpp`
- **Key Tasks:**
  - Implement UDP socket listener on port 8053 with non-blocking reactor.
  - Implement Worker Thread Pool to process incoming datagrams concurrently.
  - Implement graceful shutdown handler (`SIGINT` / Ctrl+C) with cache snapshot persistence.
  - Live testing with command-line tools: `dig @127.0.0.1 -p 8053 example.com` and `nslookup`.
  - Update `CONTEXT.md`, update this Master Plan Guide, and git commit.

#### Phase 8 — Comprehensive C++ Project Master Guide & Viva Prep
- **Files:** `docs/CPP_MASTER_GUIDE.md`
- **Key Tasks:**
  - Complete viva defense guide with 30 in-depth professor questions & answers.
  - Theoretical mapping to Computer Networks, OSPP, DAA, and DBMS.
  - Finalize all benchmarks, test outputs, and `CONTEXT.md`.

---

## 5. Course Mapping Matrix for Faculty Viva

| Subject | Where it appears in our C++ Code | Code Proof & Mechanism |
| :--- | :--- | :--- |
| **Computer Networks** | Wire format parsing, UDP sockets, upstream resolution, RFC 1035 compliance | `protocol_engine.cpp`, `upstream_engine.cpp`, big-endian bitfields, pointer compression (`0xC00C`). |
| **OSPP** | Thread pool concurrency, non-blocking socket I/O, Win32 `SRWLOCK` & `CRITICAL_SECTION`, signal handling | `main.cpp`, `vm_engine.cpp`, worker thread queues, RAII lock guards, non-blocking `select()`. |
| **DAA** | Dual-index cache algorithms: $O(1)$ LRU doubly-linked list & $O(\log N)$ Min-Heap priority queue | `storage_engine.cpp`, `std::unordered_map` hash indexing, node splicing, min-heap priority queue. |
| **DBMS** | In-memory key-value store, composite primary key, dynamic TTL expiry, ACID write locking, snapshot durability | `storage_engine.cpp`, `dns_cache.db` file serialization, multi-reader single-writer synchronization. |

---

## 6. Living Synchronization Protocol

1. **Continuous Updates:** Whenever code modifications, algorithm adjustments, or interface additions occur during implementation, they **MUST be synchronized back into this file**.
2. **State Preservation:** Every phase completion is logged in [CONTEXT.md](file:///a:/dns-resolver-caching-server/CONTEXT.md).
3. **Git Commits:** Every milestone is committed with a descriptive git commit.
4. **Official Deliverables:** All submitted Gate 0/Gate 1 PPTs and Handbook in `DOCUMENTATION/` remain preserved.
