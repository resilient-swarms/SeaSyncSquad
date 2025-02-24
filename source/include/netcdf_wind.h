#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>


class NetCDF_wind {
public:
    NetCDF_wind(){}
    NetCDF_wind(std::filesystem::path file_path);

    /**
     * Get  zonal and meridional wind velocity for a given location and time.
     */
    std::pair<double, double> get_u_v_at(double longitude, double latitude, time_t time);

private:
    std::vector<int64_t> data_time;
    std::vector<double> data_latitude;
    std::vector<double> data_longitude;
    std::vector<float> data_u10;
    std::vector<float> data_v10;
};


