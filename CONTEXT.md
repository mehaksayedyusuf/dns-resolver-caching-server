# PROJECT CONTINUITY CONTEXT: DNS RESOLVER & CACHING SERVER (C++)
## Distributed VM Multi-Cluster Architecture — Active Implementation

> **Purpose:** Authoritative live state, completed phases, architectural decisions, and next steps.
> If a session resets or token limit is reached, any new agent reads this file and resumes immediately.

---

## 1. Project Overview

- **Project Title:** DNS Resolver and Caching Server — Distributed VM Multi-Cluster
- **Course / Theme:** Theme 4: Network Systems and Tools (Academic 3-Credit Project)
- **Language Stack:** **C++ (C++14/C++17)** — cross-platform (Linux VMs + Windows host)
- **Deployment:** VMware Workstation Pro — 2 Ubuntu Server VMs (Cluster-Alpha + Cluster-Beta) on VMnet8 NAT subnet
- **Networking:** Cross-platform socket abstraction — POSIX (`<sys/socket.h>`, `fcntl`) on Linux, Winsock2 on Windows
- **Concurrency:** `std::thread`, `std::mutex`, `std::condition_variable` (Linux/Windows); Win32 `SRWLOCK` on Windows host
- **Official Submission State:** `DOCUMENTATION/` contains all submitted Gate 0 / Gate 1 / Sprint 1 files. **Never deleted.**

---

## 2. Architecture: VM Multi-Cluster Client-Server

```
HOST LAPTOP (VMware Workstation Pro)
  │
  ├─ VMnet8 NAT (192.168.150.0/24)
  │      │
  │      ├─ VM 1: Cluster-Alpha (192.168.150.10:8053)
  │      │    └─ bin/dns_server -b 0.0.0.0 --cluster-id CLUSTER-ALPHA
  │      │
  │      └─ VM 2: Cluster-Beta  (192.168.150.20:8053)
  │           └─ bin/dns_server -b 0.0.0.0 --cluster-id CLUSTER-BETA
  │
  ├─ CLIENT: dig @192.168.150.10 -p 8053 google.com A
  ├─ CLIENT: python scripts/cluster_client.py --mode all
  └─ UPSTREAM DNS: 8.8.8.8 (primary) / 1.1.1.1 (failover)
```

---

## 3. Phase Execution Status

| Phase | Description | Status |
|:---:|:---|:---:|
| **Phase 0** | Clean slate, Makefile, common.hpp, .gitignore | ✅ COMPLETED |
| **Phase 1** | Architecture diagrams (orbit, pipeline, cache, wire, threadpool) | ✅ COMPLETED |
| **Phase 2** | E1: Protocol Parsing & Wire Message Engine (10 tests) | ✅ COMPLETED |
| **Phase 3** | E2: In-Memory Storage & Cache Engine (10 tests) | ✅ COMPLETED |
| **Phase 4** | E3: Upstream Resolution & Network Forwarding Engine (6 tests) | ✅ COMPLETED |
| **Phase 5** | E4: Query Execution Virtual Machine (7 tests) | ✅ COMPLETED |
| **Phase 6** | E5: Telemetry Subsystem | ✅ COMPLETED |
| **Phase 7** | Server main loop, multi-threaded worker pool | ✅ COMPLETED |
| **Phase 8** | Viva guide, benchmarks, documentation | ✅ COMPLETED |
| **Phase 9** | **VM Multi-Cluster Architecture** — cross-platform POSIX/Win32 abstraction, cluster CLI args (`--bind`, `--cluster-id`, `--peer`), VMware deployment guide, cluster client, verification suite | ✅ COMPLETED |

---

## 4. Modified Files (VM Phase)

| File | Change |
|:---|:---|
| `include/common.hpp` | Cross-platform: `#ifdef _WIN32` POSIX socket typedefs, `std::shared_timed_mutex`, `std::mutex` |
| `src/main.cpp` | `--bind`, `--cluster-id`, `--peer`, `--db` CLI args; `std::thread` pool; POSIX `signal(SIGINT/SIGTERM)` |
| `src/upstream_engine.cpp` | `fcntl` vs `ioctlsocket`, portable `select(sock+1,...)`, `ECONNREFUSED` vs `WSAECONNRESET` |
| `Makefile` | `ifeq ($(OS),Windows_NT)` auto-detection; `-lpthread` on Linux, `-lws2_32` on Windows |

---

## 5. New Files (VM Phase)

