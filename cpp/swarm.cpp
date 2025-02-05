#include "swarm.h"
#include "radio_attenuation.h"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/stoer_wagner_min_cut.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/copy.hpp>
extern "C" {
#include "constants.h"
}


struct EdgeFilter {
    const Graph* g = nullptr;  // Initialize to nullptr
    EdgeFilter() = default;  // Default constructor
    EdgeFilter(const Graph& graph) : g(&graph) {}

    bool operator()(const Graph::edge_descriptor& e) const {
        auto v1 = boost::source(e, *g);
        auto v2 = boost::target(e, *g);
        double radio_attenuation_threshold = std::min((*g)[v1].radio_attenuation_threshold, (*g)[v2].radio_attenuation_threshold) + 20.0;
        return radio_attenuation_threshold >= (*g)[e].total_attenuation;
    }
};


Swarm::Swarm(std::vector<Asv*> asvs, Sea_surface* sea_surface, double distance, double antenna_height, double radio_frequency, double rainfall_rate, double wind_speed, std::filesystem::path results_dir, std::string scenario_name) {
    this->sea_surface = sea_surface;
    this->distance = distance;
    this->antenna_height = antenna_height;
    this->radio_frequency = radio_frequency;
    this->rainfall_rate = rainfall_rate;
    this->wind_speed = wind_speed;
    this->results_dir = results_dir;
    this->scenario_name = scenario_name;
    this->radio_attenuation_threshold = get_free_space_path_loss(radio_frequency, distance);
    this->swarm_size = asvs.size();
    for (auto asv : asvs) {
        Graph::vertex_descriptor v = boost::add_vertex(graph);
        graph[v].asv = asv;
        graph[v].radio_attenuation_threshold = this->radio_attenuation_threshold;
    }
}

void Swarm::set_comms_link(std::pair<Asv*, Asv*> comms_link) {
    auto find_vertex = [&](const Asv* asv) -> Graph::vertex_descriptor {
        for (auto v : boost::make_iterator_range(boost::vertices(graph))) {
            if (graph[v].asv == asv) {
                return v;
            }
        }
        throw std::runtime_error("Vertex not found");
    };

    Graph::vertex_descriptor v1 = find_vertex(comms_link.first);
    Graph::vertex_descriptor v2 = find_vertex(comms_link.second);

    if (v1 != v2) {
        boost::add_edge(v1, v2, graph);
    }
}

