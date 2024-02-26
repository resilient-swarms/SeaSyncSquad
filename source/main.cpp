#include <iostream>
#include "simulate_LOS.h"

extern "C" 
{
    #include "constants.h"
    #include "geometry.h"
    #include "sea_surface.h"
    #include "asv.h"
}

int run(struct Asv_specification asv_spec, bool is_surge_sway_halt_set)
{
    // Initialise the sea surface
    double wave_hs = 2.5; // m
    double wave_dp = PI/3; // rad
    int count_component_waves = 21; 
    int wave_rand_seed = 1;
    struct Sea_surface* sea = sea_surface_new(wave_hs, wave_dp, wave_rand_seed, count_component_waves);

    double antenna_height = 1.0; // m
    
    // Initialise the ASV-1
    union Coordinates_3D position1 {100.0, 100.0, 0.0}; // {m, m, m}
    union Coordinates_3D attitude1 {0.0, 0.0, 0.0}; // {rad, rad, rad}
    struct Asv* asv1 = asv_new(asv_spec, sea, position1, attitude1);
    asv_set_surge_sway_halt(asv1, true);

    // Initialise the ASV-2
    // union Coordinates_3D position2 {3018.09765625, 3010.44873046875, 0.0}; // {m, m, m}
    union Coordinates_3D position2 {200.0, 100.0, 0.0}; // {m, m, m}
    union Coordinates_3D attitude2 {0.0, 0.0, 0.0}; // {rad, rad, rad}
    struct Asv* asv2 = asv_new(asv_spec, sea, position2, attitude2);
    asv_set_surge_sway_halt(asv2, true);

    // Initialise and run the simulation
    Simulate_LOS simulation = Simulate_LOS(sea, asv1, asv2, antenna_height);
    simulation.run();
}

int run_headless(struct Asv_specification asv_spec, bool is_surge_sway_halt_set)
{
    // Initialise the sea surface
    for(double wave_hs = 0.5; wave_hs < 16.0; wave_hs+=1.0)
    {
        for(int wave_rand_seed = 1; wave_rand_seed < 6; wave_rand_seed++)
        {
            // Init sea surface
            double wave_dp = PI/3; // rad
            int count_component_waves = 21; 
            struct Sea_surface* sea = sea_surface_new(wave_hs, wave_dp, wave_rand_seed, count_component_waves);

            for(double antenna_height = 1.0; antenna_height < 5.0; antenna_height+=0.5)
            {               
                // Init ASV-1
                union Coordinates_3D position1 {100.0, 100.0, 0.0}; // {m, m, m}
                union Coordinates_3D attitude1 {0.0, 0.0, 0.0}; // {rad, rad, rad}
                struct Asv* asv1 = asv_new(asv_spec, sea, position1, attitude1);
                asv_set_surge_sway_halt(asv1, true);

                // Init ASV-2
                // union Coordinates_3D position2 {3018.09765625, 3010.44873046875, 0.0}; // {m, m, m}
                union Coordinates_3D position2 {200.0, 100.0, 0.0}; // {m, m, m}
                union Coordinates_3D attitude2 {0.0, 0.0, 0.0}; // {rad, rad, rad}
                struct Asv* asv2 = asv_new(asv_spec, sea, position2, attitude2);
                asv_set_surge_sway_halt(asv2, true);

                // Init and run simulation
                Simulate_LOS simulation = Simulate_LOS(sea, asv1, asv2, antenna_height);
                simulation.run_headless();

                // Clear memory
                asv_delete(asv1);
                asv_delete(asv2);
            }

            // Clear memory
            sea_surface_delete(sea);
        }
    }
}


int main()
{
    // ASV spec
    struct Asv_specification asv_spec {
        2.1 , // L_wl (m)
        0.6,  // B_wl (m)
        0.25, // D (m)
        0.09, // T (m)
        4.0,  // max_speed (m/s)
        0.09, // disp (m3)
        0.2,  // r_roll (m)
        0.6,  // r_pitch (m)
        0.6,  // r_yaw (m)
        {1.05, 0.0, -0.2} // cog {m, m, m} 
    };
    bool is_surge_sway_halt_set = true;

    run_headless(asv_spec, is_surge_sway_halt_set);
}


// int main()
// {
//     // Prepare data.
//     int n = 5000;
//     std::vector<double> x(n), y(n), z(n), w(n,2);
//     for(int i=0; i<n; ++i) {
//         x.at(i) = i*i;
//         y.at(i) = sin(2*M_PI*i/360.0);
//         z.at(i) = log(i);
//     }

//     // Set the size of output image = 1200x780 pixels
//     plt::figure_size(1200, 780);

//     // Plot line from given x and y data. Color is selected automatically.
//     plt::plot(x, y);

//     // Plot a red dashed line from given x and y data.
//     plt::plot(x, w,"r--");

//     // Plot a line whose name will show up as "log(x)" in the legend.
//     plt::named_plot("log(x)", x, z);

//     // Set x-axis to interval [0,1000000]
//     plt::xlim(0, 1000*1000);

//     // Add graph title
//     plt::title("Sample figure");

//     // Enable legend.
//     plt::legend();
//     plt::show();

//     // save figure
// //    const char* filename = "./basic.png";
// //    std::cout << "Saving result to " << filename << std::endl;;
// //    plt::save(filename);
// }