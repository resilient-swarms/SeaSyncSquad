#ifndef SWARM_H
#define SWARM_H
#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <filesystem>

extern "C" {
#include "sea_surface.h"
#include "asv.h"
}

struct VertexProperties {
    Asv* asv;
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
    Swarm(std::vector<Asv*> asvs, Sea_surface* sea_surface, double distance, double antenna_height, double radio_frequency, double rainfall_rate, double wind_speed, std::filesystem::path results_dir, std::string scenario_name);
    void set_comms_link(std::pair<Asv*, Asv*> comms_link);
    void run_simulation();
    std::vector<Asv*> get_asvs();

private:
    double simulation_time = 15.0 * 60.0; // seconds
    double simulation_time_step_size = 40.0; // milli-sec
    double radio_attenuation_threshold; // dB
    double distance; // m
    double antenna_height; // m
    double radio_frequency; // Hz
    double rainfall_rate; // mm/hr
    double wind_speed; // m/s
    int swarm_size;
    Sea_surface* sea_surface;
    Graph graph;
    std::string scenario_name;
    std::filesystem::path results_dir;
};



#endif //SWARM_H
