#include "ASVLite/sea_surface.h"
#include "radio_attenuation.h"
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>

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

    std::filesystem::path results_file_path = results_dir / ("results.csv");
    std::ofstream results_file(results_file_path);
    results_file << "freq(MHz),ant_ht(m),wave_ht(m),dist(m),fspl(db),diff_loss(db),total_loss(db)\n";

    std::vector<double> freqs {868.0, 2400.0, 5200.0}; // MHz
    for(double freq : freqs) {
        freq = freq * 1'000'000.0; // Hz
        for(double ant_ht = 0.5; ant_ht <= 3.0; ant_ht += 0.5) { // m
            for(double wave_ht = 0.5; wave_ht <= 10.0; wave_ht += 0.5) { // m
                for(double distance = 100.0; distance <= 5000.0; distance += 100.0) { // m
                    std::cout << "freq = " << freq/ 1'000'000.0 << " ant_ht = " << ant_ht << " wave_ht = " << wave_ht << " distance = " << distance << "\n";
                    const double wave_heading = 0.0; // rad North
                    const int rng_seed = 1;
                    const ASVLite::SeaSurface<N> sea_surface {wave_ht, wave_heading, rng_seed};
                    const double fspl = get_free_space_path_loss(freq, distance);
                    const double time_duration = 2.0 * 60.0; // sec 
                    const double diff_loss = get_diffraction_loss(freq, ant_ht, distance, sea_surface, time_duration); 
                    const double total_loss = fspl + diff_loss;
                    results_file << freq/ 1'000'000.0 << "," << ant_ht << "," << wave_ht << "," << distance << "," << fspl << "," << diff_loss << "," << total_loss << "\n";
                }
            }
        }
    }

    return 0;
}