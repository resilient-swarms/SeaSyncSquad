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
    Swarm(std::vector<Asv*> asvs, Sea_surface* sea_surface, double antenna_height, double radio_frequency, double rainfall_rate, double wind_speed, std::filesystem::path comms_result_file_path);
    void set_comms_link(std::pair<Asv*, Asv*> comms_link);
    void run_simulation();

private:
    double simulation_time = 15.0 * 60.0; // seconds
    double simulation_time_step_size = 40.0; // milli-sec
    double antenna_height; // m
    double radio_frequency; // Hz
    double rainfall_rate; // mm/hr
    double wind_speed; // m/s
    Sea_surface* sea_surface;
    Graph graph;
    std::filesystem::path comms_result_file_path;
};



#endif //SWARM_H
