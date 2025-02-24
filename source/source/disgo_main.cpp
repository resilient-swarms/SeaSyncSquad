#include "disgo.h"
#include "utils.h"
#include "radio_attenuation.h"
#include <utility>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <filesystem>
#include <numeric>
#include <random>
#include <chrono>
#include <iomanip>


struct AsvNavigation {
    std::vector<ASVLite::Geometry::Coordinates3D> path;
};

int main() {
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path results_dir = root_dir / "results_disgo";

    if (!std::filesystem::exists(results_dir)) {
        if (!std::filesystem::create_directory(results_dir)) {
            std::cerr << "Failed to create results directory.\n";
            return 1;
        }
    } else {
        std::cout << results_dir << " already exists. Continuing execution.\n";
    }

    // ASV deployment locations
    std::vector<ASVLite::Geometry::Coordinates3D> deployment_locations = {
        {-1000, 1000, 0}, {0, 1000, 0}, {1000, 1000, 0},
        {-1000, 0, 0}, {0, 0, 0}, {1000, 0, 0},
        {-1000, -1000, 0}, {0, -1000, 0}, {1000, -1000, 0}
    };

    // Initialize formation
    std::vector<AsvNavigation> asvs;
    asvs.reserve(deployment_locations.size());

    for (const auto& loc : deployment_locations) {
        AsvNavigation asv;
        asv.path.push_back(loc);
        asvs.push_back(asv);
    }

    std::vector<std::pair<double, double>> current_and_new_formation_values;

    // Simulation parameters
    for (int search_iteration = 0; search_iteration < 100; ++search_iteration) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "\n[" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "] " << "Iteration: " << search_iteration << "\n";

        // Environment parameters
        double significant_wave_ht = 3.2; // m
        double antenna_height = 1.0; // m
        double radio_frequency = 2400.0 * 1'000'000; // Hz
        double rainfall_rate = 13.0; // mm/hr
        double wind_speed = 20.0 / 3.6; // m/s
        double design_antenna_range = 1'000.0; // m
        double fspl_design = get_free_space_path_loss(radio_frequency, design_antenna_range); // dB
        double environmental_excess_loss = 20.0; // dB

        // Collect current swarm formation
        std::vector<ASVLite::Geometry::Coordinates3D> current_formation;
        current_formation.reserve(asvs.size());

        for (const auto& asv : asvs) {
            if (!asv.path.empty()) {
                current_formation.push_back(asv.path.back());
            }
        }

        // Init Disgo algorithm
        Disgo disgo(significant_wave_ht, antenna_height, radio_frequency, rainfall_rate, wind_speed, fspl_design, environmental_excess_loss);

        // Current formation value
        double current_formation_value = disgo.evaluate_formation(current_formation);
        std::cout << "Current value: " << current_formation_value << "\n";

        // Compute the new formation
        double search_radius = 10.0; // m
        std::vector<ASVLite::Geometry::Coordinates3D> new_formation = disgo.get_new_formation(current_formation, search_radius);
        for (size_t i = 0; i < new_formation.size(); ++i) {
            asvs[i].path.push_back(new_formation[i]);
        }

        // New formation value
        double new_formation_value = disgo.evaluate_formation(new_formation);
        std::cout << "New value: " << new_formation_value << "\n";

        current_and_new_formation_values.push_back(std::make_pair(current_formation_value, new_formation_value));
    }

    // Write ASV path to file
    for (size_t i = 0; i < asvs.size(); i++) {
        std::filesystem::path asv_file_path = results_dir / ("asv_" + std::to_string(i) + "_path.csv");
        std::ofstream asv_file(asv_file_path);

        if (asv_file.is_open()) {
            asv_file << "x(m),y(m)\n";
            for (const auto& position : asvs[i].path) {
                asv_file << position.keys.x << "," << position.keys.y << "\n";
            }
            asv_file.close();
        } else {
            std::cerr << "Error opening file: " << asv_file_path << "\n";
        }
    }

    // Write formation values to file
    std::filesystem::path formation_value_file_path = results_dir / "formation_values.csv";
    std::ofstream formation_value_file(formation_value_file_path);

    if (formation_value_file.is_open()) {
        formation_value_file << "current_value,new_value\n";
        for (const auto& item : current_and_new_formation_values) {
            formation_value_file << item.first << "," << item.second << "\n";
        }
        formation_value_file.close();
    } else {
        std::cerr << "Error opening file: " << formation_value_file_path << "\n";
    }

    return 0;
}
