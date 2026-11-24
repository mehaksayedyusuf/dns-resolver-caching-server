#!/usr/bin/env python3
"""
DNS Multi-Cluster Client Simulation & Failover Verification Tool
Theme 4: Network Systems & Tools (Team 11)

Tests the Client-Server mechanism across VMware Workstation Pro VM clusters.
Supports:
  1. Cache-Miss vs Cache-Hit verification across remote VM nodes.
  2. Active-Active Round-Robin Load Balancing across Cluster-A and Cluster-B.
  3. Active-Passive High-Availability Failover (simulating primary cluster failure).
  4. Concurrent multi-client query benchmarking.

Requires Python 3.6+ (No third-party libraries needed; uses standard socket library).
"""

import socket
import struct
import time
import argparse
import random
import sys

def build_dns_query(domain: str, qtype: int = 1, txid: int = None) -> bytes:
    """Builds RFC 1035 UDP DNS query datagram."""
    if txid is None:
        txid = random.randint(1, 65535)
    flags = 0x0100  # Recursion Desired (RD)
    qdcount = 1
    ancount = 0
    nscount = 0
    arcount = 0
    header = struct.pack("!HHHHHH", txid, flags, qdcount, ancount, nscount, arcount)

    question = b""
    for part in domain.strip(".").split("."):
        encoded = part.encode("ascii")
        question += bytes([len(encoded)]) + encoded
    question += b"\x00"  # Root null byte
    question += struct.pack("!HH", qtype, 1)  # QTYPE, QCLASS=IN

    return header + question

def parse_dns_response(data: bytes):
    """Parses basic RFC 1035 response header and answers."""
    if len(data) < 12:
        return {"error": "Packet shorter than 12-byte DNS header"}

    txid, flags, qdcount, ancount, nscount, arcount = struct.unpack("!HHHHHH", data[:12])
    rcode = flags & 0x000F
    qr = (flags >> 15) & 1

    answers = []
    # Simple parse to extract answer count and status
    return {
        "txid": txid,
        "qr": qr,
        "rcode": rcode,
        "ancount": ancount,
        "raw_len": len(data)
    }

