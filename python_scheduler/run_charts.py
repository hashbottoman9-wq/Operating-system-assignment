from scheduler_sim import generate_random_processes, fcfs, sjf, priority_scheduling, round_robin, calculate_metrics
from gantt import generate_all_charts

processes = generate_random_processes(10, 42)
results = {}

s = fcfs(processes); m, a = calculate_metrics(processes, s)
results['FCFS'] = {'schedule': s, 'metrics': m, 'aggregates': a}

s = sjf(processes); m, a = calculate_metrics(processes, s)
results['SJF'] = {'schedule': s, 'metrics': m, 'aggregates': a}

s = priority_scheduling(processes); m, a = calculate_metrics(processes, s)
results['Priority'] = {'schedule': s, 'metrics': m, 'aggregates': a}

s = round_robin(processes, 3); m, a = calculate_metrics(processes, s)
results['RoundRobin'] = {'schedule': s, 'metrics': m, 'aggregates': a}

generate_all_charts(results, processes)
print('Done!')
