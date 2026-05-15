import argparse
import csv
import json
import random
import sys
from tabulate import tabulate

# ════════════════════════════════════════
# PROCESS GENERATION
# ════════════════════════════════════════

def generate_random_processes(n, seed=None):
    if seed is not None:
        random.seed(seed)
    processes = []
    for i in range(n):
        processes.append({
            'pid': 1000 + i,
            'name': f'Process_{i}',
            'arrival_time': random.randint(0, 10),
            'burst_time': random.randint(1, 10),
            'priority': random.randint(1, 5),
            'memory_req_kb': random.choice([256, 512, 1024]),
            'remaining_time': 0
        })
    for p in processes:
        p['remaining_time'] = p['burst_time']
    return processes

def load_from_csv(filename):
    processes = []
    with open(filename, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            processes.append({
                'pid': int(row['pid']),
                'name': row['name'],
                'arrival_time': int(row['arrival_time']),
                'burst_time': int(row['burst_time']),
                'priority': int(row['priority']),
                'memory_req_kb': int(row['memory_req_kb']),
                'remaining_time': int(row['burst_time'])
            })
    return processes

def load_from_json(filename):
    with open(filename, 'r') as f:
        data = json.load(f)
    processes = []
    for p in data:
        if p.get('state') == 'TERMINATED':
            processes.append({
                'pid': p['pid'],
                'name': p['name'],
                'arrival_time': p['arrival_time'],
                'burst_time': p['burst_time'],
                'priority': p['priority'],
                'memory_req_kb': p.get('memory_req_kb', 512),
                'remaining_time': p['burst_time']
            })
    return processes

# ════════════════════════════════════════
# SCHEDULING ALGORITHMS
# ════════════════════════════════════════

def fcfs(processes):
    """First Come First Served - Non preemptive"""
    procs = sorted(processes, key=lambda x: (x['arrival_time'], x['pid']))
    schedule = []
    time = 0
    for p in procs:
        if time < p['arrival_time']:
            time = p['arrival_time']
        start = time
        end = time + p['burst_time']
        schedule.append({'pid': p['pid'], 'name': p['name'],
                         'start': start, 'end': end,
                         'arrival_time': p['arrival_time'],
                         'burst_time': p['burst_time']})
        time = end
    return schedule

def sjf(processes):
    """Shortest Job First - Non preemptive"""
    procs = [p.copy() for p in processes]
    schedule = []
    time = 0
    remaining = procs.copy()

    while remaining:
        available = [p for p in remaining if p['arrival_time'] <= time]
        if not available:
            time = min(p['arrival_time'] for p in remaining)
            available = [p for p in remaining if p['arrival_time'] <= time]

        # Sort by burst_time then pid for ties
        chosen = min(available, key=lambda x: (x['burst_time'], x['pid']))
        remaining.remove(chosen)

        start = time
        end = time + chosen['burst_time']
        schedule.append({'pid': chosen['pid'], 'name': chosen['name'],
                         'start': start, 'end': end,
                         'arrival_time': chosen['arrival_time'],
                         'burst_time': chosen['burst_time']})
        time = end
    return schedule

def priority_scheduling(processes):
    """Priority Scheduling - Non preemptive with ageing"""
    procs = [p.copy() for p in processes]
    # Add age tracking
    for p in procs:
        p['age'] = 0
        p['effective_priority'] = p['priority']

    schedule = []
    time = 0
    remaining = procs.copy()
    last_age_update = 0

    while remaining:
        available = [p for p in remaining if p['arrival_time'] <= time]
        if not available:
            time = min(p['arrival_time'] for p in remaining)
            available = [p for p in remaining if p['arrival_time'] <= time]

        # Ageing: every 3 time units waiting processes gain +1 priority
        if time - last_age_update >= 3:
            for p in available:
                p['age'] += 1
                p['effective_priority'] = max(1, p['priority'] - p['age'])
            last_age_update = time

        # Lower number = higher priority
        chosen = min(available,
                     key=lambda x: (x['effective_priority'], x['arrival_time'], x['pid']))
        remaining.remove(chosen)

        start = time
        end = time + chosen['burst_time']
        schedule.append({'pid': chosen['pid'], 'name': chosen['name'],
                         'start': start, 'end': end,
                         'arrival_time': chosen['arrival_time'],
                         'burst_time': chosen['burst_time'],
                         'priority': chosen['priority']})
        time = end
    return schedule

def round_robin(processes, quantum=3):
    """Round Robin - Preemptive with configurable quantum"""
    procs = [p.copy() for p in processes]
    for p in procs:
        p['remaining_time'] = p['burst_time']
        p['first_run'] = -1

    schedule = []
    time = 0
    queue = []
    remaining = sorted(procs, key=lambda x: (x['arrival_time'], x['pid']))
    completed = []
    i = 0

    # Add processes that arrive at time 0
    while i < len(remaining) and remaining[i]['arrival_time'] <= time:
        queue.append(remaining[i])
        i += 1

    while queue or i < len(remaining):
        if not queue:
            time = remaining[i]['arrival_time']
            while i < len(remaining) and remaining[i]['arrival_time'] <= time:
                queue.append(remaining[i])
                i += 1

        current = queue.pop(0)
        if current['first_run'] == -1:
            current['first_run'] = time

        run_time = min(quantum, current['remaining_time'])
        start = time
        end = time + run_time
        schedule.append({'pid': current['pid'], 'name': current['name'],
                         'start': start, 'end': end,
                         'arrival_time': current['arrival_time'],
                         'burst_time': current['burst_time']})
        time = end
        current['remaining_time'] -= run_time

        # Add newly arrived processes to queue
        while i < len(remaining) and remaining[i]['arrival_time'] <= time:
            queue.append(remaining[i])
            i += 1

        if current['remaining_time'] > 0:
            queue.append(current)
        else:
            completed.append(current)

    return schedule

# ════════════════════════════════════════
# METRICS CALCULATION
# ════════════════════════════════════════

def calculate_metrics(processes, schedule):
    metrics = []
    total_time = max(s['end'] for s in schedule) if schedule else 0
    total_burst = sum(p['burst_time'] for p in processes)

    for p in processes:
        slices = [s for s in schedule if s['pid'] == p['pid']]
        if not slices:
            continue
        completion_time = max(s['end'] for s in slices)
        response_time = min(s['start'] for s in slices) - p['arrival_time']
        turnaround_time = completion_time - p['arrival_time']
        waiting_time = turnaround_time - p['burst_time']

        metrics.append({
            'pid': p['pid'],
            'name': p['name'],
            'arrival_time': p['arrival_time'],
            'burst_time': p['burst_time'],
            'completion_time': completion_time,
            'turnaround_time': turnaround_time,
            'waiting_time': waiting_time,
            'response_time': response_time
        })

    if metrics:
        avg_wt  = sum(m['waiting_time'] for m in metrics) / len(metrics)
        avg_tat = sum(m['turnaround_time'] for m in metrics) / len(metrics)
        avg_rt  = sum(m['response_time'] for m in metrics) / len(metrics)
        cpu_util = (total_burst / total_time * 100) if total_time > 0 else 0
        throughput = len(metrics) / total_time if total_time > 0 else 0
    else:
        avg_wt = avg_tat = avg_rt = cpu_util = throughput = 0

    return metrics, {
        'avg_waiting_time': round(avg_wt, 2),
        'avg_turnaround_time': round(avg_tat, 2),
        'avg_response_time': round(avg_rt, 2),
        'cpu_utilisation': round(cpu_util, 2),
        'throughput': round(throughput, 4)
    }

# ════════════════════════════════════════
# DISPLAY
# ════════════════════════════════════════

def print_results(algo_name, metrics, aggregates):
    print(f"\n{'═'*60}")
    print(f" {algo_name}")
    print(f"{'═'*60}")

    headers = ['PID', 'Name', 'Arrival', 'Burst',
               'Completion', 'TAT', 'WT', 'RT']
    rows = [[m['pid'], m['name'], m['arrival_time'], m['burst_time'],
             m['completion_time'], m['turnaround_time'],
             m['waiting_time'], m['response_time']] for m in metrics]
    print(tabulate(rows, headers=headers, tablefmt='grid'))

    print(f"\n  Average Waiting Time    : {aggregates['avg_waiting_time']}")
    print(f"  Average Turnaround Time : {aggregates['avg_turnaround_time']}")
    print(f"  Average Response Time   : {aggregates['avg_response_time']}")
    print(f"  CPU Utilisation         : {aggregates['cpu_utilisation']}%")
    print(f"  Throughput              : {aggregates['throughput']} proc/unit\n")

# ════════════════════════════════════════
# MAIN
# ════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description='EduOS Scheduling Simulator')
    parser.add_argument('--random', type=int, help='Generate N random processes')
    parser.add_argument('--seed', type=int, default=42, help='Random seed')
    parser.add_argument('--file', type=str, help='Load processes from CSV/JSON file')
    parser.add_argument('--quantum', type=int, default=3, help='Round Robin quantum')
    parser.add_argument('--mode', type=str, default='process',
                        choices=['process', 'thread'], help='Scheduling mode')
    args = parser.parse_args()

    # Load processes
    if args.random:
        processes = generate_random_processes(args.random, args.seed)
        print(f"Generated {args.random} random processes (seed={args.seed})")
    elif args.file:
        if args.file.endswith('.json'):
            processes = load_from_json(args.file)
        else:
            processes = load_from_csv(args.file)
        print(f"Loaded {len(processes)} processes from {args.file}")
    else:
        print("Error: specify --random N or --file filename")
        sys.exit(1)

    if not processes:
        print("No processes to schedule!")
        sys.exit(1)

    # Run all algorithms
    results = {}

    # FCFS
    schedule = fcfs(processes)
    metrics, agg = calculate_metrics(processes, schedule)
    print_results("FCFS - First Come First Served", metrics, agg)
    results['FCFS'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}

    # SJF
    schedule = sjf(processes)
    metrics, agg = calculate_metrics(processes, schedule)
    print_results("SJF - Shortest Job First", metrics, agg)
    results['SJF'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}

    # Priority
    schedule = priority_scheduling(processes)
    metrics, agg = calculate_metrics(processes, schedule)
    print_results("Priority Scheduling (with Ageing)", metrics, agg)
    results['Priority'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}

    # Round Robin
    schedule = round_robin(processes, args.quantum)
    metrics, agg = calculate_metrics(processes, schedule)
    print_results(f"Round Robin (quantum={args.quantum})", metrics, agg)
    results['RoundRobin'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}

    # Comparison table
    print(f"\n{'═'*60}")
    print(" ALGORITHM COMPARISON")
    print(f"{'═'*60}")
    comp_headers = ['Algorithm', 'Avg WT', 'Avg TAT', 'Avg RT', 'CPU%', 'Throughput']
    comp_rows = []
    for algo, data in results.items():
        agg = data['aggregates']

if __name__ == '__main__':
    main()