def send_query(server_ip: str, server_port: int, domain: str, timeout_sec: float = 2.0):
    """Dispatches UDP query to cluster node and measures latency."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout_sec)
    query_bytes = build_dns_query(domain)

    t0 = time.perf_counter()
    try:
        sock.sendto(query_bytes, (server_ip, server_port))
        resp_data, addr = sock.recvfrom(1024)
        t1 = time.perf_counter()
        lat_ms = (t1 - t0) * 1000.0
        parsed = parse_dns_response(resp_data)
        return {
            "success": True,
            "latency_ms": lat_ms,
            "server": f"{server_ip}:{server_port}",
            "ancount": parsed.get("ancount", 0),
            "rcode": parsed.get("rcode", 0),
            "bytes": len(resp_data)
        }
    except socket.timeout:
        t1 = time.perf_counter()
        return {
            "success": False,
            "latency_ms": (t1 - t0) * 1000.0,
            "server": f"{server_ip}:{server_port}",
            "error": "TIMEOUT"
        }
    except Exception as e:
        return {
            "success": False,
            "latency_ms": 0.0,
            "server": f"{server_ip}:{server_port}",
            "error": str(e)
        }
    finally:
        sock.close()

def parse_endpoint(ep_str: str, default_port: int = 8053):
    parts = ep_str.strip().split(":")
    ip = parts[0]
    port = int(parts[1]) if len(parts) > 1 else default_port
    return ip, port

def run_hit_miss_test(primary_ip: str, primary_port: int, domains: list):
    print("\n" + "=" * 75)
    print(f" [TEST 1] Cache-Miss vs Cache-Hit Evaluation on Node ({primary_ip}:{primary_port})")
    print("=" * 75)
    print(f"{'Domain':<24} | {'Pass 1 (Miss)':<15} | {'Pass 2 (Hit)':<15} | {'Speedup':<10}")
    print("-" * 75)

    for d in domains:
        # Pass 1: Potential Cache Miss
        r1 = send_query(primary_ip, primary_port, d)
        lat1 = f"{r1['latency_ms']:.2f} ms" if r1['success'] else "TIMEOUT"

        # Pass 2: Expected Cache Hit
        time.sleep(0.05)
        r2 = send_query(primary_ip, primary_port, d)
        lat2 = f"{r2['latency_ms']:.2f} ms" if r2['success'] else "TIMEOUT"

        if r1['success'] and r2['success'] and r2['latency_ms'] > 0:
            speedup = f"{r1['latency_ms'] / max(r2['latency_ms'], 0.05):.1f}x"
        else:
            speedup = "N/A"

        print(f"{d:<24} | {lat1:<15} | {lat2:<15} | {speedup:<10}")

def run_load_balance_test(p_ip: str, p_port: int, s_ip: str, s_port: int, domains: list, count: int = 10):
    print("\n" + "=" * 75)
    print(f" [TEST 2] Active-Active Round-Robin Load Balancing across Clusters")
    print(f"   Node A: {p_ip}:{p_port}  <--->  Node B: {s_ip}:{s_port}")
    print("=" * 75)
    print(f"{'Query #':<8} | {'Target Cluster Node':<24} | {'Domain':<20} | {'Latency':<10} | {'Status'}")
    print("-" * 75)

    nodes = [(p_ip, p_port, "CLUSTER-ALPHA"), (s_ip, s_port, "CLUSTER-BETA")]
    node_a_hits = 0
    node_b_hits = 0

    for i in range(1, count + 1):
        target_ip, target_port, label = nodes[i % 2]
        d = random.choice(domains)
        res = send_query(target_ip, target_port, d)
        status = "OK (200)" if res['success'] else f"FAIL ({res.get('error')})"
        lat_str = f"{res['latency_ms']:.2f} ms" if res['success'] else "---"
        if res['success']:
            if label == "CLUSTER-ALPHA":
                node_a_hits += 1
            else:
                node_b_hits += 1
        print(f"{i:<8} | {label + ' (' + target_ip + ')':<24} | {d:<20} | {lat_str:<10} | {status}")
        time.sleep(0.05)

    print("-" * 75)
    print(f" Load Distribution: CLUSTER-ALPHA = {node_a_hits}/{count} | CLUSTER-BETA = {node_b_hits}/{count}")

def run_failover_test(p_ip: str, p_port: int, s_ip: str, s_port: int, test_domain: str = "wikipedia.org"):
    print("\n" + "=" * 75)
    print(" [TEST 3] High-Availability Automatic Client Failover Simulation")
    print(f"   Primary Cluster Node  : {p_ip}:{p_port}")
    print(f"   Secondary (Failover)  : {s_ip}:{s_port}")
    print("=" * 75)

    print(f"1. Sending query to Primary Cluster ({p_ip}:{p_port})...")
    r1 = send_query(p_ip, p_port, test_domain, timeout_sec=1.5)
    if r1['success']:
        print(f"   -> Primary Node responded in {r1['latency_ms']:.2f} ms. (Online)")
    else:
        print(f"   -> Primary Node unreachable / timed out ({r1.get('error')}).")

    print(f"\n2. Simulating Client Failover Policy (Primary -> Secondary)...")
    t_start = time.perf_counter()
    # Try primary with short timeout
    r_try = send_query(p_ip, p_port, test_domain, timeout_sec=0.5)
    if not r_try['success']:
        print(f"   [FAILOVER DETECTED] Primary unavailable! Redirecting query to Secondary ({s_ip}:{s_port})...")
        r_failover = send_query(s_ip, s_port, test_domain, timeout_sec=2.0)
        t_total = (time.perf_counter() - t_start) * 1000.0
        if r_failover['success']:
            print(f"   [FAILOVER SUCCESS] Secondary answered in {r_failover['latency_ms']:.2f} ms (Total failover time: {t_total:.2f} ms)!")
        else:
            print(f"   [FAILOVER FAILED] Secondary also unreachable: {r_failover.get('error')}")
    else:
        print(f"   Primary is currently UP ({r_try['latency_ms']:.2f} ms). To test failover, stop the process on VM 1 and re-run.")

def main():
    parser = argparse.ArgumentParser(description="DNS Multi-Cluster Client Simulation & Failover Verification")
    parser.add_argument("--primary", default="127.0.0.1:8053", help="Primary Cluster Node IP:Port (e.g. 192.168.150.10:8053)")
    parser.add_argument("--secondary", default="127.0.0.1:8054", help="Secondary Cluster Node IP:Port (e.g. 192.168.150.20:8053)")
    parser.add_argument("--mode", choices=["all", "hit-miss", "loadbalance", "failover"], default="all", help="Test mode")
    parser.add_argument("--domains", default="google.com,github.com,cloudflare.com,amazon.com,pes.edu", help="Comma-separated test domains")

    args = parser.parse_args()
    p_ip, p_port = parse_endpoint(args.primary)
    s_ip, s_port = parse_endpoint(args.secondary)
    domain_list = [d.strip() for d in args.domains.split(",") if d.strip()]

    print("\n" + "#" * 75)
    print("      RFC 1035 DNS RESOLVER MULTI-CLUSTER CLIENT TEST HARNESS        ")
    print("      Theme 4: Network Systems & Tools (VMware Workstation Pro)        ")
    print("#" * 75)
    print(f"  Primary Cluster Node   : {p_ip}:{p_port}")
    print(f"  Secondary Cluster Node : {s_ip}:{s_port}")
    print(f"  Test Domains           : {', '.join(domain_list)}")
    print("#" * 75)

    if args.mode in ["all", "hit-miss"]:
        run_hit_miss_test(p_ip, p_port, domain_list)

    if args.mode in ["all", "loadbalance"]:
        run_load_balance_test(p_ip, p_port, s_ip, s_port, domain_list, count=10)

    if args.mode in ["all", "failover"]:
        run_failover_test(p_ip, p_port, s_ip, s_port, test_domain=domain_list[0])

    print("\n" + "=" * 75)
    print("  All Multi-Cluster Client-Server Verification Tests Completed.")
    print("=" * 75 + "\n")

if __name__ == "__main__":
    main()
