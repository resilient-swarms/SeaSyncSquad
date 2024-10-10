import pathlib
import sys
module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
asvlite_wrapper_dir = root_dir.joinpath("dependency", "ASVLite", "wrapper", "cython")
storm_explorers_dir = root_dir.joinpath("dependency", "StormExplorers", "source")
sys.path.insert(0, str(asvlite_wrapper_dir))
sys.path.insert(1, str(storm_explorers_dir))

import os
import math
import pandas as pd
import numpy as np
import cartopy.crs as ccrs
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import multiprocessing as mp
from datetime import datetime, timedelta
from shapely.geometry import Polygon, LineString, Point
from tqdm import tqdm
from sklearn.neighbors import KNeighborsClassifier

import epsg
from storms_archive import Storms_archieve
from rudder_controller import Rudder_PID_controller
from sea_surface import py_Sea_surface
from asv import py_Asv_specification, py_Asv
from geometry import py_Coordinates_3D, py_normalise_angle_PI, py_normalise_angle_2PI
from thrust_calibrator import Thrust_calibrator
from storm import Storm

_MPS_TO_KNOTS = 1.94384449  # 1 m/s = 1.94384449 knots
_SIMULATION_TIME_STEP_SIZE = 40  # milli-sec

class SeaSyncSquad:
    def __init__(self) -> None:
        # Map boundary
        self.map_boundary_south = 17
        self.map_boundary_north = 31
        self.map_boundary_west = -99
        self.map_boundary_east = -80
        # Deployment region
        self.deployment_boundary_south = 16.5
        self.deployment_boundary_north = 21.5
        self.deployment_boundary_west = -86.0
        self.deployment_boundary_east = -81.0
        x0, y0 = epsg.GCS_to_PCS(self.deployment_boundary_west, self.deployment_boundary_south)
        x1, y1 = epsg.GCS_to_PCS(self.deployment_boundary_east, self.deployment_boundary_north)
        self.deployment_region = Polygon([(x0, y0), (x1, y0), (x1, y1), (x0, y1)])
        # Set the list of storms that pass through the deployment region
        self.storms = None
        self._set_storms_in_region()
        # Download wave, current, wind, and precipitation data for the selected storms.
        for i in range(len(self.storms)):
            init_ocean_currents = True
            storm_name = self.storms.loc[i, "storm_name"]
            storm_year = self.storms.loc[i, "year"]
            # CDS data may not be complete for the current year, especially the ocean current data.
            # Therefore, init storm in years before current year.
            current_year = datetime.now().year
            if storm_year < current_year:
                Storm(storm_year,
                      storm_name,
                      self.map_boundary_north,
                      self.map_boundary_south,
                      self.map_boundary_east,
                      self.map_boundary_west,
                      init_ocean_currents)

    def _set_storms_in_region(self):
        # Download data to the directory
        self.storms = pd.DataFrame()
        storms_file = root_dir.joinpath("dependency", "StormExplorers", "data", "storms")
        storms_archieve = Storms_archieve(str(storms_file))
        df_storms = storms_archieve.get_storms()
        for i in range(len(df_storms)):
            path = df_storms.loc[i, "path(long,lat)"]
            path = [epsg.GCS_to_PCS(long, lat) for (long, lat) in path]
            path = LineString(path)
            # Check for intersection of path with deployment_region
            intersection = self.deployment_region.intersection(path)
            if not intersection.is_empty:
                if self.storms.empty:
                    self.storms = df_storms.loc[i].to_frame().T
                else:
                    self.storms = pd.concat([self.storms, df_storms.loc[i].to_frame().T], ignore_index=True)


if __name__ == '__main__':
    storm_tracking = SeaSyncSquad()
    # storm_tracking.run_all()
    # storm_tracking.compute_performance()