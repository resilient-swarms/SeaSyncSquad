"""
This module simulates line-of-sight communication of two non-stationary ASVs. 
"""

import pathlib
import sys
module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
asvlite_wrapper_dir = root_dir.joinpath("dependency", "ASVLite", "wrapper", "cython")
sys.path.insert(0, str(asvlite_wrapper_dir))

import math
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import matplotlib.patches as mpatches
import numpy as np
import utils
import collections
from shapely.geometry import Point, LineString
from geometry import py_Coordinates_3D
from sea_surface import py_Sea_surface
from asv import py_Asv

# Simulation time step size
time_step_size = 40 # milli-sec

# Simulation time in seconds
time_duration = 3*60 # sec

# Sea surface variables
wave_hs = 1.2 # Significant wave height in m
wave_dp = math.pi/3 

# Sea surface
wave_rand_seed = 1
count_component_waves = 21
sea_surface = py_Sea_surface(wave_hs, wave_dp, wave_rand_seed, count_component_waves)

# ASV positions
position_1 = py_Coordinates_3D(103.3055419921875, 101.90850830078125)
position_2 = py_Coordinates_3D(3018.09765625, 3010.44873046875)
# position_1 = py_Coordinates_3D(100, 100)
# position_2 = py_Coordinates_3D(200, 200)

# Create a list of equidistant points between the positions
def create_equidistant_points(p1, p2):
    line = LineString([p1, p2])
    offset = 1.0 # m
    cumulative_offset = 0 # m
    points = []
    offsets = []
    while cumulative_offset < line.length:
        points.append(line.interpolate(cumulative_offset))
        offsets.append(cumulative_offset)
        cumulative_offset += offset
    points.append(p2)
    offsets.append(line.length)
    return points, offsets

p1 = Point(position_1.x, position_1.y)
p2 = Point(position_2.x, position_2.y)
points, offsets = create_equidistant_points(p1, p2)

# Initialise ASV
start_attitude = py_Coordinates_3D(0.0, 0.0, 0.0) # Both ASVs will have the same initial attitude
asv_1 = py_Asv(utils.asv_spec, sea_surface, position_1, start_attitude)
asv_2 = py_Asv(utils.asv_spec, sea_surface, position_2, start_attitude)
# Uncomment the below to make the ASVs stationary
# asv_1.py_set_surge_sway_halt(True)
# asv_2.py_set_surge_sway_halt(True)

# Antenna 
antenna_height = 1.0 # m

# Initialise animated plot
# Side view
fig, ax = plt.subplots()
ax1 = plt.subplot(212)
ax1.set_ylim(-2.0, 2.0)
# ax1.grid(True)
# ax1.set_aspect('equal', adjustable='box')
# Actors
wave_profile, = ax1.plot(offsets, [0]*len(points), color="cornflowerblue") # Draw the sea surface between the two points
antenna_1, = ax1.plot([0, 0], [0,antenna_height], color="grey") # Draw the antenna at p1
antenna_2, = ax1.plot([offsets[-1], offsets[-1]], [0,antenna_height], color="grey") # Draw the antenna at p2
line_of_sight, = ax1.plot([0, offsets[-1]], [antenna_height, antenna_height], color="green", linestyle='--') # Draw the line connecting the tips of two antennas
text_asv1 = ax1.text(0, -wave_hs, "| ASV-1", ha='left', va='top')
text_asv2 = ax1.text(offsets[-1], -wave_hs, "ASV-2 |", ha='right', va='top')
ax1.set_title("Signal path between antennas on ASV-1 and ASV-2")
green_patch = mpatches.Patch(color='green', label='Signal clear')
red_patch = mpatches.Patch(color='red', label='Signal interrupted')
ax1.legend(handles=[green_patch, red_patch], loc="lower center")
ax1.set_xlabel("Distance along the line joining the two ASVs (m)")
ax1.set_ylabel("Z (m)")
# Top view - 1
ax2 = plt.subplot(221)
deque_length = 1500
asv1_x = collections.deque(maxlen = deque_length)
asv1_y = collections.deque(maxlen = deque_length)
asv1_x.append(position_1.x)
asv1_y.append(position_1.y)
asv1_path, = ax2.plot(asv1_x, asv1_y, color="grey")
ax2.set_xlim(min(asv1_x) - 2, max(asv1_x) + 2)
ax2.set_ylim(min(asv1_y) - 2, max(asv1_y) + 2)
ax2.set_title("Path of ASV-1")
ax2.set_xlabel("X (m)")
ax2.set_ylabel("Y (m)")
# Top view - 2
ax3 = plt.subplot(222)
asv2_x = collections.deque(maxlen = deque_length)
asv2_y = collections.deque(maxlen = deque_length)
asv2_x.append(position_2.x)
asv2_y.append(position_2.y)
asv2_path, = ax3.plot(asv2_x, asv2_y, color="grey")
ax3.set_xlim(min(asv2_x) - 2, max(asv2_x) + 2)
ax3.set_ylim(min(asv2_y) - 2, max(asv2_y) + 2)
ax3.set_title("Path of ASV-2")
ax3.set_xlabel("X (m)")
ax3.set_ylabel("Y (m)")
# Show signal stats in a box
props = dict(boxstyle='square', edgecolor="black", linewidth=0.5, facecolor='lightyellow') # these are matplotlib.patch.Patch properties
signal_stats_string = " Average signal window = {:.2f} sec     |     Maximum signal window = {:.2f} sec     |     Clear signal percentage = {:.2f} % "
signal_stats = plt.text(0.5, 0.02, signal_stats_string.format(0, 0, 0), transform=plt.gcf().transFigure, bbox=props, ha="center")

