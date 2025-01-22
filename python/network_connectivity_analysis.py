import multiprocessing
import pathlib
import networkx as nx
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import multiprocessing
import time

# Directories
module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
result_dir = root_dir.joinpath('results_swarm_comms')
graph_dir = root_dir.joinpath('graphs')


# Inputs
distances = [100.0, 400.0, 800.0, 1000.0]
significant_wave_hts = [0.5, 1.0, 1.5, 2.0, 3.0]
wind_speeds = [50.0,]
rainfall_rates = [10.0,]
antenna_hts = [0.05, 0.1, 0.5, 1.0, 2.0, 3.0]
radio_frequencys = [868.0, 2400.0, 5200.0]
total_simulation_time = 15 * 60 # Sec
simulation_time_step = 40 / 1000 # milli-sec
total_simulation_steps = int(total_simulation_time / simulation_time_step)


def analyse_graph(distance, significant_wave_ht, wind_speed, rainfall_rate, antenna_ht, radio_frequency):
    avg_deg_connectivity = []
    global_efficiency = []
    num_sub_graphs = []
    # Open the comms result file
    comms_result_file_path = result_dir.joinpath(f"comms_result_{int(distance)}_{significant_wave_ht:.1f}_{int(wind_speed)}_{int(rainfall_rate)}_{antenna_ht:.2f}_{int(radio_frequency)}.csv")
    # Load data
    df_comms_result = pd.read_csv(comms_result_file_path)
    # Create new columns for convenience
    df_comms_result["(x1,y1)"] = list(zip(df_comms_result["x1(m)"], df_comms_result["y1(m)"]))
    df_comms_result["(x2,y2)"] = list(zip(df_comms_result["x2(m)"], df_comms_result["y2(m)"]))
    # Group by connection pairs
    df_comms_result = df_comms_result.groupby(["(x1,y1)", "(x2,y2)"])
    # Create nodes and edges in graph
    for i in range(total_simulation_steps):
        # Init graph
        graph = nx.Graph()
        for name, group in df_comms_result:
            group = group.reset_index()
            node1, node2 = name
            x1, y1 = node1
            x2, y2 = node2
            graph.add_node(node1, x=x1, y=y1)
            graph.add_node(node2, x=x2, y=y2)
            if group["fresnel_clearance(%)"][i] >= 60.0: # %
                graph.add_edge(node1, node2,
                                distance = group["distance(m)"].mean(),
                                diff_loss = group["diffraction_loss(dB)"].to_list(),
                                fresnel_clearance = group["fresnel_clearance(%)"].to_list())
        avg_deg_connectivity.append(nx.average_degree_connectivity(graph))
        global_efficiency.append(nx.global_efficiency(graph))
        num_sub_graphs.append(nx.number_connected_components(graph))
    # Write results to file
    graph_file_path = graph_dir.joinpath(f"graph_{int(distance)}_{significant_wave_ht:.1f}_{int(wind_speed)}_{int(rainfall_rate)}_{antenna_ht:.2f}_{int(radio_frequency)}.csv")
    df_graph = pd.DataFrame()
    df_graph["avg_deg_connectivity"] = avg_deg_connectivity
    df_graph["global_efficiency"] = global_efficiency
    df_graph["num_sub_graphs"] = num_sub_graphs
    df_graph.to_csv(graph_file_path)

# Multiprocess the jobs
# Create jobs
jobs = []
for distance in distances:
    for significant_wave_ht in significant_wave_hts:
        for wind_speed in wind_speeds:
            for rainfall_rate in rainfall_rates:
                for antenna_ht in antenna_hts:
                    for radio_frequency in radio_frequencys:
                        p = multiprocessing.Process(target=analyse_graph, args=(distance, significant_wave_ht, wind_speed, rainfall_rate, antenna_ht, radio_frequency))
                        jobs.append(p)
# Process jobs
max_pool_size = multiprocessing.cpu_count()
jobs_start_index = 0
jobs_end_index = 0
exe_times = []
while jobs_end_index < len(jobs):
    print("\n")
    count_jobs_remaining = len(jobs) - jobs_end_index
    jobs_end_index = jobs_start_index + min(max_pool_size, count_jobs_remaining)
    process_pool = []
    # Spawn threads
    start_time = time.time()
    for i in range(jobs_start_index, jobs_end_index):
        print(f"proceccsing job {i}/{len(jobs)}")
        process_pool.append(jobs[i])
        jobs[i].start()
    # Join threads
    for i in range(jobs_start_index, jobs_end_index):
        print(f"joining job {i}/{len(jobs)}")
        jobs[i].join()
    end_time = time.time()
    time_elapsed = end_time - start_time
    exe_times.append(time_elapsed)
    # Compute predicted completion time
    avg_exec_time = sum(exe_times) / len(exe_times)
    jobs_completed = jobs_end_index + 1
    remaining_jobs = len(jobs) - jobs_completed
    avg_time_per_job = avg_exec_time/jobs_completed
    predicted_completion_time = end_time + remaining_jobs*avg_time_per_job
    print(f"\nPredicted completion time = {predicted_completion_time}")
    jobs_start_index = jobs_end_index