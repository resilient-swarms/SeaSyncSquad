#include "utils.h"
#include <iostream>
#include <sstream>
#include <GeographicLib/LocalCartesian.hpp>

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> words;
    std::istringstream stream(s);
    std::string word;
    while (std::getline(stream, word, delim)) {
        words.push_back(word);
    }
    return words;
}


std::time_t string_to_time(std::string s) {
    std::tm datetime;
    std::vector<std::string> tokens = split(s, ',');
    datetime.tm_year = std::stoi(tokens[0]) - 1900; // Years since 1900
    datetime.tm_mon = std::stoi(tokens[1]) - 1; // Months (0-based)
    datetime.tm_mday = std::stoi(tokens[2]);
    datetime.tm_hour = std::stoi(tokens[3]);
    datetime.tm_min = std::stoi(tokens[4]);
    datetime.tm_sec = 0;
    datetime.tm_isdst = -1;
    return std::mktime(&datetime);
}

std::string time_to_string(time_t t) {
    std::tm *ptm = std::localtime(&t);
    char result[32];
    std::strftime(result, sizeof(result), "%Y,%m,%d,%H,%M", ptm);
    return result;
}


Geo_coordinate_converter::Geo_coordinate_converter(Geo_coordinate reference_coordinate):
reference_coordinate(reference_coordinate) {
    // Create a LocalCartesian projection centered at reference coordinate
    double h0 = 0;
    proj = GeographicLib::LocalCartesian(reference_coordinate.latitude, reference_coordinate.longitude, h0);
}

Coordinates_3D Geo_coordinate_converter::convert(Geo_coordinate coordinate) {
    Coordinates_3D result {0.0, 0.0, 0.0};
    // Convert to local cartesian coordinates
    double h = 0.0;
    proj.Forward(coordinate.latitude,
        coordinate.longitude, h,
        result.keys.x, result.keys.y, result.keys.z);
    return result;
}

Geo_coordinate Geo_coordinate_converter::convert(Coordinates_3D coordinate) {
    Geo_coordinate result;
    // Convert to geodetic coordinates
    double z = 0.0;
    double h = 0.0;
    proj.Reverse(coordinate.keys.x, coordinate.keys.y, coordinate.keys.z,
        result.latitude, result.longitude, h);
    return result;
}

