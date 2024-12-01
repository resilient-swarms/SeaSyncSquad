#ifndef NETCDF_WAVE_H
#define NETCDF_WAVE_H

#include <filesystem>
#include <vector>
#include <cstdint>

class NetCDF_wave {
public:
    NetCDF_wave(){}
    NetCDF_wave(std::filesystem::path file_path);

    /**
     * Get significant wave height (m) and wave heading (deg) for a given location and time.
     */
    std::pair<double, double> get_swh_mwd_at(double longitude, double latitude, time_t time);

private:
    std::vector<int64_t> data_time;
    std::vector<double> data_latitude;
    std::vector<double> data_longitude;
    std::vector<float> data_swh;
    std::vector<float> data_mwd;
};



#endif //NETCDF_WAVE_H
