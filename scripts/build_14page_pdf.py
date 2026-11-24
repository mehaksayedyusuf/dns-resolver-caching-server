import os
import sys
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image as RLImage, PageBreak, KeepTogether, HRFlowable
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.pdfgen import canvas

class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_page_decorations(num_pages)
            super().showPage()
        super().save()

    def draw_page_decorations(self, page_count):
        self.saveState()
        self.setFont("Helvetica-Bold", 8)
        self.setFillColor(colors.HexColor("#1F497D"))
        # Top Header
        self.drawString(54, 805, "PES UNIVERSITY | THEME 4: NETWORK SYSTEMS & TOOLS")
        self.setFont("Helvetica", 8)
        self.setFillColor(colors.HexColor("#64748B"))
        self.drawRightString(54 + 487, 805, "DNS Resolver & In-Memory Caching Server (C++)")
        self.setStrokeColor(colors.HexColor("#CBD5E1"))
        self.setLineWidth(0.6)
        self.line(54, 798, 54 + 487, 798)
        
        # Bottom Footer
        self.setFont("Helvetica", 8)
        self.setFillColor(colors.HexColor("#64748B"))
        self.drawString(54, 34, "Team 11: Sprint 1 Student Handbook — Complete Solution Design & Implementation")
        page_str = f"Page {self._pageNumber} of {page_count}"
        self.drawRightString(54 + 487, 34, page_str)
        self.setStrokeColor(colors.HexColor("#CBD5E1"))
        self.setLineWidth(0.6)
        self.line(54, 46, 54 + 487, 46)
        self.restoreState()

