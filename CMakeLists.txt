cmake_minimum_required(VERSION 3.30)
project(SeaSyncSquad)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        dependency/ASVLite/include
        dependency/netcdf-cxx4/cxx4
        dependency/matplotlib-cpp
)

set( SOURCE
        cpp/main.cpp
        cpp/swarm.cpp
        cpp/radio_attenuation.cpp
        cpp/netcdf_rain.cpp
        cpp/netcdf_wave.cpp
        cpp/netcdf_wind.cpp
        cpp/storm.cpp
        cpp/utils.cpp
        dependency/ASVLite/source/errors.c
        dependency/ASVLite/source/geometry.c
        dependency/ASVLite/source/regular_wave.c
        dependency/ASVLite/source/sea_surface.c
        dependency/ASVLite/source/asv.c

)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCE})

add_subdirectory(dependency/matplotlib-cpp)
add_subdirectory(dependency/netcdf-cxx4)

find_package(Threads REQUIRED)
find_package(GeographicLib REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(Python3 COMPONENTS NumPy REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        Threads::Threads
        ${GeographicLib_LIBRARIES}
        netcdf-cxx4
        Python3::Python
        Python3::Module
        Python3::NumPy)