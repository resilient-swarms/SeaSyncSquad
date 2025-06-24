# cmake_minimum_required(VERSION 3.30)
project(SeaSyncSquad)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        source/include
        dependency/ASVLite/cpp_current/include
        dependency/netcdf-cxx4/cxx4
        dependency/matplotlib-cpp
)

set( SOURCE
        dependency/ASVLite/cpp_current/source/rudder_controller.cpp
        # source/source/netcdf_rain.cpp 
        # source/source/netcdf_wave.cpp 
        # source/source/netcdf_wind.cpp 
        # source/source/storm.cpp
        # source/source/utils.cpp
        source/source/radio_attenuation.cpp
        source/source/icgs_swarm.cpp
        source/source/icgs.cpp
        # source/source/flux_asv.cpp
        # source/source/main_icgs.cpp
        # source/source/main_flux.cpp
        source/source/adaptive_formation_control.cpp
        # source/source/main_range_table.cpp
        # source/source/temp.cpp
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCE})

add_subdirectory(dependency/matplotlib-cpp)
# add_subdirectory(dependency/netcdf-cxx4)

find_package(Threads REQUIRED)
# find_package(GeographicLib REQUIRED)
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(Python COMPONENTS NumPy REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        Threads::Threads
        # ${GeographicLib_LIBRARIES}
        # netcdf-cxx4
        Python::Python
        Python::Module
        Python::NumPy
        Eigen3::Eigen)