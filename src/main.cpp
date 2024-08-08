#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <list>
#include <string>
#include <tuple>

#include "Parser.hpp"

static inline double METERS_PER_SECOND_TO_KILOMETERS_PER_HOUR(double metersPerSecond) {
	return metersPerSecond * 3.6;
}

static inline double KILOMETERS_PER_HOUR_TO_METERS_PER_SECOND(double kilometersPerHour) {
	return kilometersPerHour / 3.6;
}

std::string ToLower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return s;
}

class CommaDecimalPoint : public std::numpunct<char> {
protected:
	char do_decimal_point() const {
		return ',';
	}
};

std::vector<std::tuple<Trackpoint, std::optional<double>>> GetSpeedFromTrackpoints(std::vector<Trackpoint> const& trackpoints) {
	std::vector<std::tuple<Trackpoint, std::optional<double>>> result;

	for (std::size_t i = 0; i < trackpoints.size(); ++i) {
		if ((i + 1) >= trackpoints.size()) {
			break;
		}

		auto const& tpA = trackpoints.at(i);
		auto const& tpB = trackpoints.at(i + 1);

		double const distanceTravelledInMeters = tpB.distanceMeters - tpA.distanceMeters;
		double const timePassedInMilliseconds = tpB.dateTime.toMSecsSinceEpoch() - tpA.dateTime.toMSecsSinceEpoch();
		double const speedInMetersPerSecond = distanceTravelledInMeters / (timePassedInMilliseconds / 1000.0);

		if (timePassedInMilliseconds != 1000 && result.size() == 0) {
			std::cerr << "Ignoring starting point with invalid time jump!" << std::endl;
			result.push_back(std::make_tuple(tpA, std::nullopt));
			continue;
		} else if (speedInMetersPerSecond <= KILOMETERS_PER_HOUR_TO_METERS_PER_SECOND(3.6)) {
			std::cerr << "Ignoring point #" << i << " with low speed!" << std::endl;
			result.push_back(std::make_tuple(tpA, std::nullopt));
			continue;
		}

		if (timePassedInMilliseconds != 1000) {
			std::cerr << "Assumption Error: Expected time to always be 1000ms, but it was " << timePassedInMilliseconds << "ms instead at point #" << i << "!" << std::endl;
			throw;
		}

		result.push_back(std::make_tuple(tpA, speedInMetersPerSecond));
	}

	return result;
}

std::vector<std::tuple<Trackpoint, std::optional<double>, std::optional<double>>> GetMovingAverageOfVector(std::vector<std::tuple<Trackpoint, std::optional<double>>> const& input, std::size_t windowSize) {
	std::vector<std::tuple<Trackpoint, std::optional<double>, std::optional<double>>> result;

	for (std::size_t i = 0; i < input.size(); ++i) {
		std::size_t lastExistingIndex = i;
		double sum = 0.0;
		std::size_t summands = 0;
		for (std::size_t j = 0; j < windowSize; ++j) {
			if ((i + j) < input.size()) {
				lastExistingIndex = (i + j);
			}
			auto const& speed = std::get<1>(input.at(lastExistingIndex));
			if (speed.has_value()) {
				sum += speed.value();
				++summands;
			}
		}

		if (summands > 0) {
			result.push_back(std::make_tuple(std::get<0>(input.at(i)), std::get<1>(input.at(i)), sum / windowSize));
		}
		else {
			result.push_back(std::make_tuple(std::get<0>(input.at(i)), std::get<1>(input.at(i)), std::nullopt));
		}
	}

	return result;
}

int main(int argc, char* argv[]) {
	std::cout << "TcxViewer" << std::endl;
	
	auto const beginTotal = std::chrono::steady_clock::now();

	std::filesystem::path inputFile = R"(C:\Users\jana\Downloads\exercise_tcx_file.tcx)";
	std::size_t windowSize = 15;
	for (int i = 1; i < argc; ++i) {
		bool const hasNext = ((i + 1) < argc);
		std::string const arg = ToLower(argv[i]);
		if (arg.compare("--file") == 0) {
			if (!hasNext) {
				std::cerr << "Expected parameter for '--file!'" << std::endl;
				return -1;
			}
			inputFile = argv[i + 1];
			++i;
		} else if (arg.compare("--windowsize") == 0) {
			if (!hasNext) {
				std::cerr << "Expected parameter for '--windowSize!'" << std::endl;
				return -1;
			}
			windowSize = std::stoull(argv[i + 1]);
			++i;
		}
		else {
			std::cerr << "Option '" << argv[i] << "' did not match any known options!" << std::endl;
			return -1;
		}
	}

	inputFile = inputFile.lexically_normal();

	std::cout << "Using file: " << inputFile << std::endl;
	std::cout << "Using window size: " << windowSize << std::endl;

	if (!std::filesystem::exists(inputFile)) {
		std::cerr << "Error: Input file " << inputFile << " does not exist!" << std::endl;
		return -1;
	}

	Parser parser(inputFile);
	auto const trackpoints = parser.GetTrackpoints();
	std::cout << "Got " << trackpoints.size() << " trackpoints from input file." << std::endl;

	auto const speeds = GetSpeedFromTrackpoints(trackpoints);
	auto const avgSpeeds = GetMovingAverageOfVector(speeds, windowSize);

	std::size_t counter = 1;
	std::ofstream csvFile("avgSpeeds.csv");
	csvFile << "Index;Date and Time;Speed in m/s;Speed in km/h;Pace in min/km;Heartbeat in bpm" << std::endl;
	std::locale loc(std::locale(), new CommaDecimalPoint());
	csvFile.imbue(loc);
	for (auto const& speed : avgSpeeds) {
		Trackpoint const& tp = std::get<0>(speed);

		csvFile << counter << ";";
		csvFile << tp.dateTime.toString(Qt::DateFormat::ISODate).toStdString() << ";";

		auto const& speedInMetersPerSecond = std::get<1>(speed);
		if (speedInMetersPerSecond.has_value()) {
			csvFile << speedInMetersPerSecond.value();
		}
		csvFile << ";";
		if (speedInMetersPerSecond.has_value()) {
			csvFile << METERS_PER_SECOND_TO_KILOMETERS_PER_HOUR(speedInMetersPerSecond.value());
		}
		csvFile << ";";

		if (speedInMetersPerSecond.has_value()) {
			double const pace = 1.0 / (METERS_PER_SECOND_TO_KILOMETERS_PER_HOUR(speedInMetersPerSecond.value()) / 60.0);
			csvFile << pace;
		}
		csvFile << ";";

		csvFile << tp.heartRateBpm << std::endl;
		
		++counter;
	}

	std::cout << "Done!" << std::endl;
	return 0;
}

#ifdef _MSC_VER
int __stdcall WinMain(struct HINSTANCE__*, struct HINSTANCE__*, char*, int) {
	return main(__argc, __argv);
}

#endif
