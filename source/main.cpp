#include "radio_attenuation.h"
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <future>
#include <filesystem>


int main(int argc, char * argv[]){ // only one argument expected, and assuming it is the path to the results directory.
    std::string dir_results = argv[1];
    // Create the results directory if it doesn't exist
    if(!std::filesystem::exists(dir_results))
    {
        std::filesystem::create_directory(dir_results);
    }
    std::string file_name = dir_results + "/results.csv";
    std::ofstream file(file_name); // write results to file

    double radio_frequency = 5.2; // Ghz
    Polarisation polarisation = Polarisation::horizontal;
    std::array<double, 5> distances = {10, 50, 100, 150, 200}; // km
    std::array<double, 5> rainfall_rates = {5, 25, 50, 100, 150}; // mm/hr
    std::array<double, 7> wind_speeds = {50, 100, 150, 200, 250, 300, 350}; // km/hr
    std::array<double, 5> significant_wave_hts = {3.0, 5.0, 7.5, 10, 12}; // m
    std::vector<std::future<void>> futures; // List of threads
    // Calculate attenuation
    int total_cases = distances.size() * rainfall_rates.size() * wind_speeds.size() * significant_wave_hts.size();
    int counter = 0;
    file << "distance(km),rainfall(mm/hr),wind_speed(km/hr),sig_wave_ht(m),free_space_path_loss(dB),diffraction_loss(dB),rain_attenuation(dB),sea_spray_attenuation(dB),total_loss(dB)" << std::endl;
    std::mutex file_mutex;
    for(double distance : distances) {
        for(double significant_wave_ht : significant_wave_hts){
            for(double wind_speed : wind_speeds) {
                 for(double rainfall_rate : rainfall_rates){
                     std::cout << "[" << ++counter << "/" << total_cases << "]  " << distance << ", " << rainfall_rate << ", " << wind_speed << ", " << significant_wave_ht << std::endl;
                     double f = radio_frequency * 1'000'000'000; // Hz
                     double d = distance * 1000.0; // m
                     double s = wind_speed/3.6; // m/s
                     // Using std::async to construct each MyClass instance in parallel
                     futures.push_back(std::async(std::launch::async, [&]() {
                         RadioAttenuation result(f, polarisation, d, significant_wave_ht, s, rainfall_rate);
                         std::lock_guard<std::mutex> lock(file_mutex);
                         file << result.communication_distance/1000.0 << "," // km
                             << result.rainfall_rate << "," // mm/hr
                             << result.wind_speed*3.6 << "," // km/hr
                             << result.significant_wave_ht << "," // m
                             << result.free_space_path_loss << "," // dB
                             << result.diffraction_loss << "," // dB
                             << result.rain_attenuation << "," // dB
                             << result.sea_spray_attenuation << "," // dB
                             << result.total_attenuation << std::endl; // dB
                     }));
                }
            }
        }
    }
    for(auto& future : futures) {
        future.wait();
    }
    file.close();
}

// TODO:
// Limit multithreadin to the number of cores.
// simulate for varying antenna height
// simulate for different frequencies
// plot results like before
