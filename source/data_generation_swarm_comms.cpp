#include "radio_attenuation.h"
#include "utils.h"
#include "utility"
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <future>
#include <filesystem>
#include <numeric>


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


int main(int argc, char * argv[]){
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path results_dir = root_dir/"results_swarm_comms";
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
                // Set deployment locations for the asvs in the swarm
                std::ofstream deployment_file(deployment_file_path); // Write deployment config to file
                std::vector<std::vector<Coordinates_3D>> deployment_locations = generate_deployment_locations(deployment_region_centroid, distance, deployment_file);
                deployment_file.close();
                // Create ASV pairs for assessing comms.
                std::vector<std::pair<Coordinates_3D, Coordinates_3D>> comms_pairs;
                //  O -- O -- O       <-- North row
                //
                //    0 -- 0 -- 0     <-- Mid row
                //
                //      0 -- 0 -- 0   <-- South row
                for (size_t i = 0; i < deployment_locations.size(); ++i) {
                    for (size_t j = 0; j < deployment_locations[i].size() - 1; ++j) {
                        std::pair<Coordinates_3D, Coordinates_3D> comms_pair;
                        comms_pair.first  = deployment_locations[i][j];
                        comms_pair.second = deployment_locations[i][j + 1];
                        comms_pairs.push_back(comms_pair);
                    }
                }
                //  O   O   O       <-- North row
                //   \   \   \
                //    0   0   0     <-- Mid row
                //     \   \   \
                //      0   0   0   <-- South row
                for (size_t i = 0; i < deployment_locations.size() - 1; ++i) {
                    for (size_t j = 0; j < deployment_locations[i].size(); ++j) {
                        std::pair<Coordinates_3D, Coordinates_3D> comms_pair;
                        comms_pair.first  = deployment_locations[i][j];
                        comms_pair.second = deployment_locations[i + 1][j];
                        comms_pairs.push_back(comms_pair);
                    }
                }
                //  O   O   O       <-- North row
                //     /   /
                //    0   0   0     <-- Mid row
                //       /   /
                //      0   0   0   <-- South row
                for (size_t i = 1; i < deployment_locations.size(); ++i) {
                    for (size_t j = 0; j < deployment_locations[i].size() - 1; ++j) {
                        std::pair<Coordinates_3D, Coordinates_3D> comms_pair;
                        comms_pair.first  = deployment_locations[i][j];
                        comms_pair.second = deployment_locations[i - 1][j + 1];
                        comms_pairs.push_back(comms_pair);
                    }
                }
                std::ofstream comms_result_file(comms_result_file_path); // write results to file
                comms_result_file << "time_step,x1(m),y1(m),x2(m),y2(m),distance(km),sig_wave_ht(m),wind_speed(km/hr),rainfall(mm/hr),ant_ht(m),radio_freq(MHz),free_space_path_loss(dB),diffraction_loss(dB),fresnel_clearance(%),rain_attenuation(dB),sea_spray_attenuation(dB)" << std::endl;
                // Compute attenuation for each pair
                for (auto& pair : comms_pairs) {
                    Polarisation polarisation = Polarisation::vertical; // Widely used in maritime communications, especially for line-of-sight VHF and UHF radio communication systems, including ship-to-ship and ship-to-shore links.
                    RadioAttenuation result(ant_ht, frequency, polarisation, pair, wave_ht, wind_speed, rainfall_rate);
                    for(int i = 0; i < result.diffraction_loss.size(); ++i) {
                        comms_result_file << i << ","
                        << pair.first.keys.x << "," // m
                        << pair.first.keys.y << "," // m
                        << pair.second.keys.x << "," // m
                        << pair.second.keys.y << "," // m
                        << result.communication_distance << "," // m
                        << result.significant_wave_ht << "," // m
                        << result.wind_speed*3.6 << "," // km/hr
                        << result.rainfall_rate << "," // mm/hr
                        << result.antenna_height << "," // m
                        << result.radio_frequency/1'000'000 << "," // MHz
                        << result.free_space_path_loss << "," // dB
                        << result.diffraction_loss[i] << "," // dB
                        << result.fresnel_clearance[i] << "," // %
                        << result.rain_attenuation << "," // dB
                        << result.sea_spray_attenuation << std::endl; // dB
                    }
                }
                comms_result_file.close();
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
}


// TODO:
// Use only fresnel zone
// Adjust formulas to take sub GHz frequencies.