void Swarm::run_simulation() {
    // Open files to write simulation results
    std::filesystem::path comms_result_file_path = results_dir/("comms_result_" + scenario_name + ".csv");
    std::filesystem::path graph_result_file_path = results_dir/("graph_result_" + scenario_name + ".csv");
    std::ofstream comms_result_file(comms_result_file_path); // write results to file
    std::ofstream graph_result_file(graph_result_file_path); // write results to file
    comms_result_file << "time(sec),x1(m),y1(m),x2(m),y2(m),distance(m),sig_wave_ht(m),wind_speed(km/hr),rainfall(mm/hr),ant_ht(m),radio_freq(MHz),free_space_path_loss(dB),diffraction_loss(dB),fresnel_clearance(%),rain_attenuation(dB),sea_spray_attenuation(dB)" << std::endl;
    graph_result_file << "time(sec),mean_edge_length(m),ref_path_loss(dB),mean_free_space_path_loss(dB),mean_rain_attenuation(dB),mean_sea_spray_attenuation(dB),mean_diffraction_loss(dB),mean_signal_attenuation(dB),mean_degree,spatial_connectivity_index" << std::endl;

    // We will use a fixed rudder angle for both ASVs
    double rudder_angle = PI/4; // rad

    // Run till end of simulation time
    for(double time = 0; time < simulation_time; time += simulation_time_step_size/1000.0) {
        // Update the dynamics for all asvs
        for (auto v : boost::make_iterator_range(boost::vertices(graph))) {
            Asv* asv = graph[v].asv;
            wave_glider_compute_dynamics(asv, rudder_angle, simulation_time_step_size);
        }

        // For each edge of the graph, compute the radio attenuation
        for (auto edge : boost::make_iterator_range(boost::edges(graph))) {
            // Get edge vertices
            Graph::vertex_descriptor v1 = boost::source(edge, graph);
            Graph::vertex_descriptor v2 = boost::target(edge, graph);
            // Get the asvs associated with the vertices
            Asv* asv1 = graph[v1].asv;
            Asv* asv2 = graph[v2].asv;
            // Get the positons of the asvs
            union Coordinates_3D p1 = asv_get_position_origin(asv1);
            union Coordinates_3D p2 = asv_get_position_origin(asv2);
            // Calculate distance between the asvs
            double x1 = p1.keys.x;
            double y1 = p1.keys.y;
            double x2 = p2.keys.x;
            double y2 = p2.keys.y;
            double d = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
            graph[edge].distance = d;
            // Calculate free space path loss
            double free_space_path_loss = get_free_space_path_loss(radio_frequency, d);
            graph[edge].free_space_path_loss = free_space_path_loss; //dB
            // Calculate rain attenuation
            Polarisation polarisation = Polarisation::vertical; // Widely used in maritime communications, especially for line-of-sight VHF and UHF radio communication systems, including ship-to-ship and ship-to-shore links.
            double rain_attenuation = get_rain_attenuation(radio_frequency, d, polarisation, rainfall_rate);
            graph[edge].rain_attenuation = rain_attenuation;
            // Calculate sea spray attenuation
            double sea_spray_attenuation = get_sea_spray_attenuation(radio_frequency, d, sea_surface, wind_speed);
            graph[edge].sea_spray_attenuation = sea_spray_attenuation;
            // Calculate diffractino loss
            std::pair<Coordinates_3D, Coordinates_3D> antenna_positions = std::make_pair(p1, p2);
            std::pair<double, double> min_fresnel_clearance_and_diffraction_loss = get_min_fresnel_clearance_and_diffraction_loss(radio_frequency, antenna_height, d, sea_surface, antenna_positions, time, simulation_time_step_size);
            graph[edge].fresnel_clearance = min_fresnel_clearance_and_diffraction_loss.first;
            graph[edge].diffraction_loss = min_fresnel_clearance_and_diffraction_loss.second;
            // Calculate total loss
            double total_loss = graph[edge].free_space_path_loss + graph[edge].rain_attenuation + graph[edge].sea_spray_attenuation + graph[edge].diffraction_loss;
            graph[edge].total_attenuation = total_loss;
            // Write results to file
            comms_result_file << time << ","
                        << x1 << "," // m
                        << y1 << "," // m
                        << x2 << "," // m
                        << y2 << "," // m
                        << d << "," // m
                        << sea_surface_get_significant_height(sea_surface) << "," // m
                        << wind_speed*3.6 << "," // km/hr
                        << rainfall_rate << "," // mm/hr
                        << antenna_height << "," // m
                        << radio_frequency/1'000'000 << "," // MHz
                        << graph[edge].free_space_path_loss << "," // dB
                        << graph[edge].diffraction_loss << "," // dB
                        << graph[edge].fresnel_clearance << "," // %
                        << graph[edge].rain_attenuation << "," // dB
                        << graph[edge].sea_spray_attenuation << std::endl; // dB
        }
        // Create filtered graph
        EdgeFilter filter(graph);
        boost::filtered_graph<Graph, EdgeFilter> filtered_graph(graph, filter);
        // Quantify graph connectivity
        // Count the number of filtered edges
        size_t edge_count = std::distance(boost::edges(filtered_graph).first, boost::edges(filtered_graph).second);
        size_t node_count = std::distance(boost::vertices(filtered_graph).first, boost::vertices(filtered_graph).second);
        // Mean edge length
        double total_edge_length = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].distance; }
            );
        double mean_edge_length = edge_count > 0 ? total_edge_length / edge_count : 0.0;
        // Mean free space path loss
        double total_free_space_path_loss = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].free_space_path_loss; }
            );
        double mean_free_space_path_loss = edge_count > 0 ? total_free_space_path_loss / edge_count : 0.0;
        // Mean diffraction loss
        double total_diffraction_loss = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].diffraction_loss; }
            );
        double mean_diffraction_loss = edge_count > 0 ? total_diffraction_loss / edge_count : 0.0;
        // Mean rain attenuation
        double total_rain_attenuation = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].rain_attenuation; }
            );
        double mean_rain_attenuation = edge_count > 0 ? total_rain_attenuation / edge_count : 0.0;
        // Mean sea spray attenuation
        double total_sea_spray_attenuation = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].sea_spray_attenuation; }
            );
        double mean_sea_spray_attenuation = edge_count > 0 ? total_sea_spray_attenuation / edge_count : 0.0;
        // Mean signal attenuation
        double total_signal_attenuation = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].total_attenuation; }
            );
        double mean_signal_attenuation = edge_count > 0 ? total_signal_attenuation / edge_count : 0.0;
        // Mean degree
        double total_degree = std::accumulate(
            boost::vertices(filtered_graph).first, boost::vertices(filtered_graph).second, 0.0,
            [&](double sum, Graph::vertex_descriptor v) { return sum + boost::degree(v, filtered_graph); }
            );
        double mean_degree = node_count > 0 ? total_degree / node_count : 0.0;
        // Calculate composite metric that quantifies the connectivity and spatial spread of the graph. Objective should be to maximise the value of the composite metric.
        double reference_path_loss = get_free_space_path_loss(radio_frequency, distance);
        double max_degree = swarm_size - 1;
        double spatial_connectivity_index = (mean_edge_length/distance) - (mean_signal_attenuation/reference_path_loss) + (mean_degree/max_degree);
        // Write results to file
        graph_result_file   << time << ","
                            << mean_edge_length << "," // m
                            << reference_path_loss << "," // dB
                            << mean_free_space_path_loss << ","
                            << mean_rain_attenuation << ","
                            << mean_sea_spray_attenuation << ","
                            << mean_diffraction_loss << ","
                            << mean_signal_attenuation << ","
                            << mean_degree << ","
                            << spatial_connectivity_index << std::endl;
    }
    comms_result_file.close();
    graph_result_file.close();
}

std::vector<Asv *> Swarm::get_asvs() {
    std::vector<Asv*> asvs;
    Graph::vertex_iterator vi, vi_end;
    for (boost::tie(vi, vi_end) = boost::vertices(graph); vi != vi_end; ++vi) {
        asvs.push_back(graph[*vi].asv);
    }
    return asvs;
}


