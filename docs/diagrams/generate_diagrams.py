import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
import os

os.makedirs('docs/diagrams', exist_ok=True)

# -------------------------------------------------------------
# 1. SYSTEM ORBIT DIAGRAM
# -------------------------------------------------------------
fig, ax = plt.subplots(figsize=(14, 11), facecolor='#0B0F19')
ax.set_facecolor('#0B0F19')
ax.set_xlim(-15, 15)
ax.set_ylim(-12, 12)
ax.axis('off')

# Title
ax.text(0, 10.8, "DNS RESOLVER & CACHING SERVER — ORBIT ARCHITECTURE", 
        color='#F3F4F6', fontsize=18, fontweight='bold', ha='center', va='center')
ax.text(0, 9.9, "C++ Core Engine Decomposition (S1 Handbook Aligned)", 
        color='#9CA3AF', fontsize=12, ha='center', va='center')

# Orbit rings
orbit1 = plt.Circle((0, 0), 6.5, color='#1F2937', fill=False, linestyle='--', linewidth=2, alpha=0.7)
orbit2 = plt.Circle((0, 0), 10.2, color='#374151', fill=False, linestyle=':', linewidth=1.5, alpha=0.5)
ax.add_patch(orbit1)
ax.add_patch(orbit2)

# Central Core (Engine E4 VM)
core_circle = plt.Circle((0, 0), 3.2, color='#1E1B4B', ec='#6366F1', linewidth=3, zorder=3)
ax.add_patch(core_circle)
ax.text(0, 1.2, "ENGINE E4", color='#818CF8', fontsize=11, fontweight='bold', ha='center', va='center', zorder=4)
ax.text(0, 0.4, "Query Execution VM", color='#FFFFFF', fontsize=14, fontweight='bold', ha='center', va='center', zorder=4)
ax.text(0, -0.6, "State Machine Pipeline\nFETCH ➔ DECODE ➔ LOOKUP\nRESOLVE ➔ COMMIT ➔ EMIT", 
        color='#C7D2FE', fontsize=9.5, ha='center', va='center', zorder=4)
ax.text(0, -2.1, "[Worker Thread Pool Dispatcher]", color='#A5B4FC', fontsize=8.5, fontstyle='italic', ha='center', va='center', zorder=4)

# Satellites on Orbit 1
satellites = [
    {
        "name": "ENGINE E1\nProtocol Parsing Engine",
        "detail": "RFC 1035 Wire Format Unpacker\nLabel Pointer Decompression (Loop Guard 16)\nEndianness Conversion (htons/ntohs)\nDNS Response Wire Serializer",
        "angle": 135, "color": "#064E3B", "ec": "#10B981", "badge": "Owner: Vageesh", "badge_col": "#34D399"
    },
    {
        "name": "ENGINE E2\nIn-Memory Cache Engine",
        "detail": "Hash Map + Doubly-Linked List (O(1) LRU)\nMin-Heap Priority Queue (O(log N) TTL)\nDynamic TTL Decrement (TTL_orig - Δt)\nSRWLOCK Reader-Writer Concurrency",
        "angle": 45, "color": "#14532D", "ec": "#22C55E", "badge": "Owner: Mehak", "badge_col": "#4ADE80"
    },
    {
        "name": "ENGINE E3\nUpstream Resolution Engine",
        "detail": "Non-Blocking UDP Sockets (Select/Timeout)\nExponential Retry Backoff\nRandomized 16-bit TxID Matching\nKaminsky Cache Poisoning Defense",
        "angle": 225, "color": "#701A75", "ec": "#C026D3", "badge": "Owner: Ishani", "badge_col": "#E879F9"
    },
    {
        "name": "TELEMETRY & KPIs\nPerformance Subsystem",
        "detail": "Real-time Hit/Miss Ratio Analytics\nMicrosecond Latency Tracking\nQuery Rate (QPS) Benchmarking\nLive Console Dashboard",
        "angle": 315, "color": "#7C2D12", "ec": "#EA580C", "badge": "Owner: Joel", "badge_col": "#FB923C"
    }
]

for sat in satellites:
    rad = np.radians(sat["angle"])
    x = 6.5 * np.cos(rad)
    y = 6.5 * np.sin(rad)
    
    # Connecting vector
    ax.annotate("", xy=(x*0.5, y*0.5), xytext=(x*0.85, y*0.85),
                arrowprops=dict(arrowstyle="<|-|>", color=sat["ec"], lw=2, mutation_scale=15))
    
    # Satellite box
    box = patches.FancyBboxPatch((x - 2.8, y - 1.8), 5.6, 3.6,
                                boxstyle="round,pad=0.3", fc=sat["color"], ec=sat["ec"], lw=2.5, zorder=5)
    ax.add_patch(box)
    ax.text(x, y + 1.2, sat["name"], color='#FFFFFF', fontsize=11, fontweight='bold', ha='center', va='center', zorder=6)
    ax.text(x, y + 0.1, sat["detail"], color='#E5E7EB', fontsize=8, ha='center', va='center', zorder=6)
    ax.text(x, y - 1.3, sat["badge"], color=sat["badge_col"], fontsize=8.5, fontweight='bold', ha='center', va='center', zorder=6)

