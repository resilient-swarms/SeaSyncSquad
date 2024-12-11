#include "storm.h"
#include "simulation.h"
#include <sstream>

int main() {
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path results_dir = root_dir/"results_deployment";
    if (!std::filesystem::exists(results_dir)) {
        std::filesystem::create_directory(results_dir);
    }
    std::filesystem::path storms_file = root_dir/"dependency"/"StormExplorers"/"data"/"storms";
    std::filesystem::path nc_dir_waves = root_dir/"dependency"/"StormExplorers"/"data"/"cds"/"north_atlantic"/"waves";
    std::filesystem::path nc_dir_winds = root_dir/"dependency"/"StormExplorers"/"data"/"cds"/"north_atlantic"/"winds";
    std::filesystem::path nc_dir_rains = root_dir/"dependency"/"StormExplorers"/"data"/"cds"/"north_atlantic"/"precipitation";

    // Initialise the storm
    Storm storm(storms_file, 2011, "hurricane_rina");

    // Deployment coordinates
    Geo_coordinate deployment_region_centroid {.longitude = -83.5, .latitude = 19.0};

    // Initialise simulation
    time_t start_time = string_to_time("2011,10,23,21,0");
    time_t end_time   = string_to_time("2011,10,28,21,0");

    Simulation simulation(
        storm,
        deployment_region_centroid,
        start_time,
        end_time,
        nc_dir_waves,
        nc_dir_winds,
        nc_dir_rains);
}