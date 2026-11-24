# C++ MASTER PLAN & ARCHITECTURAL SPECIFICATION GUIDE
## DNS Resolver & Caching Server — Distributed VM Multi-Cluster
### Academic Theme 4: Network Systems & Tools (Team 11)

> **LIVING DOCUMENT:** Whenever code is modified, this file must be updated immediately to remain synchronized with the codebase.

---

## 1. 30-Second Viva Pitch

> *"We built an RFC 1035 compliant DNS Resolver and Caching Server from scratch in C++ (C++14), deployed as a distributed multi-cluster system on VMware Workstation Pro. It listens on UDP `0.0.0.0:8053` — reachable from any client on the VMnet8 subnet. It parses DNS wire messages byte-by-byte, resolves queries via a cross-platform worker thread pool, and accelerates lookups using a dual-indexed cache: O(1) LRU (hash + doubly-linked list) and O(log N) Min-Heap proactive TTL expiration. Cache misses forward to `8.8.8.8`/`1.1.1.1` over non-blocking UDP with randomized TxID defense. In benchmark runs, cache hits resolved in ~0.414 ms median vs ~154 ms upstream. The system runs on two Ubuntu Server VMs (Cluster-Alpha and Cluster-Beta) with identity-tagged telemetry and automatic client failover."*

---

## 2. VM Multi-Cluster Architecture (The Orbit Model)

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
         │   FETCH → DECODE → LOOKUP → RESOLVE         │
         │        → COMMIT → EMIT                      │
         └──────┬──────────────┬────────────────┬──────┘
                │              │                │
        Parse/Serialize  Cache/Evict     Forward/Failover
                ▼              ▼                ▼
     ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
     │   ENGINE E1  │ │   ENGINE E2  │ │   ENGINE E3  │
     │  Wire Parser │ │  Cache & LRU │ │  Upstream    │
     │  (Vageesh M) │ │  (Mehak S.Y.)│ │  (Ishani G)  │
     └──────────────┘ └──────┬───────┘ └──────────────┘
                             │                │
                   dns_cache_<id>.db    8.8.8.8 / 1.1.1.1
```

---

## 3. 6-Stage VM Execution Pipeline

Every incoming DNS request executes through E4's 6-stage state machine:

| Stage | Name | Engine | Action |
|:---:|:---|:---:|:---|
| 1 | **FETCH** | E4 | Ingest raw UDP datagram, copy buffer, timestamp, log client IP |
| 2 | **DECODE** | E1 | Parse 12-byte header bitfields, decompress QNAME (depth ≤ 16) |
| 3 | **LOOKUP** | E2 | Query cache by `(domain, qtype, qclass)` → HIT/MISS/EXPIRED |
| 4 | **RESOLVE** | E3 | On MISS/EXPIRED: forward to `8.8.8.8:53`, validate TxID |
| 5 | **COMMIT** | E2 | Insert into hash+LRU+Min-Heap, evict LRU tail if full |
| 6 | **EMIT** | E1 | Serialize with `0xC00C`, `sendto()` client, log telemetry |

---

## 4. Dual-Index Cache Architecture (Engine E2)

- **O(1) LRU Capacity Eviction:**
  `std::unordered_map<CompositeKey, list::iterator>` + `std::list` doubly-linked list.
  `std::list::splice(begin(), list_, it)` = O(1) MRU promotion. `pop_back()` = O(1) tail eviction.

- **O(log N) Proactive TTL Expiration:**
  `std::priority_queue` Min-Heap keyed by `ExpiresAt = now + TTL`. Root = earliest expiring entry. Lazy deletion via version counters — stale heap nodes silently discarded on pop.

- **Dynamic TTL Aging:** `TTL_rem = ExpiresAt - now`. Clients receive accurate countdown.

- **Thread Safety (cross-platform):**
  Linux: `std::shared_timed_mutex` (`lock_shared` for reads, `lock` for writes).
  Windows: `SRWLOCK` (`AcquireSRWLockShared` / `AcquireSRWLockExclusive`).

- **Per-Cluster Persistence:** `dns_cache_CLUSTER-ALPHA.db`, `dns_cache_CLUSTER-BETA.db`.

---

## 5. Wire Format & Decompression (Engine E1)

```
DNS 12-Byte Header:
+-------------------------------+-------------------------------+
|        Transaction ID         |QR| Opcode |AA|TC|RD|RA|Z|RCODE|
+-------------------------------+-------------------------------+
|          QDCOUNT (1)          |          ANCOUNT (0/1)        |
+-------------------------------+-------------------------------+
|          NSCOUNT (0)          |          ARCOUNT (0)          |
+-------------------------------+-------------------------------+
```

**Compression Pointer:** Top 2 bits = `11` → 14-bit byte offset. Max depth = 16. Exceeding = FORMERR.

---

## 6. Cross-Platform Abstraction (`include/common.hpp`)

```cpp
#ifdef _WIN32
  #include <winsock2.h>
  // SRWLOCK, CRITICAL_SECTION, WSAStartup
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  typedef int SOCKET;
  #define closesocket close
  #define INVALID_SOCKET (-1)
  using SharedMutex = std::shared_timed_mutex;
  using Mutex = std::mutex;
