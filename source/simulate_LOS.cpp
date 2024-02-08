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

void Simulate_LOS::create_ellipse(double radio_wave_length, std::vector<union Coordinates_3D>& points, std::vector<std::array<double, 2>>& ellipse_z)
{
    // Equation of ellipse
    // Ax^2 + Bxy + Cy^2 + Dx + Ey + F = 0
    // provided B^2 - 4AC < 0
    // where:
    // A = a^2 sin^2(theta) + b^2 cos^2(theta)
    // B = 2(a^2 - b^2) sin(theta) cos(theta)
    // C = a^2 cos^2(theta) + b^2 sin^2(theta)
    // D = -2A x_c - B y_c
    // E = -B x_c - 2C y_c
    // F = A(x_c)^2 + B x_c y_c + C(y_c)^2 - (a^2)(b^2)
    // a = semi-major axis
    // b = semi-minor axis
    // theta = angle between semi-major axis and x-axis
    // (x_c, y_c) = coordinates of center of the ellipse

    // Max Fresnel zone radius r = sqrt(lambda*d/4)
    // where:
    // lambda = wave length
    // d = distance between the antennas
    double x_0 = points[0].keys.x;
    double y_0 = points[0].keys.y;
    double z_0 = points[0].keys.z + this->antenna_height;
    int n = points.size() - 1;
    double x_n = points[n].keys.x;
    double y_n = points[n].keys.y;
    double z_n = points[n].keys.z + + this->antenna_height;
    double x_c = (x_n + x_0)/2.0;
    double y_c = (z_n + z_0)/2.0; // The equation of the ellipse were written for XY plane, but the Fresnel ellipse is in the XZ plane
    double d = sqrt((x_n - x_0)*(x_n - x_0) + (y_n - y_0)*(y_n - y_0) + (z_n - z_0)*(z_n - z_0));
    double lambda = radio_wave_length;
    double r = sqrt(lambda*d/4.0);
    double theta;
    if(x_0 == x_n)
    {
        theta = PI/2;
    }
    else 
    {
        theta = atan2(z_n - z_0, x_n - x_0);
    }

    // The ellipse
    double b = r; // semi-minor axis
    double a = sqrt(d*d/4 + r*r); // semi-major axis
    double A = a*a*sin(theta)*sin(theta) + b*b*cos(theta)*cos(theta);
    double B = 2*(a*a - b*b)*sin(theta)*cos(theta);
    double C = a*a*cos(theta)*cos(theta) + b*b*sin(theta)*sin(theta);
    double D = -2*A*x_c - B*y_c;
    double E = -B*x_c - 2*C*y_c;
    double F = A*x_c*x_c + B*x_c*y_c + C*y_c*y_c - a*a*b*b;

    // For the x coordinates in the points compute the correspoinding y coordinates on the ellipse.
    // From the above equation of ellipse, given x,
    // y = (-(Bx + E) +- sqrt((Bx+E)^2 - 4C(Ax^2 + Dx + F)))/ (2(Ax^2 + Dx + F))
    // provided (Bx+E)^2 - 4C(Ax^2 + Dx + F)) > 0
    for(auto& point : points)
    {
        double x = point.keys.x;
        double y1 = (-(B*x + E) + sqrt((B*x + E)*(B*x + E) - 4*C*(A*x*x + D*x + F)))/(2*C);
        double y2 = (-(B*x + E) - sqrt((B*x + E)*(B*x + E) - 4*C*(A*x*x + D*x + F)))/(2*C);
        std::array<double, 2> y{std::min(y1, y2), std::max(y1, y2)};
        ellipse_z.push_back(y);
    }
}


void Simulate_LOS::run()
{
    plt::draw();

    for(int i = 0; ;++i)
    {
        std::vector<double> wave_elevations;
        std::vector<double> waveform;
        double time = i * this->time_step_size/1000.0;

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
        create_ellipse(0.12, sea_points, ellipse_z);
        
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
        plt::plot(x_sea, y_sea, "b");
        // Plot Fresnel zone
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
            }
            else
            {
                y_ellipse_section_blocked.push_back(y1);
                y_ellipse_section_blocked.push_back(y0);
                y_ellipse_section_clear.push_back(y0);
                y_ellipse_section_clear.push_back(y2);
                plt::plot(x_ellipse_section, y_ellipse_section_clear, "g");
                plt::plot(x_ellipse_section, y_ellipse_section_blocked, "r");
            }
        }

        plt::axis("equal");
        plt::pause(0.04);
    }
}