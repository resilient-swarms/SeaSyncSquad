#include "radio_attenuation.h"
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <numeric>
#include <utility>
extern "C" {
#include "constants.h"
}

double get_free_space_path_loss(double radio_frequency, double communication_distance) {
    // Ref: ITU-R P.525-4
    double f = radio_frequency/1'000'000.0; // MHz
    double d = communication_distance/1000.0; // Km
    double free_space_path_loss = 32.4 + 20.0*std::log10(f) + 20.0*std::log10(d);
    return free_space_path_loss;
}

double get_rain_attenuation(double radio_frequency, double communication_distance, Polarisation polarisation, double rainfall_rate) {
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
        double f = radio_frequency/1000000000.0; // GHz
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
        double f = radio_frequency/1000000000.0; // GHz
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
    if(polarisation == Polarisation::horizontal) {
        log10_k = calc_log10_k(a_k_h, b_k_h, c_k_h, M_k_h, C_k_h);
    } else {
        log10_k = calc_log10_k(a_k_v, b_k_v, c_k_v, M_k_v, C_k_v);
    }
    double k = std::pow(10.0, log10_k);
    // Calculate alpha
    double alpha = 0.0;
    if(polarisation == Polarisation::horizontal) {
        alpha = calc_alpha(a_alpha_h, b_alpha_h, c_alpha_h, M_alpha_h, C_alpha_h);
    } else {
        alpha = calc_alpha(a_alpha_v, b_alpha_v, c_alpha_v, M_alpha_v, C_alpha_v);
    }
    // Rain attenuation
    double d = communication_distance/1000.0; // Km
    double rain_attenuation =  d * k * std::pow(rainfall_rate, alpha); // dB
    return rain_attenuation;
}


double get_sea_spray_attenuation(double radio_frequency, double communication_distance, Sea_surface* sea, double wind_speed ) {
    // There are two parts to this caluclation
    // part 1 - estimation of volume of sea spray in air, computed as a function of sea state and wind speed,
    // part 2 - estimation of signal attenuation, based on ITU-R P.840.9

    // Part - 1
    // Ref: A Wind–Wave-Dependent Sea Spray Volume Flux Model Based on Field Experiments, Xingkun Xu, 2021
    // V = U * 1.99 * sqrt(s) * 10^-8
    // where:
    // V - is sea spray volume flux in (m3/s)/(m2)
    // U - wind velocity in m/s
    // s - wave steepness
    // s = Hs * k / 2
    // k - mean wavenumber
    // Hs - significant wave height
    // Get mean wavenumber
    double mean_wavenumber = sea_surface_get_mean_wavenumber(sea);
    // Calculate volume flux
    double significant_wave_ht = sea_surface_get_significant_height(sea);
    double s = significant_wave_ht * mean_wavenumber / 2.0;
    double V = wind_speed * 1.99 * std::sqrt(s) / std::pow(10, 8); // (m3/s)/(m2)
    // Calculate mass flux
    double sea_water_density = 1025 * 1000; // g/m3
    double M = V * sea_water_density;

    // Part - 2
    // Ref - ITU-R P.840-9
    double f = radio_frequency/1'000'000'000.0; // GHz
    double T = 25.0; // deg C
    T = T + 273.15; // Kelvim
    double epsilon_0 = 77.66 + 103.3 * (300.0/T - 1);
    double epsilon_1 = 0.0671 * epsilon_0;
    double epsilon_2 = 3.52;
    double f_p = 20.20 - 146.0*(300.0/T - 1) + 316.0*std::pow(300.0/T - 1, 2); // GHz
    double f_s = 39.8 * f_p;
    double epsilon__ = f*(epsilon_0 - epsilon_1)/(f_p*(1 + std::pow(f/f_p, 2))) + f*(epsilon_1 - epsilon_2)/(f_s*(1 + std::pow(f/f_s, 2)));
    double epsilon_ = (epsilon_0 - epsilon_1)/(1 + std::pow(f/f_p, 2)) + (epsilon_1 - epsilon_2)/(1 + std::pow(f/f_s, 2)) + epsilon_2;
    double eta = (2.0 + epsilon_)/epsilon__;
    double K = 0.819*f/(epsilon__*(1 + eta*eta)); // (dB/km)/(g/m3)
    // Calculate total attenuation due to sea water
    double d = communication_distance/1000.0; // Km
    double sea_spray_attenuation = K * M * d;
    return sea_spray_attenuation;
}


