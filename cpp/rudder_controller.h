#ifndef RUDDER_CONTROLLER_H
#define RUDDER_CONTROLLER_H

#include <filesystem>
#include <Eigen/Dense>
extern "C" {
#include "constants.h"
#include "geometry.h"
#include "asv.h"
}

class Rudder_controller {
public:
    Rudder_controller(Asv_specification asv_spec, Eigen::Vector3d& initial_K);
    double get_rudder_angle(Asv* asv, Coordinates_3D waypoint);

private:
    double get_relative_heading(Asv* asv, Coordinates_3D waypoint);


private:
    Asv_specification asv_spec_;
    double max_rudder_angle = M_PI / 6.0; // 30 degrees
    Eigen::Vector3d K;
    double error = 0.0;
    double previous_error = 0.0;
    double cumulative_error = 0.0;
    double delta_error = 0.0;
    std::filesystem::path out_dir;

};



#endif //RUDDER_CONTROLLER_H
