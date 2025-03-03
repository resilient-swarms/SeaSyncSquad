#include "ASVLite/sea_surface.h"
#include "radio_attenuation.h"
#include <iostream>

int main(){

    const double freq = 868.0 * 1'000'000.0; // Hz
    for(double wave_ht = 0.5; wave_ht <= 10.5; wave_ht += 1.0) { // m
        const double wave_heading = 0.0; // rad North
        const int rng_seed = 1;
        const int num_component_waves = 17;
        const ASVLite::SeaSurface sea_surface {wave_ht, wave_heading, rng_seed, num_component_waves};
        for(double ant_ht = 0.5; ant_ht <= 3.0; ant_ht += 0.5) { // m
            for(double distance = 200.0; distance <= 2000.0; distance += 200.0) { // m
                std::cout << "wave_ht = " << wave_ht << " ant_ht = " << ant_ht << " distance = " << distance << "\n";
                const double fspl = get_free_space_path_loss(freq, distance);
                const double time_duration = 2.0 * 60.0; // sec 
                const double diff_loss = get_diffraction_loss(freq, ant_ht, distance, sea_surface, time_duration); 
                const double total_loss = fspl + diff_loss;
                std::cout << "fspl = " << fspl << " diff loss = " << diff_loss << " total loss = " << total_loss << "\n";
            }
        }
    }

    return 0;
}