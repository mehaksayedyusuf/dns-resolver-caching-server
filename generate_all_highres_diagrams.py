import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
import os
import shutil

# Ensure output directories exist
os.makedirs('diagrams', exist_ok=True)
os.makedirs('docs/diagrams', exist_ok=True)

# -----------------------------------------------------------------------------
# 1. ULTRA HIGH-RES SYSTEM ORBIT ARCHITECTURE DIAGRAM
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(16, 12), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(-16, 16)
ax.set_ylim(-13, 13)
ax.axis('off')

# Main Header
ax.text(0, 11.8, "DNS RESOLVER & CACHING SERVER — SYSTEM ORBIT ARCHITECTURE", 
        color='#F8FAFC', fontsize=19, fontweight='bold', ha='center', va='center')
ax.text(0, 10.9, "C++ Core Engine Decomposition (PES University S1 Handbook Aligned)", 
        color='#94A3B8', fontsize=12.5, ha='center', va='center')

# Orbit rings with glow effect
orbit1 = plt.Circle((0, 0), 7.0, color='#1E293B', fill=False, linestyle='--', linewidth=2.2, alpha=0.8)
orbit2 = plt.Circle((0, 0), 11.2, color='#334155', fill=False, linestyle=':', linewidth=1.8, alpha=0.6)
ax.add_patch(orbit1)
ax.add_patch(orbit2)

# Central Core: Engine E4 VM
core_circle = plt.Circle((0, 0), 3.4, color='#1E1B4B', ec='#6366F1', linewidth=3.5, zorder=3)
ax.add_patch(core_circle)
ax.text(0, 1.4, "ENGINE E4 (CORE)", color='#818CF8', fontsize=12, fontweight='bold', ha='center', va='center', zorder=4)
ax.text(0, 0.5, "Query Execution VM", color='#FFFFFF', fontsize=15, fontweight='bold', ha='center', va='center', zorder=4)
ax.text(0, -0.6, "6-Stage Instruction Pipeline\nFETCH ➔ DECODE ➔ LOOKUP\nRESOLVE ➔ COMMIT ➔ EMIT", 
        color='#C7D2FE', fontsize=10, ha='center', va='center', zorder=4)
ax.text(0, -2.1, "[Worker Thread Pool Dispatcher]", color='#A5B4FC', fontsize=9, fontstyle='italic', ha='center', va='center', zorder=4)

# Satellites on Orbit 1
satellites = [
    {
        "name": "ENGINE E1\nProtocol Parsing Engine",
        "detail": "• RFC 1035 Wire Unpacker\n• Label Pointer Decompression\n  (Loop Guard Depth <= 16)\n• Endianness (htons/ntohs)\n• Response Wire Serializer",
        "angle": 135, "color": "#064E3B", "ec": "#10B981", "badge": "Owner: Vageesh Mathad", "badge_col": "#34D399"
    },
    {
        "name": "ENGINE E2\nIn-Memory Storage & Cache",
        "detail": "• Hash Table + Doubly-Linked List\n  (O(1) LRU Eviction)\n• Min-Heap Priority Queue\n  (O(log N) Proactive TTL Purge)\n• Dynamic TTL Aging (TTL_orig - Δt)\n• Win32 SRWLOCK Concurrency",
        "angle": 45, "color": "#14532D", "ec": "#22C55E", "badge": "Owner: Mehak Sayed Yusuf", "badge_col": "#4ADE80"
    },
    {
        "name": "ENGINE E3\nUpstream Resolution Engine",
        "detail": "• Non-Blocking Sockets (select())\n• 2.0s Query Timeout Cap\n• Exponential Retry Backoff\n• Randomized 16-bit TxID Check\n• Kaminsky Poisoning Defense",
        "angle": 225, "color": "#701A75", "ec": "#C026D3", "badge": "Owner: Ishani G", "badge_col": "#E879F9"
    },
    {
        "name": "TELEMETRY & KPIs\nPerformance Subsystem",
        "detail": "• Real-Time Hit Ratio Tracking\n• Microsecond Latency Histograms\n• Query Rate (QPS) Benchmarking\n• Live Console Status Dashboard",
        "angle": 315, "color": "#7C2D12", "ec": "#EA580C", "badge": "Owner: Joel Biju", "badge_col": "#FB923C"
    }
]

