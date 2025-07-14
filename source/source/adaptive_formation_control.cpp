#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>
#include <Eigen/Dense>
#include "matplotlibcpp.h"
#include "template_param.h"
#include "ASVLite/geometry.h"
#include "ASVLite/sea_surface.h"
#include "ASVLite/asv.h"
#include "ASVLite/rudder_controller.h"
#include "radio_attenuation.h"
#include <future> 
#include <utility>

namespace plt = matplotlibcpp;

bool run_animation = false;


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



int main() {
    // Load the model data once
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
    std::filesystem::path results_file_path_comms_window_case_study = results_dir / ("comms_window_case_study.csv");
    load_comm_model_data(results_file_path_comms_window_case_study);

    // Formation parameters
    const size_t swarm_size             = 6;
    const double proximity_threashold   = 10.0;   // Proximity to circle
    const double ref_formation_radius   = 1000.0;  // m
    double formation_radius             = ref_formation_radius;
    const double field_length           = 2500.0; // m
    const double formation_centre_x     = 0.0;    // m
    const double formation_centre_y     = 0.0;    // m
    const double threshold              = 20.0; // dB
    const double time_duration          = 30 * 60.0; // sec 
    ASVLite::Geometry::Coordinates3D formation_center {formation_centre_x, formation_centre_y, 0.0};
    const double k_circle    = 1.0; // N/m
    const double k_neighbour = 1.0; // N/m

    // Comms parameters
    const double required_comms_window = 70.0; // %

    // Random number generator
    const int rng_seed = 1;  
    std::mt19937 gen(rng_seed);
    std::uniform_real_distribution<double> distribution(-field_length/2.0, field_length/2.0);  

    // Functions to calculate the artificial force
    auto get_F_circle = [&](ASVLite::Geometry::Coordinates3D position) {
        Eigen::Vector2d p0 {formation_center.keys.x, formation_center.keys.y}; 
        Eigen::Vector2d p1 {position.keys.x, position.keys.y}; 
        Eigen::Vector2d p01 = p0 - p1; 
        Eigen::Vector<double, 2> F = -k_circle * (formation_radius - p01.norm())/p01.norm() * p01; // Atrrection to circle
        return F;
    };

    auto get_F_neighbour = [&](ASVLite::Geometry::Coordinates3D position_1, ASVLite::Geometry::Coordinates3D position_2) {
        Eigen::Vector2d p1 {position_1.keys.x, position_1.keys.y}; 
        Eigen::Vector2d p2 {position_2.keys.x, position_2.keys.y}; 
        Eigen::Vector2d p21 = (p2 - p1); 
        Eigen::Vector2d F = Eigen::Vector2d::Zero();
        if(p21.norm() <= formation_radius) {
            F = -k_neighbour * (formation_radius - p21.norm())/p21.norm() * p21; // Repulsion from neighbours
        }
        return F;
    };

    // Initial sea surface
    const double wave_ht = 2.0; // m
    const double wave_dp = M_PI/3.0; // rad
    const int wave_rand_seed = 1;
    const ASVLite::SeaSurface<N> sea_surface {wave_ht, wave_dp, wave_rand_seed};

    // ASV specifications
    ASVLite::AsvSpecification asv_spec {
        .L_wl = 2.1, // m
        .B_wl = 0.6, // m
        .D = 0.25,   // m
        .T = 0.15,   // m
    };

    // Init rudder controller
    ASVLite::RudderController rudder_controller {asv_spec, {4.75, 0.0, 4.75}};

    // Initialise and deploy swarm
    const double antenna_height = 0.5; // m
    const double radio_frequency = 2.4e9; // Hz
    std::vector<ASVLite::Asv<N>> swarm;
    for(size_t i = 0; i < swarm_size; ++i) {
        double angle = i*(2.0*M_PI)/swarm_size;
        double x = formation_center.keys.x + (formation_radius * std::cos(angle + M_PI/6));
        double y = formation_center.keys.y + (formation_radius * std::sin(angle + M_PI/6));
        const double heading = std::atan2(x, y); 
        const double z = 0.0;
        const ASVLite::Geometry::Coordinates3D position {x, y, 0};
        const ASVLite::Geometry::Coordinates3D attitude {0, 0, heading};
        ASVLite::Asv<N> asv {asv_spec, &sea_surface, position, attitude};
        swarm.push_back(asv);
    }

    // Record results
    std::filesystem::path results_file_path_swarm_adaptation_sim = results_dir / ("swarm_adaptation_sim.csv");
    std::ofstream results_file_swarm_adaptation_sim(results_file_path_swarm_adaptation_sim);
    results_file_swarm_adaptation_sim << "wave_ht(m),dist(m),comms_window(%),ref_comms_window(%)\n";

    // Run simulation
    std::vector<std::vector<double>> x_paths; // x coordinate of the paths traced by the vehicles in the swarm
    std::vector<std::vector<double>> y_paths; // y coordinate of the paths traced by the vehicles in the swarm
    x_paths.resize(swarm_size);
    y_paths.resize(swarm_size);
    int animation_update_frequency = 1.0 * 60 * (1000 / 40); // Update every minutes
    int sea_state_update_frequency = 30.0  * 60 * (1000 / 40); // Update every 30 minutes
    int comms_update_frequency = 5 * 60 * (1000 / 40); // Update every 5 minutes
    std::string plot_file_name_subscript = std::to_string(1);
    std::vector<double> wave_hts {2.0, 3.0, 4.0, 5.0, 6.0, 5.0, 4.0, 3.0, 2.0}; 
    double time = 0;
    for(const double wave_ht : wave_hts) {
        if(wave_ht == 6.0) {
            plot_file_name_subscript = std::to_string(2);
        }
        // Update sea surface
        const ASVLite::SeaSurface<N> sea_surface_updated {wave_ht, wave_dp, wave_rand_seed};
        for(size_t i = 0; i < swarm_size; ++i) {
            swarm[i].set_sea_state(&sea_surface_updated);
        }
        // Update formation_radius for the sea state
        // Get the new maximum allowable distance to maintain the required comms window
        // double new_distance = get_max_allowable_distance(std::min(wave_ht+1.0, 6.0), required_comms_window);
        double new_distance = get_max_allowable_distance(wave_ht, required_comms_window);
        formation_radius = new_distance;

        for(int counter = 0; counter  < sea_state_update_frequency; ++counter) {
            // Compute the net artificial force
            Eigen::Matrix<double, swarm_size, 2> F_circle    = Eigen::Matrix<double, swarm_size, 2>::Zero(); // Artifical attractive force to circle
            Eigen::Matrix<double, swarm_size, 2> F_neighbour = Eigen::Matrix<double, swarm_size, 2>::Zero(); // Net artificial force due to neighbours that are within the radio range
            // For each vehicle in the swarm, compute the net force
            for(size_t i = 0; i < swarm_size; ++i) {
                // Compute the attractive force to the circumference of the circle. 
                const ASVLite::Geometry::Coordinates3D asv_position = swarm[i].get_position(); // ASV position
                F_circle.row(i) += get_F_circle(asv_position);
                // Compute repusive force from each neighbour that is within the radio range
                for(size_t j = 0; j < swarm_size; ++j) {
                    if (i != j) {
                        const ASVLite::Geometry::Coordinates3D neighbour_position = swarm[j].get_position(); // Neighbour position
                        F_neighbour.row(i) += get_F_neighbour(asv_position, neighbour_position);
                    }
                }
            }
            Eigen::Matrix<double, swarm_size, 2> F_net = F_circle + F_neighbour;
            // Update the dynamics of each vehicle
            for(size_t i = 0; i < swarm_size; ++i) {
                // Compute desired vehicle heading w.r.t north
                const double desired_heading = std::atan2(F_net.row(i).x(), F_net.row(i).y());  
                // Halt the vehicle if the net force is below the threashold
                if(F_circle.row(i).norm() <= proximity_threashold && F_neighbour.row(i).norm() <= proximity_threashold) {
                    swarm[i].set_surge_sway_halt(true);    
                } else {
                    swarm[i].set_surge_sway_halt(false); 
                }
                // Compute dynamics
                double rudder_angle = rudder_controller.get_rudder_angle(desired_heading, swarm[i].get_attitude());
                auto [thrust_position, thrust_magnitude] = get_wave_glider_thrust(swarm[i], rudder_angle, sea_surface_updated.significant_wave_height);
                swarm[i].step_simulation(thrust_position, thrust_magnitude);
                time = swarm[i].get_time();
                // Get the latest position 
                ASVLite::Geometry::Coordinates3D  position = swarm[i].get_position();
                x_paths[i].push_back(position.keys.x);
                y_paths[i].push_back(position.keys.y);
            }

            // Compute the comms window for adjacent pairs of vehicles
            if (!run_animation) {
                if(counter % comms_update_frequency == 0.0) {
                    std::vector<std::future<std::pair<double, double>>> comms_window_futures;
                    for (size_t i = 0; i < swarm_size; ++i) {
                        comms_window_futures.push_back(std::async(std::launch::async, [=]() {
                            size_t j = (i == swarm_size - 1) ? 0 : i + 1;
                            auto position_i = swarm[i].get_position();
                            auto position_j = swarm[j].get_position();
                            double dx = position_i.keys.x - position_j.keys.x;
                            double dy = position_i.keys.y - position_j.keys.y;
                            double distance = std::sqrt(dx * dx + dy * dy);
                            double loss = get_comms_window_loss(threshold, radio_frequency, antenna_height, distance, sea_surface_updated, time_duration);
                            return std::make_pair(loss, distance);
                        }));
                    }
                    // Collect the results from all the futures (threads)
                    std::vector<double> comms_window_losses;
                    std::vector<double> distances;
                    comms_window_losses.reserve(comms_window_futures.size());
                    distances.reserve(comms_window_futures.size());

                    for (auto& fut : comms_window_futures) {
                        auto loss_and_dist = fut.get();  
                        comms_window_losses.push_back(loss_and_dist.first);
                        distances.push_back(loss_and_dist.second);
                    }

                    // Calculate the min comms_window for the swarm
                    auto min_iter = std::min_element(comms_window_losses.begin(), comms_window_losses.end());
                    size_t min_index = std::distance(comms_window_losses.begin(), min_iter);

                    // Calculate the reference comms window for the swarm without adaptation
                    double ref_loss = get_comms_window_loss(threshold, radio_frequency, antenna_height, ref_formation_radius, sea_surface_updated, time_duration);

                    // Write the values to the CSV file
                    const double min_comms_window = 100.0 - comms_window_losses[min_index];
                    const double ref_comms_window = 100.0 - ref_loss;
                    results_file_swarm_adaptation_sim << wave_ht << "," << distances[min_index] << "," << min_comms_window << "," << ref_comms_window << "\n";
                }
            }

            // Update plot to file
            if(counter % sea_state_update_frequency == sea_state_update_frequency/2.0) {
                // Plot to file for the timestep
                plt::clf();  // Clear previous frame
                // 1. Draw vector field in the background
                std::vector<double> X, Y, U, V;
                const double dx = 75.0;
                double axis_min = -field_length / 2.0;
                double axis_max =  field_length / 2.0;
                for (double x = axis_min; x <= axis_max; x += dx) {
                    for (double y = axis_min; y <= axis_max; y += dx) {
                        ASVLite::Geometry::Coordinates3D position {x, y, 0.0};
                        Eigen::Vector2d F = Eigen::Vector2d::Zero();
                        F += get_F_circle(position);
                        for (size_t j = 0; j < swarm_size; ++j) {
                            const ASVLite::Geometry::Coordinates3D neighbour_position = swarm[j].get_position();
                            F += get_F_neighbour(position, neighbour_position);
                        }
                        X.push_back(x);
                        Y.push_back(y);
                        U.push_back(F.x());
                        V.push_back(F.y());
                    }
                }
                for (size_t i = 0; i < X.size(); ++i) {
                    double x_start = X[i];
                    double y_start = Y[i];
                    double x_end = x_start + 0.1 * U[i];
                    double y_end = y_start + 0.1 * V[i];
                    std::vector<double> xs = {x_start, x_end};
                    std::vector<double> ys = {y_start, y_end};
                    plt::plot(xs, ys, {{"color", "#a6cee3"}});  // Light blue arrows
                    // Arrowhead
                    double arrow_len = 5.0;
                    double angle = std::atan2(V[i], U[i]);
                    double left_angle = angle + M_PI - M_PI / 6.0;
                    double right_angle = angle + M_PI + M_PI / 6.0;
                    double x_left = x_end + arrow_len * std::cos(left_angle);
                    double y_left = y_end + arrow_len * std::sin(left_angle);
                    double x_right = x_end + arrow_len * std::cos(right_angle);
                    double y_right = y_end + arrow_len * std::sin(right_angle);
                    plt::plot({x_end, x_left}, {y_end, y_left}, {{"color", "#a6cee3"}});
                    plt::plot({x_end, x_right}, {y_end, y_right}, {{"color", "#a6cee3"}});
                }
                // 2. Draw circle formation
                std::vector<double> x_circle; // x coordinates of points around the formation circle
                std::vector<double> y_circle; // y coordinates of points around the formation circle
                const int num_points_circle = 100; // number of points to plot on the formation circle
                for (int i = 0; i <= num_points_circle; ++i) {
                    double theta = 2.0 * M_PI * i / num_points_circle;
                    x_circle.push_back(formation_centre_x + formation_radius * std::cos(theta));
                    y_circle.push_back(formation_centre_x + formation_radius * std::sin(theta));
                } 
                plt::plot(x_circle, y_circle, {{"color", "green"}, {"linestyle", "--"}});
                // 3. Draw vehicle paths in a darker color
                for (size_t i = 0; i < swarm_size; ++i) {
                    plt::plot(x_paths[i], y_paths[i], {{"color", "grey"}}); 
                }
                // 4. Draw current vehicle positions on top
                for (size_t i = 0; i < swarm_size; ++i) {
                    const ASVLite::Geometry::Coordinates3D pos = swarm[i].get_position();
                    std::string color = "black";
                    plt::plot({pos.keys.x}, {pos.keys.y}, {{"color", color}, {"marker", "o"}, {"markersize", "8"}});
                }
                // 5. Final layout and render
                plt::xlim(axis_min, axis_max);
                plt::ylim(axis_min, axis_max);
                plt::axis("equal");
                plt::tight_layout();
                // Save to file or show once
                std::filesystem::path formation_plot = results_dir / ("case_study" + std::to_string(wave_ht) + "_" + plot_file_name_subscript + ".png");
                plt::save(formation_plot, 300);
            }

            
            // Update Animation
            if(run_animation) {
                if(counter % animation_update_frequency == 0.0){
                    plt::clf();  // Clear previous frame
                    // 1. Draw vector field in the background
                    std::vector<double> X, Y, U, V;
                    const double dx = 75.0;
                    double axis_min = -field_length / 2.0;
                    double axis_max =  field_length / 2.0;
                    for (double x = axis_min; x <= axis_max; x += dx) {
                        for (double y = axis_min; y <= axis_max; y += dx) {
                            ASVLite::Geometry::Coordinates3D position {x, y, 0.0};
                            Eigen::Vector2d F = Eigen::Vector2d::Zero();
                            F += get_F_circle(position);
                            for (size_t j = 0; j < swarm_size; ++j) {
                                const ASVLite::Geometry::Coordinates3D neighbour_position = swarm[j].get_position();
                                F += get_F_neighbour(position, neighbour_position);
                            }
                            X.push_back(x);
                            Y.push_back(y);
                            U.push_back(F.x());
                            V.push_back(F.y());
                        }
                    }
                    for (size_t i = 0; i < X.size(); ++i) {
                        double x_start = X[i];
                        double y_start = Y[i];
                        double x_end = x_start + 0.1 * U[i];
                        double y_end = y_start + 0.1 * V[i];
                        std::vector<double> xs = {x_start, x_end};
                        std::vector<double> ys = {y_start, y_end};
                        plt::plot(xs, ys, {{"color", "#a6cee3"}});  // Light blue arrows
                        // Arrowhead
                        double arrow_len = 5.0;
                        double angle = std::atan2(V[i], U[i]);
                        double left_angle = angle + M_PI - M_PI / 6.0;
                        double right_angle = angle + M_PI + M_PI / 6.0;
                        double x_left = x_end + arrow_len * std::cos(left_angle);
                        double y_left = y_end + arrow_len * std::sin(left_angle);
                        double x_right = x_end + arrow_len * std::cos(right_angle);
                        double y_right = y_end + arrow_len * std::sin(right_angle);
                        plt::plot({x_end, x_left}, {y_end, y_left}, {{"color", "#a6cee3"}});
                        plt::plot({x_end, x_right}, {y_end, y_right}, {{"color", "#a6cee3"}});
                    }
                    // 2. Draw circle formation
                    std::vector<double> x_circle; // x coordinates of points around the formation circle
                    std::vector<double> y_circle; // y coordinates of points around the formation circle
                    const int num_points_circle = 100; // number of points to plot on the formation circle
                    for (int i = 0; i <= num_points_circle; ++i) {
                        double theta = 2.0 * M_PI * i / num_points_circle;
                        x_circle.push_back(formation_centre_x + formation_radius * std::cos(theta));
                        y_circle.push_back(formation_centre_x + formation_radius * std::sin(theta));
                    } 
                    plt::plot(x_circle, y_circle, {{"color", "green"}, {"linestyle", "--"}});
                    // 3. Draw vehicle paths in a darker color
                    for (size_t i = 0; i < swarm_size; ++i) {
                        plt::plot(x_paths[i], y_paths[i], {{"color", "#444444"}});  // Dark grey
                    }
                    // 4. Draw current vehicle positions on top
                    for (size_t i = 0; i < swarm_size; ++i) {
                        const ASVLite::Geometry::Coordinates3D pos = swarm[i].get_position();
                        bool is_surge_sway_halt_set = swarm[i].get_surge_sway_halt();
                        std::string color = (is_surge_sway_halt_set)? "green" : "red";
                        plt::plot({pos.keys.x}, {pos.keys.y}, {{"color", color}, {"marker", "o"}, {"markersize", "4"}});
                    }
                    // 5. Show sea state on top-right
                    std::string text = "Time: " + std::to_string(time) + " sec";
                    double text_x = axis_max - 0.15 * (axis_max - axis_min);
                    double text_y = axis_max - 0.05 * (axis_max - axis_min);
                    plt::text(text_x, text_y, text);
                    text = "Sig wave ht: " + std::to_string(wave_ht) + " m";
                    text_x = axis_max - 0.15 * (axis_max - axis_min);
                    text_y = axis_max - 0.075 * (axis_max - axis_min);
                    plt::text(text_x, text_y, text);
                    text = "Formation radius: " + std::to_string(formation_radius) + " m";
                    text_x = axis_max - 0.15 * (axis_max - axis_min);
                    text_y = axis_max - 0.10 * (axis_max - axis_min);
                    plt::text(text_x, text_y, text);
                    // 6. Final plot config
                    plt::xlim(axis_min, axis_max);
                    plt::ylim(axis_min, axis_max);
                    plt::axis("equal");
                    plt::title("ASV Swarm: Paths, Forces & Positions");
                    plt::tight_layout();
                    plt::draw();
                    plt::pause(0.00001);
                } 
            }
        }
  
    }
    results_file_swarm_adaptation_sim.close();

    return 0;
}

