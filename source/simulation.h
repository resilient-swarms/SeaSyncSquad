//
// Created by toby on 29/11/24.
//

#ifndef SIMULATION_H
#define SIMULATION_H

extern "C" {
#include "geometry.h"
#include "sea_surface.h"
#include "asv.h"
}
#include "utils.h"
#include "storm.h"
#include "netcdf_wave.h"
#include "netcdf_wind.h"
#include "netcdf_rain.h"
#include <vector>
#include <array>
#include <netcdf>

class Cell {
public:
    std::array<double, 2> bounding_longitudes;
    std::array<double, 2> bounding_latitudes;
    Geo_coordinate cell_centroid;
    Sea_surface* sea;
};

class Sea_surface_point {
public:
    Coordinates_3D cartesian_coordinates;
    Geo_coordinate geo_coordinate;
    std::reference_wrapper<Cell> cell;
};

class Node {
public:
    Coordinates_3D position;
    Cell& cell;
    Asv* asv;
    double antenna_height;  // m
    double radio_frequency; // GHz
};


class Edge {
public:
    std::array<std::reference_wrapper<Node>, 2> nodes;
    std::vector<Sea_surface_point> sea_surface_points;
};


class Simulation {
public:
    explicit Simulation(
        const Storm& storm,
        const Geo_coordinate& deployment_region_centroid,
        time_t start_time,
        time_t end_time,
        const std::filesystem::path& nc_dir_waves,
        const std::filesystem::path& nc_dir_winds,
        const std::filesystem::path& nc_dir_rains);

    ~Simulation();

private:
    void plot();

    void set_environment(
        const std::filesystem::path& nc_dir_waves,
        const std::filesystem::path& nc_dir_winds,
        const std::filesystem::path& nc_dir_rains);
    void set_deployment_locations(const Geo_coordinate &deployment_region_centroid);
    void set_asv_formation();
    void set_spatial_grid();
    Cell& get_cell(Coordinates_3D point);

    Geo_coordinate_converter geo_coordinate_converter;
    double asv_spacing = 200'000.0; // m
    double time_step_size;  // milli-sec
    const Storm& storm;
    time_t start_time;
    time_t end_time;
    std::array<std::pair<std::string, NetCDF_wave>, 2>  wave_data;
    std::array<std::pair<std::string, NetCDF_wind>, 2>  wind_data;
    std::array<std::pair<std::string, NetCDF_rain>, 2>  rain_data;
    std::vector<std::vector<Coordinates_3D>> deployment_locations;
    std::vector<std::vector<Node>> asv_formation;
    std::vector<Cell> cells;
};

#endif //SIMULATION_H
