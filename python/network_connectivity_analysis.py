import math
import pathlib
import networkx as nx
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
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
    edge_connectivities = []
    num_sub_graphs = []
    clustering_coefficients = []
    # Open the comms result file
    comms_result_file_path = result_dir.joinpath(f"comms_result_{int(distance)}_{significant_wave_ht:.1f}_{int(wind_speed)}_{int(rainfall_rate)}_{antenna_ht:.2f}_{int(radio_frequency)}.csv")
    # Load data
    df_comms_result = pd.read_csv(comms_result_file_path)
    # Create new columns for convenience
    df_comms_result["(x1,y1)"] = list(zip(df_comms_result["x1(m)"], df_comms_result["y1(m)"]))
    df_comms_result["(x2,y2)"] = list(zip(df_comms_result["x2(m)"], df_comms_result["y2(m)"]))
    # Group by connection pairs
    df_comms_result = df_comms_result.groupby(["(x1,y1)", "(x2,y2)"])
    # Init graph
    graph = nx.Graph()
    # Create nodes and edges in graph
    for name, group in df_comms_result:
        group = group.reset_index()
        node1, node2 = name
        x1, y1 = node1
        x2, y2 = node2
        graph.add_node(node1, x=x1, y=y1)
        graph.add_node(node2, x=x2, y=y2)
        graph.add_edge(node1, node2,
                       distance = group["distance(m)"].mean(),
                       diff_loss = group["diffraction_loss(dB)"].to_list(),
                       fresnel_clearance = group["fresnel_clearance(%)"].to_list(),
                       total_loss = (group["free_space_path_loss(dB)"] +
                                     group["diffraction_loss(dB)"] +
                                     group["rain_attenuation(dB)"] +
                                     group["sea_spray_attenuation(dB)"]).to_list())
    for i in range(total_simulation_steps):
        fresnel_clearances = [edge['fresnel_clearance'][i] for _, _, edge in graph.edges(data=True)]
        avg_fresnel_clearance = sum(fresnel_clearances) / len(fresnel_clearances)
        avg_fresnel_clearances.append(avg_fresnel_clearance)
        min_fresnel_clearance = min(fresnel_clearances)
        min_fresnel_clearances.append(min_fresnel_clearance)
        diff_losses = [edge['diff_loss'][i] for _, _, edge in graph.edges(data=True)]
        avg_diff_loss = sum(diff_losses) / len(diff_losses)
        avg_diff_losses.append(avg_diff_loss)
        min_diff_loss = min(diff_losses)
        min_diff_losses.append(min_diff_loss)
        total_losses = [edge['total_loss'][i] for _, _, edge in graph.edges(data=True)]
        avg_total_loss = sum(total_losses) / len(total_losses)
        avg_total_losses.append(avg_total_loss)
        min_total_loss = min(total_losses)
        min_total_losses.append(min_total_loss)
        # Subgraphs for network analysis
        edges_to_keep = [(u, v) for u, v, edge in graph.edges(data=True) if edge.get('fresnel_clearance', 0)[i] >= 20]
        subgraph = graph.edge_subgraph(edges_to_keep).copy()
        edge_connectivities.append(nx.edge_connectivity(subgraph))
        num_sub_graphs.append(nx.number_connected_components(subgraph))
        clustering_coefficients.append(nx.average_clustering(subgraph, weight='distance'))
    # Write results to file
    graph_file_path = graph_dir.joinpath(f"graph_{int(distance)}_{significant_wave_ht:.1f}_{int(wind_speed)}_{int(rainfall_rate)}_{antenna_ht:.2f}_{int(radio_frequency)}.csv")
    df_graph = pd.DataFrame()
    df_graph["avg_fresnel_clearance"] = avg_fresnel_clearances
    df_graph["min_fresnel_clearance"] = min_fresnel_clearances
    df_graph["avg_diff_loss"] = avg_diff_losses
    df_graph["min_diff_loss"] = min_diff_losses
    df_graph["avg_total_loss"] = avg_total_losses
    df_graph["min_total_loss"] = min_total_losses
    df_graph["edge_connectivity"] = edge_connectivities
    df_graph["num_sub_graphs"] = num_sub_graphs
    df_graph["clustering_coefficient"] = clustering_coefficients
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
number_of_process_pools = math.ceil(len(jobs) / max_pool_size)
jobs_start_index = 0
jobs_end_index = 0
exe_times = []
start_time = datetime.now()
while jobs_end_index < len(jobs):
    print("\n")
    count_jobs_remaining = len(jobs) - jobs_end_index
    jobs_end_index = jobs_start_index + min(max_pool_size, count_jobs_remaining)
    process_pool = []
    # Spawn threads
    process_pool_start_time = time.time()
    for i in range(jobs_start_index, jobs_end_index):
        print(f"proceccsing job {i+1}/{len(jobs)}")
        process_pool.append(jobs[i])
        jobs[i].start()
    # Join threads
    for i in range(jobs_start_index, jobs_end_index):
        print(f"joining job {i+1}/{len(jobs)}")
        jobs[i].join()
    process_pool_end_time = time.time()
    time_elapsed = process_pool_end_time - process_pool_start_time
    exe_times.append(time_elapsed)
    # Compute predicted completion time
    avg_exec_time_per_pool = sum(exe_times) / len(exe_times)
    predicted_completion_time = start_time + timedelta(seconds=avg_exec_time_per_pool*number_of_process_pools)
    print(f"\nPredicted completion time = {predicted_completion_time}")
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
                               "min_total_loss,"
                               "edge_connectivity",
                               "num_sub_graphs",
                               "clustering_coefficient"])
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


# Plot Edge Connectivity
print("Plotting Edge Connectivity")
plt.clf()
num_rows = len(distances)
num_cols = len(antenna_hts)
fig, axes = plt.subplots(num_rows, num_cols, sharex=True, sharey=True, figsize=(12, 8))  # Adjust figure size for clarity
plt.subplots_adjust(left=0.07, right=0.95, top=0.92, bottom=0.1, hspace=0.3, wspace=0.2)  # Improved spacing
# Setting the values for all axes.
handles = None
labels = None
for i in range(num_rows):
    for j in range(num_cols):
        distance = distances[i]
        ant_ht = antenna_hts[j]
        data = df.loc[(df["ant_ht(m)"] == ant_ht) & (df["distance(m)"] == distance)]
        lineplt1 = sns.lineplot(
            data=data, x="sig_wave_ht(m)", y="edge_connectivity",
            linestyle="--", marker="o", hue="radio_freq(Hz)",
            estimator="mean", errorbar="ci", err_kws={"alpha": 0.1}, ax=axes[i][j]
        )
        axes[i][j].set(xlabel=None, ylabel=None)
        handles, labels = axes[i][j].get_legend_handles_labels()
        axes[i][j].get_legend().remove()
        axes[i][j].tick_params(axis='both', which='major', labelsize=6) # Reduce tick size
        axes[i][j].yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f"{x:.1f}"))  # One decimal place
# Add legend
labels = [
    "Edge connectivity (868MHz)",
    "Edge connectivity (2400MHz)",
    "Edge connectivity (5200MHz)"
]
fig.legend(
    handles=handles, labels=labels, loc="lower center", ncols=3, fontsize=6,
    frameon=False
)
# Title
fig.suptitle(
    "Impact of Wave Height, Antenna Height, Antenna Distance, and Radio Frequency on Edge Connectivity",
    fontsize=10, fontweight='bold', y=0.98
)
# Axis labels
fig.text(0.5, 0.05, 'Significant Wave Height (m)', ha='center', fontsize=8)
fig.text(0.04, 0.5, 'Edge Connectivity', va='center', rotation='vertical', fontsize=8)
# Column annotations (Antenna heights)
x = 0.13
y = 0.94
dx = 0.153
for i in range(num_cols):
    fig.text(
        x + (i * dx), y, f'Antenna Height = {antenna_hts[i]} m',
        ha='center', fontsize=6, fontweight='medium'
    )
# Row annotations (Distances)
x = 0.96
y = 0.83
dy = 0.22
for i in range(num_rows):
    fig.text(
        x, y - (i * dy), f'Distance = {distances[i]} m',
        va='center', rotation='vertical', fontsize=6, fontweight='medium'
    )
# Ensure layout is tight
# fig.tight_layout(rect=[0, 0, 1, 0.96])
# Save and display
file_fig = graph_dir.joinpath("edge_connectivity.png")
plt.savefig(file_fig, dpi=300)
# plt.show()


# Plot Number of Sub-graphs
print("Plotting Number of Sub-graphs")
plt.clf()
num_rows = len(distances)
num_cols = len(antenna_hts)
fig, axes = plt.subplots(num_rows, num_cols, sharex=True, sharey=True, figsize=(12, 8))  # Adjust figure size for clarity
plt.subplots_adjust(left=0.07, right=0.95, top=0.92, bottom=0.1, hspace=0.3, wspace=0.2)  # Improved spacing
# Setting the values for all axes.
handles = None
labels = None
for i in range(num_rows):
    for j in range(num_cols):
        distance = distances[i]
        ant_ht = antenna_hts[j]
        data = df.loc[(df["ant_ht(m)"] == ant_ht) & (df["distance(m)"] == distance)]
        lineplt1 = sns.lineplot(
            data=data, x="sig_wave_ht(m)", y="num_sub_graphs",
            linestyle="--", marker="o", hue="radio_freq(Hz)",
            estimator="mean", errorbar="ci", err_kws={"alpha": 0.1}, ax=axes[i][j]
        )
        axes[i][j].set(xlabel=None, ylabel=None)
        handles, labels = axes[i][j].get_legend_handles_labels()
        axes[i][j].get_legend().remove()
        axes[i][j].tick_params(axis='both', which='major', labelsize=6) # Reduce tick size
# Add legend
labels = [
    "Number of Sub-graphs (868MHz)",
    "Number of Sub-graphs (2400MHz)",
    "Number of Sub-graphs (5200MHz)"
]
fig.legend(
    handles=handles, labels=labels, loc="lower center", ncols=3, fontsize=6,
    frameon=False
)
# Title
fig.suptitle(
    "Impact of Wave Height, Antenna Height, Antenna Distance, and Radio Frequency on Number of Sub-graphs",
    fontsize=10, fontweight='bold', y=0.98
)
# Axis labels
fig.text(0.5, 0.05, 'Significant Wave Height (m)', ha='center', fontsize=8)
fig.text(0.04, 0.5, 'Number of Sub-graphs', va='center', rotation='vertical', fontsize=8)
# Column annotations (Antenna heights)
x = 0.13
y = 0.94
dx = 0.153
for i in range(num_cols):
    fig.text(
        x + (i * dx), y, f'Antenna Height = {antenna_hts[i]} m',
        ha='center', fontsize=6, fontweight='medium'
    )
# Row annotations (Distances)
x = 0.96
y = 0.83
dy = 0.22
for i in range(num_rows):
    fig.text(
        x, y - (i * dy), f'Distance = {distances[i]} m',
        va='center', rotation='vertical', fontsize=6, fontweight='medium'
    )
# Ensure layout is tight
# fig.tight_layout(rect=[0, 0, 1, 0.96])
# Save and display
file_fig = graph_dir.joinpath("sub_graphs.png")
plt.savefig(file_fig, dpi=300)
# plt.show()


# Plot Number of fresnel clearance
print("Plotting Fresnel Clearance")
plt.clf()
num_rows = len(distances)
num_cols = len(antenna_hts)
fig, axes = plt.subplots(num_rows, num_cols, sharex=True, sharey=True, figsize=(12, 8))  # Adjust figure size for clarity
plt.subplots_adjust(left=0.07, right=0.95, top=0.92, bottom=0.1, hspace=0.3, wspace=0.2)  # Improved spacing
# Setting the values for all axes.
handles = None
labels = None
for i in range(num_rows):
    for j in range(num_cols):
        distance = distances[i]
        ant_ht = antenna_hts[j]
        data = df.loc[(df["ant_ht(m)"] == ant_ht) & (df["distance(m)"] == distance)]
        lineplt1 = sns.lineplot(
            data=data, x="sig_wave_ht(m)", y="min_fresnel_clearance",
            linestyle=":", marker="o", hue="radio_freq(Hz)",
            estimator="mean", errorbar="ci", err_kws={"alpha": 0.1}, ax=axes[i][j]
        )
        lineplt2 = sns.lineplot(
            data=data, x="sig_wave_ht(m)", y="avg_fresnel_clearance",
            linestyle="--", marker="s", hue="radio_freq(Hz)",
            estimator="mean", errorbar="ci", err_kws={"alpha": 0.1}, ax=axes[i][j]
        )
        axes[i][j].set(xlabel=None, ylabel=None)
        handles, labels = axes[i][j].get_legend_handles_labels()
        axes[i][j].get_legend().remove()
        axes[i][j].tick_params(axis='both', which='major', labelsize=6) # Reduce tick size
# Add legend
labels = [
    "Min Fresnel Clearance (868MHz)",
    "Min Fresnel Clearance (2400MHz)",
    "Min Fresnel Clearance (5200MHz)",
    "Avg Fresnel Clearance (868MHz)",
    "Avg Fresnel Clearance (2400MHz)",
    "Avg Fresnel Clearance (5200MHz)"
]
fig.legend(
    handles=handles, labels=labels, loc="lower center", ncols=6, fontsize=6,
    frameon=False
)
# Title
fig.suptitle(
    "Impact of Wave Height, Antenna Height, Antenna Distance, and Radio Frequency on Fresnel Clearance",
    fontsize=10, fontweight='bold', y=0.98
)
# Axis labels
fig.text(0.5, 0.05, 'Significant Wave Height (m)', ha='center', fontsize=8)
fig.text(0.04, 0.5, 'Fresnel Clearance (%)', va='center', rotation='vertical', fontsize=8)
# Column annotations (Antenna heights)
x = 0.13
y = 0.94
dx = 0.153
for i in range(num_cols):
    fig.text(
        x + (i * dx), y, f'Antenna Height = {antenna_hts[i]} m',
        ha='center', fontsize=6, fontweight='medium'
    )
# Row annotations (Distances)
x = 0.96
y = 0.83
dy = 0.22
for i in range(num_rows):
    fig.text(
        x, y - (i * dy), f'Distance = {distances[i]} m',
        va='center', rotation='vertical', fontsize=6, fontweight='medium'
    )
# Ensure layout is tight
# fig.tight_layout(rect=[0, 0, 1, 0.96])
# Save and display
file_fig = graph_dir.joinpath("fresnel_clearance.png")
plt.savefig(file_fig, dpi=300)
# plt.show()
