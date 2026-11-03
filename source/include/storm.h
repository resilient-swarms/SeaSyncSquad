#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <ctime>
#include "utils.h"

class Storm {
public:
    Storm(std::filesystem::path storms_database,
        int storm_year,
        std::string storm_name,
        bool init_ocean_current=false);

private:
    int storm_year;
    std::string storm_name;
    std::vector<std::time_t> timestamps;
    std::vector<Geo_coordinate> storm_path;
    std::vector<std::vector<Geo_coordinate>> predicted_paths;
    bool is_storm_initialised = false;
};

