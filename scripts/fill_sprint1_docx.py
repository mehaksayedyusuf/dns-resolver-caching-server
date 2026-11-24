import docx
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import parse_xml
from docx.oxml.ns import nsdecls
import os

doc_path = "DOCUMENTATION/sprint 1'.docx"
doc = docx.Document(doc_path)

def set_cell_background(cell, fill_hex):
    tcPr = cell._element.get_or_add_tcPr()
    shd = parse_xml(f'<w:shd {nsdecls("w")} w:fill="{fill_hex}"/>')
    tcPr.append(shd)

def set_cell_margins(cell, top=120, bottom=120, left=150, right=150):
    tcPr = cell._element.get_or_add_tcPr()
    tcMar = parse_xml(f'<w:tcMar {nsdecls("w")}><w:top w:w="{top}" w:type="dxa"/><w:bottom w:w="{bottom}" w:type="dxa"/><w:left w:w="{left}" w:type="dxa"/><w:right w:w="{right}" w:type="dxa"/></w:tcMar>')
    tcPr.append(tcMar)

def format_cell(cell, text, bold=False, italic=False, color_rgb=(0,0,0), font_size=9.5, align=WD_ALIGN_PARAGRAPH.LEFT, bg_hex=None):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = align
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(2)
    p.paragraph_format.line_spacing = 1.15
    run = p.add_run(text)
    run.bold = bold
    run.italic = italic
    run.font.name = 'Calibri'
    run.font.size = Pt(font_size)
    run.font.color.rgb = RGBColor(*color_rgb)
    if bg_hex:
        set_cell_background(cell, bg_hex)
    set_cell_margins(cell)

def add_image_to_cell(cell, img_path, width_inches=6.0):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(4)
    p.paragraph_format.space_after = Pt(4)
    if os.path.exists(img_path):
        p.add_run().add_picture(img_path, width=Inches(width_inches))
    else:
        p.add_run(f"[Image: {img_path}]")

