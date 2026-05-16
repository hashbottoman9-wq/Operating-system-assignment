import subprocess
import json
import os
import sys
import time
from datetime import datetime

# ════════════════════════════════════════
# PATHS
# ════════════════════════════════════════
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
C_BINARY = os.path.join(BASE_DIR, 'c_core', 'eduos')
PCB_SNAPSHOT = os.path.join(BASE_DIR, 'c_core', 'pcb_snapshot.json')
SCHEDULER_DIR = os.path.join(BASE_DIR, 'python_scheduler')
SCHEDULER_SCRIPT = os.path.join(SCHEDULER_DIR, 'scheduler_sim.py')
REPORT_FILE = os.path.join(BASE_DIR, 'simulation_report.json')

# ════════════════════════════════════════
# STEP 1: LAUNCH C SIMULATOR
# ════════════════════════════════════════
def launch_c_simulator():
    print("=" * 60)
    print(" STEP 1: Launching C Simulator")
    print("=" * 60)

    if not os.path.exists(C_BINARY):
        print(f"[ERROR] C binary not found at {C_BINARY}")
        print("[INFO] Please run 'make all' in c_core/ first")
        return False

    try:
        process = subprocess.Popen(
            [C_BINARY],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=os.path.join(BASE_DIR, 'c_core')
        )

        print("[C Simulator Output]")
        for line in process.stdout:
            print(f"  {line}", end='')

        process.wait()
        print(f"\n[C Simulator] Exited with code {process.returncode}")
        return process.returncode == 0

    except Exception as e:
        print(f"[ERROR] Failed to launch C simulator: {e}")
        return False

# ════════════════════════════════════════
# STEP 2: READ PCB SNAPSHOT
# ════════════════════════════════════════
def read_pcb_snapshot():
    print("\n" + "=" * 60)
    print(" STEP 2: Reading PCB Snapshot")
    print("=" * 60)

    snapshot_path = os.path.join(BASE_DIR, 'c_core', 'pcb_snapshot.json')

    if not os.path.exists(snapshot_path):
        print(f"[ERROR] PCB snapshot not found at {snapshot_path}")
        return None

    try:
        with open(snapshot_path, 'r') as f:
            pcb_data = json.load(f)
        print(f"[PCB] Loaded {len(pcb_data)} processes from snapshot")
        for p in pcb_data:
            print(f"  PID={p['pid']} name={p['name']} state={p['state']}")
        return pcb_data
    except Exception as e:
        print(f"[ERROR] Failed to read PCB snapshot: {e}")
        return None

# ════════════════════════════════════════
# STEP 3: CONVERT PCB TO SCHEDULER FORMAT
# ════════════════════════════════════════
def convert_pcb_to_processes(pcb_data):
    print("\n" + "=" * 60)
    print(" STEP 3: Converting PCB data for Scheduler")
    print("=" * 60)

    processes = []
    for i, p in enumerate(pcb_data):
        proc = {
            'pid': p['pid'],
            'name': p['name'],
            'arrival_time': i,
            'burst_time': max(1, p.get('burst_time', 5)),
            'priority': p.get('priority', 1),
            'memory_req_kb': p.get('memory_req_kb', 512),
            'remaining_time': max(1, p.get('burst_time', 5))
        }
        processes.append(proc)

    print(f"[Converter] Converted {len(processes)} processes")
    return processes

