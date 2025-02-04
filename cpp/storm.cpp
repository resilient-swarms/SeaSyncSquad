#include "storm.h"
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <ranges>

Storm::Storm(std::filesystem::path storms_database, int storm_year, std::string storm_name, bool init_ocean_current):
storm_year(storm_year),
storm_name(storm_name) {
    // Check if the storms file exist, if not then throw and exception.
    // Use python script to generate the storms file.
    if (!std::filesystem::exists(storms_database)) {
        throw std::runtime_error("Storms file does not exist");
    }
    // Open file for reading
    std::ifstream storms_file(storms_database);
    if (!storms_file.is_open()) {
        throw std::runtime_error("Storms file could not be opened");
    }
    // Read lines from the file
    // Read and discard the first line, since it contains col headers
    std::string line;
    std::getline(storms_file, line);
    // Read and process the remaining lines from the file
    int index_year            = 0; // index of column "year" in the file
    int index_name            = 1; // index of column "storm_name" in the file
    int index_time_stamps     = 2; // index of column "time_stamps" in the file
    int index_path            = 3; // index of column "path(long,lat)" in the file
    int index_predicted_paths = 6; // index of column "predicted_paths(long,lat)" in the file
    while (std::getline(storms_file, line)) {
        std::vector<std::string> words = split(line, '\t');
        int year_from_line = std::stoi(words[index_year]);
        std::string name_from_line = words[index_name];
        if (year_from_line == storm_year && name_from_line == storm_name) {
            std::cout<<"Initialising " << words[index_name] << "..." << std::endl;
            std::cout<< "... retrieving data from " << storms_database.string() << std::endl;
            // Set timestamps
            std::vector<std::string> time_strings = split(words[index_time_stamps], ';');
            for (std::string time_string : time_strings) {
                time_string = time_string.substr(1, time_string.length() - 2); // remove the "(" and ")"
                std::time_t timestamp = string_to_time(time_string);
                timestamps.push_back(timestamp);
            }
            std::cout<< "... timestamps initialised. Count: " << timestamps.size() << std::endl;

            // Lambda expression to convert a string of format "(long,lat)/(long,lat)/.....(long,lat)"
            // to a vector of Geo_coordinates.
            auto path_to_geocoordinates = [](std::string path) -> std::vector<Geo_coordinate> {
                std::vector<Geo_coordinate> result;
                std::vector<std::string> points_string = split(path, '/');
                for (std::string point_string : points_string) {
                    point_string = point_string.substr(1, point_string.length() - 2); // remove the "(" and ")"
                    std::vector<std::string> long_lat = split(point_string, ',');
                    double longitude = std::stod(long_lat[0]);
                    double latitude = std::stod(long_lat[1]);
                    Geo_coordinate position {.longitude = longitude, .latitude = latitude};
                    result.push_back(position);
                }
                return result;
            };

            // Set storm path
            this->storm_path = path_to_geocoordinates(words[index_path]);
            std::cout<< "... storm path initialised. Count: " << storm_path.size() << std::endl;

            // Set predicted paths
            std::vector<std::string> predicted_paths_string = split(words[index_predicted_paths], ';');
            for (std::string predicted_path_string : predicted_paths_string) {
                predicted_path_string = predicted_path_string.substr(1, predicted_path_string.length() - 2); // remove the "[" and "]"
                std::vector<Geo_coordinate> predicted_path_geo_coordinates = path_to_geocoordinates(predicted_path_string);
                predicted_paths.push_back(predicted_path_geo_coordinates);
            }
            std::cout<< "... predicted paths initialised. Count: " << predicted_paths.size() << std::endl;

            break; // Found the storm, skip reading the rest of the database.
        }
    }
    // Check if storm initialised correctly
    int count_timestamps      = timestamps.size();
    int count_paths           = storm_path.size();
    int count_predicted_paths = predicted_paths.size();
    if (count_timestamps != 0 &&
        count_timestamps == count_paths &&
        count_paths == count_predicted_paths) {
        is_storm_initialised = true;
        std::cout<<"... storm initialisation complete." << std::endl;
    }
    else {
        std::cout<<"... storm initialisation failed." << std::endl;
        is_storm_initialised = false;
        throw std::runtime_error("Storm initialisation failed.");
    }
}

