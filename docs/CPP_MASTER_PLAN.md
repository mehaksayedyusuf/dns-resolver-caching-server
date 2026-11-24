# C++ MASTER PLAN & ARCHITECTURAL SPECIFICATION GUIDE
## DNS Resolver & Caching Server — Distributed VM Multi-Cluster
### Academic Theme 4: Network Systems & Tools (Team 11)

> **LIVING DOCUMENT:** Whenever code is modified, this file must be updated immediately to remain synchronized with the codebase.

---

## 1. 30-Second Viva Pitch

> *"We built an RFC 1035 compliant DNS Resolver and Caching Server from scratch in C++ (C++14), deployed as a distributed multi-cluster system on VMware Workstation Pro. It listens on UDP `0.0.0.0:8053` — reachable from any client on the VMnet8 subnet. It parses DNS wire messages byte-by-byte, resolves queries via a cross-platform worker thread pool, and accelerates lookups using a dual-indexed cache: O(1) LRU (hash + doubly-linked list) and O(log N) Min-Heap proactive TTL expiration. Cache misses forward to `8.8.8.8`/`1.1.1.1` over non-blocking UDP with randomized TxID defense. In benchmark runs, cache hits resolved in ~0.414 ms median vs ~154 ms upstream. The system runs on two Ubuntu Server VMs (Cluster-Alpha and Cluster-Beta) with identity-tagged telemetry and automatic client failover."*

---

## 2. Visual System Architecture: VM Multi-Cluster Orbit Model

The system follows an **Orbit Architecture** where **Engine E4 (Query Execution VM)** is the gravitational center, coordinating satellite engines, deployed across multiple VMs behind VMnet8.

```mermaid
flowchart TD
    subgraph External["External Network & Storage Boundaries"]
        Client["DNS Client\n(dig / nslookup / cluster_client.py)\nPort 8053 — any VM IP"]
        Upstream["Upstream Recursive Resolvers\n8.8.8.8 (primary) / 1.1.1.1 (failover)\nPort 53 (UDP)"]
        Disk[("Disk Snapshot\n(dns_cache_CLUSTER-ID.db)")]
    end

    subgraph VMnet8["VMware VMnet8 NAT (192.168.150.0/24)"]
        subgraph Alpha["Cluster-Alpha VM (192.168.150.10)"]
            CoreVM_A["ENGINE E4: Query Execution VM\nFETCH→DECODE→LOOKUP→RESOLVE→COMMIT→EMIT"]
            E1_A["E1: Protocol Parser"]
            E2_A["E2: Cache & LRU"]
            E3_A["E3: Upstream Engine"]
            E5_A["E5: Telemetry"]
        end
        subgraph Beta["Cluster-Beta VM (192.168.150.20)"]
            CoreVM_B["ENGINE E4: Query Execution VM\nFETCH→DECODE→LOOKUP→RESOLVE→COMMIT→EMIT"]
            E1_B["E1: Protocol Parser"]
            E2_B["E2: Cache & LRU"]
            E3_B["E3: Upstream Engine"]
            E5_B["E5: Telemetry"]
        end
    end

    Client ==>|"UDP Datagram :8053"| Alpha
    Client ==>|"Failover UDP :8053"| Beta
    CoreVM_A <===>|"Parse/Serialize"| E1_A
    CoreVM_A <===>|"LRU Lookup/Commit"| E2_A
    CoreVM_A <===>|"Forward on MISS"| E3_A
    CoreVM_A -.->|"Metrics"| E5_A
    E3_A <===>|"Recursive Query"| Upstream
    E2_A <===>|"Dump/Warm-up"| Disk
    Alpha <-->|"Inter-Cluster Peering"| Beta
```

---

## 3. Visual 6-Stage VM Execution Pipeline

