#include "icgs.h"
#include "swarm.h"
#include "radio_attenuation.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <thread>


ICGS::ICGS(const double significant_wave_ht, const double antenna_height, const double radio_frequency, const double rainfall_rate, const double wind_speed, const double fspl_design, const double environmental_excess_loss) :
  significant_wave_ht(significant_wave_ht),
  antenna_height(antenna_height),
  radio_frequency(radio_frequency),
  rainfall_rate(rainfall_rate),
  wind_speed(wind_speed),
  fspl_design(fspl_design),
  environmental_excess_loss(environmental_excess_loss) {
}


std::vector<ASVLite::Geometry::Coordinates3D> ICGS::get_uniform_points_on_circle(const ASVLite::Geometry::Coordinates3D& point, const double radius, const int n) const {
  std::vector<ASVLite::Geometry::Coordinates3D> points;
  points.reserve(n+1);
  // Insert the centre into points
  points.push_back(point);
  for (int i = 0; i < n ; ++i) {
    double angle = i*(2.0*M_PI)/n;
    double x = point.keys.x + (radius * std::cos(angle));
    double y = point.keys.y + (radius * std::sin(angle));
    ASVLite::Geometry::Coordinates3D new_position = {x, y, 0};
    points.push_back(new_position);
    }
  return points;
}


double ICGS::evaluate_formation(const std::vector<ASVLite::Geometry::Coordinates3D>& deployment_locations) const {
  // Initialise sea surface
  const double wave_dp = M_PI / 3.0;
  const int count_component_waves = 9;
  const int wave_rand_seed = 1;
  const ASVLite::SeaSurface sea_surface {significant_wave_ht, wave_dp, wave_rand_seed, count_component_waves};

  // Initialise swarm and establish communication links
  Swarm swarm(deployment_locations, sea_surface, antenna_height, radio_frequency, rainfall_rate, wind_speed, fspl_design, environmental_excess_loss);
  for (size_t i = 0; i < deployment_locations.size(); ++i) {
    for (size_t j = i + 1; j < deployment_locations.size(); ++j) {
      swarm.set_comms_link({deployment_locations[i], deployment_locations[j]});
    }
  }

  // Run the simulation and retrieve the computed optimisation value
  double value = swarm.run_simulation();

  return value;
}


std::vector<ASVLite::Geometry::Coordinates3D> ICGS::get_new_formation(const std::vector<ASVLite::Geometry::Coordinates3D>& current_formation, double search_radius) const {
  std::vector<std::pair<ASVLite::Geometry::Coordinates3D, double>> proposed_positions_per_vehicle;
  for (const auto& position : current_formation) {
    // Generate possible new positions around the candidate for greedy selection.
    int count_new_positions = 10;
    const std::vector<ASVLite::Geometry::Coordinates3D> evaluation_set = get_uniform_points_on_circle(position, search_radius, count_new_positions);

    // Assuming the rest of the swarm positions as not changed, evaluate the positions in the evaluation set to identify
    // the best new position.

    // Multi-thread the evaluation; one thread per new position to be evaluated.
    std::mutex vec_mutex;
    std::vector<double> evaluation_values(evaluation_set.size(), 0.0);

    // Evaluate each potential new position
    auto thread_worker_function = [&](int i)  {
      std::vector<ASVLite::Geometry::Coordinates3D> formation_to_assess = current_formation;
      // Change candidate to eval_pos
      auto it = std::find_if(formation_to_assess.begin(), formation_to_assess.end(),
                                 [&](const ASVLite::Geometry::Coordinates3D& p) { return p.keys.x == position.keys.x && p.keys.y == position.keys.y; });
      int index = std::distance(formation_to_assess.begin(), it);
      formation_to_assess[index] = evaluation_set[i];

      // Compute the optimisation function value for the new configuration
      double value = evaluate_formation(formation_to_assess);

      std::lock_guard<std::mutex> lock(vec_mutex);
      evaluation_values[i] = value;
    };

    // Start the threads
    std::vector<std::thread*> thread_pool;
    for (int i = 0; i < evaluation_set.size(); i++) {
      std::thread* thread_object = new std::thread(thread_worker_function, i);
      thread_pool.push_back(thread_object);
    }
    // Join the threads
    for(auto thread : thread_pool)
    {
      thread->join();
    }

    // Select the best new position (one with the highest value)
    std::cout << "Evaluation result: ";
    for (double value : evaluation_values) {
      std::cout << value << " ";
    }
    int best_index = std::distance(evaluation_values.begin(),
                                   std::max_element(evaluation_values.begin(), evaluation_values.end()));
    std::pair<ASVLite::Geometry::Coordinates3D, double> best_position(evaluation_set[best_index], evaluation_values[best_index]);
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
  std::vector<ASVLite::Geometry::Coordinates3D> new_formation = current_formation;
  new_formation[best_index] = proposed_positions_per_vehicle[best_index].first;

  return new_formation;
}