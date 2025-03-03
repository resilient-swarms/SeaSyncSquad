cmake_minimum_required(VERSION 3.30)
project(SeaSyncSquad)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        source/include
        dependency/ASVLite/cpp_current/include
        dependency/netcdf-cxx4/cxx4
        dependency/matplotlib-cpp
)

set( SOURCE
        dependency/ASVLite/cpp_current/source/geometry.cpp
        dependency/ASVLite/cpp_current/source/regular_wave.cpp
        dependency/ASVLite/cpp_current/source/sea_surface.cpp
        dependency/ASVLite/cpp_current/source/asv.cpp
        source/source/netcdf_rain.cpp 
        source/source/netcdf_wave.cpp 
        source/source/netcdf_wind.cpp 
        source/source/storm.cpp
        source/source/utils.cpp
        source/source/radio_attenuation.cpp
        source/source/swarm.cpp
        source/source/icgs.cpp
        # source/source/icgs_main.cpp
        source/source/range_table.cpp
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCE})

add_subdirectory(dependency/matplotlib-cpp)
add_subdirectory(dependency/netcdf-cxx4)

find_package(Threads REQUIRED)
find_package(GeographicLib REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(Python3 COMPONENTS NumPy REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        Threads::Threads
        ${GeographicLib_LIBRARIES}
        netcdf-cxx4
        Python3::Python
        Python3::Module
        Python3::NumPy
        Eigen3::Eigen)