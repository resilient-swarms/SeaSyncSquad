#ifndef UTILS_H
#define UTILS_H

extern "C" {
#include "geometry.h"
}
#include <string>
#include <vector>
#include <ctime>
#include <GeographicLib/LocalCartesian.hpp>


/**
 * Represents geographical coordinates with longitude (deg) and latitude (deg).
 */
struct Geo_coordinate {
    double longitude;
    double latitude;
};

/**
* A class that provides methods to convert between geodetic and cartesian coordinates.
*/
class Geo_coordinate_converter {
    public:
    /**
    * Constructor. Takes a reference coordinate about which the transformations are made.
    */
    Geo_coordinate_converter(Geo_coordinate reference_coordinate=Geo_coordinate(0.0, 0.0));

    /**
    * Converts geodetic to cartesian coordinate.
    */
    Coordinates_3D convert(Geo_coordinate coordinate);

    /**
    * Convert cartesian to geodetic coordinate.
    */
    Geo_coordinate convert(Coordinates_3D coordinate);

    private:
    Geo_coordinate reference_coordinate;
    GeographicLib::LocalCartesian proj;
};



/**
 * Splits a given line into a vector of words.
 */
std::vector<std::string> split(const std::string& s, char delim);

/**
 * Converts a string representing a date and time into a std::tm structure.
 * The string should be of the format "year,month,day,hour,minute".
 */
std::time_t string_to_time(std::string s);

std::string time_to_string(time_t t);


#endif //UTILS_H
