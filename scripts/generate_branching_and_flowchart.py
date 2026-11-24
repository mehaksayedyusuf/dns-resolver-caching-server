import matplotlib.pyplot as plt
import matplotlib.patches as patches
import shutil
import os

os.makedirs('diagrams', exist_ok=True)
os.makedirs('docs/diagrams', exist_ok=True)

# -----------------------------------------------------------------------------
# 1. SIX-STAGE VM PIPELINE WITH VISUAL CACHE-HIT & CACHE-MISS BRANCHING
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(18, 9.5), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(0, 18)
ax.set_ylim(0, 10)
ax.axis('off')

# Title
ax.text(9, 9.4, "ENGINE E4: 6-STAGE VM PIPELINE — CACHE-HIT & CACHE-MISS BRANCHING", 
        color='#F8FAFC', fontsize=16, fontweight='bold', ha='center', va='center')
ax.text(9, 8.8, "Deterministic Lifecycle State Machine with Visual Fast-Path and Exception Routing", 
        color='#94A3B8', fontsize=11, ha='center', va='center')

stages = [
    {"num": "STAGE 1", "name": "FETCH", "desc": "• Datagram Ingest\n• Port 8053 UDP\n• Buffer Slice Copy\n• Timestamp Start", "col": "#0284C7", "eng": "Reactor"},
    {"num": "STAGE 2", "name": "DECODE", "desc": "• 12B Header Parse\n• QNAME Decode\n• Loop Guard <=16\n• Malformed Check", "col": "#059669", "eng": "Engine E1"},
    {"num": "STAGE 3", "name": "LOOKUP", "desc": "• CompositeKey Query\n• SRWLOCK Shared\n• Dynamic TTL Calc\n• Hit / Miss Branch", "col": "#16A34A", "eng": "Engine E2"},
    {"num": "STAGE 4", "name": "RESOLVE", "desc": "• Upstream 8.8.8.8\n• select() 2.0s\n• Kaminsky TxID\n• Failover 1.1.1.1", "col": "#9333EA", "eng": "Engine E3"},
    {"num": "STAGE 5", "name": "COMMIT", "desc": "• SRWLOCK Exclusive\n• Insert Hash Map\n• Push Min-Heap\n• O(1) Tail Evict", "col": "#D97706", "eng": "Engine E2"},
    {"num": "STAGE 6", "name": "EMIT", "desc": "• Wire Serialize\n• 0xC00C Compression\n• UDP sendto Client\n• Microsecond Log", "col": "#E11D48", "eng": "Engine E1"}
]

box_w = 2.4
gap = 0.45
start_x = 0.75
by = 4.2

for i, st in enumerate(stages):
    bx = start_x + i * (box_w + gap)
    p = patches.FancyBboxPatch((bx, by), box_w, 3.4, boxstyle="round,pad=0.2",
                               fc='#111827', ec=st["col"], lw=2.5, zorder=3)
    ax.add_patch(p)
    
    ax.text(bx + box_w/2, by + 2.9, st["num"], color=st["col"], fontsize=9.5, fontweight='bold', ha='center', zorder=4)
    ax.text(bx + box_w/2, by + 2.3, st["name"], color='#FFFFFF', fontsize=13, fontweight='bold', ha='center', zorder=4)
    ax.text(bx + box_w/2, by + 1.25, st["desc"], color='#CBD5E1', fontsize=8.0, ha='center', va='center', zorder=4)
    ax.text(bx + box_w/2, by + 0.35, f"[{st['eng']}]", color=st["col"], fontsize=8.0, fontstyle='italic', ha='center', zorder=4)
    
    # Sequential arrows between stages
    if i < len(stages) - 1:
        ax.annotate("", xy=(bx + box_w + gap - 0.05, by + 1.7), xytext=(bx + box_w + 0.05, by + 1.7),
                    arrowprops=dict(arrowstyle="-|>", color='#64748B', lw=2.2, mutation_scale=12), zorder=2)

# -----------------------------------------------------------------------------
# BRANCH ARROWS
# -----------------------------------------------------------------------------
# 1. CACHE-HIT FAST PATH: Stage 3 (LOOKUP) -> JUMP OVER S4 & S5 -> Stage 6 (EMIT)
hit_start_x = start_x + 2 * (box_w + gap) + box_w/2
emit_end_x = start_x + 5 * (box_w + gap) + box_w/2

