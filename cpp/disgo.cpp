#include "disgo.h"
#include "swarm.h"
#include "radio_attenuation.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <thread>
extern "C" {
#include "constants.h"
}


Disgo::Disgo(const double significant_wave_ht, const double antenna_height, const double radio_frequency, const double rainfall_rate, const double wind_speed, const double fspl_design, const double environmental_excess_loss) :
  significant_wave_ht(significant_wave_ht),
  antenna_height(antenna_height),
  radio_frequency(radio_frequency),
  rainfall_rate(rainfall_rate),
  wind_speed(wind_speed),
  fspl_design(fspl_design),
  environmental_excess_loss(environmental_excess_loss) {
}


std::vector<Coordinates_3D> Disgo::get_uniform_points_on_circle(const Coordinates_3D& point, const double radius, const int n) const {
  std::vector<Coordinates_3D> points;
  points.reserve(n+1);
  // Insert the centre into points
  points.push_back(point);
  for (int i = 0; i < n ; ++i) {
    double angle = i*(2.0*PI)/n;
    double x = point.keys.x + (radius * std::cos(angle));
    double y = point.keys.y + (radius * std::sin(angle));
    Coordinates_3D new_position = {x, y, 0};
    points.push_back(new_position);
    }
  return points;
}


double Disgo::evaluate_formation(const std::vector<Coordinates_3D>& deployment_locations) const {
  // Initialise sea surface
  const double wave_dp = PI / 3.0;
  const int count_component_waves = 9;
  const int wave_rand_seed = 1;
  Sea_surface* sea_surface = sea_surface_new(significant_wave_ht, wave_dp, wave_rand_seed, count_component_waves);

  // Define ASV specifications
  const Asv_specification asv_spec{
  .L_wl = 2.1, // m
  .B_wl = 0.6, // m
  .D = 0.25, // m
  .T = 0.09, // m
  .max_speed = 4.0, // m/s
  .disp = 0.09, // m3
  .r_roll = 0.2, // m
  .r_pitch = 0.6, // m
  .r_yaw = 0.06, // m
  .cog = {1.05, 0.0, -0.2}, // {m, m, m}
  };
  const bool is_surge_sway_halt_set = true;

  // Initialise ASVs at the deployment locations
  std::vector<Asv*> asvs;
  Coordinates_3D attitude = {0.0, 0.0, 0.0};
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


std::vector<Coordinates_3D> Disgo::get_new_formation(const std::vector<Coordinates_3D>& current_formation, double search_radius) const {
  std::vector<std::pair<Coordinates_3D, double>> proposed_positions_per_vehicle;
  for (const Coordinates_3D& position : current_formation) {
    // Generate possible new positions around the candidate for greedy selection.
    int count_new_positions = 10;
    const std::vector<Coordinates_3D> evaluation_set = get_uniform_points_on_circle(position, search_radius, count_new_positions);

    // Assuming the rest of the swarm positions as not changed, evaluate the positions in the evaluation set to identify
    // the best new position.

    // Multi-thread the evaluation; one thread per new position to be evaluated.
    std::mutex vec_mutex;
    std::vector<double> evaluation_values(evaluation_set.size(), 0.0);

    // Evaluate each potential new position
    for (int i = 0; i < evaluation_set.size(); i++) {
      std::vector<Coordinates_3D> formation_to_assess = current_formation;
      // Change candidate to eval_pos
      auto it = std::find_if(formation_to_assess.begin(), formation_to_assess.end(),
                                 [&](const Coordinates_3D& p) { return p.keys.x == position.keys.x && p.keys.y == position.keys.y; });
      int index = std::distance(formation_to_assess.begin(), it);
      formation_to_assess[index] = evaluation_set[i];

      // Compute the optimisation function value for the new configuration
      double value = evaluate_formation(formation_to_assess);

      std::lock_guard<std::mutex> lock(vec_mutex);
      evaluation_values[i] = value;
    };

    // Select the best new position (one with the highest value)
    std::cout << "Evaluation result: ";
    for (double value : evaluation_values) {
      std::cout << value << " ";
    }
    int best_index = std::distance(evaluation_values.begin(),
                                   std::max_element(evaluation_values.begin(), evaluation_values.end()));
    std::pair<Coordinates_3D, double> best_position(evaluation_set[best_index], evaluation_values[best_index]);
    proposed_positions_per_vehicle.push_back(best_position);
    std::cout << " max value = " << evaluation_values[best_index] << std::endl;
  }

  // From all the proposed positions select only one with the best value for the new formation
  auto max_it = std::max_element(proposed_positions_per_vehicle.begin(), proposed_positions_per_vehicle.end(),
                                   [](const auto& a, const auto& b) {
                                       return a.second < b.second;
                                   });
  int best_index = std::distance(proposed_positions_per_vehicle.begin(), max_it);
  // New formation
  std::vector<Coordinates_3D> new_formation = current_formation;
  new_formation[best_index] = proposed_positions_per_vehicle[best_index].first;

  return new_formation;
}