for sat in satellites:
    rad = np.radians(sat["angle"])
    x = 7.0 * np.cos(rad)
    y = 7.0 * np.sin(rad)
    
    # Connecting vector line
    ax.annotate("", xy=(x*0.48, y*0.48), xytext=(x*0.84, y*0.84),
                arrowprops=dict(arrowstyle="<|-|>", color=sat["ec"], lw=2.2, mutation_scale=15))
    
    # Satellite box
    box = patches.FancyBboxPatch((x - 3.0, y - 2.0), 6.0, 4.0,
                                boxstyle="round,pad=0.35", fc=sat["color"], ec=sat["ec"], lw=2.5, zorder=5)
    ax.add_patch(box)
    ax.text(x, y + 1.3, sat["name"], color='#FFFFFF', fontsize=11.5, fontweight='bold', ha='center', va='center', zorder=6)
    ax.text(x, y + 0.1, sat["detail"], color='#E2E8F0', fontsize=8.5, ha='center', va='center', zorder=6)
    ax.text(x, y - 1.45, sat["badge"], color=sat["badge_col"], fontsize=9, fontweight='bold', ha='center', va='center', zorder=6)

# Outer Actors (Orbit 2)
# DNS Client
c_x, c_y = -11.6, 0
client_box = patches.FancyBboxPatch((c_x - 2.6, c_y - 1.5), 5.2, 3.0,
                                    boxstyle="round,pad=0.25", fc='#0F172A', ec='#38BDF8', lw=2.5, zorder=5)
ax.add_patch(client_box)
ax.text(c_x, c_y + 0.7, "DNS CLIENTS", color='#38BDF8', fontsize=12, fontweight='bold', ha='center', va='center', zorder=6)
ax.text(c_x, c_y - 0.4, "Standard DNS Tools\n(dig / nslookup / stub)\nUDP Port 8053", color='#94A3B8', fontsize=9.5, ha='center', va='center', zorder=6)

# External Upstream DNS
u_x, u_y = 11.6, 0
up_box = patches.FancyBboxPatch((u_x - 2.6, u_y - 1.5), 5.2, 3.0,
                               boxstyle="round,pad=0.25", fc='#0F172A', ec='#F43F5E', lw=2.5, zorder=5)
ax.add_patch(up_box)
ax.text(u_x, u_y + 0.7, "UPSTREAM DNS", color='#F43F5E', fontsize=12, fontweight='bold', ha='center', va='center', zorder=6)
ax.text(u_x, u_y - 0.4, "Recursive Resolvers\n(8.8.8.8 / 1.1.1.1)\nUDP Port 53", color='#94A3B8', fontsize=9.5, ha='center', va='center', zorder=6)

# Disk Persistence
d_x, d_y = 0, -11.0
disk_box = patches.FancyBboxPatch((d_x - 3.0, d_y - 1.1), 6.0, 2.2,
                                 boxstyle="round,pad=0.25", fc='#0F172A', ec='#EAB308', lw=2.5, zorder=5)
ax.add_patch(disk_box)
ax.text(d_x, d_y + 0.4, "DISK SNAPSHOT (dns_cache.db)", color='#EAB308', fontsize=11, fontweight='bold', ha='center', va='center', zorder=6)
ax.text(d_x, d_y - 0.4, "Graceful Dump on SIGINT & Boot Warm-up", color='#94A3B8', fontsize=8.5, ha='center', va='center', zorder=6)

# Flow connection arrows
ax.annotate("", xy=(-3.4, 0), xytext=(-9.0, 0),
            arrowprops=dict(arrowstyle="<|-|>", color='#38BDF8', lw=2.5, mutation_scale=16))
ax.annotate("", xy=(9.0, 0), xytext=(4.5, -4.2),
            arrowprops=dict(arrowstyle="<|-|>", color='#F43F5E', lw=2.5, mutation_scale=16))
ax.annotate("", xy=(0, -9.9), xytext=(0, -3.4),
            arrowprops=dict(arrowstyle="<|-|>", color='#EAB308', lw=2.2, linestyle='--', mutation_scale=16))