def build_pdf():
    pdf_path = "DOCUMENTATION/Team11_Chapter4_Sprint1_Handbook_Final.pdf"
    doc = SimpleDocTemplate(
        pdf_path,
        pagesize=A4,
        leftMargin=54,
        rightMargin=54,
        topMargin=54,
        bottomMargin=54
    )

    styles = getSampleStyleSheet()
    
    title_style = ParagraphStyle(
        'DocTitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=15,
        leading=18,
        textColor=colors.HexColor("#1F497D"),
        spaceAfter=2
    )

    subtitle_style = ParagraphStyle(
        'DocSubTitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=11,
        leading=14,
        textColor=colors.HexColor("#0284C7"),
        spaceAfter=6
    )

    h1_style = ParagraphStyle(
        'H1',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=12,
        leading=15,
        textColor=colors.HexColor("#1F497D"),
        spaceBefore=4,
        spaceAfter=4
    )

    h2_style = ParagraphStyle(
        'H2',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=10,
        leading=13,
        textColor=colors.HexColor("#1E293B"),
        spaceBefore=4,
        spaceAfter=3
    )

    body_style = ParagraphStyle(
        'Body',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.0,
        leading=10.5,
        textColor=colors.HexColor("#1E293B"),
        spaceAfter=3
    )

    table_header_style = ParagraphStyle(
        'TH',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=7.5,
        leading=9.5,
        textColor=colors.white
    )

    table_cell_style = ParagraphStyle(
        'TC',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=7.5,
        leading=9.5,
        textColor=colors.HexColor("#0F172A")
    )

    table_cell_bold = ParagraphStyle(
        'TCB',
        parent=table_cell_style,
        fontName='Helvetica-Bold'
    )

    link_style = ParagraphStyle(
        'EvidenceLink',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=7.5,
        leading=9.5,
        textColor=colors.HexColor("#1F497D"),
        spaceBefore=3,
        spaceAfter=3
    )

    def p(text, style=body_style):
        return Paragraph(text, style)

    def th(text):
        return Paragraph(text, table_header_style)

    def tc(text, bold=False):
        return Paragraph(text, table_cell_bold if bold else table_cell_style)

    def make_table(data_rows, col_widths, is_header=True):
        formatted_rows = []
        for r_idx, row in enumerate(data_rows):
            f_row = []
            for c_idx, val in enumerate(row):
                if r_idx == 0 and is_header:
                    f_row.append(th(val))
                elif c_idx == 0:
                    f_row.append(tc(val, bold=True))
                else:
                    f_row.append(tc(val, bold=False))
            formatted_rows.append(f_row)
        
        t = Table(formatted_rows, colWidths=col_widths)
        t_style = [
            ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#1F497D")),
            ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
            ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
            ('VALIGN', (0, 0), (-1, -1), 'TOP'),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
            ('TOPPADDING', (0, 0), (-1, -1), 3),
            ('LEFTPADDING', (0, 0), (-1, -1), 4),
            ('RIGHTPADDING', (0, 0), (-1, -1), 4),
            ('GRID', (0, 0), (-1, -1), 0.5, colors.HexColor("#CBD5E1")),
        ]
        for i in range(1, len(data_rows)):
            bg = colors.HexColor("#F8FAFC") if i % 2 == 1 else colors.white
            t_style.append(('BACKGROUND', (0, i), (-1, i), bg))
        t.setStyle(TableStyle(t_style))
        return t

    repo_base = "https://github.com/mehaksayedyusuf/dns-resolver-caching-server/blob/main/"
    story = []

    # =========================================================================
    # PAGE 1: 4.1 Team and Engine Register
    # =========================================================================
    story.append(p("CHAPTER 4 — SPRINT 1 STUDENT HANDBOOK", title_style))
    story.append(p("Complete Solution Design & Implementation", subtitle_style))
    story.append(p("4.1 Team and Engine Register", h1_style))
    story.append(p("<i>Enter team information only here. In the remaining sections use Engine ID (E1, E2, ...), requirement ID and diagram reference. Do not re-enter names or team details.</i>", body_style))
    story.append(Spacer(1, 4))

    t0_data = [
        ("Field", "Student entry"),
        ("Team number", "11"),
        ("Theme", "Theme 4: Network Systems and Tools"),
        ("Artefact", "RFC 1035 DNS Resolver and In-Memory Caching Server in C++"),
        ("Guide name", "Mr. Pavankumar S"),
        ("Department", "Department of Computer Science and Engineering, PES University"),
        ("Repository link", "https://github.com/mehaksayedyusuf/dns-resolver-caching-server"),
        ("Date", "20/11/2026")
    ]
    story.append(make_table(t0_data, [130, 357]))
    story.append(Spacer(1, 6))

    story.append(p("Team members", h2_style))
    t1_data = [
        ("Student", "SRN / USN", "Student name", "Engine / main responsibility"),
        ("Student A", "01fe24bci008", "Vageesh Mathad", "Engine E1: Protocol Parsing & Wire Message Engine"),
        ("Student B", "01fe24bci012", "Mehak Sayed Yusuf", "Engine E2: In-Memory Storage & Cache Engine"),
        ("Student C", "01fe22bci009", "Ishani G", "Engine E3: Upstream Resolution & Network Forwarding Engine"),
        ("Student D", "01fe24bci021", "Joel Biju", "Engine E4: Query Execution VM & Telemetry Engine")
    ]
    story.append(make_table(t1_data, [65, 80, 110, 232]))
    story.append(Spacer(1, 6))

    story.append(p("Approved engines", h2_style))
    t2_data = [
        ("Engine ID", "Approved engine name", "Owner (A–E)", "Main function"),
        ("E1", "Protocol Parsing & Wire Message Engine", "Owner A (Vageesh Mathad)", "RFC 1035 wire datagram unpacking, bitfield extraction, recursion-bounded pointer decompression (depth <= 16), wire response serialization (0xC00C)."),
        ("E2", "In-Memory Storage & Cache Engine", "Owner B (Mehak Sayed Yusuf)", "Dual-indexed cache combining O(1) LRU capacity eviction (hash table + doubly-linked list), O(log N) proactive Min-Heap TTL expiration, dynamic TTL aging, SRWLOCK concurrency, and disk snapshot (dns_cache.db)."),
        ("E3", "Upstream Resolution & Network Forwarding Engine", "Owner C (Ishani G)", "Recursive query forwarder to 8.8.8.8 / 1.1.1.1 over non-blocking UDP sockets (select() with 2.0s deadline), exponential retry backoff, and Kaminsky 16-bit TxID spoofing defense."),
        ("E4", "Query Execution VM & Telemetry Engine", "Owner D (Joel Biju)", "6-Stage VM instruction pipeline (FETCH->DECODE->LOOKUP->RESOLVE->COMMIT->EMIT), worker thread pool concurrency, query outcome dispatch (HIT, MISS, EXPIRED, NXDOMAIN, SERVFAIL, FORMERR), and microsecond telemetry."),
        ("E5", "Performance Telemetry & Analytics Subsystem", "Owner D (Joel Biju)", "Real-time query throughput (QPS) benchmarking, cache hit-ratio tracking, microsecond latency histogram profiling via std::chrono::steady_clock, and live terminal dashboard.")
    ]
    story.append(make_table(t2_data, [45, 125, 105, 212]))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 2: Overall System Architecture
    # =========================================================================
    story.append(p("Overall System Architecture", h1_style))
    story.append(p("Show actors, the main parts/engines, data storage and links between them. Do this before describing any engine internally.", body_style))
    story.append(p("Overall System Architecture Diagram", h2_style))
    story.append(RLImage('diagrams/system_orbit_diagram.png', width=487, height=270))
    story.append(Spacer(1, 4))

    t4_data = [
        ("Main part / engine", "What it does"),
        ("DNS Client Actor (dig / nslookup / stub)", "Issues standard RFC 1035 UDP queries on port 8053 and receives resolved DNS response packets with accurate records and dynamic TTLs."),
        ("Core Engine E4 (Query Execution VM)", "Acts as the central gravitational state machine; reactor listens on UDP 8053, worker thread pool executes 6-stage instruction pipeline (FETCH->DECODE->LOOKUP->RESOLVE->COMMIT->EMIT), and emits responses."),
        ("Satellite Engine E1 (Protocol Parsing)", "Performs raw byte unpacking of DNS headers, question labels, and RDATA; prevents cyclic pointer DoS attacks (depth <= 16); serializes wire responses with 0xC00C compression."),
        ("Satellite Engine E2 (In-Memory Cache)", "Maintains dual-indexed in-memory cache: O(1) LRU capacity eviction (unordered_map + std::list) and O(log N) proactive TTL expiration (Min-Heap); computes dynamic remaining TTLs; provides SRWLOCK concurrency; persists to dns_cache.db."),
        ("Satellite Engine E3 (Upstream Resolution)", "Forwards unresolved queries to upstream resolvers (8.8.8.8, 1.1.1.1) via non-blocking UDP sockets with 2.0s timeouts, retries, and Kaminsky TxID spoofing validation.")
    ]
    story.append(make_table(t4_data, [145, 342]))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/system_orbit_diagram.png | {repo_base}diagrams/system_orbit_diagram.png", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 3: Engine Interaction and Dependencies
    # =========================================================================
    story.append(p("Engine Interaction and Dependencies", h1_style))
    story.append(p("Show which engine communicates with which other engine and what information is exchanged.", body_style))
    story.append(p("Engine Interaction Table & Inter-Engine Messages", h2_style))

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
    story.append(make_table(t6_data, [95, 95, 297]))
    story.append(Spacer(1, 6))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/engine_pipeline_diagram.png | {repo_base}diagrams/engine_pipeline_diagram.png", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 4: Six-Stage VM Instruction Pipeline with Visual Branching
    # =========================================================================
    story.append(p("Six-Stage Query Execution VM Workflow", h1_style))
    story.append(p("Every inbound client query executes deterministically through an explicit 6-stage instruction pipeline with visual branching for Cache-Hits and Cache-Misses:", body_style))
    story.append(p("<b>1. STAGE 1 (FETCH):</b> Ingests raw datagram slice (>= 12 bytes), timestamps start with std::chrono::steady_clock, allocates an isolated heap buffer copy to prevent socket buffer race conditions.", body_style))
    story.append(p("<b>2. STAGE 2 (DECODE - Engine E1):</b> Unpacks 12-byte header bitfields, decodes domain labels with a recursion guard (depth <= 16). <i>[FORMERR Shortcut]</i>: If malformed or pointer loop detected, branches directly to EMIT with FORMERR (RCODE 1).", body_style))
    story.append(p("<b>3. STAGE 3 (LOOKUP - Engine E2):</b> Queries cache using CompositeKey under SRWLOCK Shared.<br/>"
                   "• <b>Cache-Hit Fast-Path:</b> Valid non-expired record found! Recalculates dynamic remaining TTL (TTL_rem = TTL_orig - delta_t), splices node to MRU head in O(1), and <b>JUMPS DIRECTLY TO STAGE 6 (EMIT)</b>, completing in <b>0.414 ms</b> (bypassing Stages 4 and 5 entirely)!<br/>"
                   "• <b>Cache-Negative-Hit Path:</b> Returns cached NXDOMAIN immediately.<br/>"
                   "• <b>Cache-Miss / Expired Path:</b> Purges stale Min-Heap root if expired, and branches to STAGE 4 (RESOLVE).", body_style))
    story.append(p("<b>4. STAGE 4 (RESOLVE - Engine E3):</b> Dispatches query to upstream 8.8.8.8:53 with a 2.0s non-blocking select() timeout. Replaces TxID with cryptographic 16-bit integer (std::mt19937). <i>[SERVFAIL Shortcut]</i>: On timeout/retries exhausted, branches directly to EMIT with SERVFAIL (RCODE 2). Drops spoofed packets on TxID mismatch.", body_style))
    story.append(p("<b>5. STAGE 5 (COMMIT - Engine E2):</b> Under SRWLOCK Exclusive, commits upstream RRSet to Hash Map + MRU Head, pushes expiration to Min-Heap, and evicts LRU Tail in O(1) if capacity > 1000.", body_style))
    story.append(p("<b>6. STAGE 6 (EMIT - Engine E1):</b> Serializes RFC 1035 wire datagram with 0xC00C pointer compression, records microsecond elapsed time in Engine E5 telemetry, and transmits datagram back to client via sendto().", body_style))
    story.append(RLImage('diagrams/vm_pipeline_branching_diagram.png', width=487, height=255))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/vm_pipeline_branching_diagram.png | {repo_base}diagrams/vm_pipeline_branching_diagram.png", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 5: Engine-wise Architecture — Overview
    # =========================================================================
    story.append(p("Engine-wise Architecture", h1_style))
    story.append(p("Prepare this once per approved engine. Show the parts inside the engine, its inputs, outputs and main operations.", body_style))

    t7_data = [
        ("Engine ID", "Main responsibility", "Inputs", "Outputs"),
        ("E1", "RFC 1035 Wire Parsing, Decompression Loop Guard & Response Serialization", "Raw UDP datagram buffer (512B max), packet byte length, pointer offset", "Parsed DNSPacket struct (12B header bitfields, questions, answers) OR binary wire response byte buffer with 0xC00C pointer compression."),
        ("E2", "Dual-Indexed In-Memory Caching, Dynamic TTL Recalculation & Eviction", "CacheKey(domain, qtype, qclass) for lookup; or upstream DNSResourceRecord list + original TTL for insertion; shutdown signal", "CacheVerdict (HIT, MISS, EXPIRED, NEGATIVE_HIT), answer records with aged remaining TTL (TTL_rem = TTL_orig - elapsed_time), binary snapshot dns_cache.db."),
        ("E3", "Non-Blocking Upstream Recursive Forwarding & Kaminsky Spoofing Defense", "Unresolved raw query datagram bytes, upstream target IPs (8.8.8.8, 1.1.1.1), 2.0s timeout limit, max retries (2)", "UpstreamResult (SUCCESS, TIMEOUT, MISMATCHED_TXID, SERVFAIL), verified raw upstream response bytes, round-trip latency."),
        ("E4", "6-Stage Instruction VM Pipeline Coordination, Worker Concurrency & Telemetry", "Client UDP datagrams on port 8053, worker queue events, query outcomes, telemetry sample ticks", "Final response datagram emitted via sendto(), transaction condition routing (HIT, MISS, NXDOMAIN, SERVFAIL, FORMERR), live dashboard KPIs (QPS, hit %, latency)."),
        ("E5", "Integrated Telemetry Subsystem: Microsecond Benchmarking & Metrics Dashboard", "Stage timing marks (std::chrono::steady_clock), query outcome events, console poll timer", "Real-time hit ratio %, microsecond latency histograms, throughput (QPS), formatted console telemetry dashboard.")
    ]
    story.append(make_table(t7_data, [45, 130, 132, 180]))
    story.append(Spacer(1, 6))

    story.append(p("Decomposition Boundaries and Separation of Concerns", h2_style))
    story.append(p("• <b>Engine E1 (Protocol Engine):</b> Decouples RFC 1035 wire packing from networking. Operates solely on memory buffers, endianness conversions (htons/ntohs), label decompression, and loop prevention.", body_style))
    story.append(p("• <b>Engine E2 (Storage Engine):</b> Decouples caching algorithms from query lifecycle. Encapsulates dual-indexing, dynamic aging, and reader-writer thread safety. Exposes simple GET/PUT semantics.", body_style))
    story.append(p("• <b>Engine E3 (Upstream Forwarder):</b> Encapsulates recursive outbound UDP sockets, retry timers, failovers, and Kaminsky transaction ID defenses away from local client handling.", body_style))
    story.append(p("• <b>Engine E4 (Virtual Machine):</b> Orchestrates state transitions, dispatches tasks across the 8 worker threads, and binds local UDP port 8053.", body_style))
    story.append(p("• <b>Engine E5 (Telemetry Engine):</b> Gathers monotonic microsecond latency samples and atomic query counters without degrading packet processing throughput.", body_style))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> include/ | {repo_base}include/", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 6: Engine E1 Deep-Dive (Protocol Parsing)
    # =========================================================================
    story.append(p("Engine E1: Protocol Parsing & Wire Message Engine", h1_style))
    story.append(p("<b>Lead Owner:</b> Vageesh Mathad (01fe24bci008) | <b>Academic Subject:</b> Computer Networks", subtitle_style))
    story.append(p("Engine E1 performs byte-by-byte deserialization and serialization compliant with RFC 1035 without any external parsing libraries. It decodes the 12-byte header bitfields, resolves compressed domain labels, and serializes responses.", body_style))
    story.append(p("• <b>12-Byte Header Bitfields:</b> Unpacks 16-bit Transaction ID, Flags (QR, Opcode, AA, TC, RD, RA, Z, RCODE), and section counts (QDCOUNT, ANCOUNT, NSCOUNT, ARCOUNT) in big-endian network byte order.", body_style))
    story.append(p("• <b>14-Bit Compression Pointer Decompression:</b> Compression pointers start with two 1s (0xC0). The lower 14 bits indicate the byte offset from the header start. The parser jumps cursor to target offset and reads labels recursively.", body_style))
    story.append(p("• <b>Loop Guard Depth Counter (Security Feature):</b> Binds pointer dereference depth to MAX_DECOMPRESSION_DEPTH = 16. If cyclic or malicious self-referencing pointers are detected, recursion halts immediately and emits FORMERR (RCODE 1), neutralizing Denial-of-Service freeze exploits.", body_style))
    story.append(p("• <b>0xC00C Response Serializer:</b> Serializes Answer records pointing back to offset 12 (the Question section QNAME), minimizing response datagram byte size.", body_style))
    story.append(RLImage('diagrams/wire_protocol_format_diagram.png', width=487, height=270))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/wire_protocol_format_diagram.png | {repo_base}src/protocol_engine.cpp", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 7: Engine E2 Deep-Dive (In-Memory Storage & Cache)
    # =========================================================================
    story.append(p("Engine E2: In-Memory Storage & Cache Engine", h1_style))
    story.append(p("<b>Lead Owner:</b> Mehak Sayed Yusuf (01fe24bci012) | <b>Academic Subject:</b> DAA & DBMS", subtitle_style))
    story.append(p("Engine E2 provides a high-throughput dual-indexed in-memory caching engine that simultaneously guarantees O(1) LRU capacity eviction and O(log N) proactive TTL expiration.", body_style))
    story.append(p("• <b>Primary Hash Index:</b> std::unordered_map&lt;CacheKey, std::list&lt;CacheEntry&gt;::iterator&gt; provides O(1) key lookups using a custom 64-bit hash combining domain name, query type, and query class.", body_style))
    story.append(p("• <b>Doubly-Linked Recency List:</b> std::list&lt;CacheEntry&gt; maintains strict access recency. Head = Most Recently Used (MRU); Tail = Least Recently Used (LRU). On a cache hit, splice() promotes the node to Head in O(1) without memory allocation. At capacity (1,000 records), pop_back() evicts the Tail in O(1).", body_style))
    story.append(p("• <b>Proactive Min-Heap Priority Queue:</b> std::priority_queue&lt;MinHeapNode&gt; keyed by absolute expiration timestamp (ExpiresAt = CachedAt + TTL). Earliest expiring record is always at the heap root. Background purge cleans expired records in O(log N) without full-table scans.", body_style))
    story.append(p("• <b>Dynamic TTL Aging:</b> Decays TTL at query time: TTL_rem = TTL_orig - (Now - CachedAt). Downstream clients receive strictly non-stale countdowns.", body_style))
    story.append(p("• <b>Concurrency Control:</b> Native Win32 SRWLOCK provides shared read access for concurrent lookups (AcquireSRWLockShared) and exclusive write access for insertions and evictions (AcquireSRWLockExclusive).", body_style))
    story.append(p("• <b>Durability Snapshot:</b> Graceful SIGINT shutdown serializes active valid cache entries to dns_cache.db, and boot reloads entries to eliminate cold-start cache latency.", body_style))
    story.append(RLImage('diagrams/cache_lru_minheap_diagram.png', width=487, height=270))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/cache_lru_minheap_diagram.png | {repo_base}src/storage_engine.cpp", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 8: Engine E3 Deep-Dive (Upstream Resolution)
    # =========================================================================
    story.append(p("Engine E3: Upstream Resolution & Network Forwarding Engine", h1_style))
    story.append(p("<b>Lead Owner:</b> Ishani G (01fe22bci009) | <b>Academic Subject:</b> Computer Networks & OS", subtitle_style))
    story.append(p("Engine E3 resolves cache misses by forwarding queries to recursive DNS upstreams over non-blocking UDP sockets, enforcing tight timeout thresholds, retries, and cryptographic anti-poisoning defenses.", body_style))
    story.append(p("• <b>Non-Blocking Sockets with select():</b> Outbound UDP client socket uses Winsock2 select() with a 2.0s deadline. Prevents worker threads from freezing indefinitely if upstream drops datagrams.", body_style))
    story.append(p("• <b>Exponential Retry Backoff & Failover:</b> Automatically retries failed queries with exponential backoff before failing over from primary (8.8.8.8:53) to secondary (1.1.1.1:53). If both fail, emits SERVFAIL (RCODE 2).", body_style))
    story.append(p("• <b>Dan Kaminsky Cache Poisoning Defense:</b> Generates a cryptographically random 16-bit Transaction ID (std::mt19937) for every outbound request. Validates inbound responses: if received TxID does not match outbound query TxID, packet is flagged as MISMATCHED_TXID and discarded immediately, neutralizing forged DNS injection attacks.", body_style))
    story.append(p("• <b>RFC 2308 Negative Caching:</b> Upstream NXDOMAIN (RCODE 3) responses are parsed and cached with the SOA minimum TTL, preventing redundant recursive traffic for non-existent domains.", body_style))
    story.append(RLImage('diagrams/engine_e3_upstream_diagram.png', width=487, height=270))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/engine_e3_upstream_diagram.png | {repo_base}src/upstream_engine.cpp", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 9: Engine E4 Deep-Dive (Query Execution VM)
    # =========================================================================
    story.append(p("Engine E4: Query Execution Virtual Machine Engine", h1_style))
    story.append(p("<b>Lead Owner:</b> Joel Biju (01fe24bci021) | <b>Academic Subject:</b> OSPP", subtitle_style))
    story.append(p("Engine E4 is the gravitational orchestrator of the server. It manages the network reactor, coordinates the worker thread pool, and executes the 6-stage instruction pipeline.", body_style))
    story.append(p("• <b>Network Reactor Event Loop:</b> Binds to UDP 127.0.0.1:8053 using a non-blocking Winsock2 socket. Continuously executes recvfrom() to ingest raw 512-byte datagrams.", body_style))
    story.append(p("• <b>Buffer Isolation & Zero Race Conditions:</b> In Stage 1 (FETCH), allocates an isolated std::vector&lt;uint8_t&gt; packet slice for each incoming query. Eliminates socket buffer overwrites between concurrent worker threads.", body_style))
    story.append(p("• <b>Worker Thread Pool Concurrency:</b> A pool of 8 OS worker threads consumes tasks from a thread-safe task queue synchronized via Win32 CRITICAL_SECTION and CONDITION_VARIABLE. Worker threads process transactions concurrently, sustaining 6,538 QPS with 0 dropped packets.", body_style))
    story.append(p("• <b>Deterministic State Machine:</b> Routes transactions through FETCH -> DECODE -> LOOKUP -> RESOLVE -> COMMIT -> EMIT. Implements exact query condition handling (HIT, MISS, EXPIRED, NXDOMAIN, SERVFAIL, FORMERR).", body_style))
    story.append(RLImage('diagrams/concurrency_threadpool_diagram.png', width=487, height=270))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/concurrency_threadpool_diagram.png | {repo_base}src/vm_engine.cpp", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 10: Engine E5 Deep-Dive (Performance Telemetry)
    # =========================================================================
    story.append(p("Engine E5: Performance Telemetry & Analytics Subsystem", h1_style))
    story.append(p("<b>Lead Owner:</b> Joel Biju (01fe24bci021) | <b>Academic Subject:</b> OSPP & Networks", subtitle_style))
    story.append(p("Engine E5 provides microsecond-precision performance analytics, tracking query volume, cache hit ratios, latency percentiles, and live terminal dashboard rendering.", body_style))
    story.append(p("• <b>Lock-Free Atomic Counters:</b> Uses std::atomic&lt;uint64_t&gt; for total_queries, cache_hits, cache_misses, cache_expired, negative_hits, servfails, and formerrs, enabling zero-contention concurrent metric collection.", body_style))
    story.append(p("• <b>Monotonic Steady Clock Timers:</b> std::chrono::steady_clock timestamps each transaction at FETCH and EMIT, recording latency in microseconds. Immune to backwards system clock jumps from NTP synchronization.", body_style))
    story.append(p("• <b>Real-Time Hit Ratio & QPS Analytics:</b> Dynamically calculates hit ratio: Hit Ratio % = (Hits + NegativeHits) / Total * 100. Computes queries-per-second (QPS) over live server uptime.", body_style))
    story.append(p("• <b>Terminal Status Dashboard:</b> Generates formatted ANSI console telemetry displaying live cache hit rate, min/mean/median/max latency histograms, and worker pool utilization.", body_style))
    story.append(RLImage('diagrams/engine_e5_telemetry_diagram.png', width=487, height=270))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/engine_e5_telemetry_diagram.png | {repo_base}src/telemetry_engine.cpp", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 11: Database & Storage Design
    # =========================================================================
    story.append(p("Database Design", h1_style))
    story.append(p("Complete only if the project stores structured data. Draw an ER diagram and list the important tables and fields.", body_style))
    story.append(p("Entity Relationship (ER) Diagram", h2_style))
    story.append(RLImage('diagrams/cache_storage_er_diagram.png', width=487, height=270))
    story.append(Spacer(1, 4))

    t10_data = [
        ("Table / entity", "Key fields", "Relation / purpose"),
        ("CacheKey (Composite PK Index)", "domain (string PK), qtype (uint16_t PK), qclass (uint16_t PK)", "Primary hash index in std::unordered_map; maps 1:1 to a CacheEntry node in O(1) time; custom hash functor combines domain name, query type, and query class."),
        ("CacheEntry (LRU Recency Node)", "key (CacheKey FK), records (vector<DNS_RR>), original_ttl (uint32_t), expires_at (time_point), is_negative (bool), rcode (uint8_t), version (uint64_t), prev/next", "Main memory storage entity; intrusive doubly-linked list (std::list) tracks access order; head = MRU, tail = LRU for O(1) eviction."),
        ("MinHeapNode (Proactive TTL Queue)", "expires_at (time_point PK), key (CacheKey FK), version (uint64_t)", "Organized in a binary Min-Heap (std::priority_queue with std::greater); root points to earliest expiring record for proactive O(log N) purge."),
        ("DNSResourceRecord (Wire Record)", "name (string), type (uint16_t), rclass (uint16_t), ttl (uint32_t), rdlength (uint16_t), rdata (vector<uint8_t>), readable_data", "Represents individual DNS answers (A, AAAA, CNAME); 1:N relationship with CacheEntry; holds parsed IP bytes and dynamic TTL."),
        ("DiskSnapshot (dns_cache.db)", "magic_header ('DCDB' PK), record_count (uint32_t), records_stream (binary)", "Flat binary disk storage format; captures valid unexpired cache state upon graceful server shutdown (SIGINT) and re-populates memory at boot."),
        ("TelemetrySnapshot (KPI Analytics)", "total_queries (uint64_t), cache_hits (uint64_t), cache_misses (uint64_t), hit_ratio_pct (double), avg_latency_us (double), qps (double)", "Atomic performance metric registry tracking server health, throughput, and sub-millisecond response latency.")
    ]
    story.append(make_table(t10_data, [125, 142, 220]))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/cache_storage_er_diagram.png | {repo_base}diagrams/cache_storage_er_diagram.png", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 12: Algorithm and Data Structure (WITH COMPLETE FLOWCHART & RIGOROUS COMPLEXITIES)
    # =========================================================================
    story.append(p("Algorithm and Data Structure", h1_style))
    story.append(p("For each engine that uses a significant algorithm, briefly show the steps and why the method is suitable.", body_style))

    # Rigorous Corrected Complexity Table
    t11_data = [
        ("Engine ID", "Algorithm / Data Structure", "Time Complexity", "Space Complexity", "Technical Justification"),
        ("E1", "Recursive Pointer Decompression with Depth Bound (depth <= 16)", "O(B) bounded by depth <= 16", "O(D) = O(1) stack frames", "Parses RFC 1035 wire labels. Max depth 16 guarantees strict O(1) stack usage and halts cyclic pointer DoS attacks."),
        ("E2", "Hash Table (std::unordered_map) Key-Value Index", "Average: O(1)\nWorst: O(N)", "O(N) bucket table", "64-bit composite hash function minimizes collisions, yielding instantaneous O(1) average lookup time."),
        ("E2", "Doubly-Linked List (std::list) LRU Recency Tracking", "Strict O(1) splice\nStrict O(1) evict", "O(N) node pointers", "Splices accessed nodes to MRU Head in O(1) without memory reallocation; pops LRU Tail in O(1) at capacity limit."),
        ("E2", "Binary Min-Heap (std::priority_queue) Proactive Expiry", "Check: O(1)\nPurge: O(K log N)", "O(N) heap array", "Root node holds earliest expiring record. Purging K expired records requires O(K log N), eliminating O(N) table scans."),
        ("E3", "Non-Blocking select() Socket Polling + TxID Matching", "O(1) algorithmic +\nbounded network RTT", "O(1) socket buffer", "2.0s deadline prevents worker thread starvation; random 16-bit TxID matching provides O(1) Kaminsky spoof rejection."),
        ("E4", "6-Stage Instruction VM Pipeline State Machine", "Hit: O(1) average\nMiss: O(log N) + RTT", "O(1) per-query slice", "Decouples protocol, storage, and networking; allows 8 worker threads to process requests concurrently at 6,538 QPS.")
    ]
    story.append(make_table(t11_data, [45, 125, 80, 75, 162]))
    story.append(Spacer(1, 4))

    story.append(p("Complete DNS Resolution Flowchart (Algorithm Steps)", h2_style))
    story.append(RLImage('diagrams/dns_resolution_flowchart.png', width=487, height=330))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> diagrams/dns_resolution_flowchart.png | {repo_base}diagrams/dns_resolution_flowchart.png", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 13: Security, Errors and Testing
    # =========================================================================
    story.append(p("Security, Errors and Testing", h1_style))
    story.append(p("Show important permissions, validation and error responses. Explain how you will check the design.", body_style))

    t13_data = [
        ("Area", "Simple student explanation"),
        ("Who can access or change important data?",
         "Access to the in-memory cache data structures (std::unordered_map, std::list, std::priority_queue) is synchronized using native Windows SRWLOCK (Slim Reader/Writer Lock) via C++ RAII guard classes (ReadLockGuard and WriteLockGuard). Multiple worker threads are granted concurrent shared read access (AcquireSRWLockShared) during Stage 3 LOOKUP without contention. Mutation operations during Stage 5 COMMIT, LRU capacity eviction, and TTL purge require exclusive lock acquisition (AcquireSRWLockExclusive), guaranteeing data integrity, zero race conditions, and complete thread safety. External network clients can only query the server via UDP datagrams on port 8053 and have no direct memory or administrative access."),
        ("What happens for invalid input or a failed request?",
         "The system handles all edge and failure conditions gracefully without crashing or hanging:<br/>"
         "1. Malformed Datagrams / Packets &lt; 12 Bytes: Sub-12-byte packets are dropped immediately; corrupted headers return FORMERR (RCODE 1, Format Error) with the client's Transaction ID.<br/>"
         "2. Cyclic Compression Pointers: If label decompression exceeds depth 16 or points out-of-bounds, decompression halts and emits FORMERR.<br/>"
         "3. Non-Existent Domains (NXDOMAIN): When upstream returns NXDOMAIN (RCODE 3), the server implements RFC 2308 negative caching using the SOA minimum TTL to suppress repeated upstream lookups and returns NXDOMAIN to the client.<br/>"
         "4. Upstream Resolution Failure / Timeout: If upstream queries exceed 2.0s across 2 retries and Cloudflare failover, the server returns SERVFAIL (RCODE 2, Server Failure).<br/>"
         "5. Mismatched Transaction ID: Upstream packets with spoofed TxIDs are discarded as Kaminsky attacks; original resolution is retried."),
        ("Is copying of large data necessary, and who owns it?",
         "No large copying is necessary. DNS UDP datagrams are strictly constrained to 512 bytes under standard RFC 1035 rules. At Stage 1 (FETCH), the reactor thread allocates a dedicated 512-byte buffer slice for the incoming datagram, preventing socket buffer overwrite races. Within the pipeline, parsing utilizes lightweight non-owning pointers and string references. Engine E2 exclusively owns the persistent in-memory heap records; Engine E4 owns the request transaction context (VMTransaction), which is automatically cleaned up via RAII as soon as the response is transmitted in Stage 6 (EMIT)."),
        ("How will you test the main functions?",
         "The system is rigorously validated across four distinct testing tiers:<br/>"
         "1. Modular Unit Testing: Four isolated C++ test suites validating individual engines: test_protocol.cpp (10 tests: header parsing, label decompression, recursion guard), test_cache.cpp (8 tests: O(1) LRU splice, Min-Heap TTL purge, dynamic aging, thread safety), test_upstream.cpp (6 tests: UDP select() timeouts, retry logic, TxID validation), and test_vm_integration.cpp (6 tests: full 6-stage pipeline transitions).<br/>"
         "2. Industry Standard DNS Client Verification: Live query verification using dig @127.0.0.1 -p 8053 example.com A and nslookup -port=8053 example.com 127.0.0.1, verifying RFC 1035 packet conformity, answer fidelity, and TTL decrement countdowns.<br/>"
         "3. Concurrency Stress Benchmarking: Multi-threaded Python stress benchmark (benchmark_stress.py) testing pure cache hits, pure misses, and 80/20 mixed query workloads under concurrent client threads (1,000 queries sustaining 6,538 QPS, median cache-hit latency of 0.414 ms, demonstrating a 220.9x speedup over upstream miss latency).<br/>"
         "4. Real-Time Telemetry Dashboard: Microsecond timer auditing (std::chrono::steady_clock), real-time hit ratio tracking, and verification of zero packet drops under sustained load.")
    ]
    story.append(make_table(t13_data, [130, 357]))
    story.append(Spacer(1, 4))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> benchmarks/BENCHMARK_REPORT.md | {repo_base}benchmarks/BENCHMARK_REPORT.md", link_style))
    story.append(PageBreak())

    # =========================================================================
    # PAGE 14: Empirical Testing & Concurrency Benchmark Results (WITH ARTIFACT EVIDENCE)
    # =========================================================================
    story.append(p("Empirical Testing, Benchmarks & Supporting Evidence", h1_style))
    story.append(p("All performance claims are supported by concrete empirical test logs and JSON artifacts in benchmarks/:", body_style))
    
    story.append(p("1. Automated Modular C++ Unit Test Verification (30 / 30 Passed)", h2_style))
    story.append(p("• <b>Engine E1 Test Suite (tests/test_protocol.cpp):</b> 10 / 10 Tests Passed (Bitfield flags, domain normalization, pointer decompression, cyclic loop detection, IPv4/IPv6 packing).<br/>"
                   "• <b>Engine E2 Test Suite (tests/test_cache.cpp):</b> 8 / 8 Tests Passed (PUT/GET hit, miss, dynamic TTL decay, O(1) LRU eviction, O(log N) Min-Heap proactive purge, SRWLOCK concurrency, snapshot durability).<br/>"
                   "• <b>Engine E3 Test Suite (tests/test_upstream.cpp):</b> 6 / 6 Tests Passed (Socket init, cryptographic TxID generation, select() timeout, recursive resolution, Kaminsky spoof drop, 1.1.1.1 failover).<br/>"
                   "• <b>Engine E4/E5 Integration Suite (tests/test_vm_integration.cpp):</b> 6 / 6 Tests Passed (Cache miss to upstream, cache hit speedup, FORMERR on malformed wire, RFC 2308 NXDOMAIN, telemetry integration).", body_style))

    story.append(p("2. High-Concurrency Stress Benchmark Artifacts (Evidence Data)", h2_style))
    story.append(p("Evidence files: <b>benchmarks/benchmark_hit_1000.json</b> and <b>benchmarks/benchmark_mixed.json</b>. Benchmark command executed: <i>python scripts/benchmark_stress.py --mode hit --queries 1000 --threads 20</i>.", body_style))
    
    t_bench_data = [
        ("Empirical Parameter", "Pure In-Memory Cache Hit (RAM)", "Mixed Workload (80% Hit / 20% Miss)", "Upstream Recursive WAN Miss"),
        ("Source JSON Evidence", "benchmarks/benchmark_hit_1000.json", "benchmarks/benchmark_mixed.json", "benchmarks/benchmark_mixed.json"),
        ("Throughput (QPS)", "6,538.64 queries/sec", "955.24 queries/sec", "~6.5 queries/sec"),
        ("Wall Clock Duration", "0.1529 seconds (1,000 queries)", "0.5234 seconds (500 queries)", "15.4 seconds (estimated)"),
        ("Completed / Failed", "1,000 / 0 (100.0% completion)", "500 / 0 (100.0% completion)", "100.0% completion"),
        ("Minimum Latency", "0.1425 ms (142.5 μs)", "0.1632 ms (163.2 μs)", "103.587 ms"),
        ("Median Latency (p50)", "0.4145 ms (414.5 μs)", "0.4283 ms (overall) / 0.421 ms (hits)", "146.356 ms"),
        ("Mean Latency", "0.7170 ms (all) / 0.6976 ms (hits)", "7.4716 ms (overall) / 0.700 ms (hits)", "154.606 ms"),
        ("p95 Latency", "2.4336 ms", "6.1632 ms", "168.400 ms"),
        ("p99 Latency", "4.8293 ms", "158.459 ms (reflects WAN misses)", "182.120 ms"),
        ("Speedup vs Upstream", "220.9x FASTER (154.61ms / 0.70ms)", "220.9x FASTER (on cache hits)", "Baseline Reference (1x)")
    ]
    story.append(make_table(t_bench_data, [95, 130, 137, 125]))
    story.append(Spacer(1, 4))

    story.append(p("3. Final Conclusion & Submission Readiness", h2_style))
    story.append(p("The C++ DNS Resolver and Caching Server fulfills all requirements of Sprint 1. The architecture exhibits complete separation of concerns across engines E1–E5, delivers empirical sub-millisecond median response times (0.414 ms), prevents cyclic pointer and Kaminsky spoofing vulnerabilities, and sustains 6,538 QPS with 100% reliability.", body_style))
    story.append(p(f"<b>Diagram / evidence filename or repository link:</b> benchmarks/BENCHMARK_REPORT.md | {repo_base}benchmarks/BENCHMARK_REPORT.md", link_style))

    doc.build(story, canvasmaker=NumberedCanvas)
    print("PDF_14_PAGE_BUILD_COMPLETE")

build_pdf()
