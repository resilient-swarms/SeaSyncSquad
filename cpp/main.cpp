#include "swarm.h"
#include "utils.h"
#include <utility>
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <future>
#include <filesystem>
#include <numeric>
extern "C" {
#include "constants.h"
}

std::vector<Coordinates_3D> get_fomation_hex(Geo_coordinate& deployment_region_centroid, double asv_spacing) {
    // Initialise deployment coordinates
    // Deployment pattern - hexagonal.
    //  O   O   O       <-- North row
    //    0   0   0     <-- Mid row
    //      0   0   0   <-- South row
    Geo_coordinate_converter geo_coordinate_converter(deployment_region_centroid);
    Coordinates_3D region_centroid = geo_coordinate_converter.convert(deployment_region_centroid);
    std::vector<Coordinates_3D> deployment_locations;

    // North row
    Coordinates_3D north_row_mid = {region_centroid.keys.x - asv_spacing * std::cos(PI/3),
                                    region_centroid.keys.y + asv_spacing * std::sin(PI/3),
                                    0};
    Coordinates_3D north_row_west = {north_row_mid.keys.x - asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    Coordinates_3D north_row_east = {north_row_mid.keys.x + asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    deployment_locations.push_back(north_row_west);
    deployment_locations.push_back(north_row_mid);
    deployment_locations.push_back(north_row_east);
    // Mid row
    Coordinates_3D mid_row_mid = region_centroid;
    Coordinates_3D mid_row_west = {mid_row_mid.keys.x - asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    Coordinates_3D mid_row_east = {mid_row_mid.keys.x + asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    deployment_locations.push_back(mid_row_west);
    deployment_locations.push_back(mid_row_mid);
    deployment_locations.push_back(mid_row_east);
    // South row
    Coordinates_3D south_row_mid = {region_centroid.keys.x + asv_spacing * std::cos(PI/3),
                                    region_centroid.keys.y - asv_spacing * std::sin(PI/3),
                                    0};
    Coordinates_3D south_row_west = {south_row_mid.keys.x - asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    Coordinates_3D south_row_east = {south_row_mid.keys.x + asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    deployment_locations.push_back(south_row_west);
    deployment_locations.push_back(south_row_mid);
    deployment_locations.push_back(south_row_east);

    return deployment_locations;
}


std::vector<Coordinates_3D> get_fomation_squ(Geo_coordinate& deployment_region_centroid, double asv_spacing) {
    // Initialise deployment coordinates
    // Deployment pattern - square.
    //  O   O   O   <-- North row
    //  0   0   0   <-- Mid row
    //  0   0   0   <-- South row
    Geo_coordinate_converter geo_coordinate_converter(deployment_region_centroid);
    Coordinates_3D region_centroid = geo_coordinate_converter.convert(deployment_region_centroid);
    std::vector<Coordinates_3D> deployment_locations;

    // North row
    Coordinates_3D north_row_mid = {region_centroid.keys.x,
                                    region_centroid.keys.y + asv_spacing,
                                    0};
    Coordinates_3D north_row_west = {north_row_mid.keys.x - asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    Coordinates_3D north_row_east = {north_row_mid.keys.x + asv_spacing,
                                     north_row_mid.keys.y,
                                     0};
    deployment_locations.push_back(north_row_west);
    deployment_locations.push_back(north_row_mid);
    deployment_locations.push_back(north_row_east);
    // Mid row
    Coordinates_3D mid_row_mid = region_centroid;
    Coordinates_3D mid_row_west = {mid_row_mid.keys.x - asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    Coordinates_3D mid_row_east = {mid_row_mid.keys.x + asv_spacing,
                                   mid_row_mid.keys.y,
                                   0};
    deployment_locations.push_back(mid_row_west);
    deployment_locations.push_back(mid_row_mid);
    deployment_locations.push_back(mid_row_east);
    // South row
    Coordinates_3D south_row_mid = {region_centroid.keys.x,
                                    region_centroid.keys.y - asv_spacing,
                                    0};
    Coordinates_3D south_row_west = {south_row_mid.keys.x - asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    Coordinates_3D south_row_east = {south_row_mid.keys.x + asv_spacing,
                                     south_row_mid.keys.y,
                                     0};
    deployment_locations.push_back(south_row_west);
    deployment_locations.push_back(south_row_mid);
    deployment_locations.push_back(south_row_east);

    return deployment_locations;
}


std::vector<Coordinates_3D> get_fomation_line(Geo_coordinate& deployment_region_centroid, double asv_spacing) {
    // Initialise deployment coordinates
    // Deployment pattern - line.
    //  O   O   O   0   [0]   0   0   0   0
    Geo_coordinate_converter geo_coordinate_converter(deployment_region_centroid);
    Coordinates_3D region_centroid = geo_coordinate_converter.convert(deployment_region_centroid);
    Coordinates_3D leftmost = {region_centroid.keys.x - 4 * asv_spacing, region_centroid.keys.y, 0};
    std::vector<Coordinates_3D> deployment_locations;
    for (int i = 0; i < 9 ; ++i) {
        Coordinates_3D new_position = {leftmost.keys.x + (i*asv_spacing), region_centroid.keys.y, 0};
        deployment_locations.push_back(new_position);
    }

    return deployment_locations;
}


std::vector<Coordinates_3D> get_fomation_circle(Geo_coordinate& deployment_region_centroid, double asv_spacing) {
    // Initialise deployment coordinates
    // Deployment pattern - nonagon.
    //    0  -  0
    //   /       \
    //  0         0
    //  |         |
    //  0         0
    //   \       /
    //    0     0
    //      \ /
    //       0
    Geo_coordinate_converter geo_coordinate_converter(deployment_region_centroid);
    Coordinates_3D region_centroid = geo_coordinate_converter.convert(deployment_region_centroid);
    std::vector<Coordinates_3D> deployment_locations;
    for (int i = 0; i < 9 ; ++i) {
        double angle = i*(2.0*PI)/9;
        double circumradius = asv_spacing / (2 * std::sin(PI / 9));
        double x = region_centroid.keys.x + (circumradius * std::cos(angle));
        double y = region_centroid.keys.y + (circumradius * std::sin(angle));
        Coordinates_3D new_position = {x, y, 0};
        deployment_locations.push_back(new_position);
    }

    return deployment_locations;
}


int main() {
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    std::filesystem::path results_dir = root_dir/"results_swarm_comms_cpp";
    if (!std::filesystem::exists(results_dir)) {
        std::filesystem::create_directory(results_dir);
    }
    else {
        std::cout << results_dir << " already exists." << std::endl;
        return 1;
    }

    // Define a function point to the different swarm init functions
    using GetFormation = std::vector<Coordinates_3D> (*)(Geo_coordinate&, double);

    std::vector<std::future<void>> futures; // List of threads
    // Generate all scenarios to run
    std::vector<double> distances;
    std::vector<double> significant_wave_hts;
    std::vector<double> wind_speeds;
    std::vector<double> rainfall_rates;
    std::vector<double> antenna_hts;
    std::vector<double> radio_frequencies;
    std::vector<GetFormation> formation_getters;
    std::vector<std::pair<double, double>> freq_dist_pairs = {{450.0, 10'000.0}, {868.0, 5'000.0}, {2400.0, 1'000.0}, {5200.0, 500.0}}; // {MHz, m}
    for(auto freq_dist_pair : freq_dist_pairs) {
        for(double significant_wave_ht : {1.0, 3.0, 5.0, 7.0, 10.0, 12.0}) { // m
            for(double wind_speed : {10.0}) { // km/hr
                for(double rainfall_rate : {10.0}) { // mm/hr
                    for(double antenna_ht : {1.0, 2.0, 3.0}) { // m
                        for (GetFormation get_formation : {get_fomation_hex, get_fomation_squ, get_fomation_line, get_fomation_circle}) {
                            double frequency = freq_dist_pair.first; // MHz
                            double distance = freq_dist_pair.second; // m
                            distances.push_back(distance);
                            radio_frequencies.push_back(frequency);
                            significant_wave_hts.push_back(significant_wave_ht);
                            wind_speeds.push_back(wind_speed);
                            rainfall_rates.push_back(rainfall_rate);
                            antenna_hts.push_back(antenna_ht);
                            formation_getters.push_back(get_formation);
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
        for(int i = jobs_start_index; i < jobs_end_index; ++i) {
            std::string print_swarm_init_function_name;
            if (formation_getters[i] == get_fomation_hex) {
                print_swarm_init_function_name = "get_fomation_hex";
            } else if (formation_getters[i] == get_fomation_squ) {
                print_swarm_init_function_name = "get_fomation_squ";
            } else if (formation_getters[i] == get_fomation_line) {
                print_swarm_init_function_name = "get_fomation_line";
            } else if (formation_getters[i] == get_fomation_circle) {
                print_swarm_init_function_name = "get_fomation_circle";
            }
            std::cout<<"["<<i+1<<" / "  << count_total_scenarios<<"]";
            std::cout<<" distance = "   << distances[i];
            std::cout<<" wave_ht = "    << significant_wave_hts[i];
            std::cout<<"  wind_speed = "<< wind_speeds[i];
            std::cout<<" rainfall = "   << rainfall_rates[i];
            std::cout<<" ant_ht = "     << antenna_hts[i];
            std::cout<<" freq = "       << radio_frequencies[i];
            std::cout<<" function = "   << print_swarm_init_function_name<<std::endl;
            double ant_ht = antenna_hts[i];
            double frequency = radio_frequencies[i] * 1'000'000; // Hz
            double distance = distances[i]; // m
            double wind_speed = wind_speeds[i]/3.6; // m/s
            double wave_ht = significant_wave_hts[i]; // m
            double rainfall_rate = rainfall_rates[i]; // mm/hr
            GetFormation get_formation = formation_getters[i];

            auto thread_worker_function = [](double ant_ht, double frequency, double distance, double wind_speed, double wave_ht, double rainfall_rate, GetFormation get_formation, std::filesystem::path results_dir) {
                // Formation name
                std::string formation_name;
                if (get_formation == get_fomation_hex) {
                    formation_name = "hex";
                } else if (get_formation == get_fomation_squ) {
                    formation_name = "squ";
                } else if (get_formation == get_fomation_line) {
                    formation_name = "line";
                } else if (get_formation == get_fomation_circle) {
                    formation_name = "circle";
                }
                // Construct scenario name
                std::ostringstream oss_ant_ht; // Create a string stream to format the double
                std::ostringstream oss_wave_ht; // Create a string stream to format the double
                oss_ant_ht << std::fixed << std::setprecision(2) << ant_ht;
                oss_wave_ht << std::fixed << std::setprecision(1) << wave_ht;
                std::string scenario_name = std::to_string(int(distance)) + "_" +
                                            oss_wave_ht.str() + "_" +
                                            std::to_string(int(wind_speed*3.6)) + "_" +
                                            std::to_string(int(rainfall_rate)) + "_" +
                                            oss_ant_ht.str() + "_" +
                                            std::to_string(int(frequency/1'000'000)) + "_" +
                                            formation_name;
                // Set the centroid of the deployment
                Geo_coordinate deployment_region_centroid {.longitude = -83.5, .latitude = 19.0}; // Deployment coordinates
                // Get deployment locations
                std::vector<Coordinates_3D> deployment_locations = get_formation(deployment_region_centroid, distance);
                // Write deployment locations to file
                std::filesystem::path deployment_file_path = results_dir/("deployment_" + scenario_name + ".csv");
                std::ofstream deployment_file(deployment_file_path); // Write deployment config to file
                deployment_file << "x(m)" << "," << "y(m)" << std::endl;
                for (auto& deployment : deployment_locations) {
                    deployment_file << deployment.keys.x << "," << deployment.keys.y << std::endl;
                }
                deployment_file.close();
                // Initialise the sea surface
                double wave_dp = PI/3.0; // rad
                int count_component_waves = 15;
                int wave_rand_seed = 1;
                Sea_surface* sea_surface = sea_surface_new(wave_ht, wave_dp, wave_rand_seed, count_component_waves);
                // Set ASV spec
                Asv_specification asv_spec;
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
                bool is_surge_sway_halt_set = true;
                // Init ASVs
                std::vector<Asv*> asvs;
                union Coordinates_3D attitude {0.0, 0.0, 0.0}; // {rad, rad, rad}
                for(auto& deployment_location : deployment_locations) {
                    Asv* asv = asv_new(asv_spec, sea_surface, deployment_location, attitude);
                    asv_set_surge_sway_halt(asv, is_surge_sway_halt_set);
                    asvs.push_back(asv);
                }
                // Init swarm
                Swarm swarm(asvs, sea_surface, distance, ant_ht, frequency, rainfall_rate, wind_speed, results_dir, scenario_name);
                // Create ASV pairs
                for (int i = 0; i < asvs.size(); ++i) {
                    for (int j = i+1; j < asvs.size(); ++j) {
                        std::pair<Asv*, Asv*> comms_pair;
                        comms_pair.first  = asvs[i];
                        comms_pair.second = asvs[j];
                        swarm.set_comms_link(comms_pair);
                    }
                }
                // Run simulation
                swarm.run_simulation();
                // Clean heap
                for (Asv* asv : swarm.get_asvs()) {
                    asv_delete(asv);
                }
                sea_surface_delete(sea_surface);
            };

            std::thread* thread_object = new std::thread(thread_worker_function, ant_ht, frequency, distance, wind_speed, wave_ht, rainfall_rate, get_formation, results_dir);
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

    return 0;
}