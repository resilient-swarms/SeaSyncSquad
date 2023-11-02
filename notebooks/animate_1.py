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

class Simulate_LOS_communication:
    def __init__(self, sea, asv1, asv2):
        self.sea = sea
        self.asv1 = asv1 
        self.asv2 = asv2 
        # Simulation time step size
        self.time_step_size = 40 # milli-sec
        # Sea surface
        wave_rand_seed = 1
        count_component_waves = 21
        self.sea_surface = py_Sea_surface(sea["wave_hs"], sea["wave_dp"], wave_rand_seed, count_component_waves)
        # Initialise ASV
        self.py_asv1 = py_Asv(utils.asv_spec, self.sea_surface, asv1["position"], asv1["attitude"])
        self.py_asv2 = py_Asv(utils.asv_spec, self.sea_surface, asv2["position"], asv2["attitude"])
        self.py_asv1.py_set_surge_sway_halt(asv1["is_surge_sway_halt_set"])
        self.py_asv1.py_set_surge_sway_halt(asv1["is_surge_sway_halt_set"])
        # Create a list of equidistant points between the two ASVs
        p1 = Point(asv1["position"].x, asv1["position"].y)
        p2 = Point(asv2["position"].x, asv2["position"].y)
        points, offsets = self.__create_equidistant_points(p1, p2)
        # Initialise animation window
        self.fig, self.ax = plt.subplots()
        # Initialise the line of sight view
        self.ax1 = plt.subplot(212)
        self.ax1.set_ylim(-2*round(sea["wave_hs"]), 2*round(sea["wave_hs"]))
        # self.ax1.grid(True)
        # self.ax1.set_aspect('equal', adjustable='box')
        self.wave_plot, = self.ax1.plot(offsets, [0]*len(points), color="cornflowerblue") # Draw the sea surface between the two points
        self.antenna1_plot, = self.ax1.plot([offsets[0],  offsets[0]],  [0,self.asv1["antenna_height"]], color="black") # Draw the antenna at p1
        self.antenna2_plot, = self.ax1.plot([offsets[-1], offsets[-1]], [0,self.asv2["antenna_height"]], color="black") # Draw the antenna at p2
        self.los_plot, = self.ax1.plot([offsets[0], offsets[-1]], [self.asv1["antenna_height"], self.asv2["antenna_height"]], color="green", linestyle='--')
        self.text_asv1 = self.ax1.text(offsets[0], -sea["wave_hs"], "| ASV-1", ha='left', va='top')
        self.text_asv2 = self.ax1.text(offsets[-1], -sea["wave_hs"], "ASV-2 |", ha='right', va='top')
        self.ax1.set_title("Signal path between antennas on ASV-1 and ASV-2")
        green_patch = mpatches.Patch(color='green', label='Signal clear')
        red_patch = mpatches.Patch(color='red', label='Signal interrupted')
        self.ax1.legend(handles=[green_patch, red_patch], loc="lower center")
        self.ax1.set_xlabel("Distance along the line joining the two ASVs (m)")
        self.ax1.set_ylabel("Z (m)")
        # Initialise ASV-1 path view
        deque_length = 1500 
        self.ax2 = plt.subplot(231)
        self.asv1_path = collections.deque(maxlen = deque_length)
        self.asv1_path.append({"x":asv1["position"].x, "y":asv1["position"].y})
        self.asv1_path_plot, = self.ax2.plot([p["x"] for p in self.asv1_path], [p["y"] for p in self.asv1_path], color="lightgrey")
        self.ax2.set_xlim(min([p["x"] for p in self.asv1_path]) - utils.asv_spec.L_wl, max([p["x"] for p in self.asv1_path]) + utils.asv_spec.L_wl)
        self.ax2.set_ylim(min([p["y"] for p in self.asv1_path]) - utils.asv_spec.L_wl, max([p["x"] for p in self.asv1_path]) + utils.asv_spec.L_wl)
        self.ax2.set_title("Real-time position, heading, and trailing path of ASV-1")
        self.ax2.set_xlabel("X (m)")
        self.ax2.set_ylabel("Y (m)")
        self.ax2.set_aspect('equal', adjustable='box', share=True)
        # Initialise ASV-2 path view
        self.ax3 = plt.subplot(232)
        self.asv2_path = collections.deque(maxlen = deque_length)
        self.asv2_path.append({"x":asv2["position"].x, "y":asv2["position"].y})
        self.asv2_path_plot, = self.ax3.plot([p["x"] for p in self.asv2_path], [p["y"] for p in self.asv2_path], color="lightgrey")
        self.ax3.set_xlim(min([p["x"] for p in self.asv2_path]) - utils.asv_spec.L_wl, max([p["x"] for p in self.asv2_path]) + utils.asv_spec.L_wl)
        self.ax3.set_ylim(min([p["y"] for p in self.asv2_path]) - utils.asv_spec.L_wl, max([p["x"] for p in self.asv2_path]) + utils.asv_spec.L_wl)
        self.ax3.set_title("Real-time position, heading, and trailing path of ASV-2")
        self.ax3.set_xlabel("X (m)")
        self.ax3.set_ylabel("Y (m)")
        self.ax3.set_aspect('equal', adjustable='box')
        # Also draw arrows on the path views to show the real-time position and heading of the ASVs 
        self.arrow_length = utils.asv_spec.L_wl
        asv1_heading = asv1["attitude"].z
        asv1_delta_x = self.arrow_length * math.sin(asv1_heading)
        asv1_delta_y = self.arrow_length * math.cos(asv1_heading)
        self.arrow1_plot = self.ax2.arrow(asv1["position"].x, asv1["position"].y, asv1_delta_x, asv1_delta_y, shape='full', color="black", length_includes_head=True, head_width=.1)
        asv2_heading = asv2["attitude"].z
        asv2_delta_x = self.arrow_length * math.sin(asv2_heading)
        asv2_delta_y = self.arrow_length * math.cos(asv2_heading)
        self.arrow2_plot = self.ax3.arrow(asv2["position"].x, asv2["position"].y, asv2_delta_x, asv2_delta_y, shape='full', color="black", length_includes_head=True, head_width=.1)
        # Show a scatter plot of clear signal window sizes
        self.ax4 = plt.subplot(233)
        self.signal_window_sizes = []
        self.scatter_plot, = self.ax4.plot(list(range(len(self.signal_window_sizes))), self.signal_window_sizes, marker="o", linewidth=0, color="green")
        self.ax4.set_xlim(-0.5, 2)
        self.ax4.set_ylim(-0.5, 2)
        self.ax4.tick_params(
                            axis='x',          # changes apply to the x-axis
                            which='both',      # both major and minor ticks are affected
                            bottom=False,      # ticks along the bottom edge are off
                            top=False,         # ticks along the top edge are off
                            labelbottom=False) # labels along the bottom edge are off
        self.ax4.set_title("Scatter plot of clear signal window sizes")
        self.ax4.set_ylabel("Signal window sizes (sec)")
        # Show signal stats in a box
        props = dict(boxstyle='square', edgecolor="black", linewidth=0.5, facecolor='lightyellow') # these are matplotlib.patch.Patch properties
        self.signal_stats_string = " Average signal window = {:.2f} sec \n Maximum signal window = {:.2f} sec \n Clear signal window ratio = {:.2f} "
        self.signal_stats_plot = self.ax4.text(0, 1.5, self.signal_stats_string.format(0, 0, 0), bbox=props, ha="left", va="top")

    def __create_equidistant_points(self, p1, p2):
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
    
    def __animate(self, time_index):
        time = time_index * self.time_step_size/1000 # sec
        wave_elevations = []
        waveform = []
        # Update asv dynamics
        self.py_asv1.py_wg_compute_dynamics(self.asv1["rudder_angle"], self.time_step_size)
        self.py_asv2.py_wg_compute_dynamics(self.asv2["rudder_angle"], self.time_step_size)
        asv1_position = self.py_asv1.py_get_position_origin()
        asv2_position = self.py_asv2.py_get_position_origin()
        asv1_attitude = self.py_asv1.py_get_attitude()
        asv2_attitude = self.py_asv2.py_get_attitude()    
        p1 = Point(asv1_position.x, asv1_position.y)
        p2 = Point(asv2_position.x, asv2_position.y)
        points, offsets = self.__create_equidistant_points(p1, p2)  
        # Update the sea surface view
        for i in range(len(points)):
            p = py_Coordinates_3D(points[i].x, points[i].y)
            elevation = self.sea_surface.py_get_elevation(p, time)
            wave_elevations.append(elevation)
            waveform.append((offsets[i], elevation))
        self.wave_plot.set_data(offsets, wave_elevations)  
        # Update the ASV-1 path view
        self.asv1_path.append({"x":asv1_position.x, "y":asv1_position.y})
        self.asv1_path_plot.set_data([p["x"] for p in self.asv1_path], [p["y"] for p in self.asv1_path])
        self.ax2.set_xlim(min([p["x"] for p in self.asv1_path]) - utils.asv_spec.L_wl, max([p["x"] for p in self.asv1_path]) + utils.asv_spec.L_wl)
        self.ax2.set_ylim(min([p["y"] for p in self.asv1_path]) - utils.asv_spec.L_wl, max([p["y"] for p in self.asv1_path]) + utils.asv_spec.L_wl)
        # Update the ASV-2 path view
        self.asv2_path.append({"x":asv2_position.x, "y":asv2_position.y})
        self.asv2_path_plot.set_data([p["x"] for p in self.asv2_path], [p["y"] for p in self.asv2_path])
        self.ax3.set_xlim(min([p["x"] for p in self.asv2_path]) - utils.asv_spec.L_wl, max([p["x"] for p in self.asv2_path]) + utils.asv_spec.L_wl)
        self.ax3.set_ylim(min([p["y"] for p in self.asv2_path]) - utils.asv_spec.L_wl, max([p["y"] for p in self.asv2_path]) + utils.asv_spec.L_wl)
        # Update arrows for showing the instantaneous position of the ASVs 
        asv1_heading = asv1_attitude.z
        asv1_delta_x = self.arrow_length * math.sin(asv1_heading)
        asv1_delta_y = self.arrow_length * math.cos(asv1_heading)
        self.arrow1_plot.set_data(x=asv1_position.x, y=asv1_position.y, dx=asv1_delta_x, dy=asv1_delta_y)
        asv2_heading = asv2_attitude.z
        asv2_delta_x = self.arrow_length * math.sin(asv2_heading)
        asv2_delta_y = self.arrow_length * math.cos(asv2_heading)
        self.arrow2_plot.set_data(x=asv2_position.x, y=asv2_position.y, dx=asv2_delta_x, dy=asv2_delta_y)
        # Compute the antenna heights considering roll and pitch of the vehicles
        antenna1_z = wave_elevations[0]  + self.asv1["antenna_height"] * math.cos(asv1_attitude.x)*math.cos(asv1_attitude.y)
        antenna2_z = wave_elevations[-1] + self.asv2["antenna_height"] * math.cos(asv2_attitude.x)*math.cos(asv2_attitude.y)
        # Update the antennas in the line of sight view
        self.antenna1_plot.set_data([offsets[0],  offsets[0]],  [wave_elevations[0],  antenna1_z]) 
        self.antenna2_plot.set_data([offsets[-1], offsets[-1]], [wave_elevations[-1], antenna2_z]) 
        # Update signal path in the line of sight view
        signal_path = LineString([(offsets[0], antenna1_z), (offsets[-1], antenna2_z)])
        waveform = LineString(waveform) # Convert to a line string
        is_signal_path_clear = True if signal_path.intersection(waveform).is_empty else False 
        self.los_plot.set_data([offsets[0], offsets[-1]], [antenna1_z, antenna2_z]) 
        self.los_plot.set_color("green" if is_signal_path_clear else "red") 
        # Update the scatter plot of clear signal window sizes
        if is_signal_path_clear:
            self.current_signal_window_length += self.time_step_size/1000
        else:
            if self.current_signal_window_length != 0.0:
                self.max_signal_window_length = self.current_signal_window_length if self.current_signal_window_length > self.max_signal_window_length else self.max_signal_window_length
                total_signal_length = self.avg_signal_window_length * self.count_signal_windows + self.current_signal_window_length
                self.count_signal_windows += 1
                self.avg_signal_window_length = total_signal_length / self.count_signal_windows
                self.signal_window_sizes.append(self.current_signal_window_length)
                self.current_signal_window_length = 0.0
            else:
                # Signal window not yet open
                pass
        clear_signal_window_ratio  = ((self.avg_signal_window_length * self.count_signal_windows) + self.current_signal_window_length) / time if time != 0 else 0
        self.signal_stats_plot.set_text(self.signal_stats_string.format(self.avg_signal_window_length, self.max_signal_window_length, clear_signal_window_ratio))
        self.scatter_plot.set_data(list(range(len(self.signal_window_sizes))), self.signal_window_sizes)
        if len(self.signal_window_sizes) > 0:
            self.ax4.set_xlim(-0.5, len(self.signal_window_sizes))
            self.ax4.set_ylim(-0.5, math.ceil(max(self.signal_window_sizes)+2))
            self.signal_stats_plot.set_y(max(self.signal_window_sizes)+2)
        # Update the text markers in the line of sight view
        self.text_asv1.set_x(0)
        self.text_asv2.set_x(offsets[-1])
        self.ax1.set_xlim(-100, offsets[-1]+100)
        return self.wave_plot, self.antenna1_plot, self.antenna2_plot, self.los_plot, self.asv1_path_plot, self.asv2_path_plot, self.scatter_plot
    
    def run(self):
        self.current_signal_window_length = 0
        self.max_signal_window_length     = 0
        self.avg_signal_window_length     = 0
        self.count_signal_windows         = 0
        # Start simulation and animation
        ani = animation.FuncAnimation(self.fig, self.__animate, interval=self.time_step_size, blit=False, save_count=50)
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


if __name__ == '__main__': 
    sea = {"wave_hs":1.2, # m
        "wave_dp":math.pi/3} # rad
    asv1 = {"position": py_Coordinates_3D(103.3055419921875, 101.90850830078125), # (m, m)
            "attitude": py_Coordinates_3D(0.0, 0.0, 0.0), # (rad, rad, rad)
            "is_surge_sway_halt_set": False, 
            "antenna_height": 1.0, # m
            "rudder_angle": math.pi/4} # rad
    asv2 = {"position": py_Coordinates_3D(3018.09765625, 3010.44873046875), # (m, m)
            "attitude":py_Coordinates_3D(0.0, 0.0, 0.0), # (rad, rad, rad)
            "is_surge_sway_halt_set": False,
            "antenna_height": 1.0, # m
            "rudder_angle": math.pi/4} # rad
    simulation = Simulate_LOS_communication(sea, asv1, asv2)
    simulation.run()