#endif
```

---

## 7. Five-Engine Decomposition

| Engine | Name & Owner | Subject | Core C++ |
|:---:|:---|:---|:---|
| **E1** | Protocol Parsing *(Vageesh Mathad)* | Computer Networks | RFC 1035 wire, decompression depth ≤ 16, `0xC00C` |
| **E2** | Storage & Cache *(Mehak Sayed Yusuf)* | DAA & DBMS | O(1) LRU, O(log N) Min-Heap, `shared_timed_mutex` |
| **E3** | Upstream Engine *(Ishani G)* | CN & OS | `fcntl`/`ioctlsocket`, `select(sock+1,...)`, `mt19937` TxID |
| **E4** | VM Engine *(Joel Biju)* | OSPP | `std::thread` pool, `--cluster-id`, signal handling |
| **E5** | Telemetry *(Joel Biju)* | OSPP | `std::atomic`, CAS min/max, QPS, hit ratio % |

---

## 8. Implementation Phases (All Completed)

```
PHASE 0: Clean Slate & Makefile              ✅ COMPLETED
PHASE 1: Architecture Diagrams              ✅ COMPLETED
PHASE 2: Engine E1 (Protocol Parsing)       ✅ COMPLETED
PHASE 3: Engine E2 (Cache & LRU)            ✅ COMPLETED
PHASE 4: Engine E3 (Upstream Forwarding)    ✅ COMPLETED
PHASE 5: Engine E4 (VM Pipeline)            ✅ COMPLETED
PHASE 6: Telemetry Subsystem                ✅ COMPLETED
PHASE 7: Main Server Loop                   ✅ COMPLETED
PHASE 8: Viva Guide & Docs                  ✅ COMPLETED
PHASE 9: VM Multi-Cluster Architecture      ✅ COMPLETED
         → cross-platform POSIX/Win32 abstraction
         → --bind, --cluster-id, --peer CLI args
         → VMware Workstation Pro deployment guide
         → cluster_client.py & deploy_vm_cluster.sh
         → 72/72 verification checks passed
```

---

## 9. Course Mapping for Faculty Viva

| Subject | Where in Code | Mechanism |
|:---|:---|:---|
| **Computer Networks** | `protocol_engine.cpp`, `upstream_engine.cpp` | Wire parsing, UDP, RFC 1035, `0xC00C` |
| **OSPP** | `main.cpp`, `vm_engine.cpp`, VMware VMs | Thread pool, Reactor, non-blocking I/O, cluster deployment |
| **DAA** | `storage_engine.cpp` | O(1) LRU `splice`, O(log N) Min-Heap |
| **DBMS** | `storage_engine.cpp`, `dns_cache_<id>.db` | Composite key, write lock, snapshot durability |

---

## 10. Living Synchronization Protocol

1. Any code change → update this file immediately.
2. Every phase logged in `CONTEXT.md`.
3. Every milestone committed with descriptive git message.
4. `DOCUMENTATION/` files are preserved and never deleted.
5. For VM deployment resumption → see `VM_CLUSTER_IMPLEMENTATION.md`.
