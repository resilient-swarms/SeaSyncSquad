#include "ASVLite/sea_surface.h"
#include "radio_attenuation.h"
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <future> 
#include <map>

// --- Data Structures and Model Data Store ---

// A structure to hold a single data point for comms window vs. distance
struct DistanceCommData {
    double distance_m;
    double comms_window_pct;
};

// The main data store, declared as a file-scope static variable.
static std::map<double, std::vector<DistanceCommData>> model_data_;


// --- Forward Declarations ---

double get_distance_for_wave_height(double wave_height, double required_comms_window);
double get_comms_for_wave_height(double wave_height, double current_distance);


// --- Public API Functions ---

/**
 * @brief Parses the communication model CSV file and populates the global model_data_ map.
 * @param csv_path The full path to the comms_window_case_study.csv file.
 */
void load_comm_model_data(const std::string& csv_path) {
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open CSV file: " + csv_path);
    }
    model_data_.clear(); 
    std::string line;
    std::getline(file, line); // Discard header
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::getline(ss, cell, ','); // freq
        std::getline(ss, cell, ','); // ant_ht
        double distance = std::stod((std::getline(ss, cell, ','), cell));
        double wave_ht = std::stod((std::getline(ss, cell, ','), cell));
        double comms_window = std::stod((std::getline(ss, cell, ','), cell));
        model_data_[wave_ht].push_back({distance, comms_window});
    }
    for (auto& pair : model_data_) {
        std::sort(pair.second.begin(), pair.second.end(), [](const auto& a, const auto& b) {
            return a.distance_m < b.distance_m;
        });
    }
    std::cout << "Communication model loaded successfully with " << model_data_.size() << " unique wave height entries.\n";
}

/**
 * @brief Calculates the maximum allowable inter-vehicle distance that can be interpolated
 * from the loaded data to meet a minimum communication window requirement.
 * This function performs a two-dimensional linear interpolation.
 * @param current_wave_height The current significant wave height in meters.
 * @param required_comms_window The minimum acceptable communication window (e.g., 85.0 for 85%).
 * @return The maximum allowable inter-vehicle distance in meters. Returns edge values if inputs are outside the data range.
 */
double get_max_allowable_distance(double current_wave_height, double required_comms_window) {
    if (model_data_.empty()) throw std::runtime_error("Model data is empty. Call load_comm_model_data() first.");
    
    // Find the wave height entries in our table that bracket the current wave height
    auto it_upper = model_data_.lower_bound(current_wave_height);
    
    // Handle edge cases: if outside the wave height range, clamp to the nearest data.
    if (it_upper == model_data_.end()) return get_distance_for_wave_height(model_data_.rbegin()->first, required_comms_window);
    if (it_upper == model_data_.begin()) return get_distance_for_wave_height(it_upper->first, required_comms_window);
    
    auto it_lower = std::prev(it_upper);
    double lower_wave_ht = it_lower->first;
    double upper_wave_ht = it_upper->first;
    
    // For each of the two bracketing wave heights, find the max distance for the required comms window.
    double dist_lower = get_distance_for_wave_height(lower_wave_ht, required_comms_window);
    double dist_upper = get_distance_for_wave_height(upper_wave_ht, required_comms_window);
    
    // If the wave height is an exact match, no final interpolation needed.
    if (std::abs(upper_wave_ht - lower_wave_ht) < 1e-6) return dist_lower;
    
    // Perform the final interpolation across the wave height dimension.
    double factor = (current_wave_height - lower_wave_ht) / (upper_wave_ht - lower_wave_ht);
    return dist_lower + factor * (dist_upper - dist_lower);
}

/**
 * @brief Predicts the communication window for a given wave height and distance using 2D interpolation.
 * @return The predicted communication window percentage.
 */
double get_predicted_comms_window(double current_wave_height, double current_distance) {
    if (model_data_.empty()) throw std::runtime_error("Model data is empty. Call load_comm_model_data() first.");
    auto it_upper = model_data_.lower_bound(current_wave_height);
    if (it_upper == model_data_.end()) return get_comms_for_wave_height(model_data_.rbegin()->first, current_distance);
    if (it_upper == model_data_.begin()) return get_comms_for_wave_height(it_upper->first, current_distance);
    auto it_lower = std::prev(it_upper);
    double lower_wave_ht = it_lower->first;
    double upper_wave_ht = it_upper->first;
    double comms_lower = get_comms_for_wave_height(lower_wave_ht, current_distance);
    double comms_upper = get_comms_for_wave_height(upper_wave_ht, current_distance);
    if (std::abs(upper_wave_ht - lower_wave_ht) < 1e-6) return comms_lower;
    double factor = (current_wave_height - lower_wave_ht) / (upper_wave_ht - lower_wave_ht);
    return comms_lower + factor * (comms_upper - comms_lower);
}


// --- Helper Function Implementations ---

/**
 * @brief (Helper) For a single, specific wave height from the table, finds the maximum distance
 * that provides the required comms window, using linear interpolation on the distance axis.
 */
