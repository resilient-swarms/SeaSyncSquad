#ifndef NETCDF_RAIN_H
#define NETCDF_RAIN_H

#include <filesystem>
#include <vector>
#include <cstdint>

class NetCDF_rain {
public:
    NetCDF_rain(){}
    NetCDF_rain(std::filesystem::path file_path);

    /**
     * Get total precipitaion (m) for a given location and time.
     */
    double get_tp_at(double longitude, double latitude, time_t time);

private:
    std::vector<int64_t> data_time;
    std::vector<double> data_latitude;
    std::vector<double> data_longitude;
    std::vector<float> data_tp;
};



#endif //NETCDF_RAIN_H
