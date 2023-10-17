import math
import numpy as np
from shapely.geometry import Point, LineString
from shapely.ops import nearest_points
from geometry import py_Coordinates_3D
from scipy.signal import find_peaks
from sea_surface import py_Sea_surface
from asv import py_Asv_specification

# Simulation time step size
time_step_size = 40 # milli-sec

# Sea surface variables
wave_rand_seed = 1

# ASV spec
asv_spec = py_Asv_specification()
asv_spec.L_wl       = 2.1 # m
asv_spec.B_wl       = 0.6 # m
asv_spec.D          = 0.25 # m 
asv_spec.T          = 0.09 # m
asv_spec.max_speed  = 4.0 # m/s
asv_spec.disp       = 0.09 # m3
asv_spec.r_roll     = 0.2 # m
asv_spec.r_pitch    = 0.6 # m
asv_spec.r_yaw      = 0.6 # m
asv_spec.cog        = py_Coordinates_3D(1.05, 0.0, -0.2) # m

def simulate(time_duration, sea_surface, asv):
    '''
    Function to simulate the dynamics of an ASV on a given sea_surface for a given time_duration (sec).
    '''
    positions = [] # store asv positions
    attitudes = [] # store asv attitudes
    accelerations = [] # store asv accelerations
    elevations= [] # store sea surface elivations
    # Simulation is performed for the duration defined above with a time step size of 40 milli sec.
    for i in range(int(time_duration * 1000.0 /time_step_size)):  
        time = i * time_step_size/1000.0
        # Record the position, attitude and elevation for ploting
        position = asv.py_get_position_cog() # Get current position of the ASV
        positions.append(position)
        attitude = asv.py_get_attitude() # Get current attitude of the ASV
        attitudes.append(attitude)
        acceleration = asv.py_get_A()
        accelerations.append(acceleration)
        elevation= sea_surface.py_get_elevation(position, time)
        elevations.append(elevation)
        # Compute asv dynamics
        asv.py_compute_dynamics(time_step_size)
    return(positions, attitudes, accelerations, elevations)

def peak_time_diffs(wave_1, wave_2):
    peaks_1, _ = find_peaks(wave_1)
    peaks_2, _ = find_peaks(wave_2)
    min_length = min(len(peaks_1), len(peaks_2))
    peaks_1 = peaks_1[:min_length]
    peaks_2 = peaks_2[:min_length]
    time_diffs = [(peaks_1[i] - peaks_2[i])*time_step_size/1000 for i in range(min_length)]
    return np.array(time_diffs)

def get_new_position(angle, current_position, freq):
    p_r0 = Point(0, 0) # Original reference point
    # Create a reference line
    len = 1000000000000 # length of a very long line
    p_r1 = Point(-len*math.sin(angle), -len*math.cos(angle))
    p_r2 = Point(len*math.sin(angle), len*math.cos(angle))
    ref_line_0 = LineString([p_r1, p_r2]) # Create the very long line
    # Offset the reference line to pass through the given point
    tolerance = 1/1000.0 # tolerance = 1mm
    p_c = Point(current_position.x, current_position.y)
    d = ref_line_0.distance(p_c)
    ref_line = None # new reference line
    p_0 = None # new refernce point
    if d <= tolerance:
        # The current reference is line is already passing through the point
        ref_line = ref_line_0
        p_0 = p_r0
    else:
        ref_line_1 = ref_line_0.offset_curve(d) # Offset to one side
        ref_line_2 = ref_line_0.offset_curve(-d) # Offset to the other side
        # Find which of these ref lines pass through the current position
        if(ref_line_1.distance(p_c) <= tolerance):
            ref_line = ref_line_1
        elif(ref_line_2.distance(p_c) <= tolerance):
            ref_line = ref_line_2
        # Find the new reference point on the new reference line
        p_0 = nearest_points(ref_line, p_r0)[0]
    # Distance of the current point from the new reference point
    len_0 = ((p_0.x - p_c.x)**2 + (p_0.y - p_c.y)**2)**0.5
    # Distance of the new reference point from the start point of the new reference line
    len_1 = p_r1.distance(p_0)
    # Find the nearest new point to move to 
    wave_length = 9.81/(2.0* np.pi * freq**2) # Get the wave length from the frequency
    len_2 = round(len_0/wave_length) * wave_length # Find the lenth on the line that is a multiple of the wave length
    p_new = ref_line.interpolate(len_1+len_2)
    return py_Coordinates_3D(p_new.x, p_new.y)

def fast_fourier_transform(vector):
    n = len(vector)
    fhat = np.fft.fft(vector)
    PSD = fhat * np.conj(fhat) # Power Spectral Density (Power Spectrum)
    freq = np.fft.fftfreq(len(vector), d=time_step_size/1000)
    i_max = np.argmax(PSD) # index corresponding to max power
    L = np.arange(1, np.floor(n/2), dtype="int")
    return(L, fhat, freq, PSD, i_max)