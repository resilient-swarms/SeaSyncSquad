#pragma once

/**
 * Iterative Coordinate-wise Greedy Search (ICGS)
 */


#include <vector>
#include "ASVLite/geometry.h"
#include "ASVLite/sea_surface.h"
#include "ASVLite/asv.h"


class ICGS {
public:
    ICGS(const double significant_wave_ht, const double antenna_height, const double radio_frequency, const double rainfall_rate, const double wind_speed, const double fspl_design, const double environmental_excess_loss);
    std::vector<ASVLite::Geometry::Coordinates3D> get_new_formation(const std::vector<ASVLite::Geometry::Coordinates3D>& current_formation, const double search_radius) const;
    double evaluate_formation(const std::vector<ASVLite::Geometry::Coordinates3D>& deployment_locations) const;

private:
    std::vector<ASVLite::Geometry::Coordinates3D> get_uniform_points_on_circle(const ASVLite::Geometry::Coordinates3D& point, const double radius, const int n) const;

private:
    const double antenna_height; // m
    const double radio_frequency; // Hz
    const double rainfall_rate; // mm/hr
    const double wind_speed; // m/s
    const double significant_wave_ht; // m
    const double fspl_design; // Design free space path loss (dB)
    const double environmental_excess_loss; // Excess radio loss due to environment (dB)
};

