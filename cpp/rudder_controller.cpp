#include "rudder_controller.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>


Rudder_controller::Rudder_controller(Asv_specification asv_spec, Eigen::Vector3d &initial_K) {
    this->asv_spec_ = asv_spec;
    this->K = initial_K;
    std::filesystem::path root_dir = std::filesystem::current_path().parent_path();
    // std::filesystem::path out_dir = root_dir/ "results" / "rudder_controller" / "tuning";
    // if (!std::filesystem::exists(out_dir)) {
    //     std::filesystem::create_directory(out_dir);
    // }
    // else {
    //     throw std::runtime_error("Could not create output directory - " + out_dir.string());
    // }
}


double Rudder_controller::get_relative_heading(Asv *asv, Coordinates_3D waypoint) {
    Coordinates_3D p1 = asv_get_position_origin(asv);
    Coordinates_3D p2 = asv_get_position_cog(asv);
    Coordinates_3D p3 = waypoint;
    Eigen::Vector2d p_origin(p1.keys.x, p1.keys.y);
    Eigen::Vector2d p_cog(p2.keys.x, p2.keys.y);
    Eigen::Vector2d p_waypoint(p3.keys.x, p3.keys.y);

    // Compute direction vectors
    Eigen::Vector2d v1 = p_cog - p_origin;
    Eigen::Vector2d v2 = p_waypoint - p_origin;

    // Compute dot product
    double dot_product = v1.dot(v2);

    // Compute magnitudes
    double magnitude_v1 = v1.norm();
    double magnitude_v2 = v2.norm();

    // Ensure no division by zero
    if (magnitude_v1 == 0 || magnitude_v2 == 0) {
        throw std::runtime_error("One of the lines has zero length!");
    }

    // Compute cosine of the angle
    double cos_theta = dot_product / (magnitude_v1 * magnitude_v2);

    // Clamp value to avoid numerical instability
    cos_theta = std::clamp(cos_theta, -1.0, 1.0);

    // Compute angle in radians
    double theta = std::acos(cos_theta);

    return theta;
}


double Rudder_controller::get_rudder_angle(Asv *asv, Coordinates_3D waypoint) {
    // Compute the relative angle between the vehicle heading and the waypoint.
    double theta = get_relative_heading(asv, waypoint);
    // Set error as the difference of the current heading and the desired heading.
    previous_error = error;
    error = theta;
    double gamma = 0.7; // Rate at which the past errors reduces.
    cumulative_error = error + (gamma * cumulative_error);
    delta_error = error - previous_error;
    // Compute the rudder angle
    Eigen::Vector3d E(error, cumulative_error, delta_error); // P, I, D errors.
    double phi = K.dot(E);
    // Limit the rudder angle within the range (-PI/6, PI/6)
    if (phi > max_rudder_angle) {
        phi = max_rudder_angle;
    } else if (phi < -max_rudder_angle) {
        phi = -max_rudder_angle;
    }
    return phi; // radians
}

