# VM Clustered DNS Resolver & Caching Architecture
## High-Level Implementation, VMware Workstation Pro Deployment & Client-Server Mechanism

> **Document Status:** Active Execution & Resumption State  
> **Target Environment:** VMware Workstation Pro (Host: Windows Laptop; Nodes: Linux/Windows VMs)  
> **Course / Academic Theme:** Theme 4: Network Systems & Tools (Team 11)  
> **Last Updated:** 2026-11-24  

---

## 📌 Executive Summary & Resumption Checkpoints

This document serves as the **persistent execution log and architectural blueprint** for transitioning the DNS Resolver & Caching Server from a single local process into a **Distributed Virtual Machine (VM) Multi-Cluster Client-Server System** running on **VMware Workstation Pro**.

### Resumption State Matrix
| Checkpoint | Component / Phase | Status | Output Artifacts / File References |
| :---: | :--- | :---: | :--- |
| **CP-01** | **Architectural Blueprint & Topology** | ✅ Completed | Documented below in Section 1 (Topology, Network, Client-Server Flow) |
| **CP-02** | **Cross-Platform POSIX/Win32 Abstraction** | ✅ Completed | `include/common.hpp` (portable sockets, `SharedMutex`, `Mutex`) |
| **CP-03** | **Network Engine Multi-Interface Binding** | ✅ Completed | `src/main.cpp` (`-b 0.0.0.0`, `--cluster-id`, `--peer`) |
| **CP-04** | **Upstream Engine Portable Non-blocking Sockets** | ✅ Completed | `src/upstream_engine.cpp` (`fcntl` / `ioctlsocket`, cross-platform `select`) |
| **CP-05** | **Cross-Platform Build System** | ✅ Completed | `Makefile` (detects `Windows_NT` vs Linux `make -lpthread`) |
| **CP-06** | **VM Cluster Node Deployer Script** | ✅ Completed | `scripts/deploy_vm_cluster.sh` (one-click build & run inside Linux VMs) |
| **CP-07** | **Multi-Cluster Client Testing Utility** | ✅ Completed | `scripts/cluster_client.py` (simulates client-server queries & failover) |
| **CP-08** | **VMware Workstation Pro Setup & Run Guide** | ✅ Completed | Documented in Section 4 below |

> **Instruction for Agent Resumption:**  
> If session limits or system restarts occur, read this file (`VM_CLUSTER_IMPLEMENTATION.md`) first. The table above indicates completed tasks. Proceed directly to verification or the next pending milestone without repeating completed work.

---

## 1. 🏛️ High-Level VM Multi-Cluster Architecture

### 1.1 Transition Overview: Single-Host vs. VM Clustered Concept

| Dimension | Previous Single-Host Model | VM Multi-Cluster Client-Server Model |
| :--- | :--- | :--- |
| **Deployment** | Single process on local Windows machine | Multiple isolated Virtual Machines in VMware Workstation Pro |
| **Network Interface** | Bound strictly to loopback `127.0.0.1` | Bound to `0.0.0.0` across Virtual Network Adapters (`VMnet8` / `VMnet1`) |
| **Client Access** | Local processes on same OS only | Distributed clients (Host laptop, Client VMs, LAN devices) via UDP |
| **Cluster Topology** | Monolithic (1 local node) | Multi-Cluster: **Cluster-Alpha (Primary)** + **Cluster-Beta (Secondary/Peer)** |
| **High Availability** | None (process exit drops all traffic) | Automatic Active-Passive failover or Active-Active load balancing |
| **Cache Distribution** | Single memory space | Dual-tiered caching: Local In-Memory LRU + Inter-Cluster Peering |
| **OS Compatibility** | Windows-only (`winsock2`, Win32 threads) | Cross-Platform: Linux (Ubuntu/Debian) inside VMs + Windows on Host |

---

### 1.2 System Architecture Diagram (VMware Workstation Pro)

