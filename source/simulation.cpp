extern "C" {
#include "constants.h"
}
#include "simulation.h"
#include "matplotlibcpp.h"
#include <cmath>
#include <format>
#include <float.h>
#include <iostream>

namespace plt = matplotlibcpp;

Simulation::Simulation(
    const Storm &storm,
    const Geo_coordinate &deployment_region_centroid,
    time_t start_time,
    time_t end_time,
    const std::filesystem::path& nc_dir_waves,
    const std::filesystem::path& nc_dir_winds,
    const std::filesystem::path& nc_dir_rains):
storm(storm),
start_time(start_time),
end_time(end_time) {
    geo_coordinate_converter = Geo_coordinate_converter(deployment_region_centroid);
    set_environment(nc_dir_waves, nc_dir_winds, nc_dir_rains);
    set_deployment_locations(deployment_region_centroid);
    set_spatial_grid();
    set_asv_formation();
}


Simulation::~Simulation() {
    for (auto& cell : cells) {
        sea_surface_delete(cell.sea);
    }
    for (auto& row : asv_formation) {
        for (auto& node : row) {
            asv_delete(node.asv);
        }
    }
}


void Simulation::set_environment(const std::filesystem::path& nc_dir_waves, const std::filesystem::path& nc_dir_winds, const std::filesystem::path& nc_dir_rains) {
    // Load environmental data
    // Construct the names of the nc files to load.
    std::tm start_tm = *gmtime(&start_time);
    std::tm end_tm = *gmtime(&end_time);
    int month_string_width = 2;
    std::string nc_file_1 = std::to_string(start_tm.tm_year + 1900) + "_" + std::format("{:0{}}", start_tm.tm_mon + 1, month_string_width) + ".nc";
    std::string nc_file_2 = std::to_string(end_tm.tm_year + 1900) + "_" + std::format("{:0{}}", end_tm.tm_mon + 1, month_string_width) + ".nc";
    // Load data
    wave_data[0] = std::make_pair(nc_file_1, NetCDF_wave(nc_dir_waves/nc_file_1));
    wind_data[0] = std::make_pair(nc_file_1, NetCDF_wind(nc_dir_winds/nc_file_1));
    rain_data[0] = std::make_pair(nc_file_1, NetCDF_rain(nc_dir_rains/nc_file_1));
    if (nc_file_2 != nc_file_1) {
        wave_data[1] = std::make_pair(nc_file_1, NetCDF_wave(nc_dir_waves/nc_file_2));
        wind_data[1] = std::make_pair(nc_file_1, NetCDF_wind(nc_dir_winds/nc_file_2));
        rain_data[1] = std::make_pair(nc_file_1, NetCDF_rain(nc_dir_rains/nc_file_2));
    }
}


