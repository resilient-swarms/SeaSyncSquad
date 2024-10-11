#ifndef RADIOATTENUATION_H
#define RADIOATTENUATION_H

enum class Polarisation {horizontal, vertical};

class RadioAttenuation {
public:
    RadioAttenuation(
        double radio_frequency,
        Polarisation polarisation,
        double communication_distance,
        double significant_wave_ht,
        double wind_speed,
        double rainfall_rate);

    double radio_frequency; // Hz
    Polarisation polarisation;
    double communication_distance; // m
    double significant_wave_ht; // m
    double wind_speed; // m/s
    double rainfall_rate; // mm/hr
    double free_space_path_loss; // dB
    double diffraction_loss; // dB
    double rain_attenuation; // dB
    double sea_spray_attenuation; // dB
    double total_attenuation; //

private:
    void set_free_space_path_loss();
    void set_diffraction_loss();
    void set_rain_attenuation();
    void set_sea_spray_attenuation();
};

#endif //RADIOATTENUATION_H