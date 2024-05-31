#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <array>
#include <fstream>

extern "C" 
{
    #include "constants.h"
    #include "geometry.h"
    #include "regular_wave.h"
    #include "sea_surface.h"
    #include "asv.h"
}


class Simulate_LOS{
public:
    Simulate_LOS(struct Sea_surface* sea, struct Asv* asv1, struct Asv* asv2, double antenna_height);
    void run_fresnel_zones();  
    void run_headless_fresnel_zones(std::ofstream& fout, double total_simulation_time);
    void run_headless_diffraction_and_path_loss(std::ofstream& fout, double total_simulation_time);

private:
    void create_equidistant_points(const union Coordinates_3D& p1, 
                                   const union Coordinates_3D& p2, 
                                   double time,
                                   std::vector<union Coordinates_3D>& points, 
                                   std::vector<double>& offsets);
    
    void create_fresnel_zone(double radio_wave_frequency, 
                            std::vector<union Coordinates_3D>& points, 
                            std::vector<std::array<double, 2>>& fresnel_ellipse_z);

    double get_diffraction_loss(const union Coordinates_3D& p1, const union Coordinates_3D& p2, double time);
    
    // void create_ellipse(double lambda, 
    //                     std::vector<union Coordinates_3D>& points, 
    //                     std::vector<std::array<double, 2>>& ellipse_z);

private:
    struct Sea_surface* sea;
    struct Asv* asv1;
    struct Asv* asv2;
    double antenna_height; // m
    double time_step_size; // milli-sec
    double radio_frequency;// GHz
};

#endif // SIMULATION_H