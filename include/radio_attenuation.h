#ifndef RADIOATTENUATION_H
#define RADIOATTENUATION_H

#include <vector>
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
    ~RadioAttenuation();

    double antenna_height;
    double radio_frequency; // Hz
    Polarisation polarisation;
    double communication_distance; // m
    double significant_wave_ht; // m
    double wind_speed; // m/s
    double rainfall_rate; // mm/hr
    double free_space_path_loss; // dB
    std::vector<double> diffraction_loss; // dB
    double diffraction_loss_mean; // dB
    double rain_attenuation; // dB
    double sea_spray_attenuation; // dB
    double total_attenuation; //

private:
    void set_free_space_path_loss();
    void set_diffraction_loss();
    void set_rain_attenuation();
    void set_sea_spray_attenuation();
    void create_equidistant_points(const union Coordinates_3D& p1,
                                   const union Coordinates_3D& p2,
                                   double time,
                                   std::vector<union Coordinates_3D>& points,
                                   std::vector<double>& offsets);
    double get_diffraction_loss(union Coordinates_3D p1, union Coordinates_3D p2, double time);

    struct Sea_surface* sea;
    struct Asv_specification* asv_spec;
    struct Asv* asv1;
    struct Asv* asv2;
    double time_step_size;
    double total_simulation_time;
};

#endif //RADIOATTENUATION_H