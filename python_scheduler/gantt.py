import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import os

# Ensure output directory exists
OUTPUT_DIR = '../docs/screenshots'
os.makedirs(OUTPUT_DIR, exist_ok=True)

COLORS = [
    '#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4',
    '#FFEAA7', '#DDA0DD', '#98D8C8', '#F7DC6F',
    '#BB8FCE', '#85C1E9'
]

def get_color_map(processes):
    pids = list(set(p['pid'] for p in processes))
    return {pid: COLORS[i % len(COLORS)] for i, pid in enumerate(pids)}

def draw_gantt_chart(schedule, processes, title, filename):
    if not schedule:
        return

    color_map = get_color_map(processes)
    fig, ax = plt.subplots(figsize=(14, 4))

    max_time = max(s['end'] for s in schedule)
    prev_end = 0

    for slot in schedule:
        # Draw idle gap
        if slot['start'] > prev_end:
            ax.barh(0, slot['start'] - prev_end,
                    left=prev_end, height=0.5,
                    color='lightgrey', edgecolor='black', linewidth=0.5)
            ax.text(prev_end + (slot['start'] - prev_end) / 2, 0,
                    'IDLE', ha='center', va='center', fontsize=7, color='gray')

        # Draw process block
        ax.barh(0, slot['end'] - slot['start'],
                left=slot['start'], height=0.5,
                color=color_map[slot['pid']],
                edgecolor='black', linewidth=0.5)
        ax.text(slot['start'] + (slot['end'] - slot['start']) / 2, 0,
                f"P{slot['pid']}", ha='center', va='center',
                fontsize=8, fontweight='bold')
        prev_end = slot['end']

    # X axis ticks
    ax.set_xlim(0, max_time)
    ax.set_xticks(range(0, max_time + 1))
    ax.set_xlabel('Time Units')
    ax.set_yticks([])
    ax.set_title(title, fontsize=13, fontweight='bold')

    # Legend
    legend_patches = []
    seen = set()
    for slot in schedule:
        if slot['pid'] not in seen:
            patch = mpatches.Patch(color=color_map[slot['pid']],
                                   label=f"P{slot['pid']} - {slot['name']}")
            legend_patches.append(patch)
            seen.add(slot['pid'])
    ax.legend(handles=legend_patches, loc='upper right',
              fontsize=7, ncol=3)

    plt.tight_layout()
    filepath = os.path.join(OUTPUT_DIR, filename)
    plt.savefig(filepath, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {filepath}")

def draw_comparison_charts(results):
    algos = list(results.keys())
    avg_wt  = [results[a]['aggregates']['avg_waiting_time'] for a in algos]
    avg_tat = [results[a]['aggregates']['avg_turnaround_time'] for a in algos]
    cpu     = [results[a]['aggregates']['cpu_utilisation'] for a in algos]

    x = np.arange(len(algos))
    width = 0.25

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    fig.suptitle('Scheduling Algorithm Comparison', fontsize=14, fontweight='bold')

    # Average Waiting Time
    axes[0].bar(x, avg_wt, color='#FF6B6B', edgecolor='black')
    axes[0].set_title('Average Waiting Time')
    axes[0].set_xticks(x)
    axes[0].set_xticklabels(algos, rotation=15)
    axes[0].set_ylabel('Time Units')

    # Average Turnaround Time
    axes[1].bar(x, avg_tat, color='#4ECDC4', edgecolor='black')
    axes[1].set_title('Average Turnaround Time')
    axes[1].set_xticks(x)
    axes[1].set_xticklabels(algos, rotation=15)
    axes[1].set_ylabel('Time Units')

    # CPU Utilisation
    axes[2].bar(x, cpu, color='#45B7D1', edgecolor='black')
    axes[2].set_title('CPU Utilisation (%)')
    axes[2].set_xticks(x)
    axes[2].set_xticklabels(algos, rotation=15)
    axes[2].set_ylabel('Percentage')
    axes[2].set_ylim(0, 100)

    plt.tight_layout()
    filepath = os.path.join(OUTPUT_DIR, 'comparison_charts.png')
    plt.savefig(filepath, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {filepath}")

def generate_all_charts(results, processes):
    titles = {
        'FCFS': 'FCFS - First Come First Served',
        'SJF': 'SJF - Shortest Job First',
        'Priority': 'Priority Scheduling (with Ageing)',
        'RoundRobin': 'Round Robin'
    }
    filenames = {
        'FCFS': 'gantt_fcfs.png',
        'SJF': 'gantt_sjf.png',
        'Priority': 'gantt_priority.png',
        'RoundRobin': 'gantt_rr.png'
    }

    for algo, data in results.items():
        draw_gantt_chart(data['schedule'], processes,
                         titles.get(algo, algo),
                         filenames.get(algo, f'gantt_{algo}.png'))

    draw_comparison_charts(results)
