"""
This module simulates line-of-sight communication of two stationary ASVs. 
The simulation consideres only the heave motion and does not consider the 
roll, pitch or yaw motions of the ASVs.
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
import numpy as np
from shapely.geometry import Point, LineString
from geometry import py_Coordinates_3D
from sea_surface import py_Sea_surface

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
p1 = Point(position_1.x, position_1.y)
p2 = Point(position_2.x, position_2.y)
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

# Antenna 
antenna_height = 1.0 # m

# Create animated plot
fig, ax = plt.subplots()
ax.set_ylim(-2.0, 2.0)
# ax.grid(True)
# ax.set_aspect('equal', adjustable='box')
# Actors
wave_profile, = ax.plot(offsets, [0]*len(points), color="blue") # Draw the sea surface between the two points
antenna_1, = ax.plot([0, 0], [0,antenna_height], color="brown") # Draw the antenna at p1
antenna_2, = ax.plot([offsets[-1], offsets[-1]], [0,antenna_height], color="brown") # Draw the antenna at p2
line_of_sight, = ax.plot([0, offsets[-1]], [antenna_height, antenna_height], color="green", linestyle='--') # Draw the line connecting the tips of two antennas

# Update data for each animation step
time = 0
def animate(i):
    global time
    time += time_step_size/1000
    elevations = []
    wave_pattern = []
    for i in range(len(points)):
        p = py_Coordinates_3D(points[i].x, points[i].y)
        elevation = sea_surface.py_get_elevation(p, time)
        elevations.append(elevation)
        wave_pattern.append((offsets[i], elevation))
    wave_pattern = LineString(wave_pattern) # Convert to a line string
    signal_path = LineString([(0, elevations[0]+antenna_height), (offsets[-1], elevations[-1]+antenna_height)])
    is_signal_path_clear = True if signal_path.intersection(wave_pattern).is_empty else False 
    wave_profile.set_ydata(elevations)  # update the sea surface actor
    antenna_1.set_ydata([elevations[0], elevations[0]+antenna_height]) # Update the antenna actor
    antenna_2.set_ydata([elevations[-1], elevations[-1]+antenna_height]) # Update the antenna actor
    line_of_sight.set_ydata([elevations[0]+antenna_height, elevations[-1]+antenna_height]) # Update line of sight
    line_of_sight.set_color("green" if is_signal_path_clear else "red") # Chage color if signal path not clear
    return wave_profile, antenna_1, antenna_2, line_of_sight

# Start simulation
ani = animation.FuncAnimation(
    fig, animate, interval=time_step_size, blit=True, save_count=50)

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
