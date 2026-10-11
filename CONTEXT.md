# PROJECT CONTINUITY CONTEXT: DNS RESOLVER & CACHING SERVER (C++)

> **Purpose:** This file maintains the authoritative live state, completed phases, architectural decisions, and next steps for the project. If a conversation session resets or token limit is reached, any new agent can read this file and resume immediately without re-doing or re-asking anything.

---

## 1. Project Overview & Pivot Record
- **Project Title:** DNS Resolver and Caching Server
- **Course / Theme:** Theme 4: Network Systems and Tools (Academic 3-Credit Project)
- **Language Stack:** **C++ (C++14/C++17)** with MinGW GCC (`g++`) on Windows
- **Core Networking:** Windows Sockets 2 (`ws2_32`), non-blocking UDP datagram sockets
- **Concurrency & Locks:** Multi-threaded worker pool, Win32 `SRWLOCK` (Reader-Writer) and `CRITICAL_SECTION` RAII wrappers
- **Official Submission State:** [DOCUMENTATION/](file:///a:/dns-resolver-caching-server/DOCUMENTATION/) contains the official submitted files (`Gate_0_team11_document.docx`, `Gate_0_team11_ppt.pptx`, `Team11_Gate1.pptx`, `Team11_S1_Student_Handbook.docx`). **This folder is strictly protected and never deleted.**

---

## 2. Phase Execution Status Checklist

| Phase | Description | Status | Commit SHA / Tag |
| :---: | :--- | :---: | :--- |
| **Phase 0** | **Clean Slate & Environment Setup**<br>• Deleted old Go code, tests, and temporary scratch scripts.<br>• Verified and preserved `DOCUMENTATION/`.<br>• Configured `.gitignore` and `Makefile`.<br>• Created `include/common.hpp` (DNS RFC 1035 constants, RAII sync locks). | **COMPLETED** | `phase-0-clean-slate` |
| **Phase 1** | **Visual Architecture & Orbit Diagrams**<br>• Rendered `system_orbit_diagram.png` (Core VM Orbit model).<br>• Rendered `engine_pipeline_diagram.png` (6-stage VM instruction pipeline).<br>• Rendered `cache_lru_minheap_diagram.png` (Dual-index O(1) LRU + O(log N) TTL). | **COMPLETED** | `phase-1-diagrams` |
| **Phase 2** | **Engine E1: Protocol Parsing & Wire Message Engine**<br>• `include/protocol_engine.hpp` and `src/protocol_engine.cpp`.<br>• RFC 1035 wire unpacking, header bitfields, 16-level loop-guarded label decompression, response serializer.<br>• Unit tests in `tests/test_protocol.cpp` (10/10 tests passed). | **COMPLETED** | `feat(phase-2)` |
| **Phase 3** | **Engine E2: In-Memory Storage & Cache Engine**<br>• `include/storage_engine.hpp` and `src/storage_engine.cpp`.<br>• Dual index: Hash map + Doubly-linked list for O(1) LRU; Min-Heap for O(log N) proactive TTL sweep.<br>• Dynamic TTL aging, Reader-Writer locking (`SRWLOCK`), disk snapshot (`dns_cache.db`).<br>• Unit tests in `tests/test_cache.cpp` (8/8 tests passed). | **COMPLETED** | `feat(phase-3)` |
| **Phase 4** | **Engine E3: Upstream Resolution & Network Forwarding Engine**<br>• `include/upstream_engine.hpp` and `src/upstream_engine.cpp`.<br>• Non-blocking UDP socket forwarder to 8.8.8.8, timeout thresholds, exponential retries, Kaminsky randomized TxID defense.<br>• Unit tests in `tests/test_upstream.cpp` (6/6 tests passed). | **COMPLETED** | `feat(phase-4)` |
| **Phase 5** | **Engine E4: Query Execution Virtual Machine**<br>• `include/vm_engine.hpp` and `src/vm_engine.cpp`.<br>• 6-stage instruction pipeline (`FETCH->DECODE->LOOKUP->RESOLVE->COMMIT->EMIT`).<br>• Condition handling: `HIT`, `MISS`, `EXPIRED`, `NXDOMAIN` (RFC 2308 negative cache), `SERVFAIL`, `FORMERR`.<br>• Integration tests in `tests/test_vm_integration.cpp` (6/6 tests passed). | **COMPLETED** | `feat(phase-5)` |
| **Phase 6** | **Telemetry & Performance Analytics Subsystem**<br>• `include/telemetry_engine.hpp` and `src/telemetry_engine.cpp`.<br>• Real-time hit ratio tracking, microsecond latency histograms, QPS counters, live dashboard. | **COMPLETED** | `feat(phase-5)` |
| **Phase 7** | **Server Main Entry, Multi-Threaded Worker Pool & Live Validation**<br>• `src/main.cpp` listening on UDP `127.0.0.1:8053`.<br>• Worker thread pool with queue concurrency.<br>• Live validation with Python/nslookup (121.26ms miss ➔ 0.85ms hit, 140x speedup). | **COMPLETED** | `feat(phase-7)` |
| **Phase 8** | **New C++ Master Guide & Project Report**<br>• `docs/CPP_MASTER_GUIDE.md` (Theory, Engine Walkthroughs, 30 Viva Q&As, CN/OSPP/DAA/DBMS mapping, Test & Live benchmarks). | **COMPLETED** | `docs(phase-8)` |

---

## 3. Workspace File Inventory

```
a:/dns-resolver-caching-server/
├── CONTEXT.md                                 # [THIS FILE] Project continuity & state tracker
├── Makefile                                   # MinGW C++ build system (all, test, clean)
├── README.md                                  # Repository overview
├── .gitignore                                 # Ignores *.o, *.exe, bin/, *.db
├── DOCUMENTATION/                             # [PRESERVED DELIVERABLES & LIVING GUIDE]
│   ├── CPP_MASTER_PLAN_GUIDE.md               # [LIVING MASTER PLAN] Synchronized on every change
│   ├── Gate_0_team11_document.docx
│   ├── Gate_0_team11_ppt.pptx
│   ├── Team11_Gate1.pptx
│   └── Team11_S1_Student_Handbook.docx
├── diagrams/                                  # [PRIMARY HIGH-RES ASSETS - 300 DPI]
│   ├── system_orbit_diagram.png               # Orbit architecture graphic (766 KB)
│   ├── engine_pipeline_diagram.png            # 6-stage instruction pipeline graphic (348 KB)
│   ├── cache_lru_minheap_diagram.png          # Dual-index O(1) LRU + O(log N) TTL (345 KB)
│   ├── wire_protocol_format_diagram.png       # 12-byte header + pointer loop guard (390 KB)
│   └── concurrency_threadpool_diagram.png     # Reactor, Thread Pool & SRWLOCK (407 KB)
├── docs/
│   ├── CPP_MASTER_PLAN.md                     # Synchronized Master Plan
│   ├── CPP_MASTER_GUIDE.md                    # Comprehensive Viva Defense Report & Q&A
│   └── diagrams/                              # Mirrored architectural assets
├── include/
│   ├── common.hpp                             # RFC 1035 constants, RAII Mutex & SRWLOCK wrappers
│   ├── protocol_engine.hpp                    # E1: Wire Parser & Serializer definitions
│   ├── storage_engine.hpp                     # E2: Dual-index LRU + Min-Heap Cache definitions
│   ├── upstream_engine.hpp                    # E3: Upstream UDP Forwarder & Kaminsky defense
│   ├── vm_engine.hpp                          # E4: Query Execution VM Pipeline definitions
│   └── telemetry_engine.hpp                   # Telemetry & KPI metric tracking definitions
├── src/
│   ├── protocol_engine.cpp                    # E1: Wire unpacking, label decompression
│   ├── storage_engine.cpp                     # E2: LRU splice, TTL minheap, disk snapshot
│   ├── upstream_engine.cpp                    # E3: Non-blocking UDP select(), retry, TxID
│   ├── vm_engine.cpp                          # E4: 6-stage pipeline executor
│   ├── telemetry_engine.cpp                   # Telemetry analytics & dashboard
│   └── main.cpp                               # Server entry, Reactor loop, thread pool
├── tests/
│   ├── test_protocol.cpp                      # 10 unit tests for Engine E1 (Passed)
│   ├── test_cache.cpp                         # 8 unit tests for Engine E2 (Passed)
│   ├── test_upstream.cpp                      # 6 unit tests for Engine E3 (Passed)
│   └── test_vm_integration.cpp               # 6 integration tests for Engine E4 (Passed)
├── benchmarks/                                # Concurrency stress test reports & metrics
│   ├── BENCHMARK_REPORT.md                    # Stress test analysis & latency percentiles
│   ├── benchmark_hit.json                     # Pure cache hit 500-query benchmark results
│   ├── benchmark_hit_1000.json                # Pure cache hit 1000-query (6538 QPS) results
│   └── benchmark_mixed.json                   # 80/20 mixed workload benchmark results
├── scripts/
│   ├── live_client.py                         # Single-query live verification tool
│   └── benchmark_stress.py                    # Multi-threaded concurrent stress test tool
└── bin/
    └── dns_server.exe                         # Production binary executable
```

---

## 4. Architectural Summary of the 4 S1 Engines

1. **E1: Protocol Parsing & Wire Message Engine** (Owner: Vageesh Mathad)
   - Unpacks RFC 1035 byte streams from raw UDP buffers.
   - Decompresses 14-bit pointer offsets (`0xC000`) with cycle-depth limit 16 to block pointer loops.
   - Serializes outgoing DNS responses with pointer compression.
2. **E2: In-Memory Storage & Cache Engine** (Owner: Mehak Sayed Yusuf)
   - Dual-index architecture: `std::unordered_map` for $O(1)$ key lookup + `std::list` for recency order.
   - `std::priority_queue` Min-Heap keyed by `ExpiresAt` for $O(\log N)$ proactive TTL expiration.
   - Dynamic TTL recalculation: $TTL_{rem} = TTL_{orig} - \Delta t$.
   - Multi-reader single-writer locking via Win32 `SRWLOCK`.
   - Snapshot dump to `dns_cache.db` on graceful shutdown and boot warm-up.
3. **E3: Upstream Resolution & Network Forwarding Engine** (Owner: Ishani G)
   - Recursive forwarding to upstream DNS (8.8.8.8, 1.1.1.1) over UDP.
   - Non-blocking socket polling with timeout (`select`) and exponential retry.
   - Cryptographic TxID randomization to defeat Kaminsky cache poisoning.
4. **E4: Query Execution Virtual Machine & Telemetry Engine** (Owner: Joel Biju)
   - State machine coordinating the 6 instruction stages (`FETCH ➔ DECODE ➔ LOOKUP ➔ RESOLVE ➔ COMMIT ➔ EMIT`).
   - Query condition handler: `HIT`, `MISS`, `EXPIRED`, `NXDOMAIN`, `SERVFAIL`, `FORMERR`.
   - Thread pool for concurrent worker dispatch.
   - Real-time telemetry: hit/miss counters, hit ratio %, and microsecond latency tracking.

---

## 5. Continuity Instructions for New Sessions
If context limits occur:
1. Open this file (`CONTEXT.md`).
2. Identify the first phase marked as `*Pending*` in Section 2.
3. Proceed directly to implement that phase using the C++ architecture defined herein.
4. Test thoroughly, update `CONTEXT.md`, and commit changes.
