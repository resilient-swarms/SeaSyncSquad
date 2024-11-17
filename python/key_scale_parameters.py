import sys
import pathlib
import netCDF4 as nc
import numpy as np
import pandas as pd
from datetime import datetime, timedelta

module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
storm_explorers_dir = root_dir.joinpath("dependency", "StormExplorers", "source")
sys.path.insert(0, str(storm_explorers_dir))

# Get max rainfalls
dir_data = root_dir.joinpath(*("dependency/StormExplorers/data/cds/north_atlantic/precipitation".split("/"))).resolve()
max_rains = []
for file_name in dir_data.iterdir():
    file_path = dir_data.joinpath(file_name)
    if str(file_path).endswith(".nc"):
        dataset = nc.Dataset(file_path, 'r')
        tp = dataset.variables['tp'][:].data
        tp_max = np.max(tp) * 1000.0
        max_rains.append(tp_max)

# Get max wind speeds
dir_data = root_dir.joinpath(*("dependency/StormExplorers/data/cds/north_atlantic/winds".split("/"))).resolve()
max_winds = []
for file_name in dir_data.iterdir():
    file_path = dir_data.joinpath(file_name)
    if str(file_path).endswith(".nc"):
        dataset = nc.Dataset(file_path, 'r')
        u10 = dataset.variables['u10'][:].data
        v10 = dataset.variables['v10'][:].data
        wind = np.sqrt(np.square(u10) + np.square(v10))
        max_wind = np.max(wind) * 3.6 # km/hr
        max_winds.append(max_wind)

# Get max waves
dir_data = root_dir.joinpath(*("dependency/StormExplorers/data/cds/north_atlantic/waves".split("/"))).resolve()
max_waves = []
for file_name in dir_data.iterdir():
    file_path = dir_data.joinpath(file_name)
    if str(file_path).endswith(".nc"):
        dataset = nc.Dataset(file_path, 'r')
        hs = dataset.variables['swh'][:].data
        max_hs = np.max(np.nan_to_num(hs)) # m
        max_waves.append(max_hs)


df = pd.DataFrame(columns=["max_rainfall(mm)", "max_wind(km/hr)", "max_wave_ht(m)"])
df["max_rainfall(mm)"] = max_rains
df["max_wind(km/hr)"] = max_winds
df["max_wave_ht(m)"] = max_waves
# Write result to file
result_dr = root_dir.joinpath("results")
df.to_csv(result_dr.joinpath("max_values.csv"), index=False)






from shapely.geometry import Polygon, LineString, Point
from storms_archive import Storms_archieve
import epsg

storms = pd.DataFrame()
storms_archieve = Storms_archieve("../dependency/StormExplorers/data/storms")
df_storms = storms_archieve.get_storms()
storm_names = []
mean_storm_speeds = []
max_storm_speeds = []
for i in range(len(df_storms)):
    name = df_storms.loc[i, "storm_name"]
    path = df_storms.loc[i, "path(long,lat)"]
    path = [epsg.GCS_to_PCS(long, lat) for (long, lat) in path]
    time = df_storms.loc[i, "time_stamps"]
    if len(path) == len(time):
        storm_speeds = []
        for i in range(1, len(time)):
            x1 = path[i-1][0]
            y1 = path[i-1][1]
            x2 = path[i][0]
            y2 = path[i][1]
            d = ((x2-x1)**2 + (y2-y1)**2)**0.5
            dt = (time[i]-time[i-1]).total_seconds()
            if dt == 0:
                print("oh no...! ")
                continue
            speed = d/dt # m/s
            storm_speeds.append(speed)
        storm_names.append(name)
        mean_storm_speeds.append(np.mean(storm_speeds).item() * 3.6) # km/hr
        max_storm_speeds.append(np.max(storm_speeds).item() * 3.6) # km/hr
df = pd.DataFrame(columns=["storm_name", "mean_storm_speed(km/hr)", "max_storm_speed(km/hr)"])
df["storm_name"] = storm_names
df["mean_storm_speed(km/hr)"] = mean_storm_speeds
df["max_storm_speed(km/hr)"] = max_storm_speeds
# Write result to file
result_dr = root_dir.joinpath("results")
df.to_csv(result_dr.joinpath("storm_speeds.csv"), index=False)