current_signal_length = 0
max_signal_length = 0
avg_signal_length = 0
count_signals = 0

# Update data for each animation step
time = 0
def animate(i):
    global time, asv1_x, asv1_y, asv2_x, asv2_y
    global current_signal_length, max_signal_length, avg_signal_length, count_signals
    time += time_step_size/1000
    elevations = []
    wave_pattern = []
    # Update asv dynamics
    rudder_angle = math.pi/4
    asv_1.py_wg_compute_dynamics(rudder_angle, time_step_size)
    asv_2.py_wg_compute_dynamics(rudder_angle, time_step_size)
    position_1 = asv_1.py_get_position_origin()
    position_2 = asv_2.py_get_position_origin()
    asv1_attitude = asv_1.py_get_attitude()
    asv2_attitude = asv_2.py_get_attitude()    
    p1 = Point(position_1.x, position_1.y)
    p2 = Point(position_2.x, position_2.y)
    points, offsets = create_equidistant_points(p1, p2)  
    # Draw the sea surface  
    for i in range(len(points)):
        p = py_Coordinates_3D(points[i].x, points[i].y)
        elevation = sea_surface.py_get_elevation(p, time)
        elevations.append(elevation)
        wave_pattern.append((offsets[i], elevation))
    wave_pattern = LineString(wave_pattern) # Convert to a line string
    wave_profile.set_data(offsets, elevations)  # update the sea surface actor
    # Draw the paths of the ASVs
    asv1_x.append(position_1.x)
    asv1_y.append(position_1.y)
    asv1_path.set_data(asv1_x, asv1_y)
    ax2.set_xlim(min(asv1_x) - 2, max(asv1_x) + 2)
    ax2.set_ylim(min(asv1_y) - 2, max(asv1_y) + 2)
    asv2_x.append(position_2.x)
    asv2_y.append(position_2.y)
    asv2_path.set_data(asv2_x, asv2_y)
    ax3.set_xlim(min(asv2_x) - 2, max(asv2_x) + 2)
    ax3.set_ylim(min(asv2_y) - 2, max(asv2_y) + 2)
    # Compute the antenna heights considering roll and pitch of the vehicles
    z_antenna_1 = elevations[0]  + antenna_height*math.cos(asv1_attitude.x)*math.cos(asv1_attitude.y)
    z_antenna_2 = elevations[-1] + antenna_height*math.cos(asv2_attitude.x)*math.cos(asv2_attitude.y)
    # Draw the antennas
    antenna_1.set_data([0, 0], [elevations[0], z_antenna_1]) # Update the antenna actor
    antenna_2.set_data([offsets[-1], offsets[-1]], [elevations[-1], z_antenna_2]) # Update the antenna actor
    # Draw the signal path
    signal_path = LineString([(0, z_antenna_1), (offsets[-1], z_antenna_2)])
    is_signal_path_clear = True if signal_path.intersection(wave_pattern).is_empty else False 
    line_of_sight.set_data([0, offsets[-1]], [z_antenna_1, z_antenna_2]) # Update line of sight
    if is_signal_path_clear:
        current_signal_length += time_step_size/1000
    else:
        if current_signal_length != 0.0:
            max_signal_length = current_signal_length if current_signal_length > max_signal_length else max_signal_length
            total_signal_length = avg_signal_length * count_signals + current_signal_length
            count_signals += 1
            avg_signal_length = total_signal_length / count_signals
            current_signal_length = 0.0
        else:
            # Signal window not yet open
            pass
    clear_line_of_sight_percentage  = ((avg_signal_length * count_signals) + current_signal_length)/time * 100
    signal_stats.set_text(signal_stats_string.format(avg_signal_length, max_signal_length, clear_line_of_sight_percentage))
    line_of_sight.set_color("green" if is_signal_path_clear else "red") # Change color if signal path not clear
    text_asv1.set_x(0)
    text_asv2.set_x(offsets[-1])
    ax1.set_xlim(-100, offsets[-1]+100)
    return wave_profile, antenna_1, antenna_2, line_of_sight, asv1_path, asv2_path

# Start simulation and animation
ani = animation.FuncAnimation(
    fig, animate, interval=time_step_size, blit=False, save_count=50)

# To save the animation, use e.g.
#
# ani.save("movie.mp4")
#
# or
#
# writer = animation.FFMpegWriter(
#     fps=15, metadata=dict(artist='Me'), bitrate=1800)
# ani.save("movie.mp4", writer=writer)

plt.show()