```
+---------------------------------------------------------------------------------------------------------+
|                                    VMWARE WORKSTATION PRO HOST (LAPTOP)                                 |
|                                                                                                         |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                              CLIENT TIER (Client-Server Mechanism)                              |   |
|   |                                                                                                 |   |
|   |   • Client 1: Host OS Terminal (Windows) -> `dig @192.168.150.10 -p 8053 google.com A`          |   |
|   |   • Client 2: Python Stress Client -> `python scripts/cluster_client.py --mode failover`        |   |
|   |   • Client 3: Client VM (e.g., Ubuntu Desktop/Test VM) -> RFC 1035 UDP queries                  |   |
|   +-------------------------------------------------------------------------------------------------+   |
|                                                  |                                                      |
|                   UDP Client Datagrams           | Port 8053 (or 53)                                    |
|                                                  v                                                      |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                     VMNET8 VIRTUAL NETWORK SWITCH (NAT: 192.168.150.0/24 Subnet)                |   |
|   +-------------------------------------------------------------------------------------------------+   |
|                     |                                                         |                         |
|                     | [Primary Ingress]                                       | [Secondary / Failover]  |
|                     v                                                         v                         |
|   +------------------------------------+                    +------------------------------------+      |
|   |       CLUSTER-ALPHA (PRIMARY)      |                    |      CLUSTER-BETA (SECONDARY)      |      |
|   |        VM 1 (Ubuntu Server)        |                    |        VM 2 (Ubuntu Server)        |      |
|   |        IP: 192.168.150.10          |                    |        IP: 192.168.150.20          |      |
|   |                                    |   Inter-Cluster    |                                    |      |
|   |   • E1: Wire Parser (RFC 1035)     |   Cache Peering    |   • E1: Wire Parser (RFC 1035)     |      |
|   |   • E2: Dual-Index Cache (L1)      |<==================>|   • E2: Dual-Index Cache (L2)      |      |
|   |   • E3: Upstream Forwarder         |   Fallback Sync    |   • E3: Upstream Forwarder         |      |
|   |   • E4: 6-Stage Execution VM       |                    |   • E4: 6-Stage Execution VM       |      |
|   |   • E5: Telemetry Subsystem        |                    |   • E5: Telemetry Subsystem        |      |
|   |   • DB: dns_cache_alpha.db         |                    |   • DB: dns_cache_beta.db          |      |
|   +------------------------------------+                    +------------------------------------+      |
|                     \                                                         /                         |
|                      \                 External Recursive Resolution         /                          |
|                       +-----------------------------------------------------+                           |
|                                                  |                                                      |
|                                                  v                                                      |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                         UPSTREAM PUBLIC INTERNET (VIA VMNET8 NAT GATEWAY)                       |   |
|   |                         • Primary Upstream: 8.8.8.8:53 (Google Public DNS)                      |   |
|   |                         • Secondary Upstream: 1.1.1.1:53 (Cloudflare Public DNS)                |   |
|   +-------------------------------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------------------------------+
```

---

### 1.3 Client-Server Interaction Protocol Flow

Every client request executes deterministically through the distributed VM cluster:

