#!/usr/bin/env python3
"""
High-Concurrency Stress Test & Performance Benchmark Tool
DNS Resolver & In-Memory Caching Server (C++)
Theme 4: Network Systems and Tools (Team 11)

Measures:
- Throughput (Queries Per Second / QPS)
- Sub-millisecond latency distribution (Min, Mean, p50, p90, p95, p99, Max)
- Concurrency scaling across multi-threaded client workers
- Cache Hit vs. Cache Miss latency differentiation
"""

import argparse
import json
import math
import random
import socket
import struct
import sys
import threading
import time
from typing import List, Dict, Any, Tuple

def build_dns_query(domain: str, txid: int = 0x1234) -> bytes:
    """Constructs a standard RFC 1035 UDP DNS Query datagram (Type A, Class IN)."""
    header = struct.pack("!HHHHHH", txid, 0x0100, 1, 0, 0, 0)  # RD=1, QD=1
    qname = b""
    for part in domain.strip(".").split("."):
        qname += bytes([len(part)]) + part.encode("ascii")
    qname += b"\x00"
    question = qname + struct.pack("!HH", 1, 1)  # Type A (1), Class IN (1)
    return header + question

def parse_dns_response(data: bytes) -> Dict[str, Any]:
    """Parses standard RFC 1035 UDP DNS response header."""
    if len(data) < 12:
        return {"valid": False}
    txid, flags, qdcount, ancount, nscount, arcount = struct.unpack("!HHHHHH", data[:12])
    qr = (flags >> 15) & 1
    rcode = flags & 0x000F
    return {
        "valid": True,
        "txid": txid,
        "qr": qr,
        "rcode": rcode,
        "qdcount": qdcount,
        "ancount": ancount,
        "raw_len": len(data)
    }

def calculate_percentile(sorted_data: List[float], p: float) -> float:
    """Calculates nearest-rank or linear interpolation percentile."""
    if not sorted_data:
        return 0.0
    k = (len(sorted_data) - 1) * (p / 100.0)
    f = math.floor(k)
    c = math.ceil(k)
    if f == c:
        return sorted_data[int(k)]
    d0 = sorted_data[int(f)] * (c - k)
    d1 = sorted_data[int(c)] * (k - f)
    return d0 + d1

class BenchmarkWorker(threading.Thread):
    def __init__(self, worker_id: int, server_addr: Tuple[str, int],
                 domains: List[str], num_queries: int, timeout: float,
                 results_list: List[Dict[str, Any]], lock: threading.Lock):
        super().__init__()
        self.worker_id = worker_id
        self.server_addr = server_addr
        self.domains = domains
        self.num_queries = num_queries
        self.timeout = timeout
        self.results_list = results_list
        self.lock = lock

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(self.timeout)
        local_results = []

        for i in range(self.num_queries):
            domain = random.choice(self.domains)
            txid = random.randint(1, 0xFFFE)
            packet = build_dns_query(domain, txid)

            t_start = time.perf_counter()
            success = False
            is_hit = False
            latency_ms = 0.0
            error_msg = ""

            try:
                sock.sendto(packet, self.server_addr)
                resp_data, _ = sock.recvfrom(512)
                t_end = time.perf_counter()
                latency_ms = (t_end - t_start) * 1000.0

                parsed = parse_dns_response(resp_data)
                if parsed.get("valid") and parsed.get("txid") == txid:
                    success = True
                    # If latency < 15ms, it was served directly from in-memory cache
                    is_hit = (latency_ms < 15.0)
                else:
                    error_msg = "TxID mismatch or malformed"
            except socket.timeout:
                t_end = time.perf_counter()
                latency_ms = (t_end - t_start) * 1000.0
                error_msg = "Socket Timeout"
            except Exception as ex:
                t_end = time.perf_counter()
                latency_ms = (t_end - t_start) * 1000.0
                error_msg = str(ex)

            local_results.append({
                "worker_id": self.worker_id,
                "domain": domain,
                "success": success,
                "is_hit": is_hit,
                "latency_ms": latency_ms,
                "error": error_msg
            })

        sock.close()
        with self.lock:
            self.results_list.extend(local_results)

