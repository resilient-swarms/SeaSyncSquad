#include "disgo.h"
#include "rudder_controller.h"
#include "utils.h"
#include "radio_attenuation.h"
#include <utility>
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>
#include <filesystem>
#include <numeric>
#include <random>
#include <chrono>
#include <iomanip>

extern "C" {
#include "constants.h"
}

struct AsvNavigation {
    Asv* asv;
    std::vector<Coordinates_3D> waypoints;
    bool has_reached_waypoint;
    Rudder_controller rudder_controller;
    std::vector<Coordinates_3D> asv_path;
};

int main() {
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path results_dir = root_dir / "results_disgo";

    if (!std::filesystem::exists(results_dir)) {
        std::filesystem::create_directory(results_dir);
    } else {
        std::cout << results_dir << " already exists.\n";
        return 1;
    }

    // Initialize the sea surface
    constexpr double wave_ht = 2.3; // m
    constexpr double wave_dp = PI / 3.0; // rad
    constexpr int count_component_waves = 15;
    constexpr int wave_rand_seed = 1;
    Sea_surface* sea_surface = sea_surface_new(wave_ht, wave_dp, wave_rand_seed, count_component_waves);

    // Set ASV specifications
    Asv_specification asv_spec{
        .L_wl = 2.1,
        .B_wl = 0.6,
        .D = 0.25,
        .T = 0.09,
        .max_speed = 4.0,
        .disp = 0.09,
        .r_roll = 0.2,
        .r_pitch = 0.6,
        .r_yaw = 0.06,
        .cog = {1.05, 0.0, -0.2} // Center of gravity
    };
    constexpr bool is_surge_sway_halt_set = false;

    // ASV deployment locations
    std::vector<Coordinates_3D> deployment_locations = {
        {4000, 6000, 0}, {5000, 6000, 0}, {6000, 6000, 0},
        {4000, 5000, 0}, {5000, 5000, 0}, {6000, 5000, 0},
        {4000, 4000, 0}, {5000, 4000, 0}, {6000, 4000, 0}
    };

    // Initialize ASVs
    std::vector<AsvNavigation> asvs;
    union Coordinates_3D attitude{0.0, 0.0, 0.0}; // {rad, rad, rad}

    for (size_t i = 0; i < deployment_locations.size(); ++i) {
        Asv* asv = asv_new(asv_spec, sea_surface, deployment_locations[i], attitude);
        asv_set_surge_sway_halt(asv, is_surge_sway_halt_set);
        Eigen::Vector3d K(1.0, 1.0, 1.0);
        Rudder_controller rudder_controller(asv_spec, K);
        asvs.emplace_back(AsvNavigation{asv, {deployment_locations[i]}, true, rudder_controller});
    }

    // Simulation parameters
    constexpr double simulation_time = 60.0 * 60.0; // seconds
    constexpr double simulation_time_step_size = 40.0; // milliseconds

    for (double time = 0; time < simulation_time; time += simulation_time_step_size / 1000.0) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "] " << "Simulation time: " << time << "\n";
        // Environment parameters
        double significant_wave_ht = 3.2; // m
        double antenna_height = 1.0; // m
        double radio_frequency = 2400.0 * 1'000'000; // Hz
        double rainfall_rate = 13.0; // mm/hr
        double wind_speed = 20.0 / 3.6; // m/s
        double design_antenna_range = 1'000.0; // m
        double fspl_design = get_free_space_path_loss(radio_frequency, design_antenna_range); // dB
        double environmental_excess_loss = 20.0; // dB

        // Copy of ASVs for use in the lambda function
        const std::vector<AsvNavigation> asvs_copy = asvs;

        // Lambda function to handle ASV operations in threads
        auto thread_worker_function = [&](int i) {
            if (asvs[i].has_reached_waypoint) {
                // std::cout << "R" << i << "\n";

                // Collect swarm positions
                std::vector<Coordinates_3D> swarm_positions;
                for (const auto& item : asvs_copy) {
                    swarm_positions.push_back(asv_get_position_cog(item.asv));
                }

                // Determine new waypoint
                constexpr double search_radius = 50.0; // m
                Coordinates_3D current_asv_position = asv_get_position_cog(asvs[i].asv);
                Disgo disgo(swarm_positions, significant_wave_ht, antenna_height, radio_frequency,
                            rainfall_rate, wind_speed, fspl_design, environmental_excess_loss);
                Coordinates_3D new_waypoint = disgo.get_new_position(current_asv_position, search_radius);

                // if (new_waypoint.keys.x == asvs[i].waypoints.back().keys.x && new_waypoint.keys.y == asvs[i].waypoints.back().keys.y) {
                //     // Waypoint has not changed. Anchor here.
                //     asv_set_surge_sway_halt(asvs[i].asv, true);
                // } else {
                //     // std::cout << "ASV" << i << ": Old waypoint = (" << asvs[i].waypoints.back().keys.x << ", " << asvs[i].waypoints.back().keys.y << "). New waypoint = (" << new_waypoint.keys.x << ", " << new_waypoint.keys.y << ").\n";
                //     asvs[i].waypoints.push_back(new_waypoint);
                //     asvs[i].has_reached_waypoint = false;
                //     asv_set_surge_sway_halt(asvs[i].asv, false);
                // }
                asvs[i].waypoints.push_back(new_waypoint);
                asvs[i].has_reached_waypoint = false;
            }

            double rudder_angle = 0.0;
            try {
                rudder_angle = asvs[i].rudder_controller.get_rudder_angle(asvs[i].asv, asvs[i].waypoints.back());
            } catch (...) {
                // Do nothing
            }

            wave_glider_compute_dynamics(asvs[i].asv, rudder_angle, simulation_time_step_size);

            Coordinates_3D asv_position = asv_get_position_cog(asvs[i].asv);
            double d = std::hypot(asv_position.keys.x - asvs[i].waypoints.back().keys.x,
                                  asv_position.keys.y - asvs[i].waypoints.back().keys.y);
            asvs[i].has_reached_waypoint = (d <= 10.0);
            // std::cout << "ASV" << i << ": Has reached waypoint? " << asvs[i].has_reached_waypoint << "\n";

            // Write position to file
            asvs[i].asv_path.push_back(asv_position);
            // std::ofstream asv_file(asvs[i].asv_file_path, std::ios::app);
            // if (asv_file.is_open()) {
            //     asv_file << asv_position.keys.x << "," << asv_position.keys.y << "\n";
            // }
        };

        // Launch threads
        std::vector<std::thread> thread_pool;
        for (int i = 0; i < asvs.size(); i++) {
            thread_pool.emplace_back(thread_worker_function, i);
        }

        // Join all threads
        for (auto& thread : thread_pool) {
            thread.join();
        }

        // TODO: write current swarm value to file.
    }

    // write asv path to file
    for (int i = 0; i < asvs.size(); i++) {
        std::filesystem::path asv_file_path = results_dir / ("asv_" + std::to_string(i) + "_path.csv");
        std::ofstream asv_file(asv_file_path);
        if (asv_file.is_open()) {
            asv_file << "x,y\n";
            for (auto& position : asvs[i].asv_path) {
                asv_file << position.keys.x << "," << position.keys.y << "\n";
            }
        }
    }

    // write asv waypoints to file
    for (int i = 0; i < asvs.size(); i++) {
        std::filesystem::path asv_file_path = results_dir / ("asv_" + std::to_string(i) + "_waypoints.csv");
        std::ofstream asv_file(asv_file_path);
        if (asv_file.is_open()) {
            asv_file << "x,y\n";
            for (auto& waypoint : asvs[i].waypoints) {
                asv_file << waypoint.keys.x << "," << waypoint.keys.y << "\n";
            }
        }
    }

    return 0;
}
