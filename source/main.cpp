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
    std::vector<double> free_space_path_losses;
    std::vector<double> diffraction_losses;
    std::vector<double> rain_attenuations;
    std::vector<double> sea_spray_attenuations;
    std::vector<double> total_losses;
    // Calculate attenuation
    for(double distance : distances) {
        for(double rainfall_rate : rainfall_rates) {
            for(double wind_speed : wind_speeds) {
                for(double significant_wave_ht : significant_wave_hts) {
                    double f = radio_frequency * 1000000000.0; // Hz
                    double d = distance * 1000.0; // m
                    double s = wind_speed/3.6; // m/s
                    RadioAttenuation radio_attenuation(f, polarisation, d, significant_wave_ht, s, rainfall_rate);
                    free_space_path_losses.push_back(radio_attenuation.free_space_path_loss);
                    diffraction_losses.push_back(radio_attenuation.diffraction_loss);
                    rain_attenuations.push_back(radio_attenuation.rain_attenuation);
                    sea_spray_attenuations.push_back(radio_attenuation.sea_spray_attenuation);
                    total_losses.push_back(radio_attenuation.total_attenuation);
                }
            }
        }
    }
    // Write to file
    std::ofstream file("./temp.csv");
    file << "free_space_path_loss(dB),diffraction_loss(dB),rain_attenuation(dB),sea_spray_attenuation(dB),total_loss(dB)" << std::endl;
    for(int i = 0; i < free_space_path_losses.size(); i++) {
        file << free_space_path_losses[i] << "," << diffraction_losses[i] << "," << rain_attenuations[i] << "," << sea_spray_attenuations[i] << "," << total_losses[i] << std::endl;
    }
}