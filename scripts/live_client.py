#!/usr/bin/env python3
"""
DNS Resolver Client — Interactive Manual Lookup Tool
Academic Theme 4: Network Systems and Tools (Team 11)

Sends RFC 1035 UDP queries to the local DNS Resolver and Caching Server (127.0.0.1:8053),
displays resolved IP addresses, DNS response codes, dynamic TTLs, and response latencies,
and supports continuous multiple lookups without restarting.
"""

import random
import socket
import struct
import sys
import time

RCODE_NAMES = {
    0: "NOERROR (Success)",
    1: "FORMERR (Format Error)",
    2: "SERVFAIL (Server Failure)",
    3: "NXDOMAIN (Non-Existent Domain)",
    4: "NOTIMP (Not Implemented)",
    5: "REFUSED (Query Refused)",
}

QTYPE_NAMES = {
    1: "A",
    2: "NS",
    5: "CNAME",
    6: "SOA",
    12: "PTR",
    15: "MX",
    16: "TXT",
    28: "AAAA",
}


def normalize_domain(domain: str) -> str:
    """Cleans up and normalizes user input domain string."""
    d = domain.strip()
    if d.startswith("http://"):
        d = d[7:]
    elif d.startswith("https://"):
        d = d[8:]
    d = d.split("/")[0].split(":")[0].strip()
    return d.rstrip(".").lower()


def build_dns_query(domain: str, txid: int = None) -> bytes:
    """Constructs a standard RFC 1035 UDP DNS Query datagram (Type A, Class IN)."""
    if txid is None:
        txid = random.randint(1, 0xFFFE)

    clean_domain = normalize_domain(domain)
    if not clean_domain:
        raise ValueError("Domain name cannot be empty")

    # Header: 12 bytes
    # Flags: 0x0100 -> QR=0 (query), Opcode=0 (standard), RD=1 (recursion desired)
    header = struct.pack("!HHHHHH", txid, 0x0100, 1, 0, 0, 0)

    # Question section: QNAME (labels) + QTYPE (1) + QCLASS (1)
    qname = b""
    for part in clean_domain.split("."):
        if len(part) > 63:
            raise ValueError(f"Label '{part}' exceeds RFC 1035 63-character limit")
        qname += bytes([len(part)]) + part.encode("ascii")
    qname += b"\x00"

    question = qname + struct.pack("!HH", 1, 1)  # Type A (1), Class IN (1)
    return header + question


def decode_domain_name(buf: bytes, offset: int, max_depth: int = 16):
    """
    Decodes an RFC 1035 wire-format domain name with pointer decompression (0xC0).
    Guards against cyclic pointer loops with max_depth limit.
    """
    labels = []
    jumped = False
    orig_offset = offset
    depth = 0
    visited = set()

    while offset < len(buf):
        length = buf[offset]

        # 2-byte compression pointer check (0xC0 prefix)
        if (length & 0xC0) == 0xC0:
            if offset + 1 >= len(buf):
                return "<truncated-pointer>", offset + 1
            ptr = ((length & 0x3F) << 8) | buf[offset + 1]
            if not jumped:
                orig_offset = offset + 2
                jumped = True

            if ptr in visited or depth >= max_depth:
                return "<pointer-loop-detected>", orig_offset
            visited.add(ptr)
            depth += 1
            offset = ptr
        elif length == 0:
            offset += 1
            break
        elif (length & 0xC0) != 0:
            return "<invalid-label-type>", offset + 1
        else:
            offset += 1
            if offset + length > len(buf):
                return "<truncated-label>", len(buf)
            labels.append(buf[offset : offset + length].decode("ascii", errors="replace"))
            offset += length

    domain = ".".join(labels)
    return domain, (orig_offset if jumped else offset)


