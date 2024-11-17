extern "C" {
#include "constants.h"
#include "geometry.h"
#include "sea_surface.h"
#include "asv.h"
}

#include <iostream>
#include <vector>
#include <numeric>
#include <fstream>
#include <filesystem>

void generate_wave_glider_speeds(std::string dir_results) {
    // Save data to file
    std::string file_name = dir_results + "/wave_glider_speed.csv";
    std::ofstream file(file_name); // write results to file
    file << "sig_wave_ht(m),surge_speed(knots)" << std::endl;


    // Set simulation clock
    double time_step_size = 40.0; // milli-sec
    double total_simulation_time = 15.0 * 60.0; // sec

    std::vector<double> hss = {1.0, 3.0, 5.0, 7.0, 9.0, 11.0, 13.0, 15.0};

    for (auto hs : hss) {
        // Initialise the sea surface
        double significant_wave_ht = hs;
        double wave_dp = PI/3.0; // rad
        int count_component_waves = 15;
        int wave_rand_seed = 1;
        struct Sea_surface* sea = sea_surface_new(significant_wave_ht, wave_dp, wave_rand_seed, count_component_waves);

        // Set ASV spec
        struct Asv_specification asv_spec;
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
        bool is_surge_sway_halt_set = false;

        // Init ASV-1
        double x1 = 100.0; // m
        double y1 = 100.0; // m
        double z = 0.0; // m
        union Coordinates_3D position {x1, y1, z}; // {m, m, m}
        union Coordinates_3D attitude {0.0, 0.0, 0.0}; // {rad, rad, rad}
        struct Asv* asv = asv_new(asv_spec, sea, position, attitude);
        asv_set_surge_sway_halt(asv, is_surge_sway_halt_set);

        std::vector<double> speeds;
        for(double time = 0; time < total_simulation_time; time += time_step_size/1000.0){
            // We will use a fixed rudder angle for both ASVs
            double rudder_angle = 0.0; // rad

            // Update ASV dynamics
            wave_glider_compute_dynamics(asv, rudder_angle, time_step_size);
            union Rigid_body_DOF velocity = asv_get_V(asv);
            speeds.push_back(velocity.keys.surge);
        }
        double mean_speed = std::accumulate(speeds.begin(), speeds.end(), 0.0) / speeds.size() * 3.6; // Km/hr
        file<<significant_wave_ht<<","<<mean_speed<<std::endl;
    }
}

int main(int argc, char **argv) {
    std::string dir_results = argv[1];
    // Create the results directory if it doesn't exist
    if(!std::filesystem::exists(dir_results))
    {
        std::filesystem::create_directory(dir_results);
    }

    generate_wave_glider_speeds(dir_results);
    return 0;
}