| File | Purpose |
|:---|:---|
| `VM_CLUSTER_IMPLEMENTATION.md` | Master architecture doc + step-by-step VMware deployment guide |
| `scripts/deploy_vm_cluster.sh` | One-click bash deployer for Linux VMs |
| `scripts/cluster_client.py` | Multi-cluster test client (hit-miss/loadbalance/failover/all modes) |
| `scripts/verify_implementation.py` | 72-check automated verification suite |
| `VERIFICATION_REPORT.txt` | Output: 72/72 passed (100.0%) |
| `diagrams/*.png` | High-res: cache_storage_er, dns_resolution_flowchart, engine_e3_upstream, engine_e5_telemetry, vm_pipeline_branching |
| `DOCUMENTATION/Team11_Chapter4_Sprint1_Handbook_Final.pdf` | Sprint 1 handbook PDF |
| `DOCUMENTATION/Team11_Chapter4_Sprint1_Handbook_Submission.docx` | Sprint 1 handbook DOCX |

---

## 6. Workspace File Inventory

```
a:/dns-resolver-caching-server/
├── CONTEXT.md                             ← [THIS FILE]
├── Makefile                               ← Cross-platform (Linux -lpthread / Windows -lws2_32)
├── README.md                              ← Updated for VM architecture
├── VM_CLUSTER_IMPLEMENTATION.md          ← Full VM deployment blueprint
├── VERIFICATION_REPORT.txt               ← 72/72 checks passed
├── .gitignore
├── DOCUMENTATION/                         ← [PRESERVED — never delete]
│   ├── Gate_0_team11_document.docx
│   ├── Gate_0_team11_ppt.pptx
│   ├── Team11_Gate1.pptx
│   ├── Team11_S1_Student_Handbook.docx
│   ├── Team11_Sprint1.docx
│   ├── Team11_Chapter4_Sprint1_Handbook_Final.pdf
│   └── Team11_Chapter4_Sprint1_Handbook_Submission.docx
├── diagrams/                              ← High-res PNGs (300 DPI)
├── docs/
│   ├── CPP_MASTER_GUIDE.md               ← Viva defense guide (30 Q&A) — updated
│   └── CPP_MASTER_PLAN.md                ← Architectural spec — updated
├── include/
│   ├── common.hpp                         ← Cross-platform abstraction
│   ├── protocol_engine.hpp
│   ├── storage_engine.hpp
│   ├── upstream_engine.hpp
│   ├── vm_engine.hpp
│   └── telemetry_engine.hpp
├── src/
│   ├── main.cpp                           ← VM cluster entry point
│   ├── protocol_engine.cpp
│   ├── storage_engine.cpp
│   ├── upstream_engine.cpp                ← Cross-platform non-blocking UDP
│   ├── vm_engine.cpp
│   └── telemetry_engine.cpp
├── tests/
│   ├── test_protocol.cpp                  ← 10 tests (E1)
│   ├── test_cache.cpp                     ← 10 tests (E2)
│   ├── test_upstream.cpp                  ← 6 tests (E3)
│   └── test_vm_integration.cpp           ← 7 tests (E4)
├── benchmarks/
│   └── BENCHMARK_REPORT.md               ← Updated for VM cluster deployment
├── scripts/
│   ├── cluster_client.py                  ← Multi-cluster test client
│   ├── deploy_vm_cluster.sh               ← VM one-click deployer
│   ├── verify_implementation.py           ← 72-check suite
│   └── benchmark_stress.py               ← Concurrent stress tool
└── bin/
    └── dns_server  (or dns_server.exe)   ← Compiled binary (after make all)
```

---

## 7. Build & Run (Canonical Commands)

### On Linux VM (Ubuntu inside VMware):
```bash
git clone https://github.com/mehaksayedyusuf/dns-resolver-caching-server.git
cd dns-resolver-caching-server
make clean && make all
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-ALPHA -c 1000 -t 4
```

### On Windows (host — requires MinGW/MSYS2):
```powershell
mingw32-make all
.\bin\dns_server.exe -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-ALPHA
```

### Test from Host (PowerShell):
```powershell
dig @192.168.150.10 -p 8053 google.com A
python scripts/cluster_client.py --primary 192.168.150.10:8053 --secondary 192.168.150.20:8053 --mode all
```

---

## 8. Continuity Instructions for New Sessions

1. Open `VM_CLUSTER_IMPLEMENTATION.md` — read the **Resumption State Matrix** (all CP-01 through CP-08 are ✅ Completed).
2. Open this file (`CONTEXT.md`) — all phases through Phase 9 are ✅ Completed.
3. The repo is live at `https://github.com/mehaksayedyusuf/dns-resolver-caching-server` (branch: `main`, commit: `5797716`).
4. All work is done. No pending phases.

---

## 9. Team

| Name | Roll | Engine |
|:---|:---|:---|
| Vageesh Mathad | 01fe24bci008 | E1: Protocol Parsing |
| Mehak Sayed Yusuf | 01fe24bci012 | E2: Storage & Cache |
| Ishani G | 01fe22bci009 | E3: Upstream Resolution |
| Joel Biju | 01fe24bci021 | E4: Query Execution VM & Telemetry |

**Faculty Guide:** Mr. Pavankumar S | **Theme:** Theme 4: Network Systems and Tools