std::vector<std::pair<double, Coordinates_3D>> generate_sea_surface_between_points(Sea_surface* sea, const union Coordinates_3D& p1, const union Coordinates_3D& p2, double time)
{
    std::vector<std::pair<double, Coordinates_3D>> result;

    // Calculate the offsets along the line (m)
    // Lets set the offset as (peak wave length)/10.
    // double peak_freq = sea_surface_get_peak_spectral_frequency(this->sea);
    // double peak_wave_length = G/(2.0 * PI * peak_freq*peak_freq);
    // double d = peak_wave_length/10.0; // offset distance along the line (m)
    double d = 10.0; // offset distance along the line (m)

    double sigma_d = 0.0; // cumulative offsets along the line (m)
    double total_d = sqrt((p2.keys.x - p1.keys.x)*(p2.keys.x - p1.keys.x) +
                          (p2.keys.y - p1.keys.y)*(p2.keys.y - p1.keys.y)); // distance between p1 and p2 (m)
    // Compute the angle of the line with the x-axis
    double theta;
    if(p2.keys.x == p1.keys.x)
    {
        theta = PI/2;
    }
    else
    {
        theta = atan2(p2.keys.y - p1.keys.y, p2.keys.x - p1.keys.x);
    }
    // Compute the points along the line
    while(sigma_d < total_d)
    {
        // Compute the coordinates of the new point
        union Coordinates_3D p;
        p.keys.x = p1.keys.x + sigma_d*cos(theta);
        p.keys.y = p1.keys.y + sigma_d*sin(theta);
        p.keys.z = sea_surface_get_elevation(sea, p, time);
        result.push_back(std::make_pair(sigma_d, p));
        sigma_d += d;
    }
    // Finally add the end point to the list
    union Coordinates_3D p{p2.keys.x, p2.keys.y, 0.0};
    p.keys.z = sea_surface_get_elevation(sea, p, time);
    result.push_back(std::make_pair(total_d, p));

    return std::move(result);
}


std::vector<std::pair<double, double>> generate_fresnel_zone(double radio_frequency, double antenna_height, std::vector<std::pair<double, Coordinates_3D>>& points)
{
    std::vector<std::pair<double, double>> result;
    // Equation of the first Fresnel zone
    // r = sqrt(d1*d2/D * c/f)
    // where:
    // c = speed of light = 2.997*10^8 (m/s)
    // d1 and d2 the distance to each antenna (m).
    // D = total distance (m) = d1 + d2
    // f = radaio wave frequency (Hz)
    double c = 2.997 * 100000000;
    double f = radio_frequency; // Hz
    double x_0 = points[0].second.keys.x;
    double y_0 = points[0].second.keys.y;
    double z_0 = points[0].second.keys.z + antenna_height; // antenna height in m.
    int n = points.size() - 1;
    double x_n = points[n].second.keys.x;
    double y_n = points[n].second.keys.y;
    double z_n = points[n].second.keys.z + + antenna_height;
    double D = sqrt((x_n - x_0)*(x_n - x_0) + (y_n - y_0)*(y_n - y_0) + (z_n - z_0)*(z_n - z_0));
    for(auto& point : points)
    {
        double x = point.second.keys.x;
        double y = point.second.keys.y;
        double z = point.second.keys.z;
        double d_1 = sqrt((x - x_0)*(x - x_0) + (y - y_0)*(y - y_0) + (z - z_0)*(z - z_0));
        double d_2 = sqrt((x_n - x)*(x_n - x) + (y_n - y)*(y_n - y) + (z_n - z)*(z_n - z));
        double r = sqrt(d_1 * d_2 * c / (D * f));
        double z_los = z_0 + (d_1/D)*(z_n - z_0);
        std::pair<double, double> fresnel_limits_at_point = std::make_pair(z_los - r, z_los + r);
        result.push_back(fresnel_limits_at_point);
    }
    return std::move(result);
}


std::pair<double, double> get_min_fresnel_clearance_and_diffraction_loss(double radio_frequency, double antenna_height, double communication_distance, Sea_surface* sea, std::pair<Coordinates_3D, Coordinates_3D> antenna_positions, double time, double time_step_size ) {
    // Ref: ITU-R P.526-15 Section 4.5 and 4.1

    union Coordinates_3D p1 = antenna_positions.first;
    union Coordinates_3D p2 = antenna_positions.second;

    // Create sea surface points between the two ASVs
    std::vector<std::pair<double, Coordinates_3D>>sea_surface_points_and_offsets = generate_sea_surface_between_points(sea, p1, p2, time);

    // Compute the fresnel clearance
    double min_clearance_current_timestep = 100.0; // % clearance
    std::vector<std::pair<double, double>> ellipse_z = generate_fresnel_zone(radio_frequency, antenna_height, sea_surface_points_and_offsets);
    for(int i = 0; i < sea_surface_points_and_offsets.size(); ++i)
    {
        double z1 = ellipse_z[i].first;
        double z2 = ellipse_z[i].second;
        double z0 = sea_surface_points_and_offsets[i].second.keys.z;
        if(z0 < z1)
        {
            // No wave blocking
        }
        else if(z0 > z2)
        {
            min_clearance_current_timestep = 0.0;
        }
        else
        {
            double clearance = (z2-z0)/(z2-z1)*100;
            if(min_clearance_current_timestep > clearance)
            {
                min_clearance_current_timestep = clearance;
            }
        }
    }

    // Compute diffraction loss
    double R = 6378.0; // Radius of earth in Km
    double Ce = 1/R; // Curvature of earth in Km^-1
    double C = 299'792'458.0; // speed of light in m/s
    double f = radio_frequency; // Hz
    double lambda = C/f; // wavelength in m
    double h_ts = p1.keys.z + antenna_height; // height of transmitter in m.
    double h_rs = p2.keys.z + antenna_height; // height of receiver in m.
    double d = sqrt((p2.keys.x - p1.keys.x)*(p2.keys.x - p1.keys.x) + (p2.keys.y - p1.keys.y)*(p2.keys.y - p1.keys.y)) / 1000.0; // Total distance between p1 and p2 in Km
    double S_tr = (h_rs - h_ts)/d; // Slope of line from transmitter to receiver in m/Km, Ref ITU-R P.526-15, Section 4.5.1, Eq-49
    std::vector<double> heights; // heights of each intermediate profile point, in m, above LoS
    std::vector<double> d1s;     // distance of each intermediate profile point from transmitter in Km
    std::vector<double> d2s;     // distance of each intermediate profile point to receiver in Km
    std::vector<double> Sts;     // Slopes of each intermediate profile point to transmitter in m/Km
    std::vector<double> Srs;     // Slopes of each intermediate profile point to receiver in m/Km
    std::vector<double> nus;     // diffraction parameters nu
    int n_intermediate_profile_points = sea_surface_points_and_offsets.size() - 1;
    for(int i = 1; i < n_intermediate_profile_points; ++i)
    {
        union Coordinates_3D p = sea_surface_points_and_offsets[i].second;
        // Calculate the Z for LOS at the location of the current point
        double d1 = sqrt((p.keys.x - p1.keys.x)*(p.keys.x - p1.keys.x) +
                         (p.keys.y - p1.keys.y)*(p.keys.y - p1.keys.y)) / 1000.0; // Km
        double d2 = d - d1; // Km
        double z_los = h_ts + (h_ts - h_rs)/d * d1; // height of LoS in m
        // Calculate the height of the current point above LOS
        double h = p.keys.z - z_los; // Height is positive for points above LoS else negative.
        double St = (h + 500*Ce*d1*d2 - h_ts)/d1; // m/Km
        double Sr = (h + 500*Ce*d1*d2 - h_rs)/d2; // m/Km
        double nu = (h + 500*Ce*d1*d2 - (h_ts*d2 + h_rs*d1)/d) * sqrt((0.002*d)/(lambda*d1*d2));
        heights.push_back(h);
        d1s.push_back(d1);
        d2s.push_back(d2);
        Sts.push_back(St);
        Srs.push_back(Sr);
        nus.push_back(nu);
    }
    // Find the intermediate profile point with the hightest slope.
    double St_max = *std::max_element(Sts.begin(), Sts.end());
    // Calculate knife-edge loss for the Bullington point
    auto knife_edge_loss = [](double nu)
    {
        double loss = (nu > -0.78)? 6.9 + 20.0 * std::log10(sqrt(pow(nu-0.1, 2.0) + 1.0) + nu -0.1) : 0.0; // dB
        return loss;
    };
    double L_uc = 0.0;
    if(St_max < S_tr)
    {
        // Path is LoS
        double nu_max = *std::max_element(nus.begin(), nus.end());
        L_uc = knife_edge_loss(nu_max);
    }
    else
    {
        // Path is trans-horizon
        double Sr_max = *std::max_element(Srs.begin(), Srs.end());
        double db = (h_rs - h_ts + Sr_max*d)/(St_max + Sr_max); // Km
        double nu_b = (h_ts + St_max*db - (h_ts*(d-db) + h_rs*db)/d) * sqrt((0.002*d)/(lambda*db*(d-db)));
        L_uc = knife_edge_loss(nu_b);
    }
    // Calculate Bullington diffraction loss for the path
    double diffraction_loss = L_uc + (1 - exp(-L_uc/6.0))*(10.0 + 0.02*d); // dB

    return std::make_pair(min_clearance_current_timestep, diffraction_loss);
}