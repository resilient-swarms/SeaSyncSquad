#ifndef RADIO_ATTENUATION_H
#define RADIO_ATTENUATION_H

#include <utility>
extern "C" {
#include "geometry.h"
#include "sea_surface.h"
}

enum class Polarisation {horizontal, vertical};


/**
 * Calculate free space path loss.
 * @param radio_frequency in Hz.
 * @param communication_distance in m.
 * @return loss in dB
 */
double get_free_space_path_loss(double radio_frequency, double communication_distance);


/**
 * Signal loss due to rain water in the air.
 * @param radio_frequency in Hz.
 * @param communication_distance in m.
 * @param polarisation i.e. vertical or horizontal.
 * @param rainfall_rate in mm/hr.
 * @return loss in dB.
 */
double get_rain_attenuation(double radio_frequency, double communication_distance, Polarisation polarisation, double rainfall_rate);


/**
 * Signal due to salt water spray in the air.
 * @param radio_frequency in Hz.
 * @param communication_distance in m.
 * @param sea
 * @param wind_speed in m/s.
 * @return loss in dB.
 */
double get_sea_spray_attenuation(double radio_frequency, double communication_distance, Sea_surface* sea, double wind_speed );


/**
 * Calculate the minimum fresnel zone clearance and diffraction loss due to ocean wave for the given time step.
 * @param radio_frequency in Hz.
 * @param antenna_height in m.
 * @param communication_distance in m.
 * @param sea surface instance.
 * @param antenna_positions.
 * @param time from the start of simulation in sec.
 * @param time_step_size of simulation in milli-sec.
 * @return (1) minimum fresnel clearance in % and (2) loss in dB.
 */
std::pair<double, double> get_min_fresnel_clearance_and_diffraction_loss(double radio_frequency, double antenna_height,
                                                                        double communication_distance,
                                                                        Sea_surface* sea,
                                                                        std::pair<Coordinates_3D, Coordinates_3D> antenna_positions,
                                                                        double time, double time_step_size );
#endif //RADIO_ATTENUATION_H
