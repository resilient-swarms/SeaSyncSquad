#ifndef RADIOATTENUATION_H
#define RADIOATTENUATION_H

#include <vector>
#include <array>
extern "C" {
#include "constants.h"
#include "geometry.h"
#include "sea_surface.h"
#include "asv.h"
}


enum class Polarisation {horizontal, vertical};

class RadioAttenuation {
public:
    RadioAttenuation(
        double antenna_ht,
        double radio_frequency,
        Polarisation polarisation,
        double communication_distance,
        double significant_wave_ht,
        double wind_speed,
        double rainfall_rate);

    RadioAttenuation(
        double antenna_ht,
        double radio_frequency,
        Polarisation polarisation,
        std::pair<Coordinates_3D, Coordinates_3D> asv_positions,
        double significant_wave_ht,
        double wind_speed,
        double rainfall_rate);

    ~RadioAttenuation();

    double antenna_height;
    double radio_frequency; // Hz
    Polarisation polarisation;
    double communication_distance; // m
    double significant_wave_ht; // m
    double wind_speed; // m/s
    double rainfall_rate; // mm/hr

    // Results
    double free_space_path_loss; // dB
    std::vector<double> diffraction_loss; // dB
    std::vector<double> fresnel_clearance; // %
    double diffraction_loss_mean; // dB
    double rain_attenuation; // dB
    double sea_spray_attenuation; // dB
    double total_attenuation; //

private:
    void set_free_space_path_loss();
    void set_diffraction_loss();
    void set_rain_attenuation();
    void set_sea_spray_attenuation();

    /**
     * Generates a set of equidistant points representing a sea surface between two given points.
     * The values returned are a set of points along with the offsets of the points from p1.
     */
    std::vector<std::pair<double, Coordinates_3D>> generate_sea_surface_between_points(const union Coordinates_3D& p1, const union Coordinates_3D& p2, double time);

    /**
     * Generates the minimum and maximum vertical boundaries of the Fresnel zone for the given points.
     * The value returned is a vector of pairs of min and max vertical boundaries.
     */
    std::vector<std::pair<double, double>> generate_fresnel_zone(std::vector<std::pair<double, Coordinates_3D>>& points);
    double get_diffraction_loss(union Coordinates_3D p1, union Coordinates_3D p2, double time);

    struct Sea_surface* sea;
    struct Asv_specification* asv_spec;
    struct Asv* asv1;
    struct Asv* asv2;
    double time_step_size;
    double total_simulation_time;
};

#endif //RADIOATTENUATION_H