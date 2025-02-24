#pragma once 

#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <filesystem>
#include "ASVLite/sea_surface.h"
#include "ASVLite/asv.h"

struct VertexProperties {
    ASVLite::Geometry::Coordinates3D position;
    double radio_attenuation_threshold; // dB
};

struct EdgeProperties {
    double distance; // m
    double diffraction_loss; // dB
    double free_space_path_loss; // dB
    double rain_attenuation; // dB
    double sea_spray_attenuation; // dB
    double total_attenuation; // dB
    double fresnel_clearance; // %
};

// Define the graph with properties using Boost's adjacency_list
using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, VertexProperties, EdgeProperties>;

class Swarm {
public:
    Swarm(const std::vector<ASVLite::Geometry::Coordinates3D>& positions, 
          const ASVLite::SeaSurface& sea_surface, 
          const double antenna_height, 
          const double radio_frequency, 
          const double rainfall_rate, 
          const double wind_speed, 
          const double fspl_design, 
          const double environmental_excess_loss);

    void set_comms_link(const std::pair<ASVLite::Geometry::Coordinates3D, ASVLite::Geometry::Coordinates3D> antenna_pair);
    double run_simulation(const std::filesystem::path& results_dir, const std::string& scenario_name);
    double run_simulation();

private:
    const double antenna_height; // m
    const double radio_frequency; // Hz
    const double rainfall_rate; // mm/hr
    const double wind_speed; // m/s
    const ASVLite::SeaSurface& sea_surface;
    const double fspl_design; // Design free space path loss (dB)
    const double environmental_excess_loss; // Excess radio loss due to environment (dB)
    const size_t swarm_size;
    const double simulation_time = 2.0 * 60.0; // seconds
    const double simulation_time_step_size = 40.0; // milli-sec
    double radio_attenuation_threshold; // dB
    double free_space_radio_range; // m
    Graph graph;
    bool print_to_dir;
};