def pre_warm_cache(server_addr: Tuple[str, int], domains: List[str]):
    """Sends warm-up queries to pre-populate cache before benchmarking."""
    print(f"[*] Pre-warming cache for {len(domains)} domains...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(4.0)
    for d in domains:
        try:
            txid = random.randint(1, 0xFFFE)
            sock.sendto(build_dns_query(d, txid), server_addr)
            data, _ = sock.recvfrom(512)
            parsed = parse_dns_response(data)
            print(f"    - Warmed '{d}': rcode={parsed['rcode']}, ancount={parsed['ancount']}")
        except Exception as e:
            print(f"    - Warning warming '{d}': {e}")
    sock.close()
    time.sleep(0.5)

def run_stress_benchmark(host: str, port: int, num_workers: int,
                         total_queries: int, mode: str, timeout: float) -> Dict[str, Any]:
    server_addr = (host, port)
    queries_per_worker = total_queries // num_workers
    actual_total = queries_per_worker * num_workers

    warm_domains = [
        "example.com",
        "google.com",
        "cloudflare.com",
        "github.com",
        "wikipedia.org",
        "microsoft.com"
    ]

    cold_domains = [
        "apple.com",
        "amazon.com",
        "netflix.com",
        "python.org",
        "pes.edu",
        "kernel.org"
    ]

    print("\n" + "=" * 76)
    print("       HIGH-CONCURRENCY STRESS & THROUGHPUT BENCHMARK (C++ SERVER)      ")
    print("=" * 76)
    print(f"Target Server      : {host}:{port}")
    print(f"Benchmark Mode     : {mode.upper()}")
    print(f"Client Threads     : {num_workers} concurrent workers")
    print(f"Total Requests     : {actual_total} queries ({queries_per_worker} per worker)")
    print(f"Per-query Timeout  : {timeout:.1f}s")

    if mode in ["hit", "both"]:
        pre_warm_cache(server_addr, warm_domains)
        selected_domains = warm_domains
    elif mode == "mixed":
        pre_warm_cache(server_addr, warm_domains[:3])
        # 80% warm, 20% cold
        selected_domains = warm_domains[:3] * 4 + cold_domains
    else:
        selected_domains = warm_domains

    print(f"\n[*] Firing {actual_total} concurrent queries across {num_workers} threads...")
    results: List[Dict[str, Any]] = []
    lock = threading.Lock()
    threads: List[BenchmarkWorker] = []

    wall_start = time.perf_counter()

    for w in range(num_workers):
        t = BenchmarkWorker(w, server_addr, selected_domains, queries_per_worker, timeout, results, lock)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    wall_end = time.perf_counter()
    wall_duration = wall_end - wall_start

    # Compute Statistics
    successful_queries = [r for r in results if r["success"]]
    failed_queries = [r for r in results if not r["success"]]
    hits = [r for r in successful_queries if r["is_hit"]]
    misses = [r for r in successful_queries if not r["is_hit"]]

    latencies_all = sorted([r["latency_ms"] for r in successful_queries])
    latencies_hit = sorted([r["latency_ms"] for r in hits])
    latencies_miss = sorted([r["latency_ms"] for r in misses])

    qps = len(successful_queries) / wall_duration if wall_duration > 0 else 0.0
    hit_ratio = (len(hits) / len(successful_queries) * 100.0) if successful_queries else 0.0

    stats = {
        "wall_duration_s": wall_duration,
        "total_sent": actual_total,
        "total_success": len(successful_queries),
        "total_failed": len(failed_queries),
        "qps": qps,
        "hit_ratio_percent": hit_ratio,
        "hits_count": len(hits),
        "misses_count": len(misses),
        "latencies_all": {
            "min": latencies_all[0] if latencies_all else 0.0,
            "mean": (sum(latencies_all) / len(latencies_all)) if latencies_all else 0.0,
            "p50": calculate_percentile(latencies_all, 50.0),
            "p90": calculate_percentile(latencies_all, 90.0),
            "p95": calculate_percentile(latencies_all, 95.0),
            "p99": calculate_percentile(latencies_all, 99.0),
            "max": latencies_all[-1] if latencies_all else 0.0
        },
        "latencies_hit": {
            "min": latencies_hit[0] if latencies_hit else 0.0,
            "mean": (sum(latencies_hit) / len(latencies_hit)) if latencies_hit else 0.0,
            "p50": calculate_percentile(latencies_hit, 50.0),
            "p95": calculate_percentile(latencies_hit, 95.0),
            "max": latencies_hit[-1] if latencies_hit else 0.0
        } if latencies_hit else None,
        "latencies_miss": {
            "min": latencies_miss[0] if latencies_miss else 0.0,
            "mean": (sum(latencies_miss) / len(latencies_miss)) if latencies_miss else 0.0,
            "p50": calculate_percentile(latencies_miss, 50.0),
            "max": latencies_miss[-1] if latencies_miss else 0.0
        } if latencies_miss else None
    }

    # Print Formatted Dashboard Report
    print("\n" + "=" * 76)
    print("                       BENCHMARK RESULTS REPORT                         ")
    print("=" * 76)
    print(f" Total Wall Duration    : {wall_duration:.3f} seconds")
    print(f" Requests Completed     : {len(successful_queries)} / {actual_total} ({len(successful_queries)/actual_total*100:.1f}%)")
    print(f" Requests Failed        : {len(failed_queries)}")
    print(f" Throughput (QPS)       : {qps:.2f} queries/sec")
    print(f" In-Memory Cache Hit %  : {hit_ratio:.2f}% ({len(hits)} hits, {len(misses)} misses)")
    print("-" * 76)
    print(" LATENCY PERCENTILES (Overall)")
    print(f"   • Min Latency        : {stats['latencies_all']['min']:.3f} ms")
    print(f"   • Mean Latency       : {stats['latencies_all']['mean']:.3f} ms")
    print(f"   • p50 (Median)       : {stats['latencies_all']['p50']:.3f} ms")
    print(f"   • p90 Latency        : {stats['latencies_all']['p90']:.3f} ms")
    print(f"   • p95 Latency        : {stats['latencies_all']['p95']:.3f} ms")
    print(f"   • p99 Latency        : {stats['latencies_all']['p99']:.3f} ms")
    print(f"   • Max Latency        : {stats['latencies_all']['max']:.3f} ms")
    print("-" * 76)

    if stats["latencies_hit"]:
        print(" IN-MEMORY CACHE HIT LATENCY (RAM O(1) Speed)")
        print(f"   • Min / Mean / Max   : {stats['latencies_hit']['min']:.3f} ms / {stats['latencies_hit']['mean']:.3f} ms / {stats['latencies_hit']['max']:.3f} ms")
        print(f"   • p50 / p95          : {stats['latencies_hit']['p50']:.3f} ms / {stats['latencies_hit']['p95']:.3f} ms")

    if stats["latencies_miss"]:
        print(" UPSTREAM RECURSIVE MISS LATENCY (WAN Network Roundtrip)")
        print(f"   • Min / Mean / Max   : {stats['latencies_miss']['min']:.3f} ms / {stats['latencies_miss']['mean']:.3f} ms / {stats['latencies_miss']['max']:.3f} ms")
        print(f"   • p50 (Median)       : {stats['latencies_miss']['p50']:.3f} ms")

    if stats["latencies_hit"] and stats["latencies_miss"] and stats["latencies_hit"]["mean"] > 0:
        speedup = stats["latencies_miss"]["mean"] / stats["latencies_hit"]["mean"]
        print("-" * 76)
        print(f" [+] Cache Acceleration : In-memory hits are {speedup:.1f}x FASTER than upstream misses!")

    print("=" * 76 + "\n")
    return stats

def main():
    parser = argparse.ArgumentParser(description="DNS Caching Server High-Concurrency Stress Benchmark")
    parser.add_argument("--host", default="127.0.0.1", help="Target server IP (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8053, help="Target server UDP port (default: 8053)")
    parser.add_argument("-w", "--workers", type=int, default=10, help="Number of concurrent worker threads (default: 10)")
    parser.add_argument("-n", "--queries", type=int, default=500, help="Total number of queries to fire (default: 500)")
    parser.add_argument("-m", "--mode", choices=["hit", "mixed"], default="hit", help="Benchmark mode: 'hit' (100%% cache hits) or 'mixed' (80%% hits, 20%% misses)")
    parser.add_argument("-t", "--timeout", type=float, default=3.0, help="Socket timeout in seconds (default: 3.0)")
    parser.add_argument("-o", "--output-json", help="Path to write JSON results summary")
    args = parser.parse_args()

    stats = run_stress_benchmark(
        host=args.host,
        port=args.port,
        num_workers=args.workers,
        total_queries=args.queries,
        mode=args.mode,
        timeout=args.timeout
    )

    if args.output_json:
        with open(args.output_json, "w") as f:
            json.dump(stats, f, indent=2)
        print(f"[+] Output JSON written to '{args.output_json}'.")

if __name__ == "__main__":
    main()
