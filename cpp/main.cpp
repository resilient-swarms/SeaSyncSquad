#include "swarm.h"
#include "utils.h"
#include <utility>
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <future>
#include <filesystem>
#include <numeric>
extern "C" {
#include "constants.h"
}

std::vector<std::vector<Coordinates_3D>> generate_deployment_locations(const Geo_coordinate& deployment_region_centroid, double asv_spacing, std::ofstream& file) {
    // Initialise deployment coordinates
    // Deployment pattern - hexagonal.
    //  O   O   O       <-- North row
    //    0   0   0     <-- Mid row
    //      0   0   0   <-- South row
    Geo_coordinate_converter geo_coordinate_converter(deployment_region_centroid);
    Coordinates_3D region_centroid = geo_coordinate_converter.convert(deployment_region_centroid);
    std::vector<std::vector<Coordinates_3D>> deployment_locations;

    // North row
    std::vector<Coordinates_3D> north_row;
    Coordinates_3D north_row_mid = {region_centroid.keys.x - asv_spacing * std::cos(PI/3),
                                    region_centroid.keys.y + asv_spacing * std::sin(PI/3),
                                    0};
    Coordinates_3D north_row_west = {north_row_mid.keys.x - asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    Coordinates_3D north_row_east = {north_row_mid.keys.x + asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    north_row.push_back(north_row_west);
    north_row.push_back(north_row_mid);
    north_row.push_back(north_row_east);
    file << "(" << north_row_west.keys.x << " " << north_row_west.keys.y << "),";
    file << "(" << north_row_mid.keys.x  << " " << north_row_mid.keys.y  << "),";
    file << "(" << north_row_east.keys.x << " " << north_row_east.keys.y << ")" << std::endl;
    // Mid row
    std::vector<Coordinates_3D> mid_row;
    Coordinates_3D mid_row_mid = region_centroid;
    Coordinates_3D mid_row_west = {mid_row_mid.keys.x - asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    Coordinates_3D mid_row_east = {mid_row_mid.keys.x + asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    mid_row.push_back(mid_row_west);
    mid_row.push_back(mid_row_mid);
    mid_row.push_back(mid_row_east);
    file << "(" << mid_row_west.keys.x << " " << mid_row_west.keys.y << "),";
    file << "(" << mid_row_mid.keys.x  << " " << mid_row_mid.keys.y  << "),";
    file << "(" << mid_row_east.keys.x << " " << mid_row_east.keys.y << ")" << std::endl;
    // South row
    std::vector<Coordinates_3D> south_row;
    Coordinates_3D south_row_mid = {region_centroid.keys.x + asv_spacing * std::cos(PI/3),
                                    region_centroid.keys.y - asv_spacing * std::sin(PI/3),
                                    0};
    Coordinates_3D south_row_west = {south_row_mid.keys.x - asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    Coordinates_3D south_row_east = {south_row_mid.keys.x + asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    south_row.push_back(south_row_west);
    south_row.push_back(south_row_mid);
    south_row.push_back(south_row_east);
    file << "(" << south_row_west.keys.x << " " << south_row_west.keys.y << "),";
    file << "(" << south_row_mid.keys.x  << " " << south_row_mid.keys.y  << "),";
    file << "(" << south_row_east.keys.x << " " << south_row_east.keys.y << ")" << std::endl;

    deployment_locations.push_back(north_row);
    deployment_locations.push_back(mid_row);
    deployment_locations.push_back(south_row);

    return deployment_locations;
}

int main() {
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path results_dir = root_dir/"results_swarm_comms_cpp";
    if (!std::filesystem::exists(results_dir)) {
        std::filesystem::create_directory(results_dir);
    }
    else {
        std::cout << results_dir << " already exists." << std::endl;
        return 1;
    }

    std::vector<std::future<void>> futures; // List of threads
    // Generate all scenarios to run
    std::vector<double> distances;
    std::vector<double> significant_wave_hts;
    std::vector<double> wind_speeds;
    std::vector<double> rainfall_rates;
    std::vector<double> antenna_hts;
    std::vector<double> radio_frequencies;
    for(double distance : {100.0, 400.0, 800.0, 1000.0}) { // m
        for(double significant_wave_ht : {0.5, 1.0, 1.5, 2.0, 3.0}) { // m
            for(double wind_speed : {50.0, 100.0}) { // km/hr
                for(double rainfall_rate : {2.0, 10.0, 20.0, 30.0}) { // mm/hr
                    for(double antenna_ht : {0.05, 0.1, 0.5, 1.0, 2.0, 3.0}) { // m
                        for(double radio_frequency : {868.0, 2400.0, 5200.0}) { // MHz
                            distances.push_back(distance);
                            significant_wave_hts.push_back(significant_wave_ht);
                            wind_speeds.push_back(wind_speed);
                            rainfall_rates.push_back(rainfall_rate);
                            antenna_hts.push_back(antenna_ht);
                            radio_frequencies.push_back(radio_frequency);
                        }
                    }
                }
            }
        }
    }
    // Spawn threads and calculate attenuation for each scenario.
    int max_pool_size = std::thread::hardware_concurrency();
    std::cout<<"Hardware concurrency = "<<max_pool_size<<"\n";
    int jobs_start_index = 0;
    int jobs_end_index = 0;
    int count_total_scenarios = distances.size();
    while(jobs_end_index < count_total_scenarios) {
        std::cout<<std::endl;
        int count_jobs_remaining = count_total_scenarios - jobs_end_index;
        jobs_end_index = jobs_start_index + std::min(max_pool_size, count_jobs_remaining);
        std::vector<std::thread*> thread_pool;
        // Spawn threads
        for(int i = jobs_start_index; i < jobs_end_index; ++i) {
            std::cout<<"["<<i+1<<" / "<<count_total_scenarios<<"]";
            std::cout<<" distance = "<<distances[i];
            std::cout<<" wave_ht = "<<significant_wave_hts[i];
            std::cout<<"  wind_speed = "<<wind_speeds[i];
            std::cout<<" rainfall = "<<rainfall_rates[i];
            std::cout<<" ant_ht = "<<antenna_hts[i];
            std::cout<<" freq = "<<radio_frequencies[i]<<std::endl;
            double ant_ht = antenna_hts[i];
            double frequency = radio_frequencies[i] * 1'000'000; // Hz
            double distance = distances[i]; // m
            double wind_speed = wind_speeds[i]/3.6; // m/s
            double wave_ht = significant_wave_hts[i]; // m
            double rainfall_rate = rainfall_rates[i]; // mm/hr

            auto thread_worker_function = [](double ant_ht, double frequency, double distance, double wind_speed, double wave_ht, double rainfall_rate, std::filesystem::path results_dir) {
                // Set the centroid of the deployment
                Geo_coordinate deployment_region_centroid {.longitude = -83.5, .latitude = 19.0}; // Deployment coordinates
                // Construct file paths for writing results
                std::ostringstream oss_ant_ht; // Create a string stream to format the double
                std::ostringstream oss_wave_ht; // Create a string stream to format the double
                oss_ant_ht << std::fixed << std::setprecision(2) << ant_ht;
                oss_wave_ht << std::fixed << std::setprecision(1) << wave_ht;
                std::string scenario_name = std::to_string(int(distance)) + "_" +
                                                oss_wave_ht.str() + "_" +
                                                    std::to_string(int(wind_speed*3.6)) + "_" +
                                                        std::to_string(int(rainfall_rate)) + "_" +
                                                            oss_ant_ht.str() + "_" +
                                                                std::to_string(int(frequency/1'000'000));
                std::filesystem::path comms_result_file_path = results_dir/("comms_result_" + scenario_name + ".csv");
                std::filesystem::path deployment_file_path = results_dir/("deployment_" + scenario_name + ".csv");
                // Initialise the sea surface
                double wave_dp = PI/3.0; // rad
                int count_component_waves = 15;
                int wave_rand_seed = 1;
                Sea_surface* sea_surface = sea_surface_new(wave_ht, wave_dp, wave_rand_seed, count_component_waves);
                // Set deployment locations for the asvs in the swarm
                std::ofstream deployment_file(deployment_file_path); // Write deployment config to file
                std::vector<std::vector<Coordinates_3D>> deployment_locations = generate_deployment_locations(deployment_region_centroid, distance, deployment_file);
                deployment_file.close();
                // Set ASV spec
                Asv_specification asv_spec;
                asv_spec.L_wl = 2.1; // m
                asv_spec.B_wl = 0.6; // m
                asv_spec.D = 0.25; // m
                asv_spec.T = 0.09; // m
                asv_spec.max_speed = 4.0; // m/s
                asv_spec.disp = 0.09; // m3
                asv_spec.r_roll = 0.2; // m
                asv_spec.r_pitch = 0.6; // m
                asv_spec.r_yaw = 0.06; // m
                asv_spec.cog = {1.05, 0.0, -0.2}; // {m, m, m}
                bool is_surge_sway_halt_set = true;
                // Init ASVs
                std::vector<std::vector<Asv*>> asvs;
                union Coordinates_3D attitude {0.0, 0.0, 0.0}; // {rad, rad, rad}
                for(auto& locations_row : deployment_locations) {
                    std::vector<Asv*> asvs_row;
                    for(auto& deployment_location : locations_row) {
                        Asv* asv = asv_new(asv_spec, sea_surface, deployment_location, attitude);
                        asv_set_surge_sway_halt(asv, is_surge_sway_halt_set);
                        asvs_row.push_back(asv);
                    }
                    asvs.push_back(asvs_row);
                }
                // Init Swarm for comms analysis
                std::vector<Asv*> swarm_asvs;
                for(auto& row : asvs) {
                    for(auto& asv : row) {
                        swarm_asvs.push_back(asv);
                    }
                }
                Swarm swarm(swarm_asvs, sea_surface, ant_ht, frequency, rainfall_rate, wind_speed, comms_result_file_path);
                // Create ASV pairs for assessing comms.
                //  O -- O -- O       <-- North row
                //
                //    0 -- 0 -- 0     <-- Mid row
                //
                //      0 -- 0 -- 0   <-- South row
                for (size_t i = 0; i < asvs.size(); ++i) {
                    for (size_t j = 0; j < asvs[i].size() - 1; ++j) {
                        std::pair<Asv*, Asv*> comms_pair;
                        comms_pair.first  = asvs[i][j];
                        comms_pair.second = asvs[i][j + 1];
                        swarm.set_comms_link(comms_pair);
                    }
                }
                //  O   O   O       <-- North row
                //   \   \   \
                //    0   0   0     <-- Mid row
                //     \   \   \
                //      0   0   0   <-- South row
                for (size_t i = 0; i < asvs.size() - 1; ++i) {
                    for (size_t j = 0; j < asvs[i].size(); ++j) {
                        std::pair<Asv*, Asv*> comms_pair;
                        comms_pair.first  = asvs[i][j];
                        comms_pair.second = asvs[i + 1][j];
                        swarm.set_comms_link(comms_pair);
                    }
                }
                //  O   O   O       <-- North row
                //     /   /
                //    0   0   0     <-- Mid row
                //       /   /
                //      0   0   0   <-- South row
                for (size_t i = 1; i < asvs.size(); ++i) {
                    for (size_t j = 0; j < asvs[i].size() - 1; ++j) {
                        std::pair<Asv*, Asv*> comms_pair;
                        comms_pair.first  = asvs[i][j];
                        comms_pair.second = asvs[i - 1][j + 1];
                        swarm.set_comms_link(comms_pair);
                    }
                }
                // Run simulation
                swarm.run_simulation();
                // Clean heap
                for (Asv* asv : swarm_asvs) {
                    asv_delete(asv);
                }
                sea_surface_delete(sea_surface);
            };

            std::thread* thread_object = new std::thread(thread_worker_function, ant_ht, frequency, distance, wind_speed, wave_ht, rainfall_rate, results_dir);
            thread_pool.push_back(thread_object);
        }
        // Join the threads
        for(auto thread : thread_pool)
        {
            thread->join();
        }
        // Free memory
        for(auto thread : thread_pool)
        {
            delete thread;
        }
        thread_pool.clear();
        jobs_start_index = jobs_end_index;
    }

    return 0;
}