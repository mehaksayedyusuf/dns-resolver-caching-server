# DNS Resolver & Caching Server — Distributed VM Multi-Cluster (C++)

[![Language: C++](https://img.shields.io/badge/Language-C%2B%2B14%2F17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![RFC: 1035 Compliant](https://img.shields.io/badge/RFC-1035%20DNS-blue.svg)](https://www.ietf.org/rfc/rfc1035.txt)
[![Platform: Cross-Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-orange.svg)]()
[![Deployment: VMware Workstation Pro](https://img.shields.io/badge/Deploy-VMware%20Workstation%20Pro-green.svg)]()
[![Academic Theme: Network Systems](https://img.shields.io/badge/Theme-Network%20Systems%20%26%20Tools-success.svg)]()
[![Team: 11](https://img.shields.io/badge/Team-11-purple.svg)]()

An RFC 1035 compliant **DNS Resolver and In-Memory Caching Server** built in **C++14/17**, deployed as a **Distributed VM Multi-Cluster Client-Server System** running on **VMware Workstation Pro**.

The server ingests raw UDP datagrams, unpacks DNS wire bitfields without external libraries, resolves queries concurrently using a worker thread pool, and accelerates lookups using a dual-indexed cache combining **O(1) LRU capacity eviction** with an **O(log N) Min-Heap for proactive TTL expiration**.

The architecture is fully **cross-platform** (Linux inside VMs + Windows on host), with multiple cluster nodes reachable from any client over the VMware virtual network.

---

## 🏛️ VM Multi-Cluster Architecture

```
+----------------------------------------------------------------------------------------------------------+
|                                    VMWARE WORKSTATION PRO HOST (LAPTOP)                                  |
|                                                                                                          |
|   CLIENT TIER                                                                                            |
|   • Host OS Terminal (Windows) → dig @192.168.150.10 -p 8053 google.com A                               |
|   • Python Cluster Client      → python scripts/cluster_client.py --mode failover                       |
|   • Client VM (Ubuntu Desktop) → RFC 1035 UDP queries                                                   |
|                                          |  Port 8053                                                   |
|                                          ▼                                                               |
|   VMNET8 VIRTUAL NETWORK SWITCH (NAT: 192.168.150.0/24)                                                 |
|           |  [Primary]                                     |  [Secondary / Failover]                    |
|           ▼                                                ▼                                             |
|   ┌─────────────────────────┐  Inter-Cluster    ┌─────────────────────────┐                             |
|   │  CLUSTER-ALPHA (VM 1)   │◄═══Cache Peering══►│  CLUSTER-BETA (VM 2)   │                             |
|   │  Ubuntu: 192.168.150.10 │                   │  Ubuntu: 192.168.150.20 │                             |
|   │  E1: Wire Parser        │                   │  E1: Wire Parser        │                             |
|   │  E2: Dual-Index Cache   │                   │  E2: Dual-Index Cache   │                             |
|   │  E3: Upstream Forwarder │                   │  E3: Upstream Forwarder │                             |
|   │  E4: 6-Stage VM         │                   │  E4: 6-Stage VM         │                             |
|   │  E5: Telemetry          │                   │  E5: Telemetry          │                             |
|   │  DB: dns_cache_alpha.db │                   │  DB: dns_cache_beta.db  │                             |
|   └─────────────────────────┘                   └─────────────────────────┘                             |
|                        \                              /                                                  |
|                         ▼  External Recursive DNS    ▼                                                  |
|            Primary: 8.8.8.8:53  |  Secondary (Failover): 1.1.1.1:53                                    |
+----------------------------------------------------------------------------------------------------------+
```

---

## ⚙️ 5-Engine Architecture

| Engine | Name | Owner | Academic Subject | Responsibilities |
|:---:|:---|:---|:---|:---|
| **E1** | Protocol Parsing & Wire Message Engine | Vageesh Mathad | Computer Networks | RFC 1035 wire unpacking, header bitfields, recursion-bounded pointer decompression (depth ≤ 16), `0xC00C` compression serializer |
| **E2** | In-Memory Storage & Cache Engine | Mehak Sayed Yusuf | DAA & DBMS | O(1) LRU (hash map + doubly-linked list), O(log N) Min-Heap proactive TTL sweep, dynamic TTL aging, reader-writer locking, disk snapshot (`dns_cache_<id>.db`) |
| **E3** | Upstream Resolution & Network Forwarding Engine | Ishani G | Computer Networks & OS | Non-blocking UDP to `8.8.8.8` / `1.1.1.1`, `select()` 2s timeout, exponential backoff, randomized TxID Kaminsky defense |
| **E4** | Query Execution VM & Telemetry Engine | Joel Biju | OSPP | 6-Stage pipeline (`FETCH→DECODE→LOOKUP→RESOLVE→COMMIT→EMIT`), condition routing (`HIT/MISS/EXPIRED/NXDOMAIN/SERVFAIL/FORMERR`), thread pool, real-time microsecond telemetry |
| **E5** | Telemetry Subsystem | Joel Biju | OSPP | Live QPS, hit ratio %, latency histogram, client IP dashboard, cluster-prefixed logging |

---

## ⚙️ 6-Stage VM Execution Pipeline

Every inbound client request executes deterministically through an explicit 6-stage instruction pipeline:

1. **FETCH:** Reactor ingests raw 512-byte UDP datagram on port `8053`, creates isolated buffer copy, pushes into worker queue.
2. **DECODE (E1):** Unpacks 12-byte header, decodes domain labels, enforces decompression depth ≤ 16.
3. **LOOKUP (E2):** Queries cache via composite key `(domain, qtype, qclass)`. **HIT** → recalculate decaying TTL and splice to MRU head.
4. **RESOLVE (E3):** On **MISS/EXPIRED** → forwards to `8.8.8.8:53` with 2s non-blocking timeout, validates 16-bit TxID.
5. **COMMIT (E2):** Inserts upstream answer into hash index + Min-Heap; evicts LRU tail if at capacity.
6. **EMIT (E1):** Serializes RFC 1035 response with `0xC00C` pointer compression, transmits via `sendto()`, logs microsecond latency.

---

## 🚀 Quick Start: Download ZIP and Run

### Prerequisites (on each VM — Ubuntu 22.04 LTS or 24.04 LTS)
```bash
sudo apt update && sudo apt install -y build-essential git python3
```

### Step 1 — Clone or Download
```bash
git clone https://github.com/mehaksayedyusuf/dns-resolver-caching-server.git
cd dns-resolver-caching-server
```
> Or: **Download ZIP** from GitHub → extract → `cd dns-resolver-caching-server`

### Step 2 — Build
```bash
make clean && make all
```
The compiled binary is at `bin/dns_server`.

### Step 3 — Run Cluster-Alpha (VM 1)
```bash
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-ALPHA -c 1000 -t 4
```

### Step 4 — Run Cluster-Beta (VM 2)
```bash
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-BETA -c 1000 -t 4
```

### Step 5 — Test from Host Laptop (Windows)
```powershell
# Single query to Cluster-Alpha
dig @192.168.150.10 -p 8053 google.com A

# Single query to Cluster-Beta
dig @192.168.150.20 -p 8053 google.com A

# Windows nslookup
nslookup -port=8053 google.com 192.168.150.10

# Automated multi-cluster + failover test
python scripts/cluster_client.py --primary 192.168.150.10:8053 --secondary 192.168.150.20:8053 --mode all
```

---

## 🖥️ Server CLI Options

| Flag | Default | Description |
|:---|:---|:---|
| `-b <ip>` / `--bind <ip>` | `0.0.0.0` | Network interface to listen on (use `0.0.0.0` for all VM interfaces) |
| `-p <port>` | `8053` | UDP port to listen on |
| `--cluster-id <name>` | `CLUSTER-ALPHA` | Cluster identity tag for telemetry and DB naming |
| `-c <capacity>` | `1000` | LRU cache max record capacity |
| `-t <threads>` | `4` | Worker thread pool size |
| `--peer <ip:port>` | _(none)_ | Peer cluster for inter-cluster query forwarding |
| `--db <file>` | `dns_cache_<cluster-id>.db` | Custom cache snapshot filename |

---

## 📂 Project Directory Structure

```
dns-resolver-caching-server/
├── Makefile                                   # Cross-platform build (auto-detects Linux vs Windows)
├── README.md                                  # This file
├── VM_CLUSTER_IMPLEMENTATION.md              # VM architecture blueprint & step-by-step deployment
├── VERIFICATION_REPORT.txt                   # 72/72 automated verification checks passed
├── CONTEXT.md                                # Project continuity & state tracker
├── DOCUMENTATION/                             # Official academic deliverables
│   ├── Team11_Chapter4_Sprint1_Handbook_Final.pdf
│   ├── Team11_Chapter4_Sprint1_Handbook_Submission.docx
│   ├── Team11_Sprint1.docx
│   ├── Gate_0_team11_document.docx
│   ├── Gate_0_team11_ppt.pptx
│   └── Team11_Gate1.pptx
├── diagrams/                                  # High-resolution architecture diagrams (PNG)
│   ├── engine_pipeline_diagram.png
│   ├── cache_storage_er_diagram.png
│   ├── dns_resolution_flowchart.png
│   ├── engine_e3_upstream_diagram.png
│   ├── engine_e5_telemetry_diagram.png
│   └── vm_pipeline_branching_diagram.png
├── include/                                   # C++ headers
│   ├── common.hpp                             # Cross-platform socket/mutex abstraction (Linux + Windows)
│   ├── protocol_engine.hpp                    # E1: Wire parser & serializer
│   ├── storage_engine.hpp                     # E2: Dual-index LRU + Min-Heap cache
│   ├── upstream_engine.hpp                    # E3: Upstream UDP forwarder
│   ├── vm_engine.hpp                          # E4: 6-stage VM pipeline
│   └── telemetry_engine.hpp                   # E5: Telemetry subsystem
├── src/                                       # C++ implementations
│   ├── main.cpp                               # Reactor loop, cluster CLI args, thread pool
│   ├── protocol_engine.cpp
│   ├── storage_engine.cpp
│   ├── upstream_engine.cpp                    # Cross-platform: fcntl/ioctlsocket, select()
│   ├── vm_engine.cpp
│   └── telemetry_engine.cpp
├── tests/                                     # Modular engine test suites
│   ├── test_protocol.cpp                      # E1: 10 tests
│   ├── test_cache.cpp                         # E2: 10 tests
│   ├── test_upstream.cpp                      # E3: 6 tests
│   └── test_vm_integration.cpp               # E4: 7 tests
├── benchmarks/
│   └── BENCHMARK_REPORT.md                   # Concurrency stress benchmark results
├── scripts/
│   ├── cluster_client.py                      # Multi-cluster test client (hit/miss/failover/loadbalance)
│   ├── deploy_vm_cluster.sh                   # One-click VM build & deploy script
│   ├── verify_implementation.py               # 72-check automated verification suite
│   └── benchmark_stress.py                   # Concurrent stress benchmark tool
└── docs/
    ├── CPP_MASTER_GUIDE.md                    # Viva defense guide & 30 Q&A
    └── CPP_MASTER_PLAN.md                     # Architectural specification
```

---

## 🧪 Running Tests

```bash
make test_protocol   # Engine E1 — 10 tests
make test_cache      # Engine E2 — 10 tests
make test_upstream   # Engine E3 — 6 tests
make test_vm         # Engine E4 — 7 tests
```

Or run the full verification suite (no compilation needed):
```bash
python scripts/verify_implementation.py
# → 72/72 checks passed (100.0%)
```

---

## 📊 Performance Benchmarks (Empirical)

| Metric | In-Memory Cache Hit | Upstream WAN Miss | Acceleration |
|:---|:---:|:---:|:---:|
| **Minimum Latency** | 0.143 ms | 103.59 ms | 724× |
| **Median (p50)** | 0.414 ms | 146.36 ms | 353× |
| **Mean** | 0.700 ms | 154.61 ms | 221× |
| **Peak Throughput** | **6,538 QPS** | — | — |

> Results from benchmark scenario: 20 concurrent client threads, 1,000 queries, pre-warmed cache.
> These are benchmark-specific measurements, not universal guarantees.

---

## 🔧 Cross-Platform Build Details

The `Makefile` auto-detects the OS:

| Platform | Socket API | Mutex | Compiler Flag | Binary |
|:---|:---|:---|:---|:---|
| **Linux (VM)** | POSIX sockets, `fcntl` | `std::shared_timed_mutex` | `-lpthread` | `bin/dns_server` |
| **Windows (Host)** | Winsock2 (`ws2_32`) | Win32 `SRWLOCK` | `-lws2_32` | `bin/dns_server.exe` |

---

## 📋 VMware Full Deployment Guide

See **[VM_CLUSTER_IMPLEMENTATION.md](VM_CLUSTER_IMPLEMENTATION.md)** for the complete step-by-step guide including:
- VMware Workstation Pro network configuration (VMnet8 NAT)
- Creating and cloning VM 1 & VM 2
- Setting static IPs
- Transferring files via Git or SCP
- Starting Cluster-Alpha and Cluster-Beta
- Testing with `dig`, `nslookup`, and the Python cluster client
- Failover simulation and load balancing tests

---

## 👥 Team 11 — PES University

| Name | Roll Number | Engine |
|:---|:---|:---|
| **Mehak Sayed Yusuf** | `01fe24bci012` | E2: In-Memory Storage & Cache Engine |
| **Vageesh Mathad** | `01fe24bci008` | E1: Protocol Parsing & Wire Message Engine |
| **Ishani G** | `01fe22bci009` | E3: Upstream Resolution & Network Forwarding Engine |
| **Joel Biju** | `01fe24bci021` | E4: Query Execution VM & Telemetry Engine |

**Faculty Guide:** Mr. Pavankumar S
**Theme:** Theme 4: Network Systems and Tools (Academic 3-Credit Mini Project)
