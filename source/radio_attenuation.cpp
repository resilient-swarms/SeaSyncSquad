#include "radio_attenuation.h"
#include <cmath>
#include <array>
#include <algorithm>

void RadioAttenuation::create_equidistant_points(const union Coordinates_3D& p1,
                                                 const union Coordinates_3D& p2,
                                                 double time,
                                                 std::vector<union Coordinates_3D>& points,
                                                 std::vector<double>& offsets)
{
    // Calculate the offsets along the line (m)
    // Lets set the offset as (peak wave length)/10.
    // double peak_freq = sea_surface_get_peak_spectral_frequency(this->sea);
    // double peak_wave_length = G/(2.0 * PI * peak_freq*peak_freq);
    // double d = peak_wave_length/10.0; // offset distance along the line (m)
    // double d = 10.0; // offset distance along the line (m)
    double d = this->communication_distance/100.0;

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
        p.keys.z = sea_surface_get_elevation(this->sea, p, time);
        points.push_back(p);
        offsets.push_back(sigma_d);
        sigma_d += d;
    }
    // Finally add the end point to the list
    union Coordinates_3D p{p2.keys.x, p2.keys.y, 0.0};
    p.keys.z = sea_surface_get_elevation(this->sea, p, time);
    points.push_back(p);
    offsets.push_back(total_d);
}

void RadioAttenuation::set_free_space_path_loss() {
    // Ref: ITU-R P.525-4
    double f = this->radio_frequency/1000000.0; // MHz
    double d = this->communication_distance/1000.0; // Km
    this->free_space_path_loss = 32.4 + 20.0*std::log10(f); + 20.0*std::log10(d);
}

void RadioAttenuation::set_diffraction_loss() {
    // Ref: ITU-R P.526-15 Section 4.5 and 4.1
    this->diffraction_loss = 0.0;

    for(double time = 0; time < this->total_simulation_time; time += this->time_step_size/1000.0) {
        // We will use a fixed rudder angle for both ASVs
        double rudder_angle = PI/4; // rad

        // Update ASV dynamics
        wave_glider_compute_dynamics(this->asv1, rudder_angle, this->time_step_size);
        wave_glider_compute_dynamics(this->asv2, rudder_angle, this->time_step_size);

        union Coordinates_3D p1 = asv_get_position_origin(this->asv1);
        union Coordinates_3D p2 = asv_get_position_origin(this->asv2);
        double R = 6378.0; // Radius of earth in Km
        double Ce = 1/R; // Curvature of earth in Km^-1
        double C = 299'792'458.0; // speed of light in m/s
        double f = this->radio_frequency; // Hz
        double lambda = C/f; // wavelength in m
        double h_ts = p1.keys.z + this->antenna_height; // height of transmitter in m.
        double h_rs = p2.keys.z + this->antenna_height; // height of receiver in m.
        double d = sqrt((p2.keys.x - p1.keys.x)*(p2.keys.x - p1.keys.x) + (p2.keys.y - p1.keys.y)*(p2.keys.y - p1.keys.y)) / 1000.0; // Total distance between p1 and p2 in Km
        double S_tr = (h_rs - h_ts)/d; // Slope of line from transmitter to receiver in m/Km

        // Create equidistant points
        std::vector<union Coordinates_3D> sea_points;
        std::vector<double> offsets;
        create_equidistant_points(p1, p2, time,  sea_points, offsets);

        std::vector<double> heights; // heights of each intermediate profile point, in m, above LoS
        std::vector<double> d1s;     // distance of each intermediate profile point from transmitter in Km
        std::vector<double> d2s;     // distance of each intermediate profile point to receiver in Km
        std::vector<double> Sts;     // Slopes of each intermediate profile point to transmitter in m/Km
        std::vector<double> Srs;     // Slopes of each intermediate profile point to receiver in m/Km
        std::vector<double> nus;     // diffraction parameters nu
        int n_intermediate_profile_points = sea_points.size() - 1;
        for(int i = 1; i < n_intermediate_profile_points; ++i)
        {
            union Coordinates_3D p = sea_points[i];
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

        // Calculate and the Bullington diffraction loss for the path
        double diffraction_loss = L_uc + (1 - exp(-L_uc/6.0))*(10.0 + 0.02*d); // dB
        if(diffraction_loss > this->diffraction_loss) {
            this->diffraction_loss = diffraction_loss;
        }
    }
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
    // Get mean wavenumber
    double mean_wavenumber = sea_surface_get_mean_wavenumber(this->sea);
    // Calculate volume flux
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

RadioAttenuation::RadioAttenuation(double radio_frequency, Polarisation polarisation, double communication_distance, double significant_wave_ht, double wind_speed, double rainfall_rate) {
    this->radio_frequency = radio_frequency;
    this->polarisation = polarisation;
    this->communication_distance = communication_distance;
    this->significant_wave_ht = significant_wave_ht;
    this->wind_speed = wind_speed;
    this->rainfall_rate = rainfall_rate;

    // Set simulation clock
    this->time_step_size = 40.0; // milli-sec
    this->total_simulation_time = 15.0 * 60.0; // sec

    // Set antenna height
    this->antenna_height = 2.0; // m

    // Initialise the sea surface
    double wave_dp = PI/3.0; // rad
    int count_component_waves = 15;
    int wave_rand_seed = 1;
    this->sea = sea_surface_new(this->significant_wave_ht, wave_dp, wave_rand_seed, count_component_waves);

    // Set ASV spec
    this->asv_spec = new struct Asv_specification;
    this->asv_spec->L_wl = 2.1; // m
    this->asv_spec->B_wl = 0.6; // m
    this->asv_spec->D = 0.25; // m
    this->asv_spec->T = 0.09; // m
    this->asv_spec->max_speed = 4.0; // m/s
    this->asv_spec->disp = 0.09; // m3
    this->asv_spec->r_roll = 0.2; // m
    this->asv_spec->r_pitch = 0.6; // m
    this->asv_spec->r_yaw = 0.06; // m
    this->asv_spec->cog = {1.05, 0.0, -0.2}; // {m, m, m}
    bool is_surge_sway_halt_set = true;

    // Init ASV-1
    double x1 = 100.0; // m
    double y1 = 100.0; // m
    double z = 0.0; // m
    union Coordinates_3D position1 {x1, y1, z}; // {m, m, m}
    union Coordinates_3D attitude1 {0.0, 0.0, 0.0}; // {rad, rad, rad}
    this->asv1 = asv_new(*asv_spec, this->sea, position1, attitude1);
    asv_set_surge_sway_halt(asv1, is_surge_sway_halt_set);

    // Init ASV-2
    double x2 = x1 + this->communication_distance; // m
    double y2 = y1;
    union Coordinates_3D position2 {x2, 100.0, 0.0}; // {m, m, m}
    union Coordinates_3D attitude2 {0.0, 0.0, 0.0}; // {rad, rad, rad}
    this->asv2 = asv_new(*asv_spec, this->sea, position2, attitude2);
    asv_set_surge_sway_halt(asv2, is_surge_sway_halt_set);

    // Calculate radio attenuation
    this->set_free_space_path_loss();
    this->set_diffraction_loss();
    this->set_rain_attenuation();
    this->set_sea_spray_attenuation();
    this->total_attenuation = this->free_space_path_loss + this->diffraction_loss + this->rain_attenuation + this->sea_spray_attenuation;
}

RadioAttenuation::~RadioAttenuation() {
    // Clean memory
    asv_delete(asv1);
    asv_delete(asv2);
    sea_surface_delete(sea);
    delete this->asv_spec;
}