# --- TABLE 0: Team Register ---
t0_data = [
    ("Field", "Student entry"),
    ("Team number", "11"),
    ("Theme", "Theme 4: Network Systems and Tools"),
    ("Artefact", "RFC 1035 DNS Resolver and In-Memory Caching Server (C++)"),
    ("Guide name", "Mr. Pavankumar S"),
    ("Department", "Department of Computer Science and Engineering, PES University"),
    ("Repository link", "https://github.com/mehaksayedyusuf/dns-resolver-caching-server"),
    ("Date", "20/11/2026")
]
t0 = doc.tables[0]
for r_idx, (f, v) in enumerate(t0_data):
    if r_idx == 0:
        format_cell(t0.rows[r_idx].cells[0], f, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        format_cell(t0.rows[r_idx].cells[1], v, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
    else:
        format_cell(t0.rows[r_idx].cells[0], f, bold=True, color_rgb=(31,73,125), bg_hex="F2F5F9")
        format_cell(t0.rows[r_idx].cells[1], v, bold=False, color_rgb=(0,0,0))

# --- TABLE 1: Team Members ---
t1_data = [
    ("Student", "SRN / USN", "Student name", "Engine / main responsibility"),
    ("Student A", "01fe24bci008", "Vageesh Mathad", "Engine E1: Protocol Parsing & Wire Message Engine"),
    ("Student B", "01fe24bci012", "Mehak Sayed Yusuf", "Engine E2: In-Memory Storage & Cache Engine"),
    ("Student C", "01fe22bci009", "Ishani G", "Engine E3: Upstream Resolution & Network Forwarding Engine"),
    ("Student D", "01fe24bci021", "Joel Biju", "Engine E4: Query Execution VM & Telemetry Engine")
]
t1 = doc.tables[1]
for r_idx, row in enumerate(t1_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t1.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            bg = "F2F5F9" if c_idx == 0 else None
            format_cell(t1.rows[r_idx].cells[c_idx], val, bold=(c_idx==0 or c_idx==2), color_rgb=(0,0,0), bg_hex=bg)

# --- TABLE 2: Approved Engines ---
t2_data = [
    ("Engine ID", "Approved engine name", "Owner (A–E)", "Main function"),
    ("E1", "Protocol Parsing & Wire Message Engine", "Owner A (Vageesh Mathad)", "RFC 1035 wire datagram unpacking, bitfield extraction, recursion-bounded pointer decompression (depth <= 16), wire response serialization (0xC00C)."),
    ("E2", "In-Memory Storage & Cache Engine", "Owner B (Mehak Sayed Yusuf)", "Dual-indexed cache combining O(1) LRU capacity eviction (hash table + doubly-linked list), O(log N) proactive Min-Heap TTL expiration, dynamic TTL aging, SRWLOCK concurrency, and disk snapshot (dns_cache.db)."),
    ("E3", "Upstream Resolution & Network Forwarding Engine", "Owner C (Ishani G)", "Recursive query forwarder to 8.8.8.8 / 1.1.1.1 over non-blocking UDP sockets (select() with 2.0s deadline), exponential retry backoff, and Kaminsky 16-bit TxID spoofing defense."),
    ("E4", "Query Execution VM & Telemetry Engine", "Owner D (Joel Biju)", "6-Stage VM instruction pipeline (FETCH->DECODE->LOOKUP->RESOLVE->COMMIT->EMIT), worker thread pool concurrency, query outcome dispatch (HIT, MISS, EXPIRED, NXDOMAIN, SERVFAIL, FORMERR), and microsecond telemetry."),
    ("E5", "Performance Telemetry & Analytics Subsystem", "Owner D (Joel Biju)", "Real-time query throughput (QPS) benchmarking, cache hit-ratio tracking, microsecond latency histogram profiling via std::chrono::steady_clock, and live terminal dashboard.")
]
t2 = doc.tables[2]
for r_idx, row in enumerate(t2_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t2.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            bg = "F2F5F9" if c_idx == 0 else None
            format_cell(t2.rows[r_idx].cells[c_idx], val, bold=(c_idx==0 or c_idx==1), color_rgb=(0,0,0), bg_hex=bg)

# --- TABLE 3: Overall System Architecture Image ---
add_image_to_cell(doc.tables[3].cell(0, 0), 'diagrams/system_orbit_diagram.png', 6.0)

# --- TABLE 4: Overall System Architecture Parts ---
t4_data = [
    ("Main part / engine", "What it does"),
    ("DNS Client Actor (dig / nslookup / stub resolver)", "Issues standard RFC 1035 UDP queries on port 8053 and receives resolved DNS response packets with accurate records and dynamic TTLs."),
    ("Core Engine E4 (Query Execution VM & Reactor Pool)", "Acts as the central gravitational state machine; reactor listens on UDP 8053, worker thread pool executes 6-stage instruction pipeline (FETCH->DECODE->LOOKUP->RESOLVE->COMMIT->EMIT), and emits responses."),
    ("Satellite Engine E1 (Protocol Parsing & Wire Serializer)", "Performs raw byte unpacking of DNS headers, question labels, and RDATA; prevents cyclic pointer DoS attacks (depth <= 16); serializes wire responses with 0xC00C compression."),
    ("Satellite Engine E2 (In-Memory Storage & Cache Engine)", "Maintains dual-indexed in-memory cache: O(1) LRU capacity eviction (unordered_map + std::list) and O(log N) proactive TTL expiration (Min-Heap); computes dynamic remaining TTLs; provides SRWLOCK concurrency; persists to dns_cache.db."),
    ("Satellite Engine E3 (Upstream Resolution Engine)", "Forwards unresolved queries to upstream resolvers (8.8.8.8, 1.1.1.1) via non-blocking UDP sockets with 2.0s timeouts, retries, and Kaminsky TxID spoofing validation.")
]
t4 = doc.tables[4]
while len(t4.rows) < len(t4_data):
    t4.add_row()
for r_idx, row in enumerate(t4_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t4.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            format_cell(t4.rows[r_idx].cells[c_idx], val, bold=(c_idx==0), color_rgb=(0,0,0), bg_hex="F2F5F9" if c_idx==0 else None)

# --- TABLE 5: Engine Interaction Diagram Image ---
add_image_to_cell(doc.tables[5].cell(0, 0), 'diagrams/engine_pipeline_diagram.png', 6.0)

# --- TABLE 6: Engine Interaction and Dependencies ---
t6_data = [
    ("From engine", "To engine", "Data / message exchanged"),
    ("Client / Reactor", "Engine E4 (VM)", "Raw 512-byte UDP datagram buffer slice, client sockaddr_in endpoint, and monotonic start timestamp (STAGE 1: FETCH)."),
    ("Engine E4 (VM)", "Engine E1 (Protocol Parser)", "Inbound datagram byte buffer slice passed for header unpacking and label decompression (STAGE 2: DECODE)."),
    ("Engine E1 (Protocol Parser)", "Engine E4 (VM)", "Structured DNSPacket (16-bit TxID, flags, QNAME, QTYPE, QCLASS) or FORMERR condition if datagram is malformed / exceeds pointer recursion depth."),
    ("Engine E4 (VM)", "Engine E2 (Cache Storage)", "CacheKey (Domain, QTYPE, QCLASS) passed under SRWLOCK shared lock for cache lookup (STAGE 3: LOOKUP); or upstream RRSet with TTL for cache insertion (STAGE 5: COMMIT)."),
    ("Engine E2 (Cache Storage)", "Engine E4 (VM)", "Cache verdict (HIT, MISS, EXPIRED, NEGATIVE_HIT), cached RRSet with dynamically decremented remaining TTL (TTL_rem = TTL_orig - elapsed_time), or eviction notifications."),
    ("Engine E4 (VM)", "Engine E3 (Upstream Forwarder)", "Raw query packet payload and upstream target (8.8.8.8:53) dispatched on cache MISS or EXPIRED condition (STAGE 4: RESOLVE)."),
    ("Engine E3 (Upstream Forwarder)", "Engine E4 (VM)", "Upstream response datagram bytes (verified against 16-bit TxID match), RTT latency, or TIMEOUT / SERVFAIL status on failure."),
    ("Engine E4 (VM)", "Engine E1 (Wire Serializer)", "Complete response packet structure (answers, remaining TTL, client TxID, QR=1 flag) for RFC 1035 binary wire encoding (STAGE 6: EMIT)."),
    ("Engine E1 (Wire Serializer)", "Client (via E4 / sendto)", "Serialized RFC 1035 wire datagram with 0xC00C pointer compression transmitted via UDP socket to client address."),
    ("Engine E2 (Cache Storage)", "Disk Storage (dns_cache.db)", "Binary snapshot serialization of valid unexpired cache entries on graceful SIGINT shutdown; reloaded during boot warm-up.")
]
t6 = doc.tables[6]
while len(t6.rows) < len(t6_data):
    t6.add_row()
for r_idx, row in enumerate(t6_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t6.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            format_cell(t6.rows[r_idx].cells[c_idx], val, bold=(c_idx==0 or c_idx==1), color_rgb=(0,0,0), bg_hex="F2F5F9" if c_idx<2 else None)

# --- TABLE 7: Engine-wise Architecture ---
t7_data = [
    ("Engine ID", "Main responsibility", "Inputs", "Outputs"),
    ("E1", "RFC 1035 Wire Parsing, Decompression Loop Guard & Response Serialization", "Raw UDP datagram buffer (512B max), packet byte length, pointer offset", "Parsed DNSPacket struct (12B header bitfields, questions, answers) OR binary wire response byte buffer with 0xC00C pointer compression."),
    ("E2", "Dual-Indexed In-Memory Caching, Dynamic TTL Recalculation & Eviction", "CacheKey(domain, qtype, qclass) for lookup; or upstream DNSResourceRecord list + original TTL for insertion; shutdown signal", "CacheVerdict (HIT, MISS, EXPIRED, NEGATIVE_HIT), answer records with aged remaining TTL (TTL_rem = TTL_orig - elapsed_time), binary snapshot dns_cache.db."),
    ("E3", "Non-Blocking Upstream Recursive Forwarding & Kaminsky Spoofing Defense", "Unresolved raw query datagram bytes, upstream target IPs (8.8.8.8, 1.1.1.1), 2.0s timeout limit, max retries (2)", "UpstreamResult (SUCCESS, TIMEOUT, MISMATCHED_TXID, SERVFAIL), verified raw upstream response bytes, round-trip latency."),
    ("E4", "6-Stage Instruction VM Pipeline Coordination, Worker Concurrency & Telemetry", "Client UDP datagrams on port 8053, worker queue events, query outcomes, telemetry sample ticks", "Final response datagram emitted via sendto(), transaction condition routing (HIT, MISS, NXDOMAIN, SERVFAIL, FORMERR), live dashboard KPIs (QPS, hit %, latency)."),
    ("E5", "Integrated Telemetry Subsystem: Microsecond Benchmarking & Metrics Dashboard", "Stage timing marks (std::chrono::steady_clock), query outcome events, console poll timer", "Real-time hit ratio %, microsecond latency histograms, throughput (QPS), formatted console telemetry dashboard.")
]
t7 = doc.tables[7]
while len(t7.rows) < len(t7_data):
    t7.add_row()
for r_idx, row in enumerate(t7_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t7.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            format_cell(t7.rows[r_idx].cells[c_idx], val, bold=(c_idx==0 or c_idx==1), color_rgb=(0,0,0), bg_hex="F2F5F9" if c_idx==0 else None)

# --- TABLE 8: Engine-wise Architecture Diagram Image ---
add_image_to_cell(doc.tables[8].cell(0, 0), 'diagrams/wire_protocol_format_diagram.png', 6.0)

# --- TABLE 9: Database Design ER Diagram Image ---
add_image_to_cell(doc.tables[9].cell(0, 0), 'diagrams/cache_storage_er_diagram.png', 6.0)

# --- TABLE 10: Database Design Tables & Entities ---
t10_data = [
    ("Table / entity", "Key fields", "Relation / purpose"),
    ("CacheKey (Composite PK Index)", "domain (std::string PK), qtype (uint16_t PK), qclass (uint16_t PK)", "Primary hash index in std::unordered_map; maps 1:1 to a CacheEntry node in O(1) time; custom hash functor combines domain name, query type, and query class."),
    ("CacheEntry (LRU Recency Node)", "key (CacheKey FK), records (vector<DNSResourceRecord>), original_ttl (uint32_t), expires_at (steady_clock::time_point), is_negative (bool), rcode (uint8_t), version (uint64_t), prev / next (pointers)", "Main memory storage entity; intrusive doubly-linked list (std::list) tracks access order; head represents Most Recently Used (MRU) and tail represents Least Recently Used (LRU) for O(1) eviction."),
    ("MinHeapNode (Proactive TTL Expiry Queue)", "expires_at (steady_clock::time_point PK), key (CacheKey FK), version (uint64_t)", "Organized in a binary Min-Heap (std::priority_queue with std::greater); root node points to the earliest expiring record; enables proactive O(log N) purge without scanning all entries."),
    ("DNSResourceRecord (Wire Record Component)", "name (std::string), type (uint16_t), rclass (uint16_t), ttl (uint32_t), rdlength (uint16_t), rdata (vector<uint8_t>), readable_data (std::string)", "Represents individual DNS answers (A, AAAA, CNAME); 1:N relationship with CacheEntry; holds parsed IP bytes and dynamic TTL."),
    ("DiskSnapshot (dns_cache.db Persistence Entity)", "magic_header (char[4] 'DCDB'), record_count (uint32_t), records_stream (serialized binary entries)", "Flat binary disk storage format; captures valid unexpired cache state upon graceful server shutdown (SIGINT) and re-populates memory at boot to eliminate cold-start cache latency."),
    ("TelemetrySnapshot (KPI Analytics Entity)", "total_queries (uint64_t), cache_hits (uint64_t), cache_misses (uint64_t), hit_ratio_pct (double), avg_latency_us (double), qps (double)", "Atomic performance metric registry tracking server health, throughput, and sub-millisecond response latency.")
]
t10 = doc.tables[10]
while len(t10.rows) < len(t10_data):
    t10.add_row()
for r_idx, row in enumerate(t10_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t10.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            format_cell(t10.rows[r_idx].cells[c_idx], val, bold=(c_idx==0), color_rgb=(0,0,0), bg_hex="F2F5F9" if c_idx==0 else None)

# --- TABLE 11: Algorithm and Data Structure ---
t11_data = [
    ("Engine ID", "Algorithm / data structure", "Why selected"),
    ("E1", "Recursive Pointer Decompression with Recursion Depth Bound (depth <= 16) & Bitwise Unpacking", "Handles RFC 1035 compression pointers (0xC000 prefix, 14-bit byte offsets) while bounding recursion to 16 calls; prevents stack overflow and infinite loop Denial-of-Service attacks caused by cyclic pointer loops."),
    ("E2", "Dual-Index LRU Eviction: Hash Table (std::unordered_map) + Doubly-Linked Recency List (std::list)", "Delivers guaranteed O(1) record lookup and O(1) node promotion to MRU head on hits. When cache capacity exceeds maximum limit, evicts tail node in O(1) time without linear scanning."),
    ("E2", "Proactive Min-Heap Priority Queue (std::priority_queue keyed by expires_at)", "Provides O(1) inspection of earliest expiring record and O(log N) proactive purging of stale records. Downstream queries dynamically recalculate remaining TTL (TTL_rem = TTL_orig - elapsed_time) preventing serving stale DNS records."),
    ("E3", "Non-Blocking Socket Polling (select()) with Exponential Backoff & 16-bit Cryptographic TxID Matching", "Enforces a strict 2.0s resolution timeout to avoid worker thread starvation, automatically fails over from primary (8.8.8.8) to secondary (1.1.1.1) resolvers, and verifies 16-bit random Transaction IDs to completely defeat Kaminsky DNS cache poisoning attacks."),
    ("E4", "6-Stage Instruction Pipeline Virtual Machine & Thread-Safe Task Queue", "Decouples network I/O, protocol parsing, memory storage, and resolution into an explicit, deterministic state machine (FETCH->DECODE->LOOKUP->RESOLVE->COMMIT->EMIT); worker thread pool processes tasks concurrently to sustain over 6,500 QPS.")
]
t11 = doc.tables[11]
while len(t11.rows) < len(t11_data):
    t11.add_row()
for r_idx, row in enumerate(t11_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t11.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            format_cell(t11.rows[r_idx].cells[c_idx], val, bold=(c_idx==0 or c_idx==1), color_rgb=(0,0,0), bg_hex="F2F5F9" if c_idx==0 else None)

# --- TABLE 12: Algorithm Flowchart / Pseudocode Image ---
add_image_to_cell(doc.tables[12].cell(0, 0), 'diagrams/cache_lru_minheap_diagram.png', 6.0)

# --- TABLE 13: Security, Errors and Testing ---
t13_data = [
    ("Area", "Simple student explanation"),
    ("Who can access or change important data?",
     "Access to the in-memory cache data structures (std::unordered_map, std::list, std::priority_queue) is synchronized using native Windows SRWLOCK (Slim Reader/Writer Lock) via C++ RAII guard classes (ReadLockGuard and WriteLockGuard). Multiple worker threads are granted concurrent shared read access (AcquireSRWLockShared) during Stage 3 LOOKUP without contention. Mutation operations during Stage 5 COMMIT, LRU capacity eviction, and TTL purge require exclusive lock acquisition (AcquireSRWLockExclusive), guaranteeing data integrity, zero race conditions, and complete thread safety. External network clients can only query the server via UDP datagrams on port 8053 and have no direct memory or administrative access."),
    ("What happens for invalid input or a failed request?",
     "The system handles all edge and failure conditions gracefully without crashing or hanging:\n" +
     "1. Malformed Datagrams / Packets < 12 Bytes: Sub-12-byte packets are dropped immediately; corrupted headers return FORMERR (RCODE 1, Format Error) with the client's Transaction ID.\n" +
     "2. Cyclic Compression Pointers: If label decompression exceeds depth 16 or points out-of-bounds, decompression halts and emits FORMERR.\n" +
     "3. Non-Existent Domains (NXDOMAIN): When upstream returns NXDOMAIN (RCODE 3), the server implements RFC 2308 negative caching using the SOA minimum TTL to suppress repeated upstream lookups and returns NXDOMAIN to the client.\n" +
     "4. Upstream Resolution Failure / Timeout: If upstream queries exceed 2.0s across 2 retries and Cloudflare failover, the server returns SERVFAIL (RCODE 2, Server Failure).\n" +
     "5. Mismatched Transaction ID: Upstream packets with spoofed TxIDs are discarded as Kaminsky attacks; original resolution is retried."),
    ("Is copying of large data necessary, and who owns it?",
     "No large copying is necessary. DNS UDP datagrams are strictly constrained to 512 bytes under standard RFC 1035 rules. At Stage 1 (FETCH), the reactor thread allocates a dedicated 512-byte buffer slice for the incoming datagram, preventing socket buffer overwrite races. Within the pipeline, parsing utilizes lightweight non-owning pointers and string references. Engine E2 exclusively owns the persistent in-memory heap records; Engine E4 owns the request transaction context (VMTransaction), which is automatically cleaned up via RAII as soon as the response is transmitted in Stage 6 (EMIT)."),
    ("How will you test the main functions?",
     "The system is rigorously validated across four distinct testing tiers:\n" +
     "1. Modular Unit Testing: Four isolated C++ test suites validating individual engines: test_protocol.cpp (10 tests: header parsing, label decompression, recursion guard), test_cache.cpp (8 tests: O(1) LRU splice, Min-Heap TTL purge, dynamic aging, thread safety), test_upstream.cpp (6 tests: UDP select() timeouts, retry logic, TxID validation), and test_vm_integration.cpp (6 tests: full 6-stage pipeline transitions).\n" +
     "2. Industry Standard DNS Client Verification: Live query verification using dig @127.0.0.1 -p 8053 example.com A and nslookup -port=8053 example.com 127.0.0.1, verifying RFC 1035 packet conformity, answer fidelity, and TTL decrement countdowns.\n" +
     "3. Concurrency Stress Benchmarking: Multi-threaded Python stress benchmark (benchmark_stress.py) testing pure cache hits, pure misses, and 80/20 mixed query workloads under concurrent client threads (1,000 queries sustaining 6,538 QPS, median cache-hit latency of 0.85 ms, demonstrating a 140x speedup over upstream miss latency).\n" +
     "4. Real-Time Telemetry Dashboard: Microsecond timer auditing (std::chrono::steady_clock), real-time hit ratio tracking, and verification of zero packet drops under sustained load.")
]
t13 = doc.tables[13]
for r_idx, row in enumerate(t13_data):
    for c_idx, val in enumerate(row):
        if r_idx == 0:
            format_cell(t13.rows[r_idx].cells[c_idx], val, bold=True, color_rgb=(255,255,255), bg_hex="1F497D")
        else:
            format_cell(t13.rows[r_idx].cells[c_idx], val, bold=(c_idx==0), color_rgb=(0,0,0), bg_hex="F2F5F9" if c_idx==0 else None)

# Update paragraphs with links
repo_base = "https://github.com/mehaksayedyusuf/dns-resolver-caching-server/blob/main/"
diagram_links = {
    15: f"diagrams/system_orbit_diagram.png | {repo_base}diagrams/system_orbit_diagram.png",
    22: f"diagrams/engine_pipeline_diagram.png | {repo_base}diagrams/engine_pipeline_diagram.png",
    29: f"diagrams/wire_protocol_format_diagram.png & concurrency_threadpool_diagram.png | {repo_base}diagrams/",
    37: f"diagrams/cache_storage_er_diagram.png | {repo_base}diagrams/cache_storage_er_diagram.png",
    44: f"diagrams/cache_lru_minheap_diagram.png | {repo_base}diagrams/cache_lru_minheap_diagram.png",
    49: f"benchmarks/BENCHMARK_REPORT.md | {repo_base}benchmarks/BENCHMARK_REPORT.md"
}

for p_idx, text in diagram_links.items():
    if p_idx < len(doc.paragraphs):
        p = doc.paragraphs[p_idx]
        p.text = f"Diagram / evidence filename or repository link: {text}"
        p.runs[0].font.name = 'Calibri'
        p.runs[0].font.size = Pt(9.5)
        p.runs[0].font.color.rgb = RGBColor(31, 73, 125)
        p.runs[0].bold = True

# Save to both the source sprint 1 docx and a filled deliverable docx
doc.save("DOCUMENTATION/sprint 1'.docx")
doc.save("DOCUMENTATION/Team11_Chapter4_Sprint1_Handbook_Filled.docx")
print("DOCX_SAVED_SUCCESSFULLY")