plt.tight_layout()
plt.savefig('diagrams/system_orbit_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

# -----------------------------------------------------------------------------
# 2. ULTRA HIGH-RES 6-STAGE VM PIPELINE DIAGRAM
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(17, 7.5), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(0, 18)
ax.set_ylim(0, 8)
ax.axis('off')

ax.text(9, 7.2, "ENGINE E4: QUERY EXECUTION VM — 6-STAGE INSTRUCTION PIPELINE", 
        color='#F8FAFC', fontsize=17, fontweight='bold', ha='center', va='center')
ax.text(9, 6.4, "Deterministic Lifecycle & State Machine for Every Inbound DNS Query", 
        color='#94A3B8', fontsize=11.5, ha='center', va='center')

stages = [
    {"num": "STAGE 1", "name": "FETCH", "desc": "• Datagram Ingestion\n• Port 8053 UDP Socket\n• Heap Buffer Isolation\n• Enqueue to Workers", "col": "#0284C7", "eng": "Reactor Loop"},
    {"num": "STAGE 2", "name": "DECODE", "desc": "• 12-byte Header Parse\n• QNAME Label Decode\n• Loop Guard (Depth <= 16)\n• FORMERR on Malformed", "col": "#059669", "eng": "Engine E1"},
    {"num": "STAGE 3", "name": "LOOKUP", "desc": "• Key: (Domain, Type, Class)\n• SRWLOCK Shared Read\n• Dynamic TTL Recalculate\n• LRU Node Promotion", "col": "#16A34A", "eng": "Engine E2"},
    {"num": "STAGE 4", "name": "RESOLVE", "desc": "• On Cache Miss / Expire\n• Forward 8.8.8.8 UDP\n• Non-blocking select()\n• Kaminsky TxID Validate", "col": "#9333EA", "eng": "Engine E3"},
    {"num": "STAGE 5", "name": "COMMIT", "desc": "• SRWLOCK Exclusive Write\n• Insert Hash Map + LRU Head\n• Min-Heap Expiration Push\n• O(1) Tail Eviction at Cap", "col": "#D97706", "eng": "Engine E2"},
    {"num": "STAGE 6", "name": "EMIT", "desc": "• RFC 1035 Response Build\n• 0xC00C Compression\n• Transmit via sendto()\n• Telemetry Timing Record", "col": "#E11D48", "eng": "Engine E1"}
]

box_w = 2.4
gap = 0.45
start_x = 0.75

for i, st in enumerate(stages):
    bx = start_x + i * (box_w + gap)
    by = 1.6
    
    p = patches.FancyBboxPatch((bx, by), box_w, 4.0, boxstyle="round,pad=0.2",
                               fc='#111827', ec=st["col"], lw=2.8)
    ax.add_patch(p)
    
    ax.text(bx + box_w/2, by + 3.4, st["num"], color=st["col"], fontsize=10, fontweight='bold', ha='center')
    ax.text(bx + box_w/2, by + 2.8, st["name"], color='#FFFFFF', fontsize=14, fontweight='bold', ha='center')
    ax.text(bx + box_w/2, by + 1.5, st["desc"], color='#CBD5E1', fontsize=8.5, ha='center', va='center')
    ax.text(bx + box_w/2, by + 0.45, f"[{st['eng']}]", color=st["col"], fontsize=8.5, fontstyle='italic', ha='center')
    
    if i < len(stages) - 1:
        ax.annotate("", xy=(bx + box_w + gap - 0.05, by + 2.0), xytext=(bx + box_w + 0.05, by + 2.0),
                    arrowprops=dict(arrowstyle="-|>", color='#64748B', lw=2.5, mutation_scale=15))

plt.tight_layout()
plt.savefig('diagrams/engine_pipeline_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

# -----------------------------------------------------------------------------
# 3. ULTRA HIGH-RES CACHE DUAL-INDEX ARCHITECTURE DIAGRAM
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(16, 9.5), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(0, 16)
ax.set_ylim(0, 9.5)
ax.axis('off')

ax.text(8, 8.8, "ENGINE E2: DUAL-INDEX IN-MEMORY CACHE STORAGE ARCHITECTURE", 
        color='#F8FAFC', fontsize=17, fontweight='bold', ha='center', va='center')
ax.text(8, 8.0, "O(1) LRU Eviction Subsystem alongside O(log N) Proactive Min-Heap TTL Expiration", 
        color='#94A3B8', fontsize=11.5, ha='center', va='center')

# Left Box: Hash Map + Doubly-Linked List
box1 = patches.FancyBboxPatch((0.8, 1.2), 6.8, 6.2, boxstyle="round,pad=0.25",
                             fc='#111827', ec='#22C55E', lw=2.5)
ax.add_patch(box1)
ax.text(4.2, 6.7, "O(1) LRU Eviction Subsystem", color='#4ADE80', fontsize=14, fontweight='bold', ha='center')
ax.text(4.2, 5.9, "std::unordered_map<CompositeKey, Node*>", color='#86EFAC', fontsize=10, family='monospace', ha='center')
ax.text(4.2, 4.4, "• Composite Key: (Domain + QTYPE + QCLASS)\n• Doubly-Linked Recency List (std::list)\n• Node Splice to Head on HIT: O(1)\n• Tail Node Eviction at Capacity Limit: O(1)\n• Zero Linear Memory Scans", 
        color='#E2E8F0', fontsize=9.5, ha='center', va='center')
ax.text(4.2, 2.2, "Concurrency: Win32 SRWLOCK (Read/Write)", color='#FBBF24', fontsize=9.5, fontweight='bold', ha='center')

# Right Box: Min-Heap TTL Expiration
box2 = patches.FancyBboxPatch((8.4, 1.2), 6.8, 6.2, boxstyle="round,pad=0.25",
                             fc='#111827', ec='#3B82F6', lw=2.5)
ax.add_patch(box2)
ax.text(11.8, 6.7, "O(log N) Proactive TTL Purge", color='#60A5FA', fontsize=14, fontweight='bold', ha='center')
ax.text(11.8, 5.9, "std::priority_queue<TTLNode, MinComparator>", color='#93C5FD', fontsize=10, family='monospace', ha='center')
ax.text(11.8, 4.4, "• Priority Key: Absolute Expiration Timestamp\n• Root Node = Earliest Expiring Record\n• Lazy + Active Heap Sweep: O(log N)\n• Dynamic TTL Aging: TTL_rem = TTL_orig - Δt\n• Unexpired Cache Validity Check in O(1)", 
        color='#E2E8F0', fontsize=9.5, ha='center', va='center')
ax.text(11.8, 2.2, "Durability: Binary Snapshot to dns_cache.db", color='#FBBF24', fontsize=9.5, fontweight='bold', ha='center')

plt.tight_layout()
plt.savefig('diagrams/cache_lru_minheap_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

# -----------------------------------------------------------------------------
# 4. ULTRA HIGH-RES WIRE PROTOCOL & DECOMPRESSION DIAGRAM
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(16, 9.5), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(0, 16)
ax.set_ylim(0, 9.5)
ax.axis('off')

ax.text(8, 8.8, "ENGINE E1: RFC 1035 WIRE PROTOCOL & POINTER DECOMPRESSION", 
        color='#F8FAFC', fontsize=17, fontweight='bold', ha='center', va='center')
ax.text(8, 8.0, "12-Byte Header Bitfields, Big-Endian Packing, and 16-Depth Cyclic Loop Guard", 
        color='#94A3B8', fontsize=11.5, ha='center', va='center')

# Left Box: DNS Header Layout
h_box = patches.FancyBboxPatch((0.8, 1.2), 6.8, 6.2, boxstyle="round,pad=0.25",
                              fc='#111827', ec='#06B6D4', lw=2.5)
ax.add_patch(h_box)
ax.text(4.2, 6.7, "RFC 1035 12-Byte Header Bitfields", color='#22D3EE', fontsize=13.5, fontweight='bold', ha='center')
ax.text(4.2, 5.0, "Bytes 0-1: Transaction ID (16-bit matching)\nBytes 2-3: Flags (QR, Opcode, AA, TC, RD, RA, RCODE)\nBytes 4-5: QDCOUNT (Question Count = 1)\nBytes 6-7: ANCOUNT (Answer Count)\nBytes 8-9: NSCOUNT (Authority Count)\nBytes 10-11: ARCOUNT (Additional Count)", 
        color='#E2E8F0', fontsize=9.5, family='monospace', ha='center', va='center')
ax.text(4.2, 2.2, "Endianness: Big-Endian Wire Network Byte Order", color='#38BDF8', fontsize=9.5, fontweight='bold', ha='center')

# Right Box: Pointer Decompression & Security Guard
p_box = patches.FancyBboxPatch((8.4, 1.2), 6.8, 6.2, boxstyle="round,pad=0.25",
                              fc='#111827', ec='#F43F5E', lw=2.5)
ax.add_patch(p_box)
ax.text(11.8, 6.7, "Label Pointer Decompression & Security", color='#FB7185', fontsize=13.5, fontweight='bold', ha='center')
ax.text(11.8, 5.0, "• Normal Label: 1-byte length L followed by L chars\n• Compression Pointer: 2 bytes starting with 0xC0\n• 14-bit Pointer Offset: (byte[0] & 0x3F) << 8 | byte[1]\n• 0xC00C: Points to offset 12 (QNAME in Question)\n• Recursion Guard: Max dereference depth = 16\n• Cyclic Pointer Loop: Detected -> FORMERR (RCODE 1)", 
        color='#E2E8F0', fontsize=9.5, family='monospace', ha='center', va='center')
ax.text(11.8, 2.2, "Security: Defeats Denial-of-Service Loop Exploits", color='#FDA4AF', fontsize=9.5, fontweight='bold', ha='center')

plt.tight_layout()
plt.savefig('diagrams/wire_protocol_format_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

# -----------------------------------------------------------------------------
# 5. ULTRA HIGH-RES CONCURRENCY & THREAD POOL DIAGRAM
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(16, 9.5), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(0, 16)
ax.set_ylim(0, 9.5)
ax.axis('off')

ax.text(8, 8.8, "CONCURRENCY ENGINE: NETWORK REACTOR & WORKER THREAD POOL", 
        color='#F8FAFC', fontsize=17, fontweight='bold', ha='center', va='center')
ax.text(8, 8.0, "Multi-Threaded Architecture with Isolated Packet Slices and Win32 SRWLOCK", 
        color='#94A3B8', fontsize=11.5, ha='center', va='center')

# Reactor Box
r_box = patches.FancyBboxPatch((0.8, 1.2), 4.2, 6.2, boxstyle="round,pad=0.25",
                              fc='#111827', ec='#818CF8', lw=2.5)
ax.add_patch(r_box)
ax.text(2.9, 6.7, "Network Reactor Thread", color='#A5B4FC', fontsize=13, fontweight='bold', ha='center')
ax.text(2.9, 4.8, "• Winsock2 UDP Socket\n• Bound to 127.0.0.1:8053\n• Non-blocking recvfrom()\n• Buffer Isolation:\n  Copies 512-byte slice\n  to prevent data race\n• Enqueues Task to Pool", 
        color='#E2E8F0', fontsize=9.5, ha='center', va='center')
ax.text(2.9, 2.2, "Main Event Loop", color='#C7D2FE', fontsize=9.5, fontstyle='italic', ha='center')

# Queue & Thread Pool Box
t_box = patches.FancyBboxPatch((5.6, 1.2), 4.8, 6.2, boxstyle="round,pad=0.25",
                              fc='#111827', ec='#F59E0B', lw=2.5)
ax.add_patch(t_box)
ax.text(8.0, 6.7, "Worker Thread Pool", color='#FBBF24', fontsize=13, fontweight='bold', ha='center')
ax.text(8.0, 4.8, "• Thread-Safe Task Queue\n• Mutex + Condition Variable\n• Pool of N Worker Threads\n• Concurrent Pipeline Exec:\n  Worker 1: Query A\n  Worker 2: Query B\n  Worker N: Query ...\n• 0% Socket Buffer Overrun", 
        color='#E2E8F0', fontsize=9.5, ha='center', va='center')
ax.text(8.0, 2.2, "High-Throughput Concurrency", color='#FDE68A', fontsize=9.5, fontstyle='italic', ha='center')

# Synchronization Box
s_box = patches.FancyBboxPatch((11.0, 1.2), 4.2, 6.2, boxstyle="round,pad=0.25",
                              fc='#111827', ec='#10B981', lw=2.5)
ax.add_patch(s_box)
ax.text(13.1, 6.7, "Synchronization (SRWLOCK)", color='#34D399', fontsize=13, fontweight='bold', ha='center')
ax.text(13.1, 4.8, "• Win32 Slim Reader/Writer\n• Shared Read Lock:\n  Concurrent Cache GETs\n• Exclusive Write Lock:\n  Atomic Cache SET / Evict\n• Zero Race Conditions\n• Minimal Lock Contention", 
        color='#E2E8F0', fontsize=9.5, ha='center', va='center')
ax.text(13.1, 2.2, "100% Thread Safety", color='#6EE7B7', fontsize=9.5, fontstyle='italic', ha='center')

# Connecting arrows
ax.annotate("", xy=(5.5, 4.3), xytext=(5.1, 4.3),
            arrowprops=dict(arrowstyle="-|>", color='#F59E0B', lw=2.5, mutation_scale=15))
ax.annotate("", xy=(10.9, 4.3), xytext=(10.5, 4.3),
            arrowprops=dict(arrowstyle="<|-|>", color='#10B981', lw=2.5, mutation_scale=15))

plt.tight_layout()
plt.savefig('diagrams/concurrency_threadpool_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

# Mirror all generated diagrams to docs/diagrams as well
for f in os.listdir('diagrams'):
    if f.endswith('.png'):
        shutil.copy(os.path.join('diagrams', f), os.path.join('docs/diagrams', f))

print("ALL_5_HIGHRES_DIAGRAMS_GENERATED_SUCCESSFULLY")