# Outer Actors (Orbit 2)
# DNS Client
c_x, c_y = -10.5, 0
client_box = patches.FancyBboxPatch((c_x - 2.4, c_y - 1.3), 4.8, 2.6,
                                    boxstyle="round,pad=0.2", fc='#0F172A', ec='#38BDF8', lw=2, zorder=5)
ax.add_patch(client_box)
ax.text(c_x, c_y + 0.5, "DNS CLIENTS", color='#38BDF8', fontsize=11, fontweight='bold', ha='center', va='center', zorder=6)
ax.text(c_x, c_y - 0.4, "Standard Tools\n(dig / nslookup / stub)\nPort 8053 (UDP)", color='#94A3B8', fontsize=8.5, ha='center', va='center', zorder=6)

# External Upstream DNS
u_x, u_y = 10.5, 0
up_box = patches.FancyBboxPatch((u_x - 2.4, u_y - 1.3), 4.8, 2.6,
                               boxstyle="round,pad=0.2", fc='#0F172A', ec='#F43F5E', lw=2, zorder=5)
ax.add_patch(up_box)
ax.text(u_x, u_y + 0.5, "UPSTREAM DNS", color='#F43F5E', fontsize=11, fontweight='bold', ha='center', va='center', zorder=6)
ax.text(u_x, u_y - 0.4, "Recursive Resolvers\n(8.8.8.8 / 1.1.1.1)\nPort 53 (UDP)", color='#94A3B8', fontsize=8.5, ha='center', va='center', zorder=6)

# Disk Persistence
d_x, d_y = 0, -10.0
disk_box = patches.FancyBboxPatch((d_x - 2.6, d_y - 1.0), 5.2, 2.0,
                                 boxstyle="round,pad=0.2", fc='#0F172A', ec='#EAB308', lw=2, zorder=5)
ax.add_patch(disk_box)
ax.text(d_x, d_y + 0.4, "DISK SNAPSHOT (dns_cache.db)", color='#EAB308', fontsize=10, fontweight='bold', ha='center', va='center', zorder=6)
ax.text(d_x, d_y - 0.4, "Boot Warm-Up & Graceful Dump", color='#94A3B8', fontsize=8, ha='center', va='center', zorder=6)

# Interaction lines
ax.annotate("", xy=(-3.2, 0), xytext=(-8.1, 0),
            arrowprops=dict(arrowstyle="<|-|>", color='#38BDF8', lw=2, mutation_scale=15))
ax.annotate("", xy=(8.1, 0), xytext=(4.2, -3.8),
            arrowprops=dict(arrowstyle="<|-|>", color='#F43F5E', lw=2, mutation_scale=15))
ax.annotate("", xy=(0, -8.8), xytext=(0, -3.2),
            arrowprops=dict(arrowstyle="<|-|>", color='#EAB308', lw=2, linestyle='--', mutation_scale=15))

plt.tight_layout()
plt.savefig('docs/diagrams/system_orbit_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

# -------------------------------------------------------------
# 2. VM PIPELINE STAGES DIAGRAM
# -------------------------------------------------------------
fig, ax = plt.subplots(figsize=(15, 6), facecolor='#0B0F19')
ax.set_facecolor('#0B0F19')
ax.set_xlim(0, 16)
ax.set_ylim(0, 7)
ax.axis('off')

ax.text(8, 6.2, "ENGINE E4: QUERY EXECUTION VM — 6-STAGE INSTRUCTION PIPELINE", 
        color='#F3F4F6', fontsize=16, fontweight='bold', ha='center', va='center')
ax.text(8, 5.5, "Lifecycle of a DNS Request through the C++ State Machine", 
        color='#9CA3AF', fontsize=11, ha='center', va='center')

stages = [
    {"num": "STAGE 1", "name": "FETCH", "desc": "UDP Datagram\nIngestion on\nPort 8053", "col": "#0284C7", "eng": "Reactor"},
    {"num": "STAGE 2", "name": "DECODE", "desc": "RFC 1035 Parsing\nPointer Loop Check\nBitfield Unpack", "col": "#059669", "eng": "Engine E1"},
    {"num": "STAGE 3", "name": "LOOKUP", "desc": "In-Memory LRU\nMin-Heap Check\nDynamic TTL Recalc", "col": "#16A34A", "eng": "Engine E2"},
    {"num": "STAGE 4", "name": "RESOLVE", "desc": "On Cache Miss:\nForward 8.8.8.8\nTimeout & Retries", "col": "#9333EA", "eng": "Engine E3"},
    {"num": "STAGE 5", "name": "COMMIT", "desc": "Atomic Cache Store\nSet Authoritative TTL\nUpdate LRU Head", "col": "#D97706", "eng": "Engine E2"},
    {"num": "STAGE 6", "name": "EMIT", "desc": "Wire Serialize\nPointer Compress\nTransmit UDP", "col": "#E11D48", "eng": "Engine E1"}
]

box_w = 2.1
gap = 0.4
start_x = 0.7

for i, st in enumerate(stages):
    bx = start_x + i * (box_w + gap)
    by = 1.4
    
    p = patches.FancyBboxPatch((bx, by), box_w, 3.2, boxstyle="round,pad=0.15",
                               fc='#111827', ec=st["col"], lw=2.5)
    ax.add_patch(p)
    
    ax.text(bx + box_w/2, by + 2.7, st["num"], color=st["col"], fontsize=9, fontweight='bold', ha='center')
    ax.text(bx + box_w/2, by + 2.2, st["name"], color='#FFFFFF', fontsize=13, fontweight='bold', ha='center')
    ax.text(bx + box_w/2, by + 1.2, st["desc"], color='#D1D5DB', fontsize=8.5, ha='center', va='center')
    ax.text(bx + box_w/2, by + 0.35, f"[{st['eng']}]", color=st["col"], fontsize=8, fontstyle='italic', ha='center')
    
    if i < len(stages) - 1:
        ax.annotate("", xy=(bx + box_w + gap - 0.05, by + 1.6), xytext=(bx + box_w + 0.05, by + 1.6),
                    arrowprops=dict(arrowstyle="-|>", color='#6B7280', lw=2.5, mutation_scale=15))

plt.tight_layout()
plt.savefig('docs/diagrams/engine_pipeline_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

# -------------------------------------------------------------
# 3. CACHE O(1) LRU + O(log N) MIN-HEAP ARCHITECTURE
# -------------------------------------------------------------
fig, ax = plt.subplots(figsize=(14, 8), facecolor='#0B0F19')
ax.set_facecolor('#0B0F19')
ax.set_xlim(0, 14)
ax.set_ylim(0, 8)
ax.axis('off')

ax.text(7, 7.4, "ENGINE E2: DUAL-INDEX IN-MEMORY STORAGE ARCHITECTURE", 
        color='#F3F4F6', fontsize=16, fontweight='bold', ha='center', va='center')
ax.text(7, 6.7, "O(1) LRU Eviction (Hash Map + Doubly-Linked List) & O(log N) Proactive TTL Heap", 
        color='#9CA3AF', fontsize=11, ha='center', va='center')

# Left Box: Hash Map + Doubly-Linked List
box1 = patches.FancyBboxPatch((0.8, 1.0), 6.0, 5.0, boxstyle="round,pad=0.2",
                             fc='#111827', ec='#22C55E', lw=2)
ax.add_patch(box1)
ax.text(3.8, 5.5, "O(1) LRU Eviction Subsystem", color='#4ADE80', fontsize=13, fontweight='bold', ha='center')
ax.text(3.8, 4.8, "std::unordered_map<CompositeKey, Node*>", color='#86EFAC', fontsize=9.5, family='monospace', ha='center')
ax.text(3.8, 3.8, "• Key: (Domain + QTYPE + QCLASS)\n• Doubly-Linked Recency List (std::list)\n• Node Splice to Head on GET/HIT: O(1)\n• Tail Eviction on Capacity Limit: O(1)", 
        color='#E5E7EB', fontsize=9, ha='center', va='center')
ax.text(3.8, 1.8, "Thread Safety: Win32 SRWLOCK (Read/Write)", color='#FBBF24', fontsize=9, fontweight='bold', ha='center')

# Right Box: Min-Heap TTL Expiration
box2 = patches.FancyBboxPatch((7.2, 1.0), 6.0, 5.0, boxstyle="round,pad=0.2",
                             fc='#111827', ec='#3B82F6', lw=2)
ax.add_patch(box2)
ax.text(10.2, 5.5, "O(log N) Proactive TTL Purge", color='#60A5FA', fontsize=13, fontweight='bold', ha='center')
ax.text(10.2, 4.8, "std::priority_queue<TTLNode, MinComparator>", color='#93C5FD', fontsize=9.5, family='monospace', ha='center')
ax.text(10.2, 3.8, "• Priority: Absolute Expiry (ExpiresAt)\n• Top of Heap = Earliest Expiring Record\n• Lazy + Active Sweep Cycle: O(log N)\n• Zero Full-Table Scans Needed", 
        color='#E5E7EB', fontsize=9, ha='center', va='center')
ax.text(10.2, 1.8, "Durability: Snapshot serialization to disk", color='#FBBF24', fontsize=9, fontweight='bold', ha='center')

plt.tight_layout()
plt.savefig('docs/diagrams/cache_lru_minheap_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
plt.close()

print("ALL_DIAGRAMS_GENERATED_SUCCESSFULLY")
