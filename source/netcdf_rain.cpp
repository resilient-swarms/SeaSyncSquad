#include "netcdf_rain.h"
#include "utils.h"
#include "netcdf"
#include <utility>
#include <algorithm>
#include <exception>
#include <iostream>

NetCDF_rain::NetCDF_rain(std::filesystem::path file_path) {
    std::cout << "Initialising rainfall data from file " << file_path << " ..." << std::endl;
    netCDF::NcFile nc_file(file_path, netCDF::NcFile::read);

    // Get valid time
    std::string var_name = "valid_time";
    std::cout << "... collecting "<< var_name << " data."  << std::endl;
    netCDF::NcVar ncvar_time = nc_file.getVar(var_name);
    if (ncvar_time.isNull()) {
        throw std::runtime_error("Could not find " + var_name + "in file" + file_path.string());
    }
    // Get the variable dimensions
    auto dims_time = ncvar_time.getDims();
    std::vector<std::pair<std::string, size_t>> dim_sizes;
    for (const auto &dim : dims_time) {
        dim_sizes.push_back(std::make_pair(dim.getName(), dim.getSize()));
    }
    // Total data size
    size_t total_size = 1;
    for (auto dim_size : dim_sizes) {
        total_size *= dim_size.second;
    }
    // Get data
    // In netcdf file, time is stored as seconds since 1970-01-01.
    // std::time_t also stores time as seconds since 1970-01-01.
    data_time.resize(total_size);
    ncvar_time.getVar(data_time.data());

    // Get latitudes
    var_name = "latitude";
    std::cout << "... collecting "<< var_name << " data."  << std::endl;
    netCDF::NcVar ncvar_latitude = nc_file.getVar(var_name);
    if (ncvar_latitude.isNull()) {
        throw std::runtime_error("Could not find " + var_name + "in file" + file_path.string());
    }
    // Get the variable dimensions
    auto dims_latitude = ncvar_latitude.getDims();
    dim_sizes.clear();
    for (const auto &dim : dims_latitude) {
        dim_sizes.push_back(std::make_pair(dim.getName(), dim.getSize()));
    }
    // Total data size
    total_size = 1;
    for (auto dim_size : dim_sizes) {
        total_size *= dim_size.second;
    }
    // Get data
    data_latitude.resize(total_size);
    ncvar_latitude.getVar(data_latitude.data());

    // Get longitude
    var_name = "longitude";
    std::cout << "... collecting "<< var_name << " data."  << std::endl;
    netCDF::NcVar ncvar_longitude = nc_file.getVar(var_name);
    if (ncvar_longitude.isNull()) {
        throw std::runtime_error("Could not find " + var_name + "in file" + file_path.string());
    }
    // Get the variable dimensions
    auto dims_longitude = ncvar_longitude.getDims();
    dim_sizes.clear();
    for (const auto &dim : dims_longitude) {
        dim_sizes.push_back(std::make_pair(dim.getName(), dim.getSize()));
    }
    // Total data size
    total_size = 1;
    for (auto dim_size : dim_sizes) {
        total_size *= dim_size.second;
    }
    // Get data
    data_longitude.resize(total_size);
    ncvar_longitude.getVar(data_longitude.data());

    // Get tp
    var_name = "tp";
    std::cout << "... collecting "<< var_name << " data."  << std::endl;
    netCDF::NcVar ncvar_tp = nc_file.getVar(var_name);
    if (ncvar_tp.isNull()) {
        throw std::runtime_error("Could not find " + var_name + "in file" + file_path.string());
    }
    // Get the variable dimensions
    auto dims_tp = ncvar_tp.getDims();
    dim_sizes.clear();
    for (const auto &dim : dims_tp) {
        dim_sizes.push_back(std::make_pair(dim.getName(), dim.getSize()));
    }
    // Total data size
    total_size = 1;
    for (auto dim_size : dim_sizes) {
        total_size *= dim_size.second;
    }
    // Get data
    data_tp.resize(total_size);
    ncvar_tp.getVar(data_tp.data());

    std::cout << "... rainfall data initialisation complete." << std::endl;
}


double NetCDF_rain::get_tp_at(double longitude, double latitude, time_t time) {
    // Check if longitude within the available data range
    size_t longitude_size = data_longitude.size();
    double longitude_min = *std::min_element(data_longitude.begin(), data_longitude.end());
    double longitude_max = *std::max_element(data_longitude.begin(), data_longitude.end());
    if (!(longitude >= longitude_min && longitude <= longitude_max)) {
        throw std::invalid_argument("Invalid longitude; value out of range [" + std::to_string(longitude_min) + ", " + std::to_string(longitude_max) + "]");
    }
    // Check if latitude within the available data range
    size_t latitude_size = data_latitude.size();
    double latitude_min = *std::min_element(data_latitude.begin(), data_latitude.end());
    double latitude_max = *std::max_element(data_latitude.begin(), data_latitude.end());
    if (!(latitude >= latitude_min && latitude <= latitude_max)) {
        throw std::invalid_argument("Invalid latitude; value out of range [" + std::to_string(latitude_min) + ", " + std::to_string(latitude_max) + "]");
    }
    // Check if time within the available data range
    size_t time_size = data_time.size();
    int64_t time_min = *std::min_element(data_time.begin(), data_time.end());
    int64_t time_max = *std::max_element(data_time.begin(), data_time.end());
    if (!(time >= time_min && time <= time_max)) {
        throw std::invalid_argument("Invalid time; value out of range [" + std::to_string(time_min) + ", " + std::to_string(time_max) + "]");
    }

    // NetCDF files have longitude in the range [-180.0, 179.5]. Therefore, if the given longitude > 179.5, then round to nearest 0.5
    if (longitude >= 179.75 && longitude <= 180.0) {
        longitude = -180.0;
    }
    else if (longitude > 179.5 && longitude < 179.75) {
        longitude = 179.5;
    }

    // Compute index
    int64_t delta_time = data_time[1] - data_time[0];
    double delta_longitude = data_longitude[1] - data_longitude[0];
    double delta_latitude = data_latitude[1] - data_latitude[0];
    size_t index_time = std::round((time - data_time[0]) / delta_time);
    size_t index_latitude = std::round((latitude - data_latitude[0]) / delta_latitude);
    size_t index_longitude = std::round((longitude - data_longitude[0]) / delta_longitude);
    size_t index = index_time * (latitude_size * longitude_size)
                 + index_latitude * longitude_size
                 + index_longitude;

    // Get rain data
    float tp = data_tp[index];
    return tp;
}