import multiprocessing
import pathlib
import networkx as nx
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import seaborn as sns
import numpy as np
import multiprocessing
import time
from datetime import datetime, timedelta

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
    avg_fresnel_clearances = []
    min_fresnel_clearances = []
    avg_diff_losses = []
    min_diff_losses = []
    avg_total_losses = []
    min_total_losses = []
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
            graph.add_edge(node1, node2,
                           distance = group["distance(m)"].mean(),
                           diff_loss = group["diffraction_loss(dB)"][i],
                           fresnel_clearance = group["fresnel_clearance(%)"][i],
                           total_loss = group["free_space_path_loss(dB)"][i] +
                                        group["diffraction_loss(dB)"][i] +
                                        group["rain_attenuation(dB)"][i] +
                                        group["sea_spray_attenuation(dB)"][i])
        fresnel_clearances = [edge['fresnel_clearance'] for _, _, edge in graph.edges(data=True)]
        avg_fresnel_clearance = sum(fresnel_clearances) / len(fresnel_clearances)
        avg_fresnel_clearances.append(avg_fresnel_clearance)
        min_fresnel_clearance = min(fresnel_clearances)
        min_fresnel_clearances.append(min_fresnel_clearance)
        diff_losses = [edge['diff_loss'] for _, _, edge in graph.edges(data=True)]
        avg_diff_loss = sum(diff_losses) / len(diff_losses)
        avg_diff_losses.append(avg_diff_loss)
        min_diff_loss = min(diff_losses)
        min_diff_losses.append(min_diff_loss)
        total_losses = [edge['total_loss'] for _, _, edge in graph.edges(data=True)]
        avg_total_loss = sum(total_losses) / len(total_losses)
        avg_total_losses.append(avg_total_loss)
        min_total_loss = min(total_losses)
        min_total_losses.append(min_total_loss)
        # num_sub_graphs.append(nx.number_connected_components(graph))
    # Write results to file
    graph_file_path = graph_dir.joinpath(f"graph_{int(distance)}_{significant_wave_ht:.1f}_{int(wind_speed)}_{int(rainfall_rate)}_{antenna_ht:.2f}_{int(radio_frequency)}.csv")
    df_graph = pd.DataFrame()
    df_graph["avg_fresnel_clearance"] = avg_fresnel_clearances
    df_graph["min_fresnel_clearance"] = min_fresnel_clearances
    df_graph["avg_diff_loss"] = avg_diff_losses
    df_graph["min_diff_loss"] = min_diff_losses
    df_graph["avg_total_loss"] = avg_total_losses
    df_graph["min_total_loss"] = min_total_losses
    # df_graph["num_sub_graphs"] = num_sub_graphs
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
    exe_times.append(time_elapsed/len(process_pool))
    # Compute predicted completion time
    avg_exec_time = sum(exe_times) / len(exe_times)
    jobs_completed = jobs_end_index + 1
    remaining_jobs = len(jobs) - jobs_completed
    predicted_completion_time = datetime.now() + timedelta(seconds=remaining_jobs*avg_exec_time)
    print(f"\nCompleted {jobs_completed}/{len(jobs)} jobs with an average execution time of {avg_exec_time:.2f} seconds.")
    print(f"Predicted completion time = {predicted_completion_time}")
    jobs_start_index = jobs_end_index

# Read all results into a single data frame
df = None
cumulative_data_file_path = graph_dir.joinpath("cumulative.csv")
if cumulative_data_file_path.exists():
    print(f"Reading {cumulative_data_file_path}")
    df = pd.read_csv(cumulative_data_file_path)
else:
    df = pd.DataFrame(columns=["distance(m)",
                               "sig_wave_ht(m)",
                               "wind_speed(km/hr)",
                               "rainfall_rate(mm/hr)",
                               "ant_ht(m)",
                               "radio_freq(Hz)",
                               "avg_fresnel_clearance",
                               "min_fresnel_clearance",
                               "avg_diff_loss",
                               "min_diff_loss",
                               "total_loss",
                               "min_total_loss"])
    for distance in distances:
        for significant_wave_ht in significant_wave_hts:
            for wind_speed in wind_speeds:
                for rainfall_rate in rainfall_rates:
                    for antenna_ht in antenna_hts:
                        for radio_frequency in radio_frequencys:
                            graph_file_path = graph_dir.joinpath(f"graph_{int(distance)}_{significant_wave_ht:.1f}_{int(wind_speed)}_{int(rainfall_rate)}_{antenna_ht:.2f}_{int(radio_frequency)}.csv")
                            print(f"Reading {graph_file_path}")
                            df_graph = pd.read_csv(graph_file_path)
                            df_graph["distance(m)"] = distance
                            df_graph["sig_wave_ht(m)"] = significant_wave_ht
                            df_graph["wind_speed(km/hr)"] = wind_speed
                            df_graph["rainfall_rate(mm/hr)"] = rainfall_rate
                            df_graph["ant_ht(m)"] = antenna_ht
                            df_graph["radio_freq(Hz)"] = radio_frequency
                            df = pd.concat([df, df_graph], ignore_index=True)
    df.to_csv(cumulative_data_file_path)

# Plot results
plt.clf()
num_rows = len(distances)
num_cols = len(antenna_hts)
fig, axes = plt.subplots(num_rows, num_cols, sharex=True, sharey=True)
plt.subplots_adjust(left=0.04, right=0.97, top=0.95, bottom=0.06, hspace=0.15, wspace=0.05,)
# Setting the values for all axes.
handles = None
labels = None
for i in range(num_rows):
    for j in range(num_cols):
        distance = distances[i]
        ant_ht = antenna_hts[j]
        data = df.loc[(df["ant_ht(m)"] == ant_ht) & (df["distance(m)"] == distance)]
        lineplt = sns.lineplot(data=data, x="sig_wave_ht(m)", y="global_efficiency", linestyle="--", marker="o",
                               hue="radio_freq(Hz)", estimator="mean", errorbar="ci", err_kws={"alpha":0.1}, ax=axes[i][j])
        lineplt.set(xlabel=None, ylabel=None)
        # handles, labels = axes[i][j].get_legend_handles_labels()
        # axes[i][j].get_legend().remove()
# Add text
fig.legend(handles=handles, labels=["868MHz", "2400MHz", "5200MHz"], loc="upper right", ncols=2, fontsize=8)
fig.suptitle("Impact of Wave Height, Antenna height, Antenna Distance, and Radio Frequency on global efficiency", fontsize=10)
# Add text
fig.text(0.5, 0.04, 'X - Significant wave height (m)', ha='center', fontsize=8)
fig.text(0.01, 0.5, 'Y - Global efficiency', va='center', rotation='vertical', fontsize=8)
x = 0.145
y = 0.935
dx = 0.190
for i in range(num_cols):
    fig.text(x+(i*dx), y, 'Antenna Height = {} m'.format(antenna_hts[i]), ha='center', fontsize=8)
x = 0.98
y = 0.84
dy = 0.174
for i in range(num_rows):
    fig.text(x, y-(i*dy), 'Distance = {} m'.format(distances[i]), va='center', rotation='vertical', fontsize=8)
fig.tight_layout()
file_fig = graph_dir.joinpath("global_eff.png")
# plt.savefig(file_fig, dpi=300)
plt.show()
