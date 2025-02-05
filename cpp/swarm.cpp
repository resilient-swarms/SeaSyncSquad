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
        double radio_attenuation_threshold = std::min((*g)[v1].radio_attenuation_threshold, (*g)[v2].radio_attenuation_threshold);
        return g && (*g)[e].total_attenuation <= radio_attenuation_threshold + 50.0;
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
    for (auto asv : asvs) {
        Graph::vertex_descriptor v = boost::add_vertex(graph);
        graph[v].asv = asv;
        graph[v].radio_attenuation_threshold = this->radio_attenuation_threshold;
    }
}

void Swarm::set_comms_link(std::pair<Asv*, Asv*> comms_link) {
    auto find_vertex = [&](const Asv* asv) -> Graph::vertex_descriptor {
        Graph::vertex_iterator vi, vi_end;
        for (boost::tie(vi, vi_end) = boost::vertices(graph); vi != vi_end; ++vi) {
            if (graph[*vi].asv == asv) {
                return *vi;
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
    graph_result_file << "time(sec),avg_edge_dist(m),avg_attenuation(dB),density,num_components,spatial_connectivity_index" << std::endl;

    // We will use a fixed rudder angle for both ASVs
    double rudder_angle = PI/4; // rad

    // Run till end of simulation time
    for(double time = 0; time < simulation_time; time += simulation_time_step_size/1000.0) {
        // Update the dynamics for all asvs
        Graph::vertex_iterator vi, vi_end;
        for (boost::tie(vi, vi_end) = boost::vertices(graph); vi != vi_end; ++vi) {
            Asv* asv = graph[*vi].asv;
            wave_glider_compute_dynamics(asv, rudder_angle, simulation_time_step_size);
        }

        // For each edge of the graph, compute the radio attenuation
        std::vector<double> edge_distances;
        std::vector<double> edge_fresnel_clearances;
        std::vector<double> edge_diffraction_losses;
        std::vector<double> edge_total_losses;
        std::vector<double> edge_normalised_losses;
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
            edge_distances.push_back(d);
            // Calculate free space path loss
            double free_space_path_loss = get_free_space_path_loss(radio_frequency, d);
            graph[edge].free_space_path_loss = free_space_path_loss;
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
            edge_fresnel_clearances.push_back(graph[edge].fresnel_clearance);
            edge_diffraction_losses.push_back(graph[edge].diffraction_loss);
            // Calculate total loss
            double total_loss = free_space_path_loss + rain_attenuation + sea_spray_attenuation + graph[edge].diffraction_loss;
            graph[edge].total_attenuation = total_loss;
            edge_total_losses.push_back(total_loss);
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
        boost::filtered_graph<Graph, EdgeFilter> trimmed_graph(graph, filter);
        auto edges = boost::edges(trimmed_graph);  // Store iterator pair
        size_t edge_count = boost::num_edges(trimmed_graph);
        // Quantify graph connectivity
        double avg_edge_distance = 0.0;
        if (edge_count > 0) {
            avg_edge_distance = std::accumulate(edges.first, edges.second, 0.0,
                [&](double sum, Graph::edge_descriptor e) {
                    return sum + trimmed_graph[e].distance;
                }) / edge_count;
        }
        double avg_attenuation = 0.0;
        if (edge_count > 0) {
            avg_attenuation = std::accumulate(edges.first, edges.second, 0.0,
                [&](double sum, Graph::edge_descriptor e) {
                    return sum + trimmed_graph[e].total_attenuation;
                }) / edge_count;
        }
        // Calculate graph density
        // Ref: https://en.wikipedia.org/wiki/Dense_graph
        // if density = 1, then graph is fully connected.
        // if density = 0, then graph is sparse.
        double density = 2.0 * boost::num_edges(trimmed_graph) / (boost::num_vertices(trimmed_graph) * (boost::num_vertices(trimmed_graph) - 1));
        // Calculate number of connected components
        // If num_components == 1, the graph is fully connected.
        // If num_components > 1, the graph is NOT fully connected.
        // Calculate connected components
        std::vector<int> components(boost::num_vertices(trimmed_graph));
        double num_components = boost::connected_components(trimmed_graph, components.data());
        // Calculate composite metric that quantifies the connectivity and spatial spread of the graph. Objective should be to maximise the value of the composite metric.
        double spatial_connectivity_index = (avg_edge_distance * density * (1.0/num_components)) - avg_attenuation;
        // Write results to file
        graph_result_file   << time << ","
                            << avg_edge_distance << "," // m
                            << avg_attenuation << "," // dB
                            << density << ","
                            << num_components << ","
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


