cmake_minimum_required(VERSION 3.29)
project(SeaSyncSquad)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        include
        dependency/ASVLite/include
)

set( SOURCE
        source/main.cpp
        source/radio_attenuation.cpp
        dependency/ASVLite/source/errors.c
        dependency/ASVLite/source/geometry.c
        dependency/ASVLite/source/regular_wave.c
        dependency/ASVLite/source/sea_surface.c
        dependency/ASVLite/source/asv.c
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCE})

find_package(Threads REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Threads::Threads)