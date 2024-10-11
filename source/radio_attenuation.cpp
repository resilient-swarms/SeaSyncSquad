#include "radio_attenuation.h"
#include <cmath>
#include <array>
extern "C" {
    #include "sea_surface.h"
}

void RadioAttenuation::set_free_space_path_loss() {
    // Ref: ITU-R P.525-4
    double f = this->radio_frequency/1000000.0; // MHz
    double d = this->communication_distance/1000.0; // Km
    this->free_space_path_loss = 32.4 + 20.0*std::log10(f); + 20.0*std::log10(d);
}


void RadioAttenuation::set_diffraction_loss() {
    // TODO
    this->diffraction_loss = 0.0;
}


void RadioAttenuation::set_rain_attenuation() {
    // Ref: ITU-R P.838-3 - Specific attenuation model for rain for use in prediction methods
    // Coefficients for kH
    std::array<double, 4> a_k_h = {-5.33908, -0.35351, -0.23789, -0.94158};
    std::array<double, 4> b_k_h = {-0.10008,  1.26970,  0.86036,  0.64552};
    std::array<double, 4> c_k_h = {1.13098,   0.45400,  0.15354,  0.16817};
    double M_k_h = -0.18961;
    double C_k_h = 0.71147;
    // Coefficients for kv
    std::array<double, 4> a_k_v = {-3.80595, -3.44965, -0.39902, 0.50167};
    std::array<double, 4> b_k_v = {0.56934,  -0.22911,  0.73042, 1.07319};
    std::array<double, 4> c_k_v = {0.81061,   0.51059,  0.11899, 0.27195};
    double M_k_v = -0.16398;
    double C_k_v = 0.63297;
    // Coefficients for alphaH
    std::array<double, 5> a_alpha_h = {-0.14318, 0.29591, 0.32177, -5.37610,  16.1721};
    std::array<double, 5> b_alpha_h = {1.82442,  0.77564, 0.63773, -0.96230, -3.29980};
    std::array<double, 5> c_alpha_h = {-0.55187, 0.19822, 0.13164,  1.47828,  3.43990};
    double M_alpha_h = 0.67849;
    double C_alpha_h = -1.95537;
    // Coefficients for alphaV
    std::array<double, 5> a_alpha_v = {-0.07771, 0.56727 , -0.20238, -48.2991,   48.5833};
    std::array<double, 5> b_alpha_v = {2.33840,  0.95545,   1.14520,  0.791669 , 0.791459};
    std::array<double, 5> c_alpha_v = {-0.76284, 0.54039,   0.26809,  0.116226,  0.116479};
    double M_alpha_v = -0.053739;
    double C_alpha_v = 0.83433;
    auto calc_log10_k = [&](
        std::array<double, 4> a,
        std::array<double, 4> b,
        std::array<double, 4> c,
        double M,
        double C)-> double {
        double f = this->radio_frequency/1000000000.0; // GHz
        double cumulative_term = 0.0;
        for (unsigned int j = 0; j < a.size(); ++j) {
            double A = std::pow((std::log10(f) - b[j])/c[j], 2.0);
            cumulative_term += a[j] * std::exp(-A);
        }
        double log10_k =  cumulative_term + M * std::log10(f) + C;
        return log10_k;
    };
    auto calc_alpha = [&](
        std::array<double, 5> a,
        std::array<double, 5> b,
        std::array<double, 5> c,
        double M,
        double C)-> double {
        double f = this->radio_frequency/1000000000.0; // GHz
        double cumulative_term = 0.0;
        for (unsigned int j = 0; j < a.size(); ++j) {
            double A = std::pow((std::log10(f) - b[j])/c[j], 2.0);
            cumulative_term += a[j] * std::exp(-A);
        }
        double alpha =  cumulative_term + M * std::log10(f) + C;
        return alpha;
    };
    // Calculate k
    double log10_k = 0.0;
    if(this->polarisation == Polarisation::horizontal) {
        log10_k = calc_log10_k(a_k_h, b_k_h, c_k_h, M_k_h, C_k_h);
    } else {
        log10_k = calc_log10_k(a_k_v, b_k_v, c_k_v, M_k_v, C_k_v);
    }
    double k = std::pow(10.0, log10_k);
    // Calculate alpha
    double alpha = 0.0;
    if(this->polarisation == Polarisation::horizontal) {
        alpha = calc_alpha(a_alpha_h, b_alpha_h, c_alpha_h, M_alpha_h, C_alpha_h);
    } else {
        log10_k = calc_alpha(a_alpha_v, b_alpha_v, c_alpha_v, M_alpha_v, C_alpha_v);
    }
    // Rain attenuation
    double d = this->communication_distance/1000.0; // Km
    this->rain_attenuation =  d * k * std::pow(this->rainfall_rate, alpha); // dB
}


void RadioAttenuation::set_sea_spray_attenuation() {
    // Ref: A Wind–Wave-Dependent Sea Spray Volume Flux Model Based on Field Experiments, Xingkun Xu, 2021
    // V = U * 1.99 * sqrt(s) * 10^-8
    // where:
    // V - is sea spray volume flux in (m3/s)/(m2)
    // U - wind velocity in m/s
    // s - wave steepness
    // s = Hs * k / 2
    // k - mean wavenumber
    // Hs - significant wave height
    // Calculate mean wavenumber
    double wave_dp = 0.0;
    int rand_seed = 1;
    int count_component_waves = 15;
    struct Sea_surface* sea = sea_surface_new(this->significant_wave_ht, wave_dp, rand_seed, count_component_waves);
    double mean_wavenumber = sea_surface_get_mean_wavenumber(sea);
    // Calculate volue flux
    double s = this->significant_wave_ht * mean_wavenumber / 2.0;
    double V = this->wind_speed * 1.99 * std::sqrt(s) / std::pow(10, 8); // (m3/s)/(m2)
    // Calculate mass flux
    double sea_water_density = 1025 * 1000; // g/m3
    double M = V * sea_water_density;
    // Calculate attenuation in (dB/km)/(g/m3)
    // Ref - Atmospheric Attenuation due to Humidity, Ch-8, Section 4, Eq. 14
    double C = 299792458;  // Speed of light in m/s
    double wave_length = C / (this->radio_frequency); // m
    wave_length = wave_length * 1000.0; // mm
    double T0 = 25;  // deg C
    double A = -1.347 + 0.0372 * wave_length + 18 / wave_length - 0.022 * T0; // (dB/km)/(g/m3)
    // Calculate total attenuation due to sea water
    double d = this->communication_distance/1000.0; // Km
    this->sea_spray_attenuation = A * M * d;
}

RadioAttenuation::RadioAttenuation(
    double radio_frequency,
    Polarisation polarisation,
    double communication_distance,
    double significant_wave_ht,
    double wind_speed,
    double rainfall_rate) {
    this->radio_frequency = radio_frequency;
    this->polarisation = polarisation;
    this->communication_distance = communication_distance;
    this->significant_wave_ht = significant_wave_ht;
    this->wind_speed = wind_speed;
    this->rainfall_rate = rainfall_rate;
    this->set_free_space_path_loss();
    this->set_diffraction_loss();
    this->set_rain_attenuation();
    this->set_sea_spray_attenuation();
    this->total_attenuation = this->free_space_path_loss + this->diffraction_loss + this->rain_attenuation + this->sea_spray_attenuation;
}
