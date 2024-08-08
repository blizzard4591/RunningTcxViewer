#pragma once

#include <cstdint>

#include <QDateTime>

struct Trackpoint {
	QDateTime dateTime;

	double latitudeDegrees;
	double longitudeDegrees;
	double altitudeMeters;
	double distanceMeters;
	std::int_fast16_t heartRateBpm;

	Trackpoint();
};
