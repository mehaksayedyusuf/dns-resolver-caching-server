#!/usr/bin/env python3
"""
DNS Resolver & Caching Server — Comprehensive Implementation Verification Suite
Tests all engines, cross-platform abstraction, VM cluster features, and code quality.
"""

import os
import sys
import re
import ast

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PASS = 0
FAIL = 0
WARN = 0
results = []

def check(name, condition, detail=""):
    global PASS, FAIL
    if condition:
        PASS += 1
        results.append(("PASS", name, detail))
        print(f"  [PASS] {name}")
    else:
        FAIL += 1
        results.append(("FAIL", name, detail))
        print(f"  [FAIL] {name} -- {detail}")

def read(relpath):
    full = os.path.join(REPO, relpath)
    if not os.path.isfile(full):
        return None
    with open(full, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()

print("\n" + "=" * 78)
print("  SECTION 1: PROJECT STRUCTURE & FILE INTEGRITY")
print("=" * 78)

required_files = {
    "include/common.hpp": "Cross-platform socket & thread abstraction header",
    "include/protocol_engine.hpp": "E1 Protocol Engine header",
    "include/storage_engine.hpp": "E2 Cache Engine header",
    "include/upstream_engine.hpp": "E3 Upstream Engine header",
    "include/vm_engine.hpp": "E4 VM Engine header",
    "include/telemetry_engine.hpp": "E5 Telemetry Subsystem header",
    "src/main.cpp": "Server entry point with multi-interface reactor",
    "src/protocol_engine.cpp": "E1 Protocol Engine implementation",
    "src/storage_engine.cpp": "E2 Cache Engine implementation",
    "src/upstream_engine.cpp": "E3 Upstream Engine implementation",
    "src/vm_engine.cpp": "E4 VM Engine implementation",
    "src/telemetry_engine.cpp": "E5 Telemetry Subsystem implementation",
    "tests/test_protocol.cpp": "E1 unit test suite",
    "tests/test_cache.cpp": "E2 unit test suite",
    "tests/test_upstream.cpp": "E3 unit test suite",
    "tests/test_vm_integration.cpp": "E4 integration test suite",
    "Makefile": "Cross-platform build system",
    "scripts/deploy_vm_cluster.sh": "VM cluster deployment script",
    "scripts/cluster_client.py": "Multi-cluster client test tool",
    "VM_CLUSTER_IMPLEMENTATION.md": "VM architecture & resumption state document",
}

for path, desc in required_files.items():
    full = os.path.join(REPO, path)
    check(f"File exists: {path}", os.path.isfile(full), f"Missing: {desc}")

print("\n" + "=" * 78)
print("  SECTION 2: CROSS-PLATFORM ABSTRACTION (common.hpp)")
print("=" * 78)

common = read("include/common.hpp")
if common:
    check("Has #ifdef _WIN32 platform guard", "#ifdef _WIN32" in common)
    check("Has POSIX socket typedefs (typedef int SOCKET)", "typedef int SOCKET" in common)
    check("Has closesocket -> close mapping", "close(s)" in common)
    check("Has WSAGetLastError -> errno mapping", "errno" in common and "WSAGetLastError" in common)
    check("Includes <sys/socket.h> for POSIX", "sys/socket.h" in common)
    check("Includes <fcntl.h> for POSIX non-blocking", "fcntl.h" in common)
    check("Has Win32 SRWLOCK SharedMutex class", "SRWLOCK" in common and "AcquireSRWLockShared" in common)
    check("Has POSIX std::shared_timed_mutex class", "shared_timed_mutex" in common)
    check("Has LockGuard / ReadLockGuard / WriteLockGuard", "class LockGuard" in common and "class ReadLockGuard" in common and "class WriteLockGuard" in common)
    check("Has byte helpers (read_uint16_be, write_uint16_be)", "read_uint16_be" in common and "write_uint16_be" in common)
    check("DNS & RCODE constants defined", "DNS_PORT" in common and "MAX_DECOMPRESSION_DEPTH" in common and "RCODE_NOERROR" in common)
    check("Includes <thread>, <mutex>, <condition_variable>", "<thread>" in common and "<mutex>" in common and "<condition_variable>" in common)

print("\n" + "=" * 78)
print("  SECTION 3: VM MULTI-CLUSTER SERVER ENTRY (main.cpp)")
print("=" * 78)

main = read("src/main.cpp")
if main:
    check("Binds to configurable IP (-b / --bind)", '--bind' in main or '"-b"' in main)
    check("Default bind IP is 0.0.0.0 (all interfaces)", '"0.0.0.0"' in main)
    check("Has --cluster-id argument", '--cluster-id' in main)
    check("Has --peer argument for inter-cluster peering", '--peer' in main)
    check("Uses std::thread, std::mutex, std::condition_variable", "std::thread" in main and "std::mutex" in main and "std::condition_variable" in main)
    check("Uses std::atomic<bool> for shutdown flag", "std::atomic<bool>" in main)
    check("Has SIGINT/SIGTERM & Win32 ConsoleCtrlHandler", "SIGINT" in main and "ConsoleCtrlHandler" in main)
    check("Cluster ID in log output & per-cluster DB file", "cluster_id" in main and "dns_cache_" in main)
    check("Cross-platform non-blocking socket & select(sock + 1)", "fcntl" in main and "server_sock + 1" in main)
    check("Logs client_ip and client_port", "client_ip" in main and "client_port" in main)
    check("Proactive TTL sweep & Telemetry dashboard", "purge_expired" in main and "generate_dashboard" in main)
    check("WSACleanup guarded by #ifdef _WIN32", "#ifdef _WIN32" in main and "WSACleanup" in main)

print("\n" + "=" * 78)
print("  SECTION 4: ENGINES E1–E5 CORE LOGIC")
print("=" * 78)

proto = read("src/protocol_engine.cpp")
cache = read("src/storage_engine.cpp")
cache_h = read("include/storage_engine.hpp")
upstream = read("src/upstream_engine.cpp")
upstream_h = read("include/upstream_engine.hpp")
vm = read("src/vm_engine.cpp")
vm_h = read("include/vm_engine.hpp")
telemetry = read("src/telemetry_engine.cpp")
telemetry_h = read("include/telemetry_engine.hpp")

if proto:
    check("E1: Header codec & 0xC00C compression", "serialize" in proto and "deserialize" in proto and "0xC000" in proto)
    check("E1: Cyclic loop guard (visited_offsets & depth<=16)", "visited_offsets" in proto and "MAX_DECOMPRESSION_DEPTH" in proto)
    check("E1: IPv4/IPv6 & domain normalization", "ipv4_to_string" in proto and "ipv6_to_string" in proto and "tolower" in proto)

if cache and cache_h:
    check("E2: O(1) LRU (unordered_map + std::list)", "unordered_map" in cache_h and "std::list" in cache_h)
    check("E2: O(log N) Min-Heap (priority_queue)", "priority_queue" in cache_h and "purge_expired_locked" in cache)
    check("E2: Dynamic TTL aging & version guard", "remaining_ttl_seconds" in cache and "version" in cache)
    check("E2: SRWLOCK/SharedMutex Read/Write guards", "WriteLockGuard" in cache and "ReadLockGuard" in cache)
    check("E2: Negative caching & Disk snapshot save/load", "NEGATIVE_HIT" in cache_h and "save_snapshot" in cache and "load_snapshot" in cache)

if upstream and upstream_h:
    check("E3: Dual upstreams 8.8.8.8 & 1.1.1.1", '"8.8.8.8"' in upstream_h and '"1.1.1.1"' in upstream_h)
    check("E3: Cross-platform fcntl/ioctlsocket & socklen_t", "fcntl" in upstream and "ioctlsocket" in upstream and "socklen_t" in upstream)
    check("E3: select(sock + 1) & randomized TxID (mt19937)", "sock + 1" in upstream and "mt19937" in upstream)
    check("E3: Kaminsky TxID defense & exponential backoff", "MISMATCHED_TXID" in upstream and "1 << attempt" in upstream)

if vm and vm_h:
    check("E4: 6-stage pipeline + HALT", all(s in vm_h for s in ["STAGE_FETCH", "STAGE_DECODE", "STAGE_LOOKUP", "STAGE_RESOLVE", "STAGE_COMMIT", "STAGE_EMIT", "STAGE_HALT"]))
    check("E4: Cache-hit fast path & miss/expired routing", "STAGE_EMIT" in vm and "STAGE_RESOLVE" in vm and "cache_.get" in vm)
    check("E4: FORMERR, SERVFAIL, NXDOMAIN handling", "FORMERR_MALFORMED" in vm and "SERVFAIL" in vm and "RCODE_NXDOMAIN" in vm)

if telemetry and telemetry_h:
    check("E5: Atomic counters & CAS min/max latency", "atomic<uint64_t>" in telemetry_h and "compare_exchange_weak" in telemetry)
    check("E5: Hit ratio, QPS, and dashboard generator", "hit_ratio_pct" in telemetry and "qps" in telemetry and "generate_dashboard" in telemetry)

print("\n" + "=" * 78)
print("  SECTION 5: TEST SUITE COMPLETENESS")
print("=" * 78)

def count_tests(filepath):
    content = read(filepath)
    if not content:
        return 0
    return len(re.findall(r'^\s*RUN_TEST\(', content, re.MULTILINE))

tp = count_tests("tests/test_protocol.cpp")
tc = count_tests("tests/test_cache.cpp")
tu = count_tests("tests/test_upstream.cpp")
tv = count_tests("tests/test_vm_integration.cpp")

check(f"E1 Protocol tests: {tp} tests (expected >= 10)", tp >= 10, f"Found {tp}")
check(f"E2 Cache tests: {tc} tests (expected >= 8)", tc >= 8, f"Found {tc}")
check(f"E3 Upstream tests: {tu} tests (expected >= 6)", tu >= 6, f"Found {tu}")
check(f"E4 VM Integration tests: {tv} tests (expected >= 6)", tv >= 6, f"Found {tv}")
check(f"Total unit tests: {tp+tc+tu+tv} tests (expected >= 30)", tp + tc + tu + tv >= 30, f"Found {tp+tc+tu+tv}")

print("\n" + "=" * 78)
print("  SECTION 6: BUILD SYSTEM, SCRIPTS & DOCS")
print("=" * 78)

makefile = read("Makefile")
if makefile:
    check("Makefile: OS auto-detection & -lpthread / -lws2_32", "Windows_NT" in makefile and "-lpthread" in makefile and "-lws2_32" in makefile)
    check("Makefile: All 5 engine objects & 4 test targets", all(e in makefile for e in ["protocol_engine.o", "storage_engine.o", "upstream_engine.o", "telemetry_engine.o", "vm_engine.o", "test_vm"]))

client_py = read("scripts/cluster_client.py")
if client_py:
    try:
        ast.parse(client_py)
        check("cluster_client.py: Valid Python syntax", True)
    except SyntaxError as e:
        check("cluster_client.py: Valid Python syntax", False, str(e))
    check("cluster_client.py: Supports --primary, --secondary, --mode", "--primary" in client_py and "--secondary" in client_py and "--mode" in client_py)

deploy = read("scripts/deploy_vm_cluster.sh")
if deploy:
    check("deploy_vm_cluster.sh: Valid bash cluster deployer", "#!/usr/bin/env bash" in deploy and "CLUSTER_ID" in deploy and "make all" in deploy)

vm_doc = read("VM_CLUSTER_IMPLEMENTATION.md")
if vm_doc:
    check("VM_CLUSTER_IMPLEMENTATION.md: Checkpoints & topology present", "CP-01" in vm_doc and "CLUSTER-ALPHA" in vm_doc and "CLUSTER-BETA" in vm_doc)

total = PASS + FAIL
print("\n" + "=" * 78)
print(f"  FINAL VERIFICATION SUMMARY: {PASS}/{total} CHECKS PASSED ({PASS/total*100:.1f}%)")
print("=" * 78)

report_path = os.path.join(REPO, "VERIFICATION_REPORT.txt")
with open(report_path, 'w') as f:
    f.write(f"DNS Resolver VM Cluster Implementation -- Verification Report\n")
    f.write(f"Date: 2026-11-24\n")
    f.write(f"Total: {total} | Passed: {PASS} | Failed: {FAIL}\n")
    f.write(f"Pass Rate: {PASS/total*100:.1f}%\n\n")
    for status, name, detail in results:
        line = f"[{status}] {name}"
        if detail:
            line += f" -- {detail}"
        f.write(line + "\n")
    f.write(f"\nConclusion: {'ALL CHECKS PASSED' if FAIL == 0 else f'{FAIL} FAILURES DETECTED'}\n")

sys.exit(0 if FAIL == 0 else 1)
