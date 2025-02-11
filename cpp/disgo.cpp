#include "disgo.h"
#include "radio_attenuation.h"
#include <fstream>
#include <algorithm>
#include <numeric>
extern "C" {
#include "constants.h"
}


Disgo::Disgo(std::vector<Coordinates_3D> swarm_positions, double significant_wave_ht, double antenna_height, double radio_frequency, double rainfall_rate, double wind_speed, double fspl_design, double environmental_excess_loss) {
  this->swarm_positions = swarm_positions;
  this->significant_wave_ht = significant_wave_ht;
  this->antenna_height = antenna_height;
  this->radio_frequency = radio_frequency;
  this->rainfall_rate = rainfall_rate;
  this->wind_speed = wind_speed;
  this->fspl_design = fspl_design;
  this->environmental_excess_loss = environmental_excess_loss;
}


static std::vector<Coordinates_3D> get_n_points_around(Coordinates_3D point, double radius, int n) {
  std::vector<Coordinates_3D> points;
  for (int i = 0; i < n ; ++i) {
    double angle = i*(2.0*PI)/n;
    double x = point.keys.x + (radius * std::cos(angle));
    double y = point.keys.y + (radius * std::sin(angle));
    Coordinates_3D new_position = {x, y, 0};
    points.push_back(new_position);
    }
  return points;
}


Coordinates_3D Disgo::get_new_position(Coordinates_3D current_asv_position, double search_radius) {
  // Create working copies of swarm positions
  std::vector<Coordinates_3D> remaining_positions = swarm_positions;
  std::vector<Coordinates_3D> finalised_positions;

  // Continue until all positions are optimised
  while (finalised_positions.size() != swarm_positions.size()) {
    // Candidate positions for remaining positions. Also, these positions compete in the round-robin.
    // Only the best candidate is pushed to finalised_position.
    std::vector<Coordinates_3D> candidate_positions;
    std::vector<double> candidate_values;

    // Iterate over each remaining candidate position
    for (Coordinates_3D& candidate : remaining_positions) {
      // Generate possible new positions around the candidate for greedy selection. Only the best evaluation_position
      // is selected into candidate_positions.
      int count_new_positions = 10;
      std::vector<Coordinates_3D> evaluation_positions = get_n_points_around(candidate, search_radius, count_new_positions);
      evaluation_positions.insert(evaluation_positions.begin(), candidate); // Include the current position

      std::vector<double> evaluation_values;

      // Evaluate each potential new position
      for (Coordinates_3D& eval_pos : evaluation_positions) {
        // Create a list of all deployment positions, replacing the candidate position
        std::vector<Coordinates_3D> deployment_locations = remaining_positions;
        deployment_locations.insert(deployment_locations.end(), finalised_positions.begin(), finalised_positions.end());

        // Remove the candidate's original position...
        auto it = std::find_if(deployment_locations.begin(), deployment_locations.end(),
                               [&](const Coordinates_3D& p) { return p.keys.x == candidate.keys.x && p.keys.y == candidate.keys.y; });
        if (it != deployment_locations.end()) {
          deployment_locations.erase(it);
        }

        // ... and instead add the new position which is to be evaluated
        deployment_locations.push_back(eval_pos);

        // Compute the optimisation function value for the new configuration
        double value = evaluate_swarm_formation(deployment_locations);
        evaluation_values.push_back(value);
      }

      // Select the best new position (one with the highest value)
      int best_index = std::distance(evaluation_values.begin(),
                                     std::max_element(evaluation_values.begin(), evaluation_values.end()));

      // If the candidate position corresponds to the current ASV position, return its best alternative
      if (candidate.keys.x == current_asv_position.keys.x && candidate.keys.y == current_asv_position.keys.y) {
        return evaluation_positions[best_index];
      } else {
        candidate_positions.push_back(evaluation_positions[best_index]);
        candidate_values.push_back(evaluation_values[best_index]);
      }
    }

    // Select the best overall candidate position and update the finalised list
    int best_candidate_index = std::distance(candidate_values.begin(),
                                             std::max_element(candidate_values.begin(), candidate_values.end()));
    finalised_positions.push_back(candidate_positions[best_candidate_index]);

    // Remove the selected position from the remaining candidates
    auto it = std::find_if(remaining_positions.begin(), remaining_positions.end(),
                           [&](const Coordinates_3D& p) { return p.keys.x == candidate_positions[best_candidate_index].keys.x && p.keys.y == candidate_positions[best_candidate_index].keys.y; });
    if (it != remaining_positions.end()) {
      remaining_positions.erase(it);
    }
  }
}


double Disgo::evaluate_swarm_formation(const std::vector<Coordinates_3D>& deployment_locations) {
  // Initialise sea surface
  double wave_dp = PI / 3.0;
  int count_component_waves = 15;
  int wave_rand_seed = 1;
  Sea_surface* sea_surface = sea_surface_new(significant_wave_ht, wave_dp, wave_rand_seed, count_component_waves);

  // Define ASV specifications
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

  // Initialise ASVs at the deployment locations
  std::vector<Asv*> asvs;
  union Coordinates_3D attitude = {0.0, 0.0, 0.0};
  for (const auto& loc : deployment_locations) {
    Asv* asv = asv_new(asv_spec, sea_surface, loc, attitude);
    asv_set_surge_sway_halt(asv, is_surge_sway_halt_set);
    asvs.push_back(asv);
  }

  // Initialise swarm and establish communication links
  Swarm swarm(asvs, sea_surface, antenna_height, radio_frequency, rainfall_rate, wind_speed, fspl_design, environmental_excess_loss);
  for (size_t i = 0; i < asvs.size(); ++i) {
    for (size_t j = i + 1; j < asvs.size(); ++j) {
      swarm.set_comms_link({asvs[i], asvs[j]});
    }
  }

  // Run the simulation and retrieve the computed optimisation value
  double value = swarm.run_simulation();

  // Clean up dynamically allocated resources
  for (Asv* asv : asvs) {
    asv_delete(asv);
  }
  sea_surface_delete(sea_surface);

  return value;
}