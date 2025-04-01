#include <iostream>
#include <random>
#include <Eigen/Dense>
#include "matplotlibcpp.h"
#include "template_param.h"
#include "ASVLite/geometry.h"
#include "ASVLite/sea_surface.h"
#include "ASVLite/asv.h"
#include "ASVLite/rudder_controller.h"

namespace plt = matplotlibcpp;

int main() {
    // Field dimensions
    const double field_length = 1000.0; // m

    // Random number generator
    const int rng_seed = 1;  
    std::mt19937 gen(rng_seed);
    std::uniform_real_distribution<double> distribution(-field_length/2.0, field_length/2.0);  

    // Simulation time
    const double simulation_time = 300.0 * 60.0; // sec   

    // Formation parameters
    const size_t swarm_size = 7;
    const double proximity_threashold = 10.0; // Proximity to circle
    const double formation_radius = 200.0; // m
    const double formation_centre_x = 0.0;
    const double formation_centre_y = 0.0;
    const double radio_range = formation_radius; // m
    ASVLite::Geometry::Coordinates3D formation_center {formation_centre_x, formation_centre_y, 0.0};
    const double k_circle = 1.0; // N/m
    const double k_neighbour = 1.0; // N/m
    const double k_dismissal = 1.0; // N/m

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
        if(p21.norm() <= radio_range) {
            F = -k_neighbour * (radio_range - p21.norm())/p21.norm() * p21; // Repulsion from neighbours
        }
        return F;
    };

    // Initial sea surface
    const double wave_ht = 3.50; // m
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
    ASVLite::RudderController rudder_controller {asv_spec, {1.0, 1.0, 1.0}};

    // Initialise and deploy swarm
    const double antenna_height = 1.0; // m
    const double radio_frequency = 2.4e9; // Hz
    std::vector<ASVLite::Asv<N>> swarm;
    for(size_t i = 0; i < swarm_size; ++i) {
        const double x = distribution(gen);
        const double y = distribution(gen);
        const double heading = std::atan2(x, y); 
        const double z = 0.0;
        const ASVLite::Geometry::Coordinates3D position {x, y, 0};
        const ASVLite::Geometry::Coordinates3D attitude {0, 0, heading};
        ASVLite::Asv<N> asv {asv_spec, &sea_surface, position, attitude};
        swarm.push_back(asv);
    }

    // Run simulation
    std::vector<std::vector<double>> x_paths; // x coordinate of the paths traced by the vehicles in the swarm
    std::vector<std::vector<double>> y_paths; // y coordinate of the paths traced by the vehicles in the swarm
    x_paths.resize(swarm_size);
    y_paths.resize(swarm_size);
    std::vector<double> x_circle; // x coordinates of points around the formation circle
    std::vector<double> y_circle; // y coordinates of points around the formation circle
    const int num_points_circle = 100; // number of points to plot on the formation circle
    for (int i = 0; i <= num_points_circle; ++i) {
        double theta = 2.0 * M_PI * i / num_points_circle;
        x_circle.push_back(formation_centre_x + formation_radius * std::cos(theta));
        y_circle.push_back(formation_centre_x + formation_radius * std::sin(theta));
    } 
    for(double time = 0; time < simulation_time;) {
        Eigen::Matrix<double, swarm_size, 2> F_circle    = Eigen::Matrix<double, swarm_size, 2>::Zero(); // Artifical attractive force to circle
        Eigen::Matrix<double, swarm_size, 2> F_neighbour = Eigen::Matrix<double, swarm_size, 2>::Zero(); // Net artificial force due to neighbours that are within the radio range
        // For each vehicle in the swarm, compute the net force
        for(size_t i = 0; i < swarm_size; ++i) {
            // Compute the attractive force to the circumference of the circle. 
            const ASVLite::Geometry::Coordinates3D asv_position = swarm[i].get_position(); // ASV position
            F_circle.row(i) += get_F_circle(asv_position);
            // Compute repusive force from each neighbour that is within the radio range
            if(i != swarm_size-1){  
                for(size_t j = i+1; j < swarm_size; ++j) {
                    const ASVLite::Geometry::Coordinates3D neighbour_position = swarm[j].get_position(); // Neighbour position
                    F_neighbour.row(i) += get_F_neighbour(asv_position, neighbour_position);
                    F_neighbour.row(j) += -F_neighbour.row(i);
                }
            }
        }
        // Compute the net artificial force
        Eigen::Matrix<double, swarm_size, 2> F_net = F_circle + F_neighbour;
        // Update the dynamics of each vehicle
        for(size_t i = 0; i < swarm_size; ++i) {
            // Compute desired vehicle heading w.r.t north
            const double desired_heading = std::atan2(F_net.row(i).x(), F_net.row(i).y());  
            // Halt the vehicle if the net force is below the threashold
            if(F_net.row(i).norm() <= proximity_threashold) {
                swarm[i].set_surge_sway_halt(true);    
            } else {
                swarm[i].set_surge_sway_halt(false); 
            }
            // Compute dynamics
            double rudder_angle = rudder_controller.get_rudder_angle(desired_heading, swarm[i].get_attitude());
            auto [thrust_position, thrust_magnitude] = get_wave_glider_thrust(swarm[i], rudder_angle, sea_surface.significant_wave_height);
            swarm[i].step_simulation(thrust_position, thrust_magnitude);
            time = swarm[i].get_time();
            // Get the latest position 
            ASVLite::Geometry::Coordinates3D  position = swarm[i].get_position();
            x_paths[i].push_back(position.keys.x);
            y_paths[i].push_back(position.keys.y);
        }
        
        // Update Animation
        plt::clf();  // Clear previous frame
        // 1. Draw vector field in the background
        std::vector<double> X, Y, U, V;
        const double dx = 50.0;
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
        plt::plot(x_circle, y_circle, {{"color", "green"}, {"linestyle", "--"}});
        // 3. Draw vehicle paths in a darker color
        for (size_t i = 0; i < swarm_size; ++i) {
            plt::plot(x_paths[i], y_paths[i], {{"color", "#444444"}});  // Dark grey
        }
        // 4. Draw current vehicle positions on top
        for (size_t i = 0; i < swarm_size; ++i) {
            const ASVLite::Geometry::Coordinates3D pos = swarm[i].get_position();
            plt::plot({pos.keys.x}, {pos.keys.y}, {{"color", "red"}, {"marker", "o"}, {"markersize", "4"}});
        }
        // 5. Final plot config
        plt::xlim(axis_min, axis_max);
        plt::ylim(axis_min, axis_max);
        plt::axis("equal");
        plt::title("ASV Swarm: Paths, Forces & Positions");
        plt::tight_layout();
        plt::draw();
        plt::pause(0.00001);
    }
    
    
    // Plot the final reault to file
    // 1. Draw vector field in the background
    std::vector<double> X, Y, U, V;
    const double dx = 50.0;
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
    plt::plot(x_circle, y_circle, {{"color", "green"}, {"linestyle", "--"}});
    // 3. Draw vehicle paths in light grey
    for (size_t i = 0; i < swarm_size; ++i) {
        plt::plot(x_paths[i], y_paths[i], {{"color", "#cccccc"}});
    }
    // 4. Draw current vehicle positions in red
    for (size_t i = 0; i < swarm_size; ++i) {
        const ASVLite::Geometry::Coordinates3D pos = swarm[i].get_position();
        plt::plot({pos.keys.x}, {pos.keys.y}, {{"color", "red"}, {"marker", "o"}, {"markersize", "4"}});
    }
    // 5. Final layout and render
    plt::xlim(axis_min, axis_max);
    plt::ylim(axis_min, axis_max);
    plt::axis("equal");
    plt::title("ASV Swarm: Paths, Forces & Positions");
    plt::tight_layout();
    // Save to file or show once
    plt::show();
    // plt::save("asv_final_state.png");


    return 0;
}

