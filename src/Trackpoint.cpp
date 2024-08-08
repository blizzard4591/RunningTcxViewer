#include "Trackpoint.hpp"


static constexpr double INVALID_VALUE = -999999.999;

Trackpoint::Trackpoint() :
	dateTime(), 
	latitudeDegrees(INVALID_VALUE), 
	longitudeDegrees(INVALID_VALUE), 
	altitudeMeters(INVALID_VALUE), 
	distanceMeters(INVALID_VALUE), 
	heartRateBpm(-999)
{}
