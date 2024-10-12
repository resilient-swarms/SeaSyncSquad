#include "radio_attenuation.h"
#include <array>
#include <vector>
#include <iostream>
#include <fstream>

int main() {
    double radio_frequency = 5.2; // Ghz
    Polarisation polarisation = Polarisation::horizontal;
    std::array<double, 5> distances = {10, 50, 100, 150, 200}; // km
    std::array<double, 5> rainfall_rates = {5, 25, 50, 100, 150}; // mm/hr
    std::array<double, 7> wind_speeds = {50, 100, 150, 200, 250, 300, 350}; // km/hr
    std::array<double, 5> significant_wave_hts = {3.0, 5.0, 7.5, 10, 12}; // m
    std::vector<double> df_distances;
    std::vector<double> df_rainfall_rates;
    std::vector<double> df_wind_speeds;
    std::vector<double> df_significant_wave_hts;
    std::vector<double> df_free_space_path_losses;
    std::vector<double> df_diffraction_losses;
    std::vector<double> df_rain_attenuations;
    std::vector<double> df_sea_spray_attenuations;
    std::vector<double> df_total_losses;
    // Calculate attenuation
    for(double distance : distances) {
        for(double rainfall_rate : rainfall_rates) {
            for(double wind_speed : wind_speeds) {
                for(double significant_wave_ht : significant_wave_hts) {
                    std::cout << distance << ", " << rainfall_rate << ", " << wind_speed << ", " << significant_wave_ht << std::endl;
                    double f = radio_frequency * 1000000000.0; // Hz
                    double d = distance * 1000.0; // m
                    double s = wind_speed/3.6; // m/s
                    RadioAttenuation radio_attenuation(f, polarisation, d, significant_wave_ht, s, rainfall_rate);
                    df_distances.push_back(distance);
                    df_rainfall_rates.push_back(rainfall_rate);
                    df_wind_speeds.push_back(wind_speed);
                    df_significant_wave_hts.push_back(significant_wave_ht);
                    df_free_space_path_losses.push_back(radio_attenuation.free_space_path_loss);
                    df_diffraction_losses.push_back(radio_attenuation.diffraction_loss);
                    df_rain_attenuations.push_back(radio_attenuation.rain_attenuation);
                    df_sea_spray_attenuations.push_back(radio_attenuation.sea_spray_attenuation);
                    df_total_losses.push_back(radio_attenuation.total_attenuation);
                }
            }
        }
    }
    // Write to file
    std::ofstream file("./temp.csv");
    file << "distance(km),rainfall(mm/hr),wind_speed(km/hr),sig_wave_ht(m),free_space_path_loss(dB),diffraction_loss(dB),rain_attenuation(dB),sea_spray_attenuation(dB),total_loss(dB)" << std::endl;
    for(int i = 0; i < df_distances.size(); i++) {
        file << df_distances[i] << ","
           << df_rainfall_rates[i] << ","
           << df_wind_speeds[i] << ","
           << df_significant_wave_hts[i] << ","
           << df_free_space_path_losses[i] << ","
           << df_diffraction_losses[i] << ","
           << df_rain_attenuations[i] << ","
           << df_sea_spray_attenuations[i] << ","
           << df_total_losses[i] << std::endl;
    }
}