/**
 * Decentralised Iterative System-wide Greedy Optimisation (DISGO)
 * Acronym Breakdown:
 * Letter	Term	        Role in the Method
 *   D	    Decentralised	Agents operate independently with no central authority.
 *   I	    Iterative	    Sequential, round-robin updates until convergence.
 *   S	    System-wide	    Each agent evaluates hypothetical moves for all swarm members (holistic planning).
 *   G	    Greedy	        Selects the best immediate improvement at each step.
 *   O	    Optimisation	Explicit focus on maximising spatial spread while maintaining connectivity.
 */


#ifndef DISGO_H
#define DISGO_H

#include <vector>

extern "C" {
#include "geometry.h"
#include "sea_surface.h"
#include "asv.h"
}



class Disgo {
public:
    Disgo(const double significant_wave_ht, const double antenna_height, const double radio_frequency, const double rainfall_rate, const double wind_speed, const double fspl_design, const double environmental_excess_loss);
    std::vector<Coordinates_3D> get_new_formation(const std::vector<Coordinates_3D>& current_formation, const double search_radius) const;
    double evaluate_formation(const std::vector<Coordinates_3D>& deployment_locations) const;

private:
    std::vector<Coordinates_3D> get_uniform_points_on_circle(const Coordinates_3D& point, const double radius, const int n) const;

private:
    const double antenna_height; // m
    const double radio_frequency; // Hz
    const double rainfall_rate; // mm/hr
    const double wind_speed; // m/s
    const double significant_wave_ht; // m
    const double fspl_design; // Design free space path loss (dB)
    const double environmental_excess_loss; // Excess radio loss due to environment (dB)
};



#endif //DISGO_H