```mermaid
flowchart TD
    Start([UDP Datagram Received — 0.0.0.0:8053]) --> S1["STAGE 1: FETCH\nIsolate buffer slice\nTimestamp start\nLog client IP:port"]
    S1 --> S2["STAGE 2: DECODE — Engine E1\n12-byte header bitfields\nDomain label decompression (depth ≤ 16)"]
    S2 --> CheckMalformed{Packet ≥ 12 bytes\n& valid?}
    CheckMalformed -- No --> EmitFORMERR["FORMERR (RCODE 1)"] --> S6_Emit
    CheckMalformed -- Yes --> S3["STAGE 3: LOOKUP — Engine E2\nCompositeKey: (domain, qtype, qclass)"]
    S3 --> CacheVerdict{Cache Verdict}
    CacheVerdict -- HIT --> DynamicTTL["Recalculate TTL_rem\nSplice to MRU head"] --> S6_Build["Build HIT response"] --> S6_Emit
    CacheVerdict -- EXPIRED --> PurgeExpired["Min-Heap pop expired"] --> S4
    CacheVerdict -- MISS --> S4
    S4["STAGE 4: RESOLVE — Engine E3\nForward to 8.8.8.8:53\nnon-blocking select() 2s"] --> UpstreamWait{Response ≤ 2s?}
    UpstreamWait -- Timeout --> Retry{Retry < 2?}
    Retry -- Yes --> S4
    Retry -- No --> Failover["Failover to 1.1.1.1"] --> UpstreamWait2{Response?}
    UpstreamWait2 -- No --> EmitSERVFAIL["SERVFAIL (RCODE 2)"] --> S6_Emit
    UpstreamWait -- Response --> ValidateTxID{TxID match?}
    UpstreamWait2 -- Yes --> ValidateTxID
    ValidateTxID -- Mismatch --> Drop["Drop — Kaminsky Spoof"] --> EmitSERVFAIL
    ValidateTxID -- Valid --> ParseUpstream["Parse RRSet"] --> CheckRCODE{Upstream RCODE?}
    CheckRCODE -- NXDOMAIN --> NegCache["RFC 2308 Negative Cache"] --> S6_Emit
    CheckRCODE -- NOERROR --> S5["STAGE 5: COMMIT — Engine E2\nInsert hash + LRU head\nMin-Heap push ExpiresAt\nEvict LRU tail if full"]
    S5 --> S6_Emit["STAGE 6: EMIT — Engine E1\n0xC00C pointer compression\nsendto() client\nTelemetry latency record"]
    S6_Emit --> End([Client Receives Response])
```

---

## 4. Dual-Index In-Memory Cache (Engine E2)

```mermaid
flowchart LR
    subgraph HashIndex["Primary Hash Index: O(1) Access"]
        Map["std::unordered_map<CompositeKey, list::iterator>\nKey: {domain, qtype, qclass}"]
    end
    subgraph LRUList["Doubly-Linked Recency List: O(1) LRU Eviction"]
        direction LR
        HeadNode["[MRU Head]\nMost Recently Used"] <--> NodeA["Node A\n(Spliced on HIT)"]
        NodeA <--> NodeB["Node B ..."]
        NodeB <--> TailNode["[LRU Tail]\n(Evicted at capacity)"]
    end
    subgraph MinHeap["Min-Heap Priority Queue: O(log N) TTL Purge"]
        Root["Root: Earliest ExpiresAt"]
        Root --> L["Left Child"]
        Root --> R["Right Child"]
    end
    Map -->|"Iterator"| NodeA
    NodeA -.->|"ExpiresAt push"| Root
```

**Key Cache Mechanisms:**
1. **O(1) LRU Promotion:** `std::list::splice(begin(), list_, it)` — O(1) pointer swaps, no allocation.
2. **O(1) LRU Eviction:** `pop_back()` + hash erase at capacity.
3. **O(log N) Min-Heap Sweep:** Root always = earliest expiring. Proactive purge without full scan.
4. **Dynamic TTL:** `TTL_rem = ExpiresAt − now`. Downstream gets accurate countdown.
5. **Thread Safety:** `std::shared_timed_mutex` (Linux) or `SRWLOCK` (Windows). Shared for reads, exclusive for writes.

---

## 5. Wire Format & Label Decompression (Engine E1)

```
DNS 12-Byte Header:
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-------------------------------+-------------------------------+
|        Transaction ID         |QR| Opcode |AA|TC|RD|RA| Z | RCODE|
+-------------------------------+-------------------------------+
|          QDCOUNT (1)          |          ANCOUNT (0/1)        |
+-------------------------------+-------------------------------+
|          NSCOUNT (0)          |          ARCOUNT (0)          |
+-------------------------------+-------------------------------+
```

```mermaid
sequenceDiagram
    autonumber
    participant P as E1 Parser
    participant B as Byte Stream (512 bytes)
    participant D as Depth Guard (≤ 16)
    P->>B: Read byte at cursor
    alt Byte < 0xC0 (Normal Label)
        P->>B: Read L bytes, append to domain, advance cursor L+1
    else Byte >= 0xC0 (Compression Pointer)
        P->>D: Increment depth
        alt Depth > 16
            D-->>P: ABORT — Cyclic Pointer Loop → FORMERR
        else Depth ≤ 16
            P->>B: Extract 14-bit offset ((byte[0] & 0x3F) << 8 | byte[1])
            P->>B: Jump cursor to offset, continue decoding
        end
    end
```

---

## 6. Concurrency & Thread Pool (Engine E4)

