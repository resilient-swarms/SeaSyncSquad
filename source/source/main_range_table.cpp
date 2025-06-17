#include "ASVLite/sea_surface.h"
#include "radio_attenuation.h"
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <future> 

int main(){

    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path results_dir = root_dir / "results_range_table";

    if (!std::filesystem::exists(results_dir)) {
        if (!std::filesystem::create_directory(results_dir)) {
            std::cerr << "Failed to create results directory.\n";
            return 1;
        }
    } else {
        std::cout << results_dir << " already exists. Continuing execution.\n";
    }

    std::filesystem::path results_file_path_diffraction = results_dir / ("diffraction.csv");
    std::ofstream results_file_diffraction(results_file_path_diffraction);
    results_file_diffraction << "freq(MHz),ant_ht(m),wave_ht(m),dist(m),fspl(db),diff_loss(db),total_loss(db)\n";

    std::vector<double> freqs {2400.0, 5200.0}; // MHz
    for(double freq : freqs) {
        freq = freq * 1'000'000.0; // Hz
        for(double ant_ht = 0.5; ant_ht <= 3.0; ant_ht += 0.5) { // m
            for(double wave_ht = 0.5; wave_ht <= 10.0; wave_ht += 0.5) { // m
                for(double distance = 500.0; distance <= 5000.0; distance += 500.0) { // m
                    std::cout << "freq = " << freq/ 1'000'000.0 << " ant_ht = " << ant_ht << " wave_ht = " << wave_ht << " distance = " << distance << "\n";
                    std::vector<std::future<double>> diffraction_futures;
                    for(double wave_heading = 0.0; wave_heading < M_PI; wave_heading+=M_PI/6.0) { // rad North
                        for(int rng_seed = 1; rng_seed <= 3; ++rng_seed) {
                            // A lambda function [=](){...} captures needed variables by value for thread safety.
                            diffraction_futures.push_back(std::async(std::launch::async, [=]() {
                                const ASVLite::SeaSurface<N> sea_surface {wave_ht, wave_heading, rng_seed};
                                const double time_duration = 30.0 * 60.0; // 30 minutes in seconds
                                return get_diffraction_loss(freq, ant_ht, distance, sea_surface, time_duration);
                            }));
                        }
                    }
                    // Collect the results from all the futures (threads)
                    std::vector<double> diff_losses;
                    diff_losses.reserve(diffraction_futures.size());
                    for(auto& fut : diffraction_futures) {
                        // fut.get() waits for the thread to finish and returns the result
                        diff_losses.push_back(fut.get());
                    }
                    // Calculate the average diffraction loss from all scenarios
                    const double diff_loss = std::accumulate(diff_losses.begin(), diff_losses.end(), 0.0) / diff_losses.size();
                    const double fspl = get_free_space_path_loss(freq, distance);
                    const double total_loss = fspl + diff_loss;
                    // Write the aggregated result to the CSV file
                    results_file_diffraction << freq/ 1'000'000.0 << "," << ant_ht << "," << wave_ht << "," << distance << "," << fspl << "," << diff_loss << "," << total_loss << "\n";
                }
            }
        }
    }
    results_file_diffraction.close();
    

    std::filesystem::path results_file_path_fspl = results_dir / ("fspl.csv");
    std::ofstream results_file_fspl(results_file_path_fspl);
    results_file_fspl << "freq(MHz),dist(m),fspl(db)\n";

    for(double freq : freqs) {
        freq = freq * 1'000'000.0; // Hz
        for(double ant_ht = 0.5; ant_ht <= 3.0; ant_ht += 0.5) { // m
            for(double wave_ht = 0.5; wave_ht <= 10.0; wave_ht += 0.5) { // m
                for(double distance = 100.0; distance <= 5000.0; distance += 100.0) { // m
                    const double fspl = get_free_space_path_loss(freq, distance);
                    results_file_fspl << freq/ 1'000'000.0 << "," << distance << "," << fspl <<  "\n";
                }
            }
        }
    }
    results_file_fspl.close();


    std::filesystem::path results_file_path_rain = results_dir / ("rain_attenuation.csv");
    std::ofstream results_file_rain_attenuation(results_file_path_rain);
    results_file_rain_attenuation << "freq(MHz),polarisation,dist(m),rainfall_rate(mm/hr),rain_attenuation(db)\n";
    std::vector<Polarisation> polarisations {Polarisation::horizontal, Polarisation::vertical};
    for(Polarisation polarisation : polarisations) {
        for(double freq : freqs) {
            freq = freq * 1'000'000.0; // Hz
            for(double distance = 100.0; distance <= 5000.0; distance += 100.0) { // m
                for(double rainfall_rate = 5.0; rainfall_rate <= 20.0; rainfall_rate += 5.0) { // mm/hr 
                    const double rain_attenuation = get_rain_attenuation(freq, distance, polarisation, rainfall_rate);
                    results_file_rain_attenuation << freq / 1'000'000.0 << "," << ((polarisation == Polarisation::horizontal)? "horizontal" : "vertical") << "," << distance << "," << rainfall_rate << "," << rain_attenuation << "\n";
                }
            }
        }
    }
    results_file_rain_attenuation.close();


    std::filesystem::path results_file_path_sea_spray = results_dir / ("sea_spray_attenuation.csv");
    std::ofstream results_file_sea_spray_attenuation(results_file_path_sea_spray);
    results_file_sea_spray_attenuation << "freq(MHz),dist(m),wave_ht(m),wind_speed(km/hr),sea_spray_attenuation(db)\n";
    for(double freq : freqs) {
        freq = freq * 1'000'000.0; // Hz
        for(double distance = 100.0; distance <= 5000.0; distance += 100.0) { // m
            for(double wave_ht = 0.5; wave_ht <= 10.0; wave_ht += 0.5) { // m
                for(double wind_speed = 50.0; wind_speed <= 200.0; wind_speed += 50.0) { // km/hr 
                    const double wave_heading = 0.0; // rad North
                    const int rng_seed = 1;
                    const ASVLite::SeaSurface<N> sea_surface {wave_ht, wave_heading, rng_seed};
                    const double sea_spray_attenuation = get_sea_spray_attenuation(freq, distance, sea_surface, wind_speed);
                    results_file_sea_spray_attenuation << freq / 1'000'000.0 << "," << distance << "," << wave_ht << "," << wind_speed << "," << sea_spray_attenuation << "\n";
                }
            }
        }
    }
    results_file_sea_spray_attenuation.close();


    std::filesystem::path results_file_path_comms_window = results_dir / ("comms_window.csv");
    std::ofstream results_file_comms_window(results_file_path_comms_window);
    results_file_comms_window << "freq(MHz),ant_ht(m),wave_ht(m),comms_window(%)\n";

    const double distance = 1000.0; // m
    const double freq = 2400.0 * 1'000'000.0; // Hz
    const double threshold = 20.0; // dB
    for(double ant_ht = 0.5; ant_ht <= 3.0; ant_ht += 0.5) { // m
        for(double wave_ht = 0.5; wave_ht <= 10.0; wave_ht += 0.5) { // m
            std::cout << " ant_ht = " << ant_ht << " wave_ht = " << wave_ht << "\n";
            std::vector<std::future<double>> comms_window_futures;
            for(double wave_heading = 0.0; wave_heading < M_PI; wave_heading+=M_PI/36.0) { // rad North
                for(int rng_seed = 1; rng_seed <= 3; ++rng_seed) {
                    comms_window_futures.push_back(std::async(std::launch::async, [=]() {
                            const ASVLite::SeaSurface<N> sea_surface {wave_ht, wave_heading, rng_seed};
                            const double time_duration = 30.0 * 60.0; // sec 
                            return get_comms_window_loss(threshold, freq, ant_ht, distance, sea_surface, time_duration); 
                    }));
                }
            }
            // Collect the results from all the futures (threads)
            std::vector<double> comms_window_losses;
            comms_window_losses.reserve(comms_window_futures.size());
            for(auto& fut : comms_window_futures) {
                // fut.get() waits for the thread to finish and returns the result
                comms_window_losses.push_back(fut.get());
            }
            // Calculate the average comms_window from all scenarios
            const double comms_window_loss = std::accumulate(comms_window_losses.begin(), comms_window_losses.end(), 0.0) / comms_window_losses.size();
            const double comms_window = 100.0 - comms_window_loss;
            // Write the aggregated result to the CSV file
            results_file_comms_window << freq/ 1'000'000.0 << "," << ant_ht << "," << wave_ht << "," << comms_window << "\n";
        }
    }
    results_file_comms_window.close();

    
    // Generte reference table for case study
    std::filesystem::path results_file_path_comms_window_case_study = results_dir / ("comms_window_case_study.csv");
    std::ofstream results_file_comms_window_case_study(results_file_path_comms_window_case_study);
    results_file_comms_window_case_study << "freq(MHz),ant_ht(m),dist(m),wave_ht(m),comms_window(%)\n";

    const double ant_ht = 0.5; // m
    const std::vector<double> distances = {75.0, 125.0, 250.0, 500.0, 1000.0}; // m
    for(double distance : distances) {
        for(double wave_ht = 0.5; wave_ht <= 10.0; wave_ht += 0.5) { // m
            std::cout << " distance = " << distance << " wave_ht = " << wave_ht << "\n";
            std::vector<std::future<double>> comms_window_futures;
            for(double wave_heading = 0.0; wave_heading < M_PI; wave_heading+=M_PI/36.0) { // rad North
                for(int rng_seed = 1; rng_seed <= 3; ++rng_seed) {
                    comms_window_futures.push_back(std::async(std::launch::async, [=]() {
                            const ASVLite::SeaSurface<N> sea_surface {wave_ht, wave_heading, rng_seed};
                            const double time_duration = 30.0 * 60.0; // sec 
                            return get_comms_window_loss(threshold, freq, ant_ht, distance, sea_surface, time_duration); 
                    }));
                }
            }
            // Collect the results from all the futures (threads)
            std::vector<double> comms_windows;
            comms_windows.reserve(comms_window_futures.size());
            for(auto& fut : comms_window_futures) {
                // fut.get() waits for the thread to finish and returns the result
                comms_windows.push_back(fut.get());
            }
            // Calculate the average comms_window from all scenarios
            const double comms_window = std::accumulate(comms_windows.begin(), comms_windows.end(), 0.0) / comms_windows.size();
            // Write the aggregated result to the CSV file
            results_file_comms_window_case_study << freq/ 1'000'000.0 << "," << ant_ht << "," << distance << "," << wave_ht << "," << comms_window << "\n";
        }
    }
    results_file_comms_window_case_study.close();

    return 0;
}