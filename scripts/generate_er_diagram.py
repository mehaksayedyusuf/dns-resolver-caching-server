import matplotlib.pyplot as plt
import matplotlib.patches as patches
import shutil
import os

fig, ax = plt.subplots(figsize=(15, 9), facecolor='#090D16', dpi=300)
ax.set_facecolor('#090D16')
ax.set_xlim(0, 16)
ax.set_ylim(0, 10)
ax.axis('off')

# Header
ax.text(8, 9.4, 'DATABASE & STORAGE DESIGN: ENTITY-RELATIONSHIP (ER) DIAGRAM',
        color='#F8FAFC', fontsize=16, fontweight='bold', ha='center', va='center')
ax.text(8, 8.8, 'Dual-Index In-Memory Cache Entities, Key Relationships, and Disk Snapshot Schema',
        color='#94A3B8', fontsize=11, ha='center', va='center')

def draw_entity(x, y, w, h, title, fields, border_col, fill_col='#111827'):
    p = patches.FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0.2', fc=fill_col, ec=border_col, lw=2.2)
    ax.add_patch(p)
    # Title bar
    tp = patches.Rectangle((x, y + h - 0.7), w, 0.7, fc=border_col, ec=border_col)
    ax.add_patch(tp)
    ax.text(x + w/2, y + h - 0.35, title, color='#FFFFFF', fontsize=10.5, fontweight='bold', ha='center', va='center')
    # Fields
    y_text = y + h - 1.05
    for f in fields:
        col = '#FBBF24' if '(PK' in f or '(FK' in f else '#E2E8F0'
        ax.text(x + 0.25, y_text, f, color=col, fontsize=8.5, va='center', family='monospace')
        y_text -= 0.38

# Entities
draw_entity(0.6, 4.4, 4.2, 3.8, 'CacheKey (Composite PK)', 
            ['+ domain: string (PK)', '+ qtype: uint16_t (PK)', '+ qclass: uint16_t (PK)', '---', 'Hash: domain ^ qtype ^ qclass', 'Matches RFC 1035 Question'], '#3B82F6')

draw_entity(5.8, 3.8, 4.6, 4.7, 'CacheEntry (LRU Node)',
            ['+ key: CacheKey (PK/FK)', '+ records: vector<DNS_RR>', '+ original_ttl: uint32_t', '+ expires_at: time_point', '+ is_negative: bool', '+ rcode: uint8_t', '+ version: uint64_t', '+ prev: CacheEntry*', '+ next: CacheEntry*'], '#10B981')

draw_entity(11.2, 4.4, 4.2, 3.8, 'MinHeapNode (TTL Expiry)',
            ['+ expires_at: time_point (PK)', '+ key: CacheKey (FK)', '+ version: uint64_t', '---', 'Min-Heap Priority Queue', 'Root = Earliest Expiring Record'], '#8B5CF6')

draw_entity(0.6, 0.5, 4.4, 3.2, 'DNSResourceRecord (RData)',
            ['+ name: string', '+ type: uint16_t', '+ rclass: uint16_t', '+ ttl: uint32_t', '+ rdlength: uint16_t', '+ rdata: vector<uint8_t> (IP)', '+ readable_data: string'], '#EC4899')

draw_entity(5.8, 0.5, 4.6, 2.7, 'DiskSnapshot (dns_cache.db)',
            ['+ magic_header: char[4] (PK)', '+ record_count: uint32_t', '+ entries: serialized binary stream', '---', 'Graceful SIGINT dump & boot warmup'], '#F59E0B')

draw_entity(11.2, 0.5, 4.2, 3.2, 'TelemetrySnapshot (Metrics)',
            ['+ total_queries: uint64_t', '+ cache_hits: uint64_t', '+ cache_misses: uint64_t', '+ hit_ratio_pct: double', '+ avg_latency_us: double', '+ qps: double'], '#06B6D4')

# Connectors
# CacheKey to CacheEntry (1 to 1)
ax.annotate('', xy=(5.8, 6.3), xytext=(4.8, 6.3), arrowprops=dict(arrowstyle='<|-|>', color='#3B82F6', lw=2))
ax.text(5.3, 6.55, '1:1', color='#93C5FD', fontsize=9, fontweight='bold', ha='center')

# CacheEntry to MinHeapNode (1 to 1 sync via version)
ax.annotate('', xy=(11.2, 6.3), xytext=(10.4, 6.3), arrowprops=dict(arrowstyle='<|-|>', color='#10B981', lw=2))
ax.text(10.8, 6.55, '1:1', color='#6EE7B7', fontsize=9, fontweight='bold', ha='center')

# CacheEntry to DNSResourceRecord (1 to N)
ax.annotate('', xy=(2.8, 3.7), xytext=(6.5, 3.8), arrowprops=dict(arrowstyle='-|>', color='#EC4899', lw=2))
ax.text(4.4, 3.6, '1 : N', color='#F472B6', fontsize=9, fontweight='bold', ha='center')

# CacheEntry to DiskSnapshot (N to 1 persistence)
ax.annotate('', xy=(8.1, 3.2), xytext=(8.1, 3.8), arrowprops=dict(arrowstyle='<|-|>', color='#F59E0B', lw=2, linestyle='--'))
ax.text(8.5, 3.5, 'Dump/Load', color='#FDE68A', fontsize=8.5, ha='center')

plt.tight_layout()
os.makedirs('diagrams', exist_ok=True)
os.makedirs('docs/diagrams', exist_ok=True)
plt.savefig('diagrams/cache_storage_er_diagram.png', dpi=300, facecolor=fig.get_facecolor(), edgecolor='none')
shutil.copy('diagrams/cache_storage_er_diagram.png', 'docs/diagrams/cache_storage_er_diagram.png')
plt.close()
print('ER_DIAGRAM_GENERATED_SUCCESSFULLY')