# Green arc jumping over S4 and S5
arc_hit = patches.Arc(( (hit_start_x + emit_end_x)/2, by + 3.4 ), emit_end_x - hit_start_x, 2.2,
                      angle=0, theta1=0, theta2=180, color='#22C55E', lw=3.0, linestyle='-')
ax.add_patch(arc_hit)
ax.annotate("", xy=(emit_end_x - 0.2, by + 3.45), xytext=(emit_end_x - 0.5, by + 3.7),
            arrowprops=dict(arrowstyle="-|>", color='#22C55E', lw=3.0, mutation_scale=16))

# Hit badge
hit_box = patches.FancyBboxPatch(((hit_start_x + emit_end_x)/2 - 3.2, by + 4.1), 6.4, 0.75,
                                boxstyle="round,pad=0.15", fc='#064E3B', ec='#22C55E', lw=2.0, zorder=5)
ax.add_patch(hit_box)
ax.text((hit_start_x + emit_end_x)/2, by + 4.47, 
        "[CACHE-HIT FAST-PATH] Direct to EMIT in 0.414 ms (Bypasses Upstream & Commit)", 
        color='#4ADE80', fontsize=9.0, fontweight='bold', ha='center', va='center', zorder=6)

# 2. CACHE-MISS PATH: Stage 3 (LOOKUP) -> Stage 4 (RESOLVE)
miss_x = start_x + 2 * (box_w + gap) + box_w
ax.text(miss_x + gap/2, by + 2.1, "MISS /\nEXPIRED", color='#A855F7', fontsize=7.5, fontweight='bold', ha='center', zorder=5)

# 3. FORMERR SHORTCUT: Stage 2 (DECODE) -> Direct to Stage 6 (EMIT)
dec_x = start_x + 1 * (box_w + gap) + box_w/2
arc_formerr = patches.Arc(( (dec_x + emit_end_x)/2, by ), emit_end_x - dec_x, 3.0,
                          angle=0, theta1=180, theta2=360, color='#EF4444', lw=2.2, linestyle='--')
ax.add_patch(arc_formerr)
ax.annotate("", xy=(emit_end_x, by - 0.05), xytext=(emit_end_x - 0.3, by - 0.3),
            arrowprops=dict(arrowstyle="-|>", color='#EF4444', lw=2.2, mutation_scale=14))

formerr_box = patches.FancyBboxPatch(((dec_x + emit_end_x)/2 - 3.4, by - 2.0), 6.8, 0.7,
                                    boxstyle="round,pad=0.15", fc='#450A0A', ec='#EF4444', lw=1.8, zorder=5)
ax.add_patch(formerr_box)
ax.text((dec_x + emit_end_x)/2, by - 1.65, 
        "[FORMERR SHORTCUT] Malformed Header or Pointer Cycle > 16 -> Emit FORMERR (RCODE 1)", 
        color='#FCA5A5', fontsize=8.0, fontweight='bold', ha='center', va='center', zorder=6)

# 4. SERVFAIL SHORTCUT: Stage 4 (RESOLVE) -> Direct to Stage 6 (EMIT)
res_x = start_x + 3 * (box_w + gap) + box_w/2
arc_servfail = patches.Arc(( (res_x + emit_end_x)/2, by ), emit_end_x - res_x, 1.8,
                           angle=0, theta1=180, theta2=360, color='#F59E0B', lw=2.0, linestyle=':')
ax.add_patch(arc_servfail)
ax.annotate("", xy=(emit_end_x - 0.3, by - 0.05), xytext=(emit_end_x - 0.5, by - 0.25),
            arrowprops=dict(arrowstyle="-|>", color='#F59E0B', lw=2.0, mutation_scale=14))

servfail_box = patches.FancyBboxPatch(((res_x + emit_end_x)/2 - 2.8, by - 1.15), 5.6, 0.65,
                                     boxstyle="round,pad=0.15", fc='#451A03', ec='#F59E0B', lw=1.5, zorder=5)