```mermaid
flowchart TD
    subgraph Reactor["Main Reactor Thread (src/main.cpp)"]
        Bind["bind() 0.0.0.0:8053"]
        Select["select() non-blocking loop"]
        Recv["recvfrom() → copy to vector<uint8_t>"]
        Bind --> Select --> Recv
    end
    subgraph Pool["std::thread Worker Pool (cross-platform)"]
        Queue["std::queue + std::mutex\n+ std::condition_variable"]
        W1["Worker Thread 1\n6-Stage VM Pipeline"]
        W2["Worker Thread 2\n6-Stage VM Pipeline"]
        WN["Worker Thread N\n6-Stage VM Pipeline"]
        Recv -->|"push + notify_one()"| Queue
        Queue -->|"pop + process"| W1
        Queue -->|"pop + process"| W2
        Queue -->|"pop + process"| WN
    end
    subgraph Cache["Shared Cache (Engine E2)"]
        RWLock["shared_timed_mutex\n(Linux) / SRWLOCK (Win)"]
        Store["LRU Hash + Min-Heap"]
        W1 <-->|"lock_shared / lock"| RWLock
        W2 <-->|"lock_shared / lock"| RWLock
        WN <-->|"lock_shared / lock"| RWLock
        RWLock <--> Store
    end
```

---

## 7. Five-Engine Decomposition

| Engine | Name & Owner | Subject | Core Algorithms & C++ |
|:---:|:---|:---|:---|
| **E1** | Protocol Parsing & Wire Message Engine *(Vageesh Mathad)* | Computer Networks | RFC 1035 wire parser, 14-bit pointer decompression (depth ≤ 16), `0xC00C` serializer, big-endian `htons`/`ntohs` |
| **E2** | In-Memory DNS Storage & Cache Engine *(Mehak Sayed Yusuf)* | DAA & DBMS | O(1) LRU (`unordered_map` + `list::splice`), O(log N) Min-Heap (`priority_queue`), dynamic TTL, `shared_timed_mutex`/`SRWLOCK`, `dns_cache_<id>.db` |
| **E3** | Upstream Resolution & Forwarding Engine *(Ishani G)* | CN & OS | Non-blocking `fcntl`/`ioctlsocket`, `select(sock+1,...)`, exponential backoff, randomized TxID (`std::mt19937`), `8.8.8.8`→`1.1.1.1` failover |
| **E4** | Query Execution VM Engine *(Joel Biju)* | OSPP | 6-stage pipeline, `std::thread` pool, `std::mutex`+`condition_variable`, `--cluster-id` tagging, `signal`/`SetConsoleCtrlHandler` |
| **E5** | Telemetry Subsystem *(Joel Biju)* | OSPP | `std::atomic<uint64_t>` counters, CAS min/max latency, hit ratio %, QPS, live dashboard |

---

## 8. Implementation Phases

```
PHASE 0: Clean Slate & Build Toolchain              [COMPLETED]
PHASE 1: Architectural & Orbit Diagrams             [COMPLETED]
PHASE 2: Engine E1 — Protocol Parsing              [COMPLETED]
PHASE 3: Engine E2 — In-Memory Cache & LRU         [COMPLETED]
PHASE 4: Engine E3 — Upstream Forwarding           [COMPLETED]
PHASE 5: Engine E4 — Virtual Machine Pipeline      [COMPLETED]
PHASE 6: Telemetry & KPI Benchmarking              [COMPLETED]
PHASE 7: Main Server Loop & Multi-threading        [COMPLETED]
PHASE 8: Viva Guide & Docs                         [COMPLETED]
PHASE 9: VM Multi-Cluster Architecture             [COMPLETED]
  ├─ Cross-platform POSIX/Win32 abstraction (common.hpp)
  ├─ --bind, --cluster-id, --peer CLI args (main.cpp)
  ├─ fcntl/ioctlsocket portable upstream (upstream_engine.cpp)
  ├─ Auto-detecting cross-platform Makefile
  ├─ VMware Workstation Pro deployment guide
  ├─ deploy_vm_cluster.sh — one-click VM deployer
  ├─ cluster_client.py — multi-cluster test client
  ├─ verify_implementation.py — 72-check verification suite
  └─ 72/72 checks passed (VERIFICATION_REPORT.txt)
```

---

## 9. Course Mapping Matrix for Faculty Viva

| Subject | Where It Appears | Code Proof |
|:---|:---|:---|
| **Computer Networks** | Wire format, UDP sockets, upstream resolution, RFC 1035 | `protocol_engine.cpp`, `upstream_engine.cpp`, `0xC00C` compression |
| **OSPP** | Thread pool, Reactor, cross-platform mutex, non-blocking I/O, VM cluster deploy | `main.cpp`, `vm_engine.cpp`, `std::thread`, VMware VMs |
| **DAA** | Dual-index cache: O(1) LRU + O(log N) Min-Heap | `storage_engine.cpp`, `list::splice`, `priority_queue` |
| **DBMS** | In-memory key-value store, composite key, write locking, snapshot durability | `storage_engine.cpp`, `dns_cache_<id>.db` |

---

## 10. Living Synchronization Protocol

1. **Continuous Updates:** Any code change → update this file immediately.
2. **State Preservation:** Every phase logged in `CONTEXT.md`.
3. **Git Commits:** Every milestone committed with descriptive message.
4. **Official Deliverables:** `DOCUMENTATION/` files remain preserved and never deleted.
5. **VM Deployment:** See `VM_CLUSTER_IMPLEMENTATION.md` for resumption checkpoints.