def parse_dns_response(data: bytes):
    """
    Fully parses an RFC 1035 UDP DNS response datagram, extracting header flags,
    questions, and all answered resource records (A, AAAA, CNAME, etc.).
    """
    if len(data) < 12:
        return {"valid": False, "error": "Datagram is smaller than 12-byte DNS header"}

    txid, flags, qdcount, ancount, nscount, arcount = struct.unpack("!HHHHHH", data[:12])
    qr = (flags >> 15) & 1
    rcode = flags & 0x000F

    parsed = {
        "valid": True,
        "txid": txid,
        "qr": qr,
        "rcode": rcode,
        "rcode_name": RCODE_NAMES.get(rcode, f"UNKNOWN ({rcode})"),
        "qdcount": qdcount,
        "ancount": ancount,
        "nscount": nscount,
        "arcount": arcount,
        "questions": [],
        "answers": [],
        "raw_len": len(data),
    }

    offset = 12

    # Parse Questions
    for _ in range(qdcount):
        if offset >= len(data):
            break
        qname, offset = decode_domain_name(data, offset)
        if offset + 4 > len(data):
            break
        qtype, qclass = struct.unpack("!HH", data[offset : offset + 4])
        offset += 4
        parsed["questions"].append({"name": qname, "type": qtype, "class": qclass})

    # Parse Answers
    for _ in range(ancount):
        if offset >= len(data):
            break
        name, offset = decode_domain_name(data, offset)
        if offset + 10 > len(data):
            break
        rtype, rclass, ttl, rdlen = struct.unpack("!HHIH", data[offset : offset + 10])
        offset += 10

        if offset + rdlen > len(data):
            break
        rdata = data[offset : offset + rdlen]
        offset += rdlen

        rec = {
            "name": name,
            "type": rtype,
            "type_name": QTYPE_NAMES.get(rtype, str(rtype)),
            "class": rclass,
            "ttl": ttl,
            "data": "",
        }

        if rtype == 1 and rdlen == 4:  # Type A (IPv4)
            rec["data"] = f"{rdata[0]}.{rdata[1]}.{rdata[2]}.{rdata[3]}"
        elif rtype == 28 and rdlen == 16:  # Type AAAA (IPv6)
            try:
                rec["data"] = socket.inet_ntop(socket.AF_INET6, rdata)
            except Exception:
                rec["data"] = "<invalid-ipv6>"
        elif rtype == 5:  # Type CNAME
            cname_target, _ = decode_domain_name(data, offset - rdlen)
            rec["data"] = cname_target
        else:
            rec["data"] = f"<type {rtype} data>"

        parsed["answers"].append(rec)

    return parsed


def query_domain(domain: str, server_ip: str = "127.0.0.1", server_port: int = 8053, timeout: float = 3.0):
    """
    Sends a DNS query for domain to server_ip:server_port, returns parsed response and latency.
    """
    clean = normalize_domain(domain)
    if not clean:
        print("  [!] Please enter a valid non-empty domain name.")
        return

    try:
        txid = random.randint(1, 0xFFFE)
        packet = build_dns_query(clean, txid)
    except Exception as e:
        print(f"  [!] Invalid domain format: {e}")
        return

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)

    t_start = time.perf_counter()
    try:
        sock.sendto(packet, (server_ip, server_port))
        resp_data, _ = sock.recvfrom(1024)
        t_end = time.perf_counter()
    except socket.timeout:
        print(f"\n  [ERROR] Request timed out ({timeout}s).")
        print(f"  Make sure the DNS server is running at {server_ip}:{server_port}.")
        sock.close()
        return
    except OSError as e:
        if getattr(e, "winerror", None) == 10054 or getattr(e, "errno", None) in (10054, 111):
            print(f"\n  [ERROR] Connection refused / port unreachable at {server_ip}:{server_port}.")
            print(f"  Make sure the DNS server is running ('bin\\dns_server.exe -p {server_port}').")
        else:
            print(f"\n  [ERROR] Network error during query: {e}")
        sock.close()
        return
    except Exception as e:
        print(f"\n  [ERROR] Unexpected error during query: {e}")
        sock.close()
        return
    finally:
        sock.close()

    latency_ms = (t_end - t_start) * 1000.0
    parsed = parse_dns_response(resp_data)

    if not parsed["valid"]:
        print(f"\n  [ERROR] Malformed server response: {parsed.get('error', 'Unknown error')}")
        return

    # Check for DNS error codes
    rcode = parsed["rcode"]
    rcode_str = parsed["rcode_name"]

    print("\n" + "-" * 55)
    print(f"  Domain Name    : {clean}")
    print(f"  Response Code  : {rcode_str}")
    print(f"  Response Time  : {latency_ms:.2f} ms")

    # Cache hit heuristic: in-memory cache responds in < 15ms
    if latency_ms < 15.0 and rcode in (0, 3):
        cache_indicator = "IN-MEMORY CACHE HIT"
    else:
        cache_indicator = "UPSTREAM RESOLUTION (Cached for subsequent queries)"
    print(f"  Resolution     : {cache_indicator}")

    if rcode == 3:  # NXDOMAIN
        print(f"\n  [ERROR] Domain does not exist (NXDOMAIN / RCODE 3).")
        print("-" * 55)
        return
    elif rcode == 2:  # SERVFAIL
        print(f"\n  [ERROR] Server failure (SERVFAIL / RCODE 2). Upstream resolution timed out or failed.")
        print("-" * 55)
        return
    elif rcode == 1:  # FORMERR
        print(f"\n  [ERROR] Format error (FORMERR / RCODE 1). The query packet format was rejected.")
        print("-" * 55)
        return
    elif rcode != 0:
        print(f"\n  [ERROR] DNS Query returned error code: {rcode_str}")
        print("-" * 55)
        return

    # Extract answers
    ip_answers = [a for a in parsed["answers"] if a["type"] == 1]
    cname_answers = [a for a in parsed["answers"] if a["type"] == 5]
    other_answers = [a for a in parsed["answers"] if a["type"] not in (1, 5)]

    print("  Resolved Records:")
    if not parsed["answers"]:
        print("    (No answer records returned by resolver)")
    else:
        for cname in cname_answers:
            print(f"    * [CNAME] {cname['name']} -> {cname['data']} (TTL: {cname['ttl']}s)")
        for ip in ip_answers:
            print(f"    * [A IP]  {ip['data']} (TTL: {ip['ttl']}s)")
        for other in other_answers:
            print(f"    * [{other['type_name']}]  {other['data']} (TTL: {other['ttl']}s)")

    print("-" * 55)


def interactive_loop(server_ip: str = "127.0.0.1", server_port: int = 8053):
    """Interactive CLI loop prompting for domains repeatedly."""
    print("=======================================================")
    print("     DNS RESOLVER & CACHING SERVER -- MANUAL CLIENT    ")
    print("=======================================================")
    print(f" Target Resolver : {server_ip}:{server_port} (UDP)")
    print(" Usage           : Enter any domain (e.g. google.com,")
    print("                   github.com, youtube.com).")
    print(" Exit            : Type 'exit' or 'quit' or press Ctrl+C")
    print("=======================================================")

    while True:
        try:
            domain = input("\nEnter domain to resolve: ").strip()
        except (KeyboardInterrupt, EOFError):
            print("\n\nExiting DNS client. Goodbye!")
            break

        if not domain:
            continue

        if domain.lower() in ("exit", "quit", "q"):
            print("Exiting DNS client. Goodbye!")
            break

        query_domain(domain, server_ip, server_port)


def test_live_server(port: int = 8053):
    """Automated verification test (backwards-compatible with test suite)."""
    print(f"[*] Testing live DNS server on 127.0.0.1:{port}...")

    # Query 1: Uncached query
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(4.0)
    server_addr = ("127.0.0.1", port)

    print("[*] Sending Query 1: 'example.com' (TxID=0x1234)...")
    t0 = time.perf_counter()
    sock.sendto(build_dns_query("example.com", 0x1234), server_addr)
    data1, _ = sock.recvfrom(512)
    t1 = time.perf_counter()
    r1 = parse_dns_response(data1)
    ms1 = (t1 - t0) * 1000.0

    print(f"[+] Query 1 Response: TxID=0x{r1['txid']:04x}, QR={r1['qr']}, RCODE={r1['rcode']}, ANCOUNT={r1['ancount']}, Latency={ms1:.2f}ms")
    assert r1["txid"] == 0x1234, f"TxID mismatch: expected 0x1234, got 0x{r1['txid']:04x}"
    assert r1["qr"] == 1, "Expected response QR=1"
    assert r1["rcode"] == 0, f"Expected NOERROR (0), got {r1['rcode']}"
    assert r1["ancount"] >= 1, "Expected at least 1 answer record"

    # Query 2: Cached query
    print("[*] Sending Query 2: 'example.com' (TxID=0x5678, testing in-memory cache hit)...")
    t2 = time.perf_counter()
    sock.sendto(build_dns_query("example.com", 0x5678), server_addr)
    data2, _ = sock.recvfrom(512)
    t3 = time.perf_counter()
    r2 = parse_dns_response(data2)
    ms2 = (t3 - t2) * 1000.0

    print(f"[+] Query 2 Response: TxID=0x{r2['txid']:04x}, QR={r2['qr']}, RCODE={r2['rcode']}, ANCOUNT={r2['ancount']}, Latency={ms2:.2f}ms")
    assert r2["txid"] == 0x5678, f"TxID mismatch: expected 0x5678, got 0x{r2['txid']:04x}"
    assert r2["ancount"] >= 1, "Expected cached answer"

    sock.close()
    print(f"[SUCCESS] Live DNS Server Verified! Query 1={ms1:.2f}ms, Query 2 (Cache Hit)={ms2:.2f}ms")
    return True


if __name__ == "__main__":
    server_port = 8053
    server_host = "127.0.0.1"

    # Parse command line options
    args = sys.argv[1:]
    if "--test" in args:
        # Run automated test mode
        test_live_server(server_port)
        sys.exit(0)

    for i, arg in enumerate(args):
        if arg in ("-p", "--port") and i + 1 < len(args):
            server_port = int(args[i + 1])
        elif arg.isdigit():
            server_port = int(arg)

    interactive_loop(server_host, server_port)