double get_distance_for_wave_height(double wave_height, double required_comms_window) {
    const auto& data_points = model_data_.at(wave_height);
    
    // Find the first data point where the comms window is less than or equal to the required one.
    // Since comms window decreases as distance increases, this gives us the upper-bound point for interpolation.
    auto it_upper = std::lower_bound(data_points.begin(), data_points.end(), required_comms_window,
        [](const DistanceCommData& point, double val) { return point.comms_window_pct > val; });
    
    // Handle edge cases: If the required comms is outside the table's range for this wave height,
    // return the maximum or minimum distance available in the table.
    if (it_upper == data_points.end()) return data_points.back().distance_m; // Required comms is better than we can do, so return max distance
    if (it_upper == data_points.begin()) return data_points.front().distance_m; // Cannot meet requirement, return min distance

    auto it_lower = std::prev(it_upper);
    const auto& p1 = *it_lower; // Point with higher comms, lower distance
    const auto& p2 = *it_upper; // Point with lower comms, higher distance
    
    // Avoid division by zero if comms values are identical
    if (std::abs(p1.comms_window_pct - p2.comms_window_pct) < 1e-6) return p1.distance_m;
    
    // Linearly interpolate to find the distance that exactly matches the required comms window
    double factor = (required_comms_window - p2.comms_window_pct) / (p1.comms_window_pct - p2.comms_window_pct);
    return p2.distance_m + factor * (p1.distance_m - p2.distance_m);
}

/**
 * @brief (Helper) For a single wave height, finds the comms window for a given distance.
 */
double get_comms_for_wave_height(double wave_height, double current_distance) {
    const auto& data_points = model_data_.at(wave_height);
    auto it_upper = std::lower_bound(data_points.begin(), data_points.end(), current_distance,
        [](const DistanceCommData& point, double val) { return point.distance_m < val; });
    if (it_upper == data_points.end()) return data_points.back().comms_window_pct;
    if (it_upper == data_points.begin()) return data_points.front().comms_window_pct;
    auto it_lower = std::prev(it_upper);
    const auto& p1 = *it_lower;
    const auto& p2 = *it_upper;
    if (std::abs(p1.distance_m - p2.distance_m) < 1e-6) return p1.comms_window_pct;
    double factor = (current_distance - p1.distance_m) / (p2.distance_m - p1.distance_m);
    return p1.comms_window_pct + factor * (p2.comms_window_pct - p1.comms_window_pct);
}


// --- Main Function to Run the Requested Scenario ---

void run_adaptive_distance_scenario(std::string output_filename) {
    // --- Scenario Parameters ---
    const double initial_distance = 1000.0; // m
    const double required_comms_window = 90.0; // %

    // --- File Output Setup ---
    std::ofstream results_file(output_filename);
    if (!results_file.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_filename << std::endl;
        return;
    }
    std::cout << "\n--- Running Adaptive Distance Scenario ---\n"
              << "Required Comms Window: " << required_comms_window << "%\n"
              << "Results will be written to: " << output_filename << "\n";

    // --- Write Header to File ---
    results_file << "Wave_Ht_m,Comms_at_1000m_Predicted_Pct,Max_Allowable_Dist_m,Comms_at_New_Dist_Actual_Pct\n";

    // --- Loop 1: Increasing Wave Height ---
    for (double wave_ht = 2.0; wave_ht <= 6.0; wave_ht += 0.5) {
        // Predict comms at the original fixed distance
        double comms_at_1000m = get_predicted_comms_window(wave_ht, initial_distance);

        // Get the new maximum allowable distance to maintain the required comms window
        double new_distance = get_max_allowable_distance(wave_ht, required_comms_window);

        // Get the actual comms window at that new distance (should be near the required %)
        double comms_at_new_dist = get_predicted_comms_window(wave_ht, new_distance);

        // Write results to the CSV file
        results_file << std::fixed << std::setprecision(2)
                     << wave_ht << ","
                     << comms_at_1000m << ","
                     << new_distance << ","
                     << comms_at_new_dist << "\n";
    }

    // --- Loop 2: Decreasing Wave Height ---
    for (double wave_ht = 5.5; wave_ht >= 2.0; wave_ht -= 0.5) {
        double comms_at_1000m = get_predicted_comms_window(wave_ht, initial_distance);
        double new_distance = get_max_allowable_distance(wave_ht, required_comms_window);
        double comms_at_new_dist = get_predicted_comms_window(wave_ht, new_distance);
        
        // Write results to the CSV file
        results_file << std::fixed << std::setprecision(2)
                     << wave_ht << ","
                     << comms_at_1000m << ","
                     << new_distance << ","
                     << comms_at_new_dist << "\n";
    }
    
    results_file.close();
    std::cout << "--- Scenario complete. ---\n";
}


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
            results_file_comms_window_case_study << freq/ 1'000'000.0 << "," << ant_ht << "," << distance << "," << wave_ht << "," << comms_window << "\n";
        }
    }
    results_file_comms_window_case_study.close();


    std::filesystem::path results_file_path_comms_window_case_study_scenario = results_dir / ("comms_window_case_study_scenario.csv");      
    // Load the model data once
    load_comm_model_data(results_file_path_comms_window_case_study);
    // Run the specific scenario you requested
    run_adaptive_distance_scenario(results_file_path_comms_window_case_study_scenario);

    return 0;
}