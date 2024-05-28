#define _USE_MATH_DEFINES
#include <cmath>
#include "matplotlibcpp.h"
#include "simulate_LOS.h"

#include <iostream>

namespace plt = matplotlibcpp;


Simulate_LOS::Simulate_LOS(Sea_surface* sea, Asv* asv1, Asv* asv2, double antenna_height)
{
    this->sea  = sea;
    this->asv1 = asv1;
    this->asv2 = asv2;
    this->antenna_height = antenna_height;
    this->time_step_size = 40.0; // milli-sec
}


void Simulate_LOS::create_equidistant_points(const union Coordinates_3D& p1, 
                                             const union Coordinates_3D& p2, 
                                             double time,
                                             std::vector<union Coordinates_3D>& points, 
                                             std::vector<double>& offsets)
{
    double d = 1.0; // offset distance along the line (m)
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


void Simulate_LOS::create_fresnel_zone(double radio_wave_frequency, std::vector<union Coordinates_3D>& points, std::vector<std::array<double, 2>>& fresnel_limits)
{
    // Equation of the first Fresnel zone
    // r = sqrt(d1*d2/D * c/f)
    // where:
    // c = speed of light = 2.997*10^8 (m/s)
    // d1 and d2 the distance to each antenna (m).
    // D = total distance (m) = d1 + d2
    // f = radaio wave frequency (Hz)
    double c = 2.997 * 100000000;
    double f = radio_wave_frequency;
    double x_0 = points[0].keys.x;
    double y_0 = points[0].keys.y;
    double z_0 = points[0].keys.z + this->antenna_height;
    int n = points.size() - 1;
    double x_n = points[n].keys.x;
    double y_n = points[n].keys.y;
    double z_n = points[n].keys.z + + this->antenna_height;
    double D = sqrt((x_n - x_0)*(x_n - x_0) + (y_n - y_0)*(y_n - y_0) + (z_n - z_0)*(z_n - z_0));
    for(auto& point : points)
    {
        double x = point.keys.x;
        double y = point.keys.y;
        double z = point.keys.z;
        double d_1 = sqrt((x - x_0)*(x - x_0) + (y - y_0)*(y - y_0) + (z - z_0)*(z - z_0));
        double d_2 = sqrt((x_n - x)*(x_n - x) + (y_n - y)*(y_n - y) + (z_n - z)*(z_n - z));
        double r = sqrt(d_1 * d_2 * c / (D * f));
        double z_los = z_0 + (d_1/D)*(z_n - z_0);
        std::array<double, 2> fresnel_limits_at_point{z_los - r, z_los + r};
        fresnel_limits.push_back(fresnel_limits_at_point);
    }
}


void Simulate_LOS::run()
{
    std::vector<double> times; // vector to store all time steps. Used for plotting. 
    std::vector<double> fresnel_clearenceses; // Fresnel zone clearence for all time steps
    std::vector<double> fresnel_clearence_min_limit; // Min required clearence of Fresnel zone = 60%

    for(int t = 0; ;++t)
    {
        std::vector<double> wave_elevations;
        std::vector<double> waveform;
        double time = t * this->time_step_size/1000.0;

        // We will use a fixed rudder angle for both ASVs
        double rudder_angle = PI/4; // rad

        // Update ASV dynamics
        wave_glider_compute_dynamics(this->asv1, rudder_angle, this->time_step_size);
        wave_glider_compute_dynamics(this->asv2, rudder_angle, this->time_step_size);

        // Get position and attitude of the ASVs
        union Coordinates_3D asv1_position = asv_get_position_origin(this->asv1);
        union Coordinates_3D asv2_position = asv_get_position_origin(this->asv2);
        union Coordinates_3D asv1_attitude = asv_get_attitude(this->asv1);
        union Coordinates_3D asv2_attitude = asv_get_attitude(this->asv2);

        // Create eqidistant points 
        std::vector<union Coordinates_3D> sea_points;
        std::vector<double> offsets;
        create_equidistant_points(asv1_position, asv2_position, time,  sea_points, offsets);

        // Create the Fresnel zone
        std::vector<std::array<double, 2>> ellipse_z;
        double radio_frequency = 2.4 * 1000000000; // Hz
        create_fresnel_zone(radio_frequency, sea_points, ellipse_z);
        
        // Plot
        // Clear previous plot
		plt::clf(); 
        // Plot sea surface
        std::vector<double> x_sea, y_sea;
        for(int i = 0; i < sea_points.size(); ++i)
        {
            x_sea.push_back(offsets[i]);
            y_sea.push_back(sea_points[i].keys.z);
        }
        plt::subplot(2, 1, 1);
        plt::named_plot("Ocean surface", x_sea, y_sea, "cornflowerblue");
        // Plot Fresnel zone
        double min_clearence_current_timestep = 100.0;
        bool legend_inserted_once = false;
        for(int i = 0; i < sea_points.size(); ++i)
        {
            double y1 = ellipse_z[i][0];
            double y2 = ellipse_z[i][1]; 
            double x0 = x_sea[i];
            double y0 = y_sea[i];
            std::vector<double> x_ellipse_section;
            x_ellipse_section.push_back(x0);
            x_ellipse_section.push_back(x0);
            std::vector<double> y_ellipse_section_clear;
            std::vector<double> y_ellipse_section_blocked;
            if(y0 < y1)
            {
                
                y_ellipse_section_clear.push_back(y1);
                y_ellipse_section_clear.push_back(y2);
                plt::plot(x_ellipse_section, y_ellipse_section_clear, "g");
            }
            else if(y0 > y2)
            {
                y_ellipse_section_blocked.push_back(y1);
                y_ellipse_section_blocked.push_back(y2);
                plt::plot(x_ellipse_section, y_ellipse_section_blocked, "r");
                min_clearence_current_timestep = 0.0;
            }
            else
            {
                y_ellipse_section_blocked.push_back(y1);
                y_ellipse_section_blocked.push_back(y0);
                y_ellipse_section_clear.push_back(y0);
                y_ellipse_section_clear.push_back(y2);
                if(! legend_inserted_once)
                {
                    plt::named_plot("Clear Fresnel zone", x_ellipse_section, y_ellipse_section_clear, "g");
                    plt::named_plot("Wave obstructed Fresnel zone", x_ellipse_section, y_ellipse_section_blocked, "r");
                    legend_inserted_once = true;
                }
                else
                {
                    plt::plot(x_ellipse_section, y_ellipse_section_clear, "g");
                    plt::plot(x_ellipse_section, y_ellipse_section_blocked, "r");
                }
                double clearence = (y2-y0)/(y2-y1)*100;
                if(min_clearence_current_timestep > clearence)
                {
                    min_clearence_current_timestep = clearence;
                }
            }
        }
        // Plot antenna positions
        int n = offsets.size() - 1;
        std::vector<double> antenna_1_x {offsets[0], offsets[0]};
        std::vector<double> antenna_1_y {sea_points[0].keys.z, sea_points[0].keys.z+antenna_height};
        plt::named_plot("Antenna", antenna_1_x, antenna_1_y, "black");
        std::vector<double> antenna_2_x {offsets[n], offsets[n]};
        std::vector<double> antenna_2_y {sea_points[n].keys.z, sea_points[n].keys.z+antenna_height};
        plt::plot(antenna_2_x, antenna_2_y, "black");
        plt::text(offsets[0], -5.0, "Tx");
        plt::text(offsets[n], -5.0, "Rx");
        // Extra info
        int wave_count = sea_surface_get_count_component_waves(this->sea);
        const struct Regular_wave* peak_wave = sea_surface_get_regular_wave_at(this->sea, (wave_count-1)/2);
        double peak_wave_length = regular_wave_get_length(peak_wave);
        plt::text(0.0, -10.0, "Ocean wave length = "+std::to_string(peak_wave_length)+" m");
        double sig_wave_height = sea_surface_get_significant_height(this->sea);
        plt::text(30.0, -10.0, "Significant wave height = "+std::to_string(sig_wave_height)+" m");
        plt::text(60.0, -10.0, "Antenna height = "+std::to_string(antenna_height)+" m");
        plt::text(90.0, -10.0, "Radio freq = "+std::to_string(radio_frequency/1000000000)+" GHz");
        double max_fresnel_radius = ellipse_z[int(ellipse_z.size()/2)][1] - ellipse_z[int(ellipse_z.size()/2)][0];
        plt::text(0.0, 10.0, "Max Fresnel zone radius = "+std::to_string(max_fresnel_radius)+" m");
        plt::title("Ocean Waves Blocking Fresnel Zone");
        plt::xlabel("Distance along the line joining the antennas Tx and Rx (m)\n");
        plt::ylabel("Z (m)");
        plt::legend();
        plt::axis("equal");

        times.push_back(time);
        fresnel_clearenceses.push_back(min_clearence_current_timestep);
        fresnel_clearence_min_limit.push_back(60.0);

        // Plot the Fresnel zone obstruction
        plt::subplot(2, 1, 2);
        plt::named_plot("Fresnel zone clearance", times, fresnel_clearenceses, "grey");
        plt::named_plot("Minimum required Fresnel zone clearance", times, fresnel_clearence_min_limit, "r--");
        plt::title("Fresnel Zone Cleareance");
        plt::xlabel("Time (sec)");
        plt::ylabel("Fresnel zone clearance (%)");
        plt::legend();

        plt::tight_layout();
        plt::pause(0.04);
    }
}


void Simulate_LOS::run_headless(std::ofstream& fout)
{
    double sig_wave_height = sea_surface_get_significant_height(this->sea);
    double wave_dp = sea_surface_get_predominant_heading(this->sea);
    int seed = sea_surface_get_rand_seed(this->sea);    

    double simulation_length = 15.0 * 60.0; // sec
    for(double time = 0; time < simulation_length; time+=this->time_step_size/1000.0)
    {
        // We will use a fixed rudder angle for both ASVs
        double rudder_angle = PI/4; // rad

        // Update ASV dynamics
        wave_glider_compute_dynamics(this->asv1, rudder_angle, this->time_step_size);
        wave_glider_compute_dynamics(this->asv2, rudder_angle, this->time_step_size);

        // Get position and attitude of the ASVs
        union Coordinates_3D asv1_position = asv_get_position_origin(this->asv1);
        union Coordinates_3D asv2_position = asv_get_position_origin(this->asv2);
        union Coordinates_3D asv1_attitude = asv_get_attitude(this->asv1);
        union Coordinates_3D asv2_attitude = asv_get_attitude(this->asv2);

        // Distance between antennas
        double distance = sqrt((asv1_position.keys.x - asv2_position.keys.x)*(asv1_position.keys.x - asv2_position.keys.x) +  
                               (asv1_position.keys.y - asv2_position.keys.y)*(asv1_position.keys.y - asv2_position.keys.y));

        // Antenna alignment angle w.r.t North
        double antennas_alignment_to_north =  atan2(asv2_position.keys.x - asv1_position.keys.x, asv2_position.keys.y - asv1_position.keys.y);

        // Peak wave length
        int wave_count = sea_surface_get_count_component_waves(this->sea);
        const struct Regular_wave* peak_wave = sea_surface_get_regular_wave_at(this->sea, (wave_count-1)/2);
        double peak_wave_length = regular_wave_get_length(peak_wave);

        // Create eqidistant points 
        std::vector<union Coordinates_3D> sea_points;
        std::vector<double> offsets;
        create_equidistant_points(asv1_position, asv2_position, time,  sea_points, offsets);

        // Create the Fresnel zone
        std::vector<std::array<double, 2>> ellipse_z;
        double radio_frequency = 2.4 * 1000000000; // Hz
        create_fresnel_zone(radio_frequency, sea_points, ellipse_z);

        // Calculate clearence
        double min_clearence_current_timestep = 100.0;
        for(int i = 0; i < sea_points.size(); ++i)
        {
            double y1 = ellipse_z[i][0];
            double y2 = ellipse_z[i][1]; 
            double y0 = sea_points[i].keys.z;
            if(y0 < y1)
            {
                // No wave blocking
            }
            else if(y0 > y2)
            {
                min_clearence_current_timestep = 0.0;
            }
            else
            {
                double clearence = (y2-y0)/(y2-y1)*100;
                if(min_clearence_current_timestep > clearence)
                {
                    min_clearence_current_timestep = clearence;
                }
            }
        }
        // Write to file
        fout << distance << "," 
             << this->antenna_height << "," 
             << sig_wave_height << "," 
             << wave_dp << "," 
             << antennas_alignment_to_north << "," 
             << peak_wave_length << ","
             << seed << "," 
             << time << "," 
             << min_clearence_current_timestep << "\n";
    }
}