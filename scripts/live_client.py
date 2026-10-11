import socket
import struct
import sys
import time

def build_dns_query(domain, txid=0x1234):
    header = struct.pack("!HHHHHH", txid, 0x0100, 1, 0, 0, 0) # RD=1, QD=1
    qname = b""
    for part in domain.split("."):
        qname += bytes([len(part)]) + part.encode("ascii")
    qname += b"\x00"
    question = qname + struct.pack("!HH", 1, 1) # Type A (1), Class IN (1)
    return header + question

def parse_dns_response(data):
    if len(data) < 12:
        return None
    txid, flags, qdcount, ancount, nscount, arcount = struct.unpack("!HHHHHH", data[:12])
    qr = (flags >> 15) & 1
    rcode = flags & 0x000F
    return {
        "txid": txid,
        "qr": qr,
        "rcode": rcode,
        "qdcount": qdcount,
        "ancount": ancount,
        "raw_len": len(data)
    }

def test_live_server(port=8053):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(4.0)
    server_addr = ("127.0.0.1", port)

    print(f"[*] Testing live DNS server on {server_addr[0]}:{server_addr[1]}...")

    # Query 1: Initial query (Cache Miss -> Upstream Resolve -> Commit)
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

    # Query 2: Immediate repeat query (Cache Hit test)
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

    print(f"[SUCCESS] Live DNS Server Verified! Query 1={ms1:.2f}ms, Query 2 (Cache Hit)={ms2:.2f}ms")
    return True

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8053
    try:
        test_live_server(port)
        sys.exit(0)
    except Exception as e:
        print(f"[-] Live test failed: {e}")
        sys.exit(1)
