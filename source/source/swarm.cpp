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


struct EdgeFilter {
    const Graph* g = nullptr;  // Initialize to nullptr
    EdgeFilter() = default;  // Default constructor
    EdgeFilter(const Graph& graph) : g(&graph) {}

    bool operator()(const Graph::edge_descriptor& e) const {
        auto v1 = boost::source(e, *g);
        auto v2 = boost::target(e, *g);
        double radio_attenuation_threshold = std::min((*g)[v1].radio_attenuation_threshold, (*g)[v2].radio_attenuation_threshold);
        return radio_attenuation_threshold >= (*g)[e].total_attenuation;
    }
};

Swarm::Swarm(const std::vector<ASVLite::Geometry::Coordinates3D>& positions, 
             const ASVLite::SeaSurface& sea_surface, 
             const double antenna_height, 
             const double radio_frequency, 
             const double rainfall_rate, 
             const double wind_speed, 
             const double fspl_design, 
             const double environmental_excess_loss) :
    swarm_size(positions.size()),
    sea_surface(sea_surface),
    antenna_height(antenna_height),
    radio_frequency(radio_frequency),
    rainfall_rate(rainfall_rate),
    wind_speed(wind_speed),
    fspl_design(fspl_design),
    environmental_excess_loss(environmental_excess_loss),
    print_to_dir(true) {

    radio_attenuation_threshold = fspl_design + environmental_excess_loss;
    free_space_radio_range = std::pow(10.0, (radio_attenuation_threshold - 32.4) / 20.0 - std::log10(radio_frequency/1'000'000.0)) * 1000.0; // m
    // Create graph vertices
    for (const auto& position : positions) {
        Graph::vertex_descriptor v = boost::add_vertex(graph);
        graph[v].position = position;
        graph[v].radio_attenuation_threshold = this->radio_attenuation_threshold;
    }
    // Create graph edges
    for(size_t i = 0; i < swarm_size; ++i) {
        for(size_t j = i+1; j < swarm_size; ++j) {
            std::pair<ASVLite::Geometry::Coordinates3D, ASVLite::Geometry::Coordinates3D> antenna_pair = std::make_pair(positions[i], positions[j]);
            set_comms_link(antenna_pair);
        }
    }
}


void Swarm::set_comms_link(const std::pair<ASVLite::Geometry::Coordinates3D, ASVLite::Geometry::Coordinates3D> antenna_pair) {
    auto find_vertex = [&](const ASVLite::Geometry::Coordinates3D& position) -> Graph::vertex_descriptor {
        for (auto v : boost::make_iterator_range(boost::vertices(graph))) {
            if (graph[v].position == position) {
                return v;
            }
        }
        throw std::runtime_error("Vertex not found");
    };

    Graph::vertex_descriptor v1 = find_vertex(antenna_pair.first);
    Graph::vertex_descriptor v2 = find_vertex(antenna_pair.second);

    if (v1 != v2) {
        boost::add_edge(v1, v2, graph);
    }
}

double Swarm::run_simulation() {
    print_to_dir = false;
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path temp_dir = root_dir/"temp";
    std::string scenario_name = "temp";
    double value = run_simulation(temp_dir, scenario_name);
    return value;
}

double Swarm::run_simulation(const std::filesystem::path& results_dir, const std::string& scenario_name) {
    double cumulative_spatial_connectivity_index = 0.0;

    // Open files to write simulation results
    std::ofstream comms_result_file;
    std::ofstream graph_result_file;
    if (print_to_dir) {
        std::filesystem::path comms_result_file_path = results_dir/("comms_result_" + scenario_name + ".csv");
        std::filesystem::path graph_result_file_path = results_dir/("graph_result_" + scenario_name + ".csv");
        std::ofstream comms_result_file(comms_result_file_path); // write results to file
        std::ofstream graph_result_file(graph_result_file_path); // write results to file
        comms_result_file << "time(sec),x1(m),y1(m),x2(m),y2(m),distance(m),sig_wave_ht(m),wind_speed(km/hr),rainfall(mm/hr),ant_ht(m),radio_freq(MHz),free_space_path_loss(dB),diffraction_loss(dB),fresnel_clearance(%),rain_attenuation(dB),sea_spray_attenuation(dB)" << std::endl;
        graph_result_file << "time(sec),mean_edge_length(m),free_space_radio_range(m),fspl_design(dB),mean_free_space_path_loss(dB),mean_rain_attenuation(dB),mean_sea_spray_attenuation(dB),mean_diffraction_loss(dB),mean_signal_attenuation(dB),mean_degree,density,num_components,dispersion_index,dispersion_shape_index,spatial_connectivity_index" << std::endl;
    }

    // Run till end of simulation time
    for(double time = 0; time < simulation_time; time += simulation_time_step_size/1000.0) { 
        // For each edge of the graph, compute the radio attenuation
        for (auto edge : boost::make_iterator_range(boost::edges(graph))) {
            // Get edge vertices
            const Graph::vertex_descriptor v1 = boost::source(edge, graph);
            const Graph::vertex_descriptor v2 = boost::target(edge, graph);
            // Get the positions associated with the vertices
            const ASVLite::Geometry::Coordinates3D p1 = graph[v1].position;
            const ASVLite::Geometry::Coordinates3D p2 = graph[v2].position;
            // Calculate distance between the asvs
            const double x1 = p1.keys.x;
            const double y1 = p1.keys.y;
            const double x2 = p2.keys.x;
            const double y2 = p2.keys.y;
            const double d = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
            graph[edge].distance = d;
            // Calculate free space path loss
            double free_space_path_loss = get_free_space_path_loss(radio_frequency, d);
            graph[edge].free_space_path_loss = free_space_path_loss; //dB
            // Calculate rain attenuation
            const Polarisation polarisation = Polarisation::vertical; // Widely used in maritime communications, especially for line-of-sight VHF and UHF radio communication systems, including ship-to-ship and ship-to-shore links.
            double rain_attenuation = get_rain_attenuation(radio_frequency, d, polarisation, rainfall_rate);
            graph[edge].rain_attenuation = rain_attenuation;
            // Calculate sea spray attenuation
            double sea_spray_attenuation = get_sea_spray_attenuation(radio_frequency, d, sea_surface, wind_speed);
            graph[edge].sea_spray_attenuation = sea_spray_attenuation;
            // Calculate diffraction loss
            const std::pair<ASVLite::Geometry::Coordinates3D, ASVLite::Geometry::Coordinates3D> antenna_positions = std::make_pair(p1, p2);
            std::pair<double, double> min_fresnel_clearance_and_diffraction_loss = get_min_fresnel_clearance_and_diffraction_loss(radio_frequency, antenna_height, sea_surface, antenna_positions, time);
            graph[edge].fresnel_clearance = min_fresnel_clearance_and_diffraction_loss.first;
            graph[edge].diffraction_loss = min_fresnel_clearance_and_diffraction_loss.second;
            // Calculate total loss
            double total_loss = graph[edge].free_space_path_loss + graph[edge].rain_attenuation + graph[edge].sea_spray_attenuation + graph[edge].diffraction_loss;
            graph[edge].total_attenuation = total_loss;
            // Write results to file
            if (print_to_dir) {
                comms_result_file << time << ","
                        << x1 << "," // m
                        << y1 << "," // m
                        << x2 << "," // m
                        << y2 << "," // m
                        << d << "," // m
                        << sea_surface.significant_wave_height << "," // m
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
        }
        // Dispersion index
        const double total_pairwise_distance = std::accumulate(
            boost::edges(graph).first, boost::edges(graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + graph[e].distance; }
            );
        const double number_of_pairs = boost::num_edges(graph);
        const double mean_pairwise_distance = number_of_pairs > 0 ? total_pairwise_distance / number_of_pairs : 0.0;
        const auto max_pair = std::max_element(
            boost::edges(graph).first, boost::edges(graph).second,
            [&](Graph::edge_descriptor e1, Graph::edge_descriptor e2) { return graph[e1].distance < graph[e2].distance; }
            );
        const double max_pairwise_distance = graph[*max_pair].distance;
        const double dispersion_shape_index = mean_pairwise_distance / max_pairwise_distance;
        // Create filtered graph
        EdgeFilter filter(graph);
        boost::filtered_graph<Graph, EdgeFilter> filtered_graph(graph, filter);
        // Copy the filtered graph to a new mutable graph
        Graph copied_graph;
        boost::copy_graph(filtered_graph, copied_graph);
        // // Quantify graph connectivity
        // Connected components
        // num_components = 1, the graph is fully connected.
        // num_components > 1, the graph is not fully connected.
        std::vector<int> component(boost::num_vertices(copied_graph));
        const int num_components = boost::connected_components(copied_graph, &component[0]);
        // Count the number of filtered edges
        const size_t edge_count = std::distance(boost::edges(filtered_graph).first, boost::edges(filtered_graph).second);
        const size_t node_count = std::distance(boost::vertices(filtered_graph).first, boost::vertices(filtered_graph).second);
        // Graph Density
        // Ref: https://en.wikipedia.org/wiki/Dense_graph
        // Density ≈ 1 → Graph is fully connected.
        // Density ≈ 0 → Graph is sparse.
        const double density = 2.0 * edge_count / (node_count * (node_count - 1));
        // Mean edge length
        const double total_edge_length = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].distance; }
            );
        const double mean_edge_length = edge_count > 0 ? total_edge_length / edge_count : 0.0;
        // Mean free space path loss
        const double total_free_space_path_loss = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].free_space_path_loss; }
            );
        const double mean_free_space_path_loss = edge_count > 0 ? total_free_space_path_loss / edge_count : 0.0;
        // Mean diffraction loss
        const double total_diffraction_loss = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].diffraction_loss; }
            );
        const double mean_diffraction_loss = edge_count > 0 ? total_diffraction_loss / edge_count : 0.0;
        // Mean rain attenuation
        const double total_rain_attenuation = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].rain_attenuation; }
            );
        const double mean_rain_attenuation = edge_count > 0 ? total_rain_attenuation / edge_count : 0.0;
        // Mean sea spray attenuation
        const double total_sea_spray_attenuation = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].sea_spray_attenuation; }
            );
        const double mean_sea_spray_attenuation = edge_count > 0 ? total_sea_spray_attenuation / edge_count : 0.0;
        // Mean signal attenuation
        const double total_signal_attenuation = std::accumulate(
            boost::edges(filtered_graph).first, boost::edges(filtered_graph).second, 0.0,
            [&](double sum, Graph::edge_descriptor e) { return sum + filtered_graph[e].total_attenuation; }
            );
        const double mean_signal_attenuation = edge_count > 0 ? total_signal_attenuation / edge_count : 0.0;
        // Mean degree
        const double total_degree = std::accumulate(
            boost::vertices(filtered_graph).first, boost::vertices(filtered_graph).second, 0.0,
            [&](double sum, Graph::vertex_descriptor v) { return sum + boost::degree(v, filtered_graph); }
            );
        const double mean_degree = node_count > 0 ? total_degree / node_count : 0.0;
        // Calculate composite metric that quantifies the connectivity and spatial spread of the graph. Objective should be to maximise the value of the composite metric.
        const double dispersion_index = mean_edge_length/free_space_radio_range;
        double spatial_connectivity_index_current_time_step = (density + dispersion_index + dispersion_shape_index) / (3 * num_components);
        cumulative_spatial_connectivity_index += spatial_connectivity_index_current_time_step;
        // Write results to file
        if (print_to_dir) {
            graph_result_file   << time << ","
                            << mean_edge_length << "," // m
                            << free_space_radio_range << "," // m
                            << fspl_design << "," // dB
                            << mean_free_space_path_loss << ","
                            << mean_rain_attenuation << ","
                            << mean_sea_spray_attenuation << ","
                            << mean_diffraction_loss << ","
                            << mean_signal_attenuation << ","
                            << mean_degree << ","
                            << density << ","
                            << num_components << ","
                            << dispersion_index << ","
                            << dispersion_shape_index << ","
                            << spatial_connectivity_index_current_time_step << std::endl;
        }
    }
    if (print_to_dir) {
        comms_result_file.close();
        graph_result_file.close();
    }

    double num_sim_steps = simulation_time / simulation_time_step_size * 1000.0;
    double mean_spatial_connectivity_index = cumulative_spatial_connectivity_index / num_sim_steps;
    return mean_spatial_connectivity_index;
}