void Simulation::set_deployment_locations(const Geo_coordinate& deployment_region_centroid) {
    std::cout << "Initialising coordinates for deployment..." << std::endl;

    // Initialise deployment coordinates
    Coordinates_3D region_centroid = geo_coordinate_converter.convert(deployment_region_centroid);
    // Deployment pattern - hexagonal.
    //  O   O   O       <-- North row
    //    0   0   0     <-- Mid row
    //      0   0   0   <-- South row
    // North row
    std::vector<Coordinates_3D> north_row;
    Coordinates_3D north_row_mid = {region_centroid.keys.x - asv_spacing * std::cos(PI/3),
                                    region_centroid.keys.y + asv_spacing * std::sin(PI/3),
                                    0};
    std::cout << "... at (" << north_row_mid.keys.x << ", " << north_row_mid.keys.y << ")" << std::endl;
    Coordinates_3D north_row_west = {north_row_mid.keys.x - asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    std::cout << "... at (" << north_row_west.keys.x << ", " << north_row_west.keys.y << ")" << std::endl;
    Coordinates_3D north_row_east = {north_row_mid.keys.x + asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    std::cout << "... at (" << north_row_east.keys.x << ", " << north_row_east.keys.y << ")" << std::endl;
    north_row.push_back(north_row_west);
    north_row.push_back(north_row_mid);
    north_row.push_back(north_row_east);
    // Mid row
    std::vector<Coordinates_3D> mid_row;
    Coordinates_3D mid_row_mid = region_centroid;
    std::cout << "... at (" << mid_row_mid.keys.x << ", " << mid_row_mid.keys.y << ")" << std::endl;
    Coordinates_3D mid_row_west = {mid_row_mid.keys.x - asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    std::cout << "... at (" << mid_row_west.keys.x << ", " << mid_row_west.keys.y << ")" << std::endl;
    Coordinates_3D mid_row_east = {mid_row_mid.keys.x + asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    std::cout << "... at (" << mid_row_east.keys.x << ", " << mid_row_east.keys.y << ")" << std::endl;
    mid_row.push_back(mid_row_west);
    mid_row.push_back(mid_row_mid);
    mid_row.push_back(mid_row_east);
    // South row
    std::vector<Coordinates_3D> south_row;
    Coordinates_3D south_row_mid = {region_centroid.keys.x + asv_spacing * std::cos(PI/3),
                                    region_centroid.keys.y - asv_spacing * std::sin(PI/3),
                                    0};
    std::cout << "... at (" << south_row_mid.keys.x << ", " << south_row_mid.keys.y << ")" << std::endl;
    Coordinates_3D south_row_west = {south_row_mid.keys.x - asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    std::cout << "... at (" << south_row_west.keys.x << ", " << south_row_west.keys.y << ")" << std::endl;
    Coordinates_3D south_row_east = {south_row_mid.keys.x + asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    std::cout << "... at (" << south_row_east.keys.x << ", " << south_row_east.keys.y << ")" << std::endl;
    south_row.push_back(south_row_west);
    south_row.push_back(south_row_mid);
    south_row.push_back(south_row_east);

    deployment_locations.push_back(north_row);
    deployment_locations.push_back(mid_row);
    deployment_locations.push_back(south_row);

    std::cout << "... deployment coordinates initialised." << std::endl;
}


void Simulation::set_spatial_grid() {
    std::cout << "Initialising spatial grid..." << std::endl;

    // The cells will maintain the same spatial resolution as specified in the NetCDF files.
    double max_x = 0.0;
    double max_y = 0.0;
    double min_x = DBL_MAX;
    double min_y = DBL_MAX;
    for (auto& row : deployment_locations) {
        for (auto& location : row) {
            max_x = (location.keys.x > max_x) ? location.keys.x : max_x;
            max_y = (location.keys.y > max_y) ? location.keys.y : max_y;
            min_x = (location.keys.x < min_x) ? location.keys.x : min_x;
            min_y = (location.keys.y < min_y) ? location.keys.y : min_y;
        }
    }
    Geo_coordinate deployment_max_corner = geo_coordinate_converter.convert(Coordinates_3D{max_x, max_y, 0.0});
    deployment_max_corner.latitude = std::ceil(deployment_max_corner.latitude);
    deployment_max_corner.longitude = std::ceil(deployment_max_corner.longitude);
    Geo_coordinate deployment_min_corner = geo_coordinate_converter.convert(Coordinates_3D{min_x, min_y, 0.0});
    deployment_min_corner.latitude = std::floor(deployment_min_corner.latitude);
    deployment_min_corner.longitude = std::floor(deployment_min_corner.longitude);
    // Create the grid of latitudes and longitudes
    std::vector<double> grid_longitudes;
    std::vector<double> grid_latitudes;
    for (double lon = deployment_min_corner.longitude; lon <= deployment_max_corner.longitude; lon += 0.5) {
        grid_longitudes.push_back(lon);
    }
    for (double lat = deployment_min_corner.latitude; lat <= deployment_max_corner.latitude; lat += 0.5) {
        grid_latitudes.push_back(lat);
    }
    // Construct each cell
    // Iterate over adjacent latitude and longitude values to create cells
    for (size_t i = 0; i < grid_longitudes.size() - 1; ++i) {
        for (size_t j = 0; j < grid_latitudes.size() - 1; ++j) {
            // Form a cell using two adjacent latitudes and two adjacent longitudes
            std::array<double, 2> cell_longitudes { grid_longitudes[i], grid_longitudes[i+1] };
            std::array<double, 2> cell_latitudes { grid_latitudes[j], grid_latitudes[j+1] };
            Geo_coordinate cell_centroid {
                .longitude = (cell_longitudes[0] + cell_longitudes[1])/2.0,
                .latitude = (cell_latitudes[0] + cell_latitudes[1])/2.0
            };
            int count_component_waves = 15;
            int wave_rand_seed = 1;
            std::pair<double, double> swh_mwd = wave_data.at(0).second.get_swh_mwd_at(cell_centroid.longitude, cell_centroid.latitude, start_time);
            Sea_surface* sea = sea_surface_new(swh_mwd.first, swh_mwd.second, wave_rand_seed, count_component_waves);
            cells.emplace_back(Cell{cell_longitudes, cell_latitudes, cell_centroid, sea});
            Coordinates_3D cell_centroid_cartesian = geo_coordinate_converter.convert(cell_centroid);
            std::cout << "... with centroid at (" << cell_centroid_cartesian.keys.x << ", " << cell_centroid_cartesian.keys.y << ")" << std::endl;
        }
    }
    std::cout << "... spatial grid initialised." << std::endl;
}


void Simulation::set_asv_formation() {
    std::cout << "Initialising nodes..." << std::endl;

    // Set ASV spec
    struct Asv_specification asv_spec;
    asv_spec.L_wl = 2.1; // m
    asv_spec.B_wl = 0.6; // m
    asv_spec.D = 0.25; // m
    asv_spec.T = 0.09; // m
    asv_spec.max_speed = 4.0; // m/s
    asv_spec.disp = 0.09; // m3
    asv_spec.r_roll = 0.2; // m
    asv_spec.r_pitch = 0.6; // m
    asv_spec.r_yaw = 0.06; // m
    asv_spec.cog = {1.05, 0.0, -0.2}; // {m, m, m}
    bool is_surge_sway_halt_set = false;
    union Coordinates_3D asv_attitude {0.0, 0.0, 0.0}; // {rad, rad, rad}
    double antenna_height = 2.0; // m
    double radio_frequency = 2.4; // GHz
    // Initialise the nodes
    for (auto& row : deployment_locations) {
        std::vector<Node> nodes;
        for (Coordinates_3D& point : row) {
            Cell& cell = get_cell(point);
            Sea_surface* sea = cell.sea;
            Asv* asv = asv_new(asv_spec, sea, point, asv_attitude);
            asv_set_surge_sway_halt(asv, is_surge_sway_halt_set);
            Node node {
                point,
                cell,
                asv,
                antenna_height,
                radio_frequency
            };
            nodes.push_back(node);
            std::cout << "... at (" << point.keys.x  << ", " << point.keys.y << ")" << std::endl;
        }
        asv_formation.push_back(nodes);
    }

    std::cout << "... nodes initialisation complete." << std::endl;
}



Cell& Simulation::get_cell(Coordinates_3D point) {
    Geo_coordinate p = geo_coordinate_converter.convert(point);
    // Compute the centroid of the cell we are looking for
    double longitude_east = std::ceil(p.longitude * 2.0)/2.0;
    double longitude_west = std::floor(p.longitude * 2.0)/2.0;
    double latitude_north = std::ceil(p.latitude * 2.0)/2.0;
    double latitude_south = std::floor(p.latitude * 2.0)/2.0;
    Geo_coordinate cell_centroid {
        .longitude = (longitude_east + longitude_west)/2.0,
        .latitude = (latitude_north + latitude_south)/2.0,
    };
    int cell_index = -1;
    for (int i = 0; i <= cells.size(); ++i) {
        if (cells[i].cell_centroid.longitude == cell_centroid.longitude &&
            cells[i].cell_centroid.latitude == cell_centroid.latitude) {
            cell_index = i;
        }
    }
    if (cell_index == -1) {
        throw std::runtime_error("Cell does not exist");
    }
    return cells[cell_index];
}


void Simulation::plot() {

    // // Clear previous plot
    // plt::clf();
    // std::vector<double> x = {north_row_west.keys.x,
    //     north_row_mid.keys.x,
    //     north_row_east.keys.x,
    //     mid_row_west.keys.x,
    //     mid_row_mid.keys.x,
    //     mid_row_east.keys.x,
    //     south_row_west.keys.x,
    //     south_row_mid.keys.x,
    //     south_row_east.keys.x};
    // std::vector<double> y = {north_row_west.keys.y,
    //     north_row_mid.keys.y,
    //     north_row_east.keys.y,
    //     mid_row_west.keys.y,
    //     mid_row_mid.keys.y,
    //     mid_row_east.keys.y,
    //     south_row_west.keys.y,
    //     south_row_mid.keys.y,
    //     south_row_east.keys.y};
    // plt::scatter(x, y);
    // plt::set_aspect_equal();
    // plt::show();


}