ax.add_patch(servfail_box)
ax.text((res_x + emit_end_x)/2, by - 0.83, 
        "[SERVFAIL] Timeout > 2.0s or Upstream Down -> Emit SERVFAIL (RCODE 2)", 
        color='#FDE68A', fontsize=7.5, fontweight='bold', ha='center', va='center', zorder=6)

plt.tight_layout()
plt.savefig('diagrams/vm_pipeline_branching_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
shutil.copy('diagrams/vm_pipeline_branching_diagram.png', 'docs/diagrams/vm_pipeline_branching_diagram.png')
# Also overwrite engine_pipeline_diagram.png so that all references get the branching version!
shutil.copy('diagrams/vm_pipeline_branching_diagram.png', 'diagrams/engine_pipeline_diagram.png')
shutil.copy('diagrams/vm_pipeline_branching_diagram.png', 'docs/diagrams/engine_pipeline_diagram.png')
plt.close()

# -----------------------------------------------------------------------------
# 2. COMPLETE DNS RESOLUTION FLOWCHART DIAGRAM
# -----------------------------------------------------------------------------
fig, ax = plt.subplots(figsize=(16, 12), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(0, 16)
ax.set_ylim(0, 16)
ax.axis('off')

ax.text(8, 15.4, "COMPLETE END-TO-END DNS RESOLUTION ALGORITHM FLOWCHART", 
        color='#F8FAFC', fontsize=16, fontweight='bold', ha='center', va='center')
ax.text(8, 14.8, "Client Query Ingestion, Loop-Guarded Parsing, Dual-Indexed Cache, Upstream Failover & Emission", 
        color='#94A3B8', fontsize=11, ha='center', va='center')

def draw_fc_box(x, y, w, h, title, subtitle, col_border, col_bg='#111827', shape='rect'):
    if shape == 'round':
        p = patches.FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.2", fc=col_bg, ec=col_border, lw=2.0)
    elif shape == 'diamond':
        pts = [[x + w/2, y], [x + w, y + h/2], [x + w/2, y + h], [x, y + h/2]]
        p = patches.Polygon(pts, closed=True, fc=col_bg, ec=col_border, lw=2.0)
    else:
        p = patches.Rectangle((x, y), w, h, fc=col_bg, ec=col_border, lw=2.0)
    ax.add_patch(p)
    ax.text(x + w/2, y + h*0.65, title, color='#FFFFFF', fontsize=9.0, fontweight='bold', ha='center', va='center')
    ax.text(x + w/2, y + h*0.35, subtitle, color='#CBD5E1', fontsize=7.5, ha='center', va='center')

# Flowchart Nodes Layout
# Start Node
draw_fc_box(6.2, 13.5, 3.6, 0.9, "1. INGEST UDP QUERY", "Port 8053 (>= 12 Bytes)", '#0284C7', shape='round')

# Check Valid Header
draw_fc_box(6.2, 12.0, 3.6, 1.0, "Packet >= 12 Bytes?", "Header Bitfield Check", '#EAB308', shape='diamond')

# Sub-12 Error
draw_fc_box(11.5, 12.0, 3.6, 0.9, "EMIT FORMERR", "RCODE 1 (Malformed Packet)", '#EF4444', shape='round')

# Decode Labels
draw_fc_box(6.2, 10.4, 3.6, 1.0, "Pointer Depth <= 16?", "Label Decompression Guard", '#EAB308', shape='diamond')

# Loop Detected Error
draw_fc_box(11.5, 10.4, 3.6, 0.9, "EMIT FORMERR", "Cyclic Pointer Attack Blocked", '#EF4444', shape='round')

# Cache Lookup
draw_fc_box(6.2, 8.8, 3.6, 1.0, "Cache Verdict?", "(Domain, Type, Class)", '#10B981', shape='diamond')

# Cache Hit Box (Left side)
draw_fc_box(0.9, 8.8, 4.2, 1.1, "CACHE HIT (O(1))", "TTL_rem = TTL_orig - delta_t\nSplice node to MRU Head", '#22C55E', shape='round')

# Upstream Resolution (Down path)
draw_fc_box(6.2, 7.0, 3.6, 1.0, "Forward to 8.8.8.8", "select() non-blocking <= 2.0s", '#8B5CF6', shape='round')

# Upstream Response Check
draw_fc_box(6.2, 5.3, 3.6, 1.0, "Upstream Timeout?", "Retry <= 2 | Failover 1.1.1.1", '#EAB308', shape='diamond')

# SERVFAIL Box
draw_fc_box(11.5, 5.3, 3.6, 0.9, "EMIT SERVFAIL", "RCODE 2 (Network Unreachable)", '#EF4444', shape='round')

# TxID Check
draw_fc_box(6.2, 3.7, 3.6, 1.0, "TxID Match?", "Kaminsky Poisoning Check", '#EAB308', shape='diamond')

# Spoofed Packet Drop
draw_fc_box(11.5, 3.7, 3.6, 0.9, "DROP PACKET", "Spoofed TxID Discarded", '#F97316', shape='round')

# Cache Commit
draw_fc_box(6.2, 2.1, 3.6, 1.0, "COMMIT TO CACHE", "Insert Hash + MinHeap Push\nO(1) LRU Tail Evict at Cap", '#F59E0B', shape='round')

# Emit Response
draw_fc_box(6.2, 0.6, 3.6, 0.9, "EMIT RFC 1035 RESPONSE", "0xC00C Pointer Compression", '#06B6D4', shape='round')

# Connectors
def connect_down(x, y1, y2, col='#64748B'):
    ax.annotate("", xy=(x, y2), xytext=(x, y1),
                arrowprops=dict(arrowstyle="-|>", color=col, lw=1.8, mutation_scale=10))

connect_down(8.0, 13.5, 13.0)
connect_down(8.0, 12.0, 11.4)
connect_down(8.0, 10.4, 9.8)
connect_down(8.0, 8.8, 8.0)
connect_down(8.0, 7.0, 6.3)
connect_down(8.0, 5.3, 4.7)
connect_down(8.0, 3.7, 3.1)
connect_down(8.0, 2.1, 1.5)

# Horizontal connectors
ax.annotate("", xy=(11.5, 12.5), xytext=(9.8, 12.5), arrowprops=dict(arrowstyle="-|>", color='#EF4444', lw=1.8))
ax.text(10.6, 12.7, "No", color='#EF4444', fontsize=8, fontweight='bold', ha='center')

ax.annotate("", xy=(11.5, 10.9), xytext=(9.8, 10.9), arrowprops=dict(arrowstyle="-|>", color='#EF4444', lw=1.8))
ax.text(10.6, 11.1, "No (>16)", color='#EF4444', fontsize=8, fontweight='bold', ha='center')

ax.annotate("", xy=(5.1, 9.35), xytext=(6.2, 9.35), arrowprops=dict(arrowstyle="-|>", color='#22C55E', lw=2.2))
ax.text(5.65, 9.55, "HIT", color='#22C55E', fontsize=8.5, fontweight='bold', ha='center')

# Hit to Emit connector
ax.annotate("", xy=(6.2, 1.05), xytext=(3.0, 8.8),
            arrowprops=dict(arrowstyle="-|>", color='#22C55E', lw=2.2, connectionstyle="angle,angleA=-90,angleB=180,rad=10"))
ax.text(2.1, 4.5, "Fast-Path Bypass (0.414 ms)", color='#4ADE80', fontsize=8, fontweight='bold', rotation=90, va='center')

ax.annotate("", xy=(11.5, 5.8), xytext=(9.8, 5.8), arrowprops=dict(arrowstyle="-|>", color='#EF4444', lw=1.8))
ax.text(10.6, 6.0, "Yes (Timeout)", color='#EF4444', fontsize=8, fontweight='bold', ha='center')

ax.annotate("", xy=(11.5, 4.2), xytext=(9.8, 4.2), arrowprops=dict(arrowstyle="-|>", color='#F97316', lw=1.8))
ax.text(10.6, 4.4, "Mismatch", color='#F97316', fontsize=8, fontweight='bold', ha='center')

plt.tight_layout()
plt.savefig('diagrams/dns_resolution_flowchart.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
shutil.copy('diagrams/dns_resolution_flowchart.png', 'docs/diagrams/dns_resolution_flowchart.png')
plt.close()

print("BRANCHING_AND_FLOWCHART_DIAGRAMS_GENERATED_SUCCESSFULLY")