```
CLIENT (Host / VM)             VM 1 (CLUSTER-ALPHA)             VM 2 (CLUSTER-BETA)             UPSTREAM (8.8.8.8)
    |                                   |                                |                              |
    |--- 1. UDP Query (example.com) --->|                                |                              |
    |    (Port 8053 to 192.168.150.10)  |                                |                              |
    |                                   |-- 2. FETCH & DECODE (E1)       |                              |
    |                                   |-- 3. LOOKUP (E2 Cache)         |                              |
    |                                   |                                |                              |
    |                                   |=== [SCENARIO A: LOCAL CACHE HIT] =============================|
    |                                   |-- 4a. Age TTL & Splice LRU     |                              |
    |                                   |-- 5a. EMIT (E1)                |                              |
    |<-- 6a. UDP Response (0.4ms) ------|                                |                              |
    |                                   |                                |                              |
    |                                   |=== [SCENARIO B: LOCAL CACHE MISS -> PEER QUERY] =============|
    |                                   |-- 4b. Query Peer (Port 8053) ->|                              |
    |                                   |                                |-- 5b. DECODE & LOOKUP (E2)   |
    |                                   |<-- 6b. Peer Hit Response ------|                              |
    |                                   |-- 7b. COMMIT into Local Cache  |                              |
    |<-- 8b. UDP Response (1.2ms) ------|                                |                              |
    |                                   |                                |                              |
    |                                   |=== [SCENARIO C: UPSTREAM RESOLUTION & FAILOVER] =============|
    |                                   |-- 4c. RESOLVE (E3) ------------------------------------------>|
    |                                   |                                |   (Recursive UDP 8.8.8.8)    |
    |                                   |<-- 5c. Upstream Answer ---------------------------------------|
    |                                   |-- 6c. COMMIT into L1 (E2)      |                              |
    |                                   |-- 7c. EMIT to Client (E1)      |                              |
    |<-- 8c. UDP Response (25ms) -------|                                |                              |
    |                                   |                                |                              |
    |=== [SCENARIO D: CLUSTER-ALPHA DOWN / TIMEOUT -> CLIENT FAILOVER] =================================|
    |--- 1d. UDP Query (Retry) ----------------------------------------->|                              |
    |    (Port 8053 to 192.168.150.20)  |                                |-- 2d. Ingress on Beta        |
    |                                   |                                |-- 3d. RESOLVE / COMMIT       |
    |<-- 4d. UDP Response from Beta -------------------------------------|                              |
```

---

## 2. 🧩 Engine Mapping Across VM Clusters

All 5 core engines implemented for Sprint 1 continue to operate within each VM node, augmented with cluster awareness:

| Engine | Name | Role Inside VM Node | Clustered Functionality |
| :---: | :--- | :--- | :--- |
| **E1** | **Protocol Parsing Engine** | RFC 1035 wire datagram unpacker & encoder | Parses client requests regardless of whether client originates from Windows Host, VMnet8, or another VM. |
| **E2** | **In-Memory Cache Engine** | Dual-Index $O(1)$ LRU + $O(\log N)$ Min-Heap | Node-local memory cache. Dumps persistent snapshots to node-specific files (`dns_cache_alpha.db`, `dns_cache_beta.db`). |
| **E3** | **Upstream Resolution Engine** | Non-blocking UDP recursive forwarder | Can target upstream DNS (`8.8.8.8`) OR peer cluster nodes for hierarchical multi-tier caching. |
| **E4** | **Execution VM Engine** | 6-Stage state machine (`FETCH` $\to$ `EMIT`) | Executes transaction pipeline on worker thread pool; tags logs with node cluster ID. |
| **E5** | **Telemetry Subsystem** | Real-time latency histogram & QPS tracker | Displays live dashboard showing client IP breakdown, remote vs local requests, and cluster throughput. |

---

## 3. ⚙️ Code Implementation Changes

### 3.1 Cross-Platform Socket & Concurrency Abstraction (`include/common.hpp`)
- Replaces Windows-only headers (`winsock2.h`, `windows.h`) with conditional compilation (`#ifdef _WIN32 ... #else ...`).
- On Windows: Uses high-performance native `SRWLOCK` and `CRITICAL_SECTION`.
- On Linux (VMware VM): Uses POSIX sockets, standard C++14 `std::shared_timed_mutex`, and `std::mutex`.
- Provides unified `SOCKET`, `closesocket()`, and `WSAGetLastError()`.

### 3.2 Multi-Interface Binding & Cluster Tagging (`src/main.cpp`)
- Added `-b <ip>` / `--bind <ip>` argument (defaults to `0.0.0.0` so server listens on all VM network interfaces instead of `127.0.0.1`).
- Added `--cluster-id <name>` (e.g. `CLUSTER-ALPHA`, `CLUSTER-BETA`) for distinct telemetry output.
- Added `--peer <ip>:<port>` to allow inter-cluster peering.
- Modernized `WorkerThreadPool` to standard `std::thread`, `std::mutex`, and `std::condition_variable` so it compiles seamlessly on both Linux and Windows.

