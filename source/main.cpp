#include <iostream>
#include <string>
#include <thread>
#include <cmath>
#include <algorithm>
#include <fmt/core.h>
#include "simulate_LOS.h"

extern "C" 
{
    #include "constants.h"
    #include "geometry.h"
    #include "sea_surface.h"
    #include "asv.h"
}

int run(struct Asv_specification asv_spec, bool is_surge_sway_halt_set)
{
    // Initialise the sea surface
    double distance = 540.0; // m
    double wave_hs = 9.0; // m
    double wave_dp = PI/3; // rad
    int count_component_waves = 1; 
    int wave_rand_seed = 19;
    struct Sea_surface* sea = sea_surface_new(wave_hs, wave_dp, wave_rand_seed, count_component_waves);

    double antenna_height = 5.0; // m
    
    // Init ASV-1
    double x1 = 100.0;
    union Coordinates_3D position1 {x1, 100.0, 0.0}; // {m, m, m}
    union Coordinates_3D attitude1 {0.0, 0.0, 0.0}; // {rad, rad, rad}
    struct Asv* asv1 = asv_new(asv_spec, sea, position1, attitude1);
    asv_set_surge_sway_halt(asv1, is_surge_sway_halt_set);

    // Init ASV-2
    double x2 = 100 + distance;
    union Coordinates_3D position2 {x2, 100.0, 0.0}; // {m, m, m}
    union Coordinates_3D attitude2 {0.0, 0.0, 0.0}; // {rad, rad, rad}
    struct Asv* asv2 = asv_new(asv_spec, sea, position2, attitude2);
    asv_set_surge_sway_halt(asv2, is_surge_sway_halt_set);

    // Initialise and run the simulation
    Simulate_LOS simulation = Simulate_LOS(sea, asv1, asv2, antenna_height);
    simulation.run();
}

int run_headless(struct Asv_specification asv_spec, bool is_surge_sway_halt_set)
{
    // Lambda function to run the simulation in threads
    auto run_in_thread = [&](double distance, double antenna_height, double wave_hs, double wave_dp, int wave_rand_seed)
    {
        std::ofstream fout;
        std::string file_name = "../results/log_";
        file_name += std::to_string((int)distance) + "_" +
                     std::to_string((int)antenna_height) + "_" +
                     std::to_string((int)wave_hs) + "_" +
                     std::to_string((int)round(wave_dp*180.0/PI)) + "_" +
                     std::to_string(wave_rand_seed) + "_.csv";
        fout.open(file_name);
        fout << "dist(m),ant_ht(m),hs(m),wave_dp(rad),peak_wave_len(m),seed,time(sec),clearence(%)\n";

        // Init sea surface
        int count_component_waves = 21; 
        struct Sea_surface* sea = sea_surface_new(wave_hs, wave_dp, wave_rand_seed, count_component_waves);
    
        // Init ASV-1
        double x1 = 100.0;
        union Coordinates_3D position1 {x1, 100.0, 0.0}; // {m, m, m}
        union Coordinates_3D attitude1 {0.0, 0.0, 0.0}; // {rad, rad, rad}
        struct Asv* asv1 = asv_new(asv_spec, sea, position1, attitude1);
        asv_set_surge_sway_halt(asv1, is_surge_sway_halt_set);

        // Init ASV-2
        double x2 = 100 + distance;
        union Coordinates_3D position2 {x2, 100.0, 0.0}; // {m, m, m}
        union Coordinates_3D attitude2 {0.0, 0.0, 0.0}; // {rad, rad, rad}
        struct Asv* asv2 = asv_new(asv_spec, sea, position2, attitude2);
        asv_set_surge_sway_halt(asv2, is_surge_sway_halt_set);

        // Init and run simulation
        Simulate_LOS simulation = Simulate_LOS(sea, asv1, asv2, antenna_height);
        simulation.run_headless(fout);

        // Clear memory
        sea_surface_delete(sea);
        asv_delete(asv1);
        asv_delete(asv2);
        fout.close();
    };

    // Define the simulations to run
    std::vector<double> distances;
    std::vector<double> antenna_heights;
    std::vector<double> wave_hss;
    std::vector<double> wave_dps;
    std::vector<int> wave_rand_seeds;
    for(double distance = 100; distance <= 600; distance+=10)
    {
        for(double antenna_height = 1.0; antenna_height <= 5.0; antenna_height+=1.0)
        {
            for(double wave_hs = 1.0; wave_hs <= 9.0; wave_hs+=1.0)
            {
                for(double wave_dp = 0.0; wave_dp <= PI/2; wave_dp+=PI/6.0)
                {
                    for(int wave_rand_seed = 1; wave_rand_seed <= 5 ; wave_rand_seed++)
                    {
                        distances.push_back(distance);
                        antenna_heights.push_back(antenna_height);
                        wave_hss.push_back(wave_hs);
                        wave_dps.push_back(wave_dp);
                        wave_rand_seeds.push_back(wave_rand_seed);
                    }
                }
            }
        }
    }

    // Spawn the threads and run simulations.
    int count_total_threads = std::thread::hardware_concurrency();
    std::cout<<"Hardware concurrency = "<<count_total_threads<<"\n";
    int count_total_simulations = distances.size();
    int count_remaining_simulations = count_total_simulations;
    int i = 0;
    while(i < count_total_simulations)
    {
        std::vector<std::thread*> threads;
        for(int j = 0; j < std::min(count_total_threads, count_remaining_simulations); ++j, ++i, --count_remaining_simulations)
        {
            std::cout<<"Distance = "<<distances[i]<<", antenna height = "<<antenna_heights[i]<<", hs = "<<wave_hss[i]<<", dp = "<<wave_dps[i]*180/PI<<", rand seed = "<<wave_rand_seeds[i]<<"\n";
            std::thread* thread_object = new std::thread(run_in_thread, distances[i], antenna_heights[i], wave_hss[i], wave_dps[i], wave_rand_seeds[i]);
            threads.push_back(thread_object);
        }
        for(auto thread : threads)
        {
            thread->join();
        }
        for(auto thread : threads)
        {
            delete thread;
        } 
        threads.clear();
        std::cout<<"\n";
    } 
}


int main()
{
    // ASV spec
    struct Asv_specification asv_spec {
        2.1 , // L_wl (m)
        0.6,  // B_wl (m)
        0.25, // D (m)
        0.09, // T (m)
        4.0,  // max_speed (m/s)
        0.09, // disp (m3)
        0.2,  // r_roll (m)
        0.6,  // r_pitch (m)
        0.6,  // r_yaw (m)
        {1.05, 0.0, -0.2} // cog {m, m, m} 
    };
    bool is_surge_sway_halt_set = true;

    run_headless(asv_spec, is_surge_sway_halt_set);
}
