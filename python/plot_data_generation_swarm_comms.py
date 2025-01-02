import pathlib
import networkx as nx
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np

# Inputs
distance = 800.0
significant_wave_ht = 1.0
wind_speed = 50.0
rainfall_rate = 20.0
antenna_ht = 2.0
radio_frequency = 868.0

# Open the comms result file
module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
result_dir = root_dir.joinpath('results_swarm_comms')
file_path = result_dir.joinpath(f"comms_result_{int(distance)}_{significant_wave_ht:.1f}_{int(wind_speed)}_{int(rainfall_rate)}_{antenna_ht:.2f}_{int(radio_frequency)}.csv")
# Load data
df = pd.read_csv(file_path)
# Create new columns for convenience
df["(x1,y1)"] = list(zip(df["x1(m)"], df["y1(m)"]))
df["(x2,y2)"] = list(zip(df["x2(m)"], df["y2(m)"]))
# Group by connection pairs
df = df.groupby(["(x1,y1)", "(x2,y2)"])
# Init graph
graph = nx.Graph()
# Create nodes and edges in graph
count_timesteps = 0
for name, group in df:
    node1, node2 = name
    x1, y1 = node1
    x2, y2 = node2
    graph.add_node(node1, x=x1, y=y1)
    graph.add_node(node2, x=x2, y=y2)
    graph.add_edge(node1, node2,
                    distance = group["distance(m)"].mean(),
                    significant_wave_ht = group["sig_wave_ht(m)"].mean(),
                    wind_speed = group["wind_speed(km/hr)"].mean(),
                    rainfall_rate = group["rainfall(mm/hr)"].mean(),
                    ant_ht = group["ant_ht(m)"].mean(),
                    radio_frequency = group["radio_freq(MHz)"].mean(),
                    path_loss = group["free_space_path_loss(dB)"].mean(),
                    diff_loss = group["diffraction_loss(dB)"].to_list(),
                    fresnel_clearance = group["fresnel_clearance(%)"].to_list(),
                    rain_attenuation = group["rain_attenuation(dB)"].mean(),
                    sea_spray_attenuation = group["sea_spray_attenuation(dB)"].mean())
    count_timesteps = len(group["fresnel_clearance(%)"].to_list())


# Function to update the color of nodes and edges at each timestep
def update_graph(frame):
    plt.clf()  # Clear the current figure to redraw the graph
    # Set node color based on the 'value' attribute
    # node_color = [graph.nodes[node]['value'] for node in graph.nodes()]
    node_color = "skyblue"
    pos = {node: (attrs['x'], attrs['y']) for node, attrs in graph.nodes(data=True)}
    # Set edge color based on the 'weight' attribute
    # edge_color = [graph[u][v]['fresnel_clearance'][frame] for u, v in graph.edges()]
    edge_color = []
    for node1, node2 in graph.edges():
        if graph[node1][node2]['fresnel_clearance'][frame] >= 80.0:
            edge_color.append('green')
        elif graph[node1][node2]['fresnel_clearance'][frame] >= 60.0:
            edge_color.append('yellow')
        else:
            edge_color.append('red')
    # Draw the graph with updated colors
    nx.draw(graph, pos, with_labels=True, node_color=node_color, edge_color=edge_color,
            cmap=plt.cm.Blues, node_size=500, font_size=12, width=2)
    plt.title(f"Graph at Timestep {frame}")

# Create the plot and animation
fig, ax = plt.subplots(figsize=(8, 6))
ani = animation.FuncAnimation(fig, update_graph, frames=count_timesteps, interval=40)
# Display the animation
plt.show()




# # Static plot the nodes and the connections.
# # Extract positions from the node attributes
# pos = {node: (attrs['x'], attrs['y']) for node, attrs in graph.nodes(data=True)}
# # Plot the graph using the custom positions
# nx.draw(graph, pos, with_labels=True, node_color="skyblue", node_size=500, font_size=10)
# # Set the axes to have equal scale
# plt.gca().set_aspect('equal', adjustable='datalim')
# # Show the plot
# plt.title("Graph with Nodes Plotted Using x, y Attributes (Uniform Scale)")
# plt.show()