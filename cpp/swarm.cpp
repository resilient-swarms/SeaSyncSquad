#include "swarm.h"
#include "radio_attenuation.h"
#include <fstream>
#include <algorithm>
#include <numeric>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/stoer_wagner_min_cut.hpp>
extern "C" {
#include "constants.h"
}



Swarm::Swarm(std::vector<Asv*> asvs, Sea_surface* sea_surface, double antenna_height, double radio_frequency, double rainfall_rate, double wind_speed, std::filesystem::path comms_result_file_path) {
    this->sea_surface = sea_surface;
    this->antenna_height = antenna_height;
    this->radio_frequency = radio_frequency;
    this->rainfall_rate = rainfall_rate;
    this->wind_speed = wind_speed;
    this->comms_result_file_path = comms_result_file_path;
    for (auto asv : asvs) {
        Graph::vertex_descriptor v = boost::add_vertex(graph);
        graph[v].asv = asv;
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
    // Open file to wirte simulation results
    std::ofstream comms_result_file(comms_result_file_path); // write results to file
    comms_result_file << "time(sec),x1(m),y1(m),x2(m),y2(m),distance(m),sig_wave_ht(m),wind_speed(km/hr),rainfall(mm/hr),ant_ht(m),radio_freq(MHz),free_space_path_loss(dB),diffraction_loss(dB),fresnel_clearance(%),rain_attenuation(dB),sea_spray_attenuation(dB)" << std::endl;

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
        std::vector<double> edge_fresnel_clearances;
        std::vector<double> edge_diffraction_losses;
        std::vector<double> edge_total_losses;
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
        // Quantify graph connectivity
        // TODO: create subgraph for fresnel clearance <= 20%.
        double avg_fresnel_clearance = std::accumulate(edge_fresnel_clearances.begin(), edge_fresnel_clearances.end(), 0.0) / edge_fresnel_clearances.size();
        double min_fresnel_clearance = *std::min_element(edge_fresnel_clearances.begin(), edge_fresnel_clearances.end());
        double avg_diffraction_loss = std::accumulate(edge_diffraction_losses.begin(), edge_diffraction_losses.end(), 0.0) / edge_diffraction_losses.size();
        double min_diffraction_loss = *std::min_element(edge_diffraction_losses.begin(), edge_diffraction_losses.end());
        double avg_total_loss = std::accumulate(edge_total_losses.begin(), edge_total_losses.end(), 0.0) / edge_total_losses.size();
        double min_total_loss = *std::min_element(edge_total_losses.begin(), edge_total_losses.end());
    }
    comms_result_file.close();
}