# ════════════════════════════════════════
# STEP 4: RUN PYTHON SCHEDULER
# ════════════════════════════════════════
def run_scheduler(processes):
    print("\n" + "=" * 60)
    print(" STEP 4: Running Python Scheduler")
    print("=" * 60)

    sys.path.insert(0, SCHEDULER_DIR)

    try:
        from scheduler_sim import (fcfs, sjf, priority_scheduling,
                                   round_robin, calculate_metrics)
        from gantt import generate_all_charts

        results = {}

        # Run all 4 algorithms
        schedule = fcfs(processes)
        metrics, agg = calculate_metrics(processes, schedule)
        results['FCFS'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}
        print(f"[FCFS] Avg WT={agg['avg_waiting_time']} Avg TAT={agg['avg_turnaround_time']}")

        schedule = sjf(processes)
        metrics, agg = calculate_metrics(processes, schedule)
        results['SJF'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}
        print(f"[SJF] Avg WT={agg['avg_waiting_time']} Avg TAT={agg['avg_turnaround_time']}")

        schedule = priority_scheduling(processes)
        metrics, agg = calculate_metrics(processes, schedule)
        results['Priority'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}
        print(f"[Priority] Avg WT={agg['avg_waiting_time']} Avg TAT={agg['avg_turnaround_time']}")

        schedule = round_robin(processes, 3)
        metrics, agg = calculate_metrics(processes, schedule)
        results['RoundRobin'] = {'schedule': schedule, 'metrics': metrics, 'aggregates': agg}
        print(f"[RoundRobin] Avg WT={agg['avg_waiting_time']} Avg TAT={agg['avg_turnaround_time']}")

        # Generate charts
        try:
            generate_all_charts(results, processes)
            print("[Charts] Gantt charts generated successfully")
        except Exception as e:
            print(f"[Charts] Skipped: {e}")

        return results

    except Exception as e:
        print(f"[ERROR] Scheduler failed: {e}")
        return None

# ════════════════════════════════════════
# STEP 5: GENERATE REPORT
# ════════════════════════════════════════
def generate_report(results):
    print("\n" + "=" * 60)
    print(" STEP 5: Generating Simulation Report")
    print("=" * 60)

    report = {
        'timestamp': datetime.now().isoformat(),
        'student': 'Hope Bottoman',
        'reg_number': '25-311-351-026',
        'module': '351 CS 2104',
        'results': {}
    }

    for algo, data in results.items():
        report['results'][algo] = data['aggregates']

    with open(REPORT_FILE, 'w') as f:
        json.dump(report, f, indent=2)

    print(f"[Report] Saved to {REPORT_FILE}")
    print("\n[SUMMARY]")
    for algo, agg in report['results'].items():
        print(f"  {algo:12} | WT={agg['avg_waiting_time']:6} | "
              f"TAT={agg['avg_turnaround_time']:6} | "
              f"CPU={agg['cpu_utilisation']}%")

    return report

# ════════════════════════════════════════
# MAIN
# ════════════════════════════════════════
def main():
    print("\n")
    print("╔══════════════════════════════════════════════╗")
    print("║     EduOS Main Controller                   ║")
    print("║     Student: Hope Bottoman                  ║")
    print("║     Reg No:  25-311-351-026                 ║")
    print("╚══════════════════════════════════════════════╝")
    print()

    # Step 1: Launch C simulator
    success = launch_c_simulator()
    if not success:
        print("[WARNING] C simulator had issues, continuing with PCB snapshot...")

    # Step 2: Read PCB snapshot
    pcb_data = read_pcb_snapshot()
    if not pcb_data:
        print("[INFO] Using default test processes instead")
        pcb_data = [
            {'pid': 1001, 'name': 'init', 'burst_time': 20,
             'priority': 1, 'memory_req_kb': 1024, 'state': 'TERMINATED'},
            {'pid': 1002, 'name': 'calculator', 'burst_time': 5,
             'priority': 1, 'memory_req_kb': 1024, 'state': 'TERMINATED'},
            {'pid': 1003, 'name': 'text_editor', 'burst_time': 8,
             'priority': 1, 'memory_req_kb': 1024, 'state': 'TERMINATED'},
        ]

    # Step 3: Convert PCB data
    processes = convert_pcb_to_processes(pcb_data)

    # Step 4: Run scheduler
    results = run_scheduler(processes)
    if not results:
        print("[ERROR] Scheduler failed!")
        sys.exit(1)

    # Step 5: Generate report
    report = generate_report(results)

    print("\n" + "=" * 60)
    print(" EduOS Simulation Complete!")
    print("=" * 60)

if __name__ == '__main__':
    main()
