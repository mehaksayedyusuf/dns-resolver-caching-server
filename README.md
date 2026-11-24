# DNS Resolver & Caching Server

**RFC 1035 compliant DNS resolver and in-memory caching server**, built from scratch in C++14 and deployed as a **distributed multi-cluster system** on VMware Workstation Pro. Clients on any machine in the virtual network can resolve domain names through the cluster with sub-millisecond cached responses and automatic failover between nodes.

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Engine Design](#engine-design)
- [Quick Start](#quick-start)
- [Server Options](#server-options)
- [Running Tests](#running-tests)
- [Benchmarks](#benchmarks)
- [Project Structure](#project-structure)
- [Team](#team)

---

## Overview

This project implements a full DNS resolution pipeline without any third-party libraries. It handles the complete lifecycle of a DNS request: parsing the binary wire format, checking an in-memory cache, forwarding to upstream resolvers on a cache miss, committing the result, and serializing a compliant response back to the client.

**Key technical decisions:**

- **Zero external dependencies** — all parsing, caching, and networking is implemented from first principles in C++.
- **Cross-platform** — the same codebase compiles on Linux (inside VMs) with POSIX sockets and on Windows with Winsock2, controlled entirely by `#ifdef _WIN32` guards in `include/common.hpp`.
- **Distributed deployment** — two Ubuntu Server VMs (Cluster-Alpha and Cluster-Beta) run on VMware Workstation Pro, reachable from the host or any other machine on VMnet8 NAT. Clients send standard RFC 1035 UDP queries; the cluster handles resolution, caching, and failover transparently.
- **Academic scope** — covers Computer Networks (wire protocol, UDP, upstream forwarding), Operating Systems (thread pool, non-blocking I/O, synchronization), Design and Analysis of Algorithms (O(1) LRU + O(log N) Min-Heap cache), and Database Systems (composite key store, snapshot persistence).

---

## Architecture

```
  ┌───────────────────────────────────────────────────────────────────┐
  │                   VMware Workstation Pro Host                      │
  │                                                                   │
  │  Client Tier                                                      │
  │  ─────────────────────────────────────────────────────────────── │
  │  · Host terminal:   dig @192.168.150.10 -p 8053 google.com A     │
  │  · Python client:   python scripts/cluster_client.py --mode all  │
  │  · Any VM on LAN:   standard RFC 1035 UDP to port 8053           │
  │                            │                                      │
  │                   VMnet8 NAT (192.168.150.0/24)                   │
  │                     ┌──────┴──────┐                               │
  │                     │             │                               │
  │          ┌──────────▼──┐    ┌─────▼──────────┐                   │
  │          │ CLUSTER-ALPHA│    │  CLUSTER-BETA  │                   │
  │          │ 192.168.150.10│   │ 192.168.150.20 │                   │
  │          │ Ubuntu Server │   │ Ubuntu Server  │                   │
  │          │               │   │                │                   │
  │          │ E1 Wire Parser│   │ E1 Wire Parser │                   │
  │          │ E2 Cache/LRU  │◄──►E2 Cache/LRU   │  (peer sync)      │
  │          │ E3 Upstream   │   │ E3 Upstream    │                   │
  │          │ E4 VM Engine  │   │ E4 VM Engine   │                   │
  │          │ E5 Telemetry  │   │ E5 Telemetry   │                   │
  │          └──────────────┘   └────────────────┘                   │
  │                     │                                             │
  │          Upstream DNS (via VMnet8 NAT gateway)                    │
  │          Primary: 8.8.8.8:53   Failover: 1.1.1.1:53             │
  └───────────────────────────────────────────────────────────────────┘
```

**Request flow through the cluster:**

1. Client sends a UDP datagram to `192.168.150.10:8053` (Cluster-Alpha).
2. The network reactor ingests it, creates an isolated buffer copy, and pushes the task to the worker thread pool.
3. The 6-stage VM pipeline executes: `FETCH → DECODE → LOOKUP → RESOLVE → COMMIT → EMIT`.
4. On a **cache hit**, the response is built from memory (~0.4 ms median latency).
5. On a **cache miss**, the query is forwarded to `8.8.8.8:53` over a non-blocking UDP socket, the answer is committed to cache, and the response is returned (~20–150 ms, depending on upstream).
6. If Cluster-Alpha is unreachable, the client retries against Cluster-Beta (`192.168.150.20:8053`) — no changes required on the server side.

---

## Engine Design

The system is decomposed into five engines, each owned by one team member and aligned to an academic subject:

| Engine | Name | Owner | Subject | Core Responsibility |
|:------:|:-----|:------|:--------|:--------------------|
| E1 | Protocol Parsing & Wire Message Engine | Vageesh Mathad | Computer Networks | RFC 1035 wire parser, 12-byte header bitfield extraction, recursion-bounded label decompression (depth ≤ 16, blocks pointer-loop DoS), `0xC00C` pointer compression serializer |
| E2 | In-Memory Storage & Cache Engine | Mehak Sayed Yusuf | DAA & DBMS | `std::unordered_map` + `std::list` for O(1) LRU eviction; `std::priority_queue` Min-Heap for O(log N) proactive TTL expiration; dynamic TTL aging; reader-writer locking; per-cluster disk snapshot |
| E3 | Upstream Resolution & Forwarding Engine | Ishani G | Computer Networks & OS | Non-blocking UDP (`fcntl`/`ioctlsocket`), `select()` with 2 s timeout, exponential retry backoff, automatic failover to `1.1.1.1`, randomized 16-bit TxID (Kaminsky poisoning defense) |
| E4 | Query Execution VM Engine | Joel Biju | OSPP | 6-stage instruction pipeline state machine, cross-platform `std::thread` worker pool, `std::mutex` + `std::condition_variable` task queue, cluster-identity-tagged logging |
| E5 | Telemetry Subsystem | Joel Biju | OSPP | Lock-free `std::atomic` QPS and hit counters, CAS-based min/max latency tracking, hit ratio percentage, live console dashboard |

**Cache data structure:**

```
Composite Key: (domain, qtype, qclass)
      │
      ├── std::unordered_map  ──────► O(1) lookup by key
      │         stores list iterator
      │
      └── std::list (doubly-linked)   O(1) LRU eviction
                 MRU Head ◄──────────── splice on cache hit
                 LRU Tail ◄──────────── evict when at capacity

      std::priority_queue (Min-Heap)
                 root = earliest ExpiresAt = O(1) expiry check
                 pop  = O(log N) proactive TTL sweep
```

**Thread safety:** `std::shared_timed_mutex` on Linux — multiple workers acquire shared locks for concurrent reads; exclusive lock only during cache write.

---

## Quick Start

### Prerequisites

On each Ubuntu Server VM (22.04 LTS or 24.04 LTS):
```
sudo apt update && sudo apt install -y build-essential git python3
```

### Step 1 — Clone the repository

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

### Step 3 — Start Cluster-Alpha (VM 1, IP: 192.168.150.10)

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
  Cache Capacity : 1000 records
  Worker Threads : 4
===========================================================================
  Server ready. Waiting for queries...
```

### Step 4 — Start Cluster-Beta (VM 2, IP: 192.168.150.20)

```
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-BETA -c 1000 -t 4
```

### Step 5 — Send a DNS query from the host machine

```
dig @192.168.150.10 -p 8053 google.com A
```

**First query (cache miss) — expected output:**
```
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 12345
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

;; ANSWER SECTION:
google.com.     299     IN      A       142.250.195.46

;; Query time: 24 msec
;; SERVER: 192.168.150.10#8053(192.168.150.10)
```

On the server console (VM 1):
```
[CLUSTER-ALPHA] [CACHE MISS]  google.com A from 192.168.150.1:52341  resolved in 24.1 ms via 8.8.8.8
```

**Second query (cache hit) — expected output:**
```
;; Query time: 0 msec
;; SERVER: 192.168.150.10#8053(192.168.150.10)
```

On the server console (VM 1):
```
[CLUSTER-ALPHA] [CACHE HIT]   google.com A from 192.168.150.1:52342  resolved in 0.4 ms  (TTL_rem: 298s)
```

### Step 6 — Run the multi-cluster test client

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
  Speedup: 58.5x on second query

[LOAD BALANCE TEST]
  Distributing 10 queries across both clusters...
  CLUSTER-ALPHA: 5 queries  avg 0.5 ms
  CLUSTER-BETA:  5 queries  avg 0.6 ms

[FAILOVER TEST]
  Sending to CLUSTER-ALPHA... timeout simulated
  Retrying on  CLUSTER-BETA... SUCCESS  18.7 ms
  Zero-downtime failover: PASS
```

### Step 7 — Windows nslookup (from host)

```
nslookup -port=8053 github.com 192.168.150.10
```

Expected output:
```
Server:   192.168.150.10
Address:  192.168.150.10#8053

Non-authoritative answer:
Name:    github.com
Address: 140.82.112.3
```

---

## Server Options

| Flag | Default | Description |
|:-----|:--------|:------------|
| `-b <ip>` / `--bind <ip>` | `0.0.0.0` | Network interface to listen on (`0.0.0.0` = all VM interfaces) |
| `-p <port>` | `8053` | UDP port |
| `--cluster-id <name>` | `CLUSTER-ALPHA` | Cluster identity tag for telemetry and DB naming |
| `-c <n>` | `1000` | LRU cache maximum record count |
| `-t <n>` | `4` | Worker thread pool size |
| `--peer <ip:port>` | _(none)_ | Peer cluster address for inter-cluster query forwarding |
| `--db <file>` | `dns_cache_<cluster-id>.db` | Custom cache snapshot file path |

---

## Running Tests

```
make test_protocol    # Engine E1 — 10 unit tests
make test_cache       # Engine E2 — 10 unit tests
make test_upstream    # Engine E3 —  6 unit tests
make test_vm          # Engine E4 —  7 integration tests
```

Expected output per suite:
```
[E1] Running 10 protocol wire tests...
  [PASS] Header parse: Transaction ID extraction
  [PASS] Header parse: Flag bitfield decoding
  [PASS] Label decompression: simple domain
  [PASS] Label decompression: pointer compression (0xC00C)
  [PASS] Pointer loop guard: depth > 16 -> FORMERR
  [PASS] Response serializer: 0xC00C compression
  ...
[E1] 10/10 PASSED
```

---

## Benchmarks

Benchmark results from a controlled stress run (20 concurrent client threads, 1,000 queries, pre-warmed cache). These are benchmark-specific measurements, not universal guarantees.

| Metric | Cache Hit (RAM) | Upstream Miss (WAN) | Ratio |
|:-------|:---------------:|:-------------------:|:-----:|
| Minimum latency | 0.143 ms | 103.6 ms | ~724x |
| Median latency | 0.414 ms | 146.4 ms | ~354x |
| Mean latency | 0.700 ms | 154.6 ms | ~221x |
| 95th percentile | 2.38 ms | 210.0 ms | ~88x |
| Peak throughput | **6,538 QPS** | — | — |

Run the benchmark yourself:
```
# Start server on VM first, then from host:
python scripts/benchmark_stress.py \
  --host 192.168.150.10 --port 8053 \
  -n 1000 -w 20 --mode hit
```

Full report: [`benchmarks/BENCHMARK_REPORT.md`](benchmarks/BENCHMARK_REPORT.md)

---

## Project Structure

```
dns-resolver-caching-server/
│
├── Makefile                          Cross-platform build (Linux -lpthread / Windows -lws2_32)
├── README.md                         This file
├── VM_CLUSTER_IMPLEMENTATION.md      Full VMware deployment guide with step-by-step instructions
├── CONTEXT.md                        Project state and architectural decisions log
│
├── include/
│   ├── common.hpp                    Cross-platform abstraction (#ifdef _WIN32 guards)
│   ├── protocol_engine.hpp           E1: Wire parser interface
│   ├── storage_engine.hpp            E2: Cache interface (LRU + Min-Heap)
│   ├── upstream_engine.hpp           E3: Upstream forwarder interface
│   ├── vm_engine.hpp                 E4: 6-stage pipeline interface
│   └── telemetry_engine.hpp          E5: Telemetry interface
│
├── src/
│   ├── main.cpp                      Reactor loop, cluster CLI args, worker thread pool
│   ├── protocol_engine.cpp           RFC 1035 wire parse, label decompression, serializer
│   ├── storage_engine.cpp            LRU splice, Min-Heap TTL, snapshot persistence
│   ├── upstream_engine.cpp           Non-blocking UDP, select(), retry, TxID validation
│   ├── vm_engine.cpp                 6-stage pipeline executor
│   └── telemetry_engine.cpp          Atomic counters, latency histogram, dashboard
│
├── tests/
│   ├── test_protocol.cpp             10 unit tests — Engine E1
│   ├── test_cache.cpp                10 unit tests — Engine E2
│   ├── test_upstream.cpp              6 unit tests — Engine E3
│   └── test_vm_integration.cpp        7 integration tests — Engine E4
│
├── scripts/
│   ├── cluster_client.py             Multi-cluster test client (hit/miss/failover/loadbalance)
│   ├── deploy_vm_cluster.sh          One-click bash deployer for Linux VMs
│   ├── benchmark_stress.py           Concurrent stress benchmark tool
│   ├── live_client.py                Interactive manual query client
│   └── verify_implementation.py      72-check automated verification suite
│
├── benchmarks/
│   ├── BENCHMARK_REPORT.md           Detailed stress test analysis and latency percentiles
│   ├── benchmark_hit.json            Raw results — pure cache hit scenario
│   ├── benchmark_hit_1000.json       Raw results — 1,000 queries, 20 threads
│   └── benchmark_mixed.json          Raw results — 80/20 mixed workload
│
├── diagrams/
│   ├── system_orbit_diagram.png      System orbit architecture
│   ├── engine_pipeline_diagram.png   6-stage VM instruction pipeline
│   ├── cache_lru_minheap_diagram.png Dual-index cache architecture
│   ├── wire_protocol_format_diagram.png  RFC 1035 header & decompression
│   ├── concurrency_threadpool_diagram.png  Reactor & thread pool
│   ├── dns_resolution_flowchart.png  Full query resolution flowchart
│   ├── vm_pipeline_branching_diagram.png   VM pipeline branch conditions
│   ├── engine_e3_upstream_diagram.png      E3 upstream engine detail
│   ├── engine_e5_telemetry_diagram.png     E5 telemetry subsystem
│   └── cache_storage_er_diagram.png        Cache storage ER diagram
│
├── docs/
│   ├── CPP_MASTER_GUIDE.md           Viva defense guide — 30 Q&A, engine walkthroughs
│   └── CPP_MASTER_PLAN.md            Architectural specification and phase breakdown
│
└── DOCUMENTATION/                    Official academic submissions (do not modify)
    ├── Gate_0_team11_document.docx
    ├── Gate_0_team11_ppt.pptx
    ├── Team11_Gate1.pptx
    ├── Team11_S1_Student_Handbook.docx
    ├── Team11_Sprint1.docx
    ├── Team11_Chapter4_Sprint1_Handbook_Final.pdf
    └── Team11_Chapter4_Sprint1_Handbook_Submission.docx
```

---

## Team

**Team 11 — PES University**
Theme 4: Network Systems and Tools (Academic 3-Credit Mini Project)
Faculty Guide: Mr. Pavankumar S

| Name | Roll Number | Engine |
|:-----|:------------|:-------|
| Vageesh Mathad | 01fe24bci008 | E1: Protocol Parsing & Wire Message Engine |
| Mehak Sayed Yusuf | 01fe24bci012 | E2: In-Memory Storage & Cache Engine |
| Ishani G | 01fe22bci009 | E3: Upstream Resolution & Forwarding Engine |
| Joel Biju | 01fe24bci021 | E4: Query Execution VM & E5: Telemetry |
