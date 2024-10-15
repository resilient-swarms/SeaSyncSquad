#include "radio_attenuation.h"
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <future>
#include <filesystem>
#include <numeric>


int main(int argc, char * argv[]){ // only one argument expected, and assuming it is the path to the results directory.
    std::string dir_results = argv[1];
    // Create the results directory if it doesn't exist
    if(!std::filesystem::exists(dir_results))
    {
        std::filesystem::create_directory(dir_results);
    }
    std::string file_name = dir_results + "/radio_attenuation_data.csv";
    std::ofstream file(file_name); // write results to file
    file << "distance(km),sig_wave_ht(m),wind_speed(km/hr),rainfall(mm/hr),ant_ht(m),radio_freq(MHz),free_space_path_loss(dB),diffraction_loss(dB),rain_attenuation(dB),sea_spray_attenuation(dB),total_loss(dB)" << std::endl;

    Polarisation polarisation = Polarisation::vertical; // Widely used in maritime communications, especially for line-of-sight VHF and UHF radio communication systems, including ship-to-ship and ship-to-shore links.
    std::vector<std::future<void>> futures; // List of threads
    // Generate all scenarios to run
    std::vector<double> distances;
    std::vector<double> significant_wave_hts;
    std::vector<double> wind_speeds;
    std::vector<double> rainfall_rates;
    std::vector<double> antenna_hts;
    std::vector<double> radio_frequencies;
    for(double distance : {10.0, 50.0, 100.0, 150.0, 200.0}) {
        // km
        for(double significant_wave_ht : {3.0, 5.0, 7.5, 10.0, 12.0}) {
            // mm/hr
            for(double wind_speed : {50.0, 100.0, 150.0, 200.0, 250.0, 300.0, 350.0}) {
                // km/hr
                for(double rainfall_rate : {2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 15.0, 20.0}) {
                    // m
                    for(double antenna_ht : {1.0, 2.0, 3.0, 4.0, 5.0}) {
                        // m
                        for(double radio_frequency : {2400.0, 5200.0}) {
                            // GHz
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
        std::mutex file_mutex;
        for(int i = jobs_start_index; i < jobs_end_index; ++i) {
            std::cout<<"["<<i<<" / "<<count_total_scenarios<<"]";
            std::cout<<" distance = "<<distances[i];
            std::cout<<" wave_ht = "<<significant_wave_hts[i];
            std::cout<<"  wind_speed = "<<wind_speeds[i];
            std::cout<<" rainfall = "<<rainfall_rates[i];
            std::cout<<" ant_ht = "<<antenna_hts[i];
            std::cout<<" freq = "<<radio_frequencies[i]<<std::endl;
            double ant_ht = antenna_hts[i];
            double frequency = radio_frequencies[i] * 1'000'000; // Hz
            double distance = distances[i] * 1000.0; // m
            double wind_speed = wind_speeds[i]/3.6; // m/s
            double wave_ht = significant_wave_hts[i]; // m
            double rainfall_rate = rainfall_rates[i]; // mm/hr
            std::thread* thread_object = new std::thread([&]() {
                RadioAttenuation result(ant_ht, frequency, polarisation, distance, wave_ht, wind_speed, rainfall_rate);
                std::lock_guard<std::mutex> lock(file_mutex);
                file << result.communication_distance/1000.0 << "," // km
                    << result.significant_wave_ht << "," // m
                    << result.wind_speed*3.6 << "," // km/hr
                    << result.rainfall_rate << "," // mm/hr
                    << result.antenna_height << "," // m
                    << result.radio_frequency/1'000'000 << "," // MHz
                    << result.free_space_path_loss << "," // dB
                    << result.diffraction_loss_mean << "," // dB
                    << result.rain_attenuation << "," // dB
                    << result.sea_spray_attenuation << "," // dB
                    << result.total_attenuation << std::endl; // dB
                // Also write the diffraction losses from each timestep of simulation to a file
                std::string file_name_diffraction = dir_results +
                    "/diffraction_loss_" +
                    std::to_string(int(result.communication_distance/1000.0)) + "_" +
                    std::to_string(int(result.significant_wave_ht)) + "_" +
                    std::to_string(int(result.wind_speed)) + "_" +
                    std::to_string(int(result.rainfall_rate)) + "_" +
                    std::to_string(int(result.antenna_height)) + "_" +
                    std::to_string(int(result.radio_frequency/1'000'000)) + ".txt";
                std::ofstream file_diffraction(file_name_diffraction); // write results to file
                file_diffraction << "diffraction_loss(dB)"<<std::endl;
                for(int i = 0; i < result.diffraction_loss.size(); ++i) {
                    file_diffraction << result.diffraction_loss[i] << std::endl;
                }
            });
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
    file.close();
}

// TODO: plot results
