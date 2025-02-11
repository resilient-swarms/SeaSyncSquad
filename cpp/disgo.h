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
#include <filesystem>
#include "swarm.h"

extern "C" {
#include "geometry.h"
#include "sea_surface.h"
#include "asv.h"
}



class Disgo {
public:
    Disgo(std::vector<Coordinates_3D> swarm_positions, double significant_wave_ht, double antenna_height, double radio_frequency, double rainfall_rate, double wind_speed, double fspl_design, double environmental_excess_loss);
    Coordinates_3D get_new_position(Coordinates_3D current_asv_position, double search_radius);

private:
    double evaluate_swarm_formation(const std::vector<Coordinates_3D>& deployment_locations);

private:
    double antenna_height; // m
    double radio_frequency; // Hz
    double rainfall_rate; // mm/hr
    double wind_speed; // m/s
    std::vector<Coordinates_3D> swarm_positions;
    double significant_wave_ht; // m
    double fspl_design; // Design free space path loss (dB)
    double environmental_excess_loss; // Excess radio loss due to environment (dB)
};



#endif //DISGO_H