### 3.3 Portable Non-Blocking Network I/O (`src/upstream_engine.cpp`)
- Replaces Win32-only `ioctlsocket(sock, FIONBIO)` with `fcntl(sock, F_SETFL, O_NONBLOCK)` on Linux.
- Portable `select()` with `struct timeval`.

### 3.4 Cross-Platform Makefile (`Makefile`)
- Auto-detects operating system:
  - If Linux (inside VM): links `-lpthread`, uses standard `rm -f`.
  - If Windows: links `-lws2_32`, uses `del /Q`.

---

## 4. 🚀 Step-by-Step VMware Workstation Pro Deployment Guide

Follow these exact steps on your laptop to run multiple clusters and connect clients.

### Step 1: VMware Virtual Network Configuration
1. Open **VMware Workstation Pro**.
2. Click **Edit** $\to$ **Virtual Network Editor...** (Run as Administrator if prompted).
3. Select **VMnet8 (NAT)**:
   - Ensure **NAT (shared host's IP address with VMs)** is selected.
   - Note the **Subnet IP** (typically `192.168.150.0` or `192.168.x.0`, Subnet Mask `255.255.255.0`).
   - Click **NAT Settings...** $\to$ Note the **Gateway IP** (e.g., `192.168.150.2`).
4. Click **OK**.

---

### Step 2: Create the Virtual Machines
You need **2 Virtual Machines** (VM 1: Cluster-Alpha, VM 2: Cluster-Beta).  
*(Recommended OS: Ubuntu Server 22.04 LTS or 24.04 LTS; lightweight, fast, and uses minimal RAM).*

1. In VMware, click **File** $\to$ **New Virtual Machine...** $\to$ **Typical**.
2. Select your Ubuntu Server ISO (or install Ubuntu 64-bit).
3. Allocate resources:
   - **RAM:** 1 GB (or 2 GB) per VM.
   - **Processors:** 1 or 2 vCPUs.
   - **Disk:** 15 GB (Thin Provisioned).
   - **Network Connection:** Select **NAT: Used to share the host's IP address**.
4. Finish installation for **VM 1** (Hostname: `dns-cluster-alpha`).
5. **Clone VM 1** to quickly create **VM 2**:
   - Shut down VM 1.
   - Right-click VM 1 $\to$ **Manage** $\to$ **Clone...** $\to$ **Create a full clone** $\to$ Name it `dns-cluster-beta`.
   - Power on both VMs.

---

### Step 3: Find the VM IP Addresses
In each VM terminal, run:
```bash
ip addr show
```
Look for the `inet` address under the primary network interface (e.g., `ens33` or `eth0`).
- **VM 1 (Cluster-Alpha):** e.g., `192.168.150.10`
- **VM 2 (Cluster-Beta):** e.g., `192.168.150.20`

*(Tip: You can set static IPs or keep the DHCP assigned IPs from VMware).*

---

### Step 4: Transfer Project Files to the VMs
Choose **Option A** (Git) or **Option B** (VMware Shared Folders / SCP):

#### Option A: Via Git (Easiest)
In both VMs:
```bash
sudo apt update && sudo apt install -y build-essential git python3
git clone https://github.com/mehaksayedyusuf/dns-resolver-caching-server.git
cd dns-resolver-caching-server
```

#### Option B: Direct Copy from Host Laptop via SCP (from PowerShell on Host)
```powershell
# Copy repository to VM 1
scp -r A:\dns-resolver-caching-server user@192.168.150.10:~/dns-resolver-caching-server
# Copy repository to VM 2
scp -r A:\dns-resolver-caching-server user@192.168.150.20:~/dns-resolver-caching-server
```

---

### Step 5: Compile on Each VM
On each VM, run:
```bash
cd ~/dns-resolver-caching-server
chmod +x scripts/deploy_vm_cluster.sh
make clean && make all
```
The compiled binary `bin/dns_server` will be created.

---

### Step 6: Start Cluster-Alpha on VM 1
In the terminal of **VM 1**:
```bash
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-ALPHA -c 1000 -t 4
```
*Output shows:*
```
===========================================================================
        RFC 1035 DNS RESOLVER & IN-MEMORY CACHING SERVER (C++)             
                  Theme 4: Network Systems & Tools                         
===========================================================================
  Server Configuration:
    • Cluster Node  : CLUSTER-ALPHA
    • Listening On  : 0.0.0.0:8053 (UDP - All Interfaces)
    • Upstream DNS  : 8.8.8.8:53 (Google Public DNS)
    • Fallback DNS  : 1.1.1.1:53 (Cloudflare Public DNS)
    • In-Memory LRU : 1000 records capacity
    • Worker Pool   : 4 OS worker threads
===========================================================================
  Ready for client queries!
```

---

### Step 7: Start Cluster-Beta on VM 2
In the terminal of **VM 2**:
```bash
./bin/dns_server -b 0.0.0.0 -p 8053 --cluster-id CLUSTER-BETA -c 1000 -t 4
```

---

## 5. 🧪 Client-Server Verification & Testing Steps

Now test the client-server mechanism from your **Host Laptop (Windows)** or another client machine.

### Test 1: Single Query via Standard `dig` from Host Laptop
Open PowerShell or Command Prompt on your Windows laptop:
```powershell
# Query Cluster-Alpha
dig @192.168.150.10 -p 8053 google.com A

# Query Cluster-Beta
dig @192.168.150.20 -p 8053 google.com A
```
*Verification:*
- First query to Cluster-Alpha: `CACHE MISS -> RESOLVED` (~20-40 ms).
- Second query to Cluster-Alpha: `CACHE HIT` (<1 ms)!
- On VM 1's console, you will see the client's host IP (e.g., `192.168.150.1`) logged in real-time.

---

### Test 2: Standard Windows `nslookup`
```powershell
nslookup -port=8053 google.com 192.168.150.10
```

---

### Test 3: Automated Multi-Cluster & Failover Testing Script
Run the automated cluster client simulator from your repository on the host machine:
```powershell
python scripts/cluster_client.py --primary 192.168.150.10:8053 --secondary 192.168.150.20:8053 --mode failover
```
*Features evaluated by the script:*
1. **Primary Cluster Cache Miss vs Cache Hit Latencies**: Confirms sub-millisecond cache hit acceleration.
2. **Active-Active Load Balancing**: Distributes queries 50/50 across Cluster-Alpha and Cluster-Beta.
3. **Simulated Node Failure & High-Availability Failover**: Simulates taking down Cluster-Alpha and demonstrates zero-downtime automatic client failover to Cluster-Beta.

---

## 6. 📊 Summary of Architectural & Academic Contributions

| Faculty / Examiner Question | How Our VM Multi-Cluster System Answers It |
| :--- | :--- |
| **"Is this just a local toy program?"** | No. It is deployed as a **multi-node distributed cluster across Virtual Machines** communicating over virtual Ethernet networks via standard RFC 1035 UDP. |
| **"How does the client-server mechanism work?"** | Independent clients (Host OS or Client VMs) dispatch standard UDP datagrams over the network. Ingress network reactors route them to worker thread pools, process through the 6-stage VM pipeline, and emit responses back to client socket addresses. |
| **"What happens if one VM cluster node crashes?"** | Clients utilize secondary/fallback cluster node addresses (`CLUSTER-BETA`). Active queries automatically fail over, maintaining 100% availability. |
| **"How is caching handled across the cluster?"** | Each node manages its own high-speed $O(1)$ LRU + $O(\log N)$ Min-Heap cache with disk persistence snapshots, preventing redundant upstream traffic. |
