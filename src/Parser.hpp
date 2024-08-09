#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include <QAnyStringView>
#include <QDomDocument>
#include <QtGlobal>

#include "MappedFileString.hpp"
#include "Trackpoint.hpp"

class Parser {
public:

	Parser(std::filesystem::path const& inputFile, bool doDebugOutput) : m_inputFile(inputFile) {
		MappedFileString mappedInputFile(inputFile.string());
		QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
		QAnyStringView contentView(mappedInputFile.GetView());
		auto const parseResult = doc.setContent(contentView, QDomDocument::ParseOption::UseNamespaceProcessing);
#else
		QString const content = QString::fromStdString(mappedInputFile.GetString());
		auto const parseResult = doc.setContent(content, true);
#endif
		if (!parseResult) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
			std::cerr << "Encountered an error in XML parsing at line " << parseResult.errorLine << " and column " << parseResult.errorColumn << ": " << parseResult.errorMessage.toStdString() << std::endl;
#else
			std::cerr << "Encountered an error in XML parsing!" << std::endl;
#endif
			throw;
		}

		m_trackpoints = ParseRunningTrackpoints(doc, doDebugOutput);

	}
	virtual ~Parser() {
		//
	}

	std::vector<Trackpoint> const& GetTrackpoints() const {
		return m_trackpoints;
	}

private:
	std::filesystem::path const m_inputFile;
	std::vector<Trackpoint> m_trackpoints;

	template<typename T>
	inline QDomNode getOnlyChild(T const& element, QString const& nodeName) const {
		auto const nodes = element.childNodes();
		if (nodes.size() != 1) {
			std::cerr << "Assumption Error: Node was expected to have exactly one child, but it did not. It has " << nodes.size() << " children. Line: " << element.lineNumber() << ", Column: " << element.columnNumber() << std::endl;
			for (int i = 0; i < nodes.size(); ++i) {
				std::cerr << "Child has name '" << nodes.at(i).nodeName().toStdString() << "'" << std::endl;
			}
			throw;
		}
		auto const node = nodes.at(0);
		if (node.nodeName().compare(nodeName, Qt::CaseInsensitive) != 0) {
			std::cerr << "Assumption Error: Node was expected to have exactly one child of type '" << nodeName.toStdString() << "', but it did not. Actual type: '" << node.nodeName().toStdString() << "'. Line: " << element.lineNumber() << ", Column: " << element.columnNumber() << std::endl;
			throw;
		}
		return node;
	}

	template<typename T>
	inline QDomNode getChildAtIndex(T const& element, int index, QString const& nodeName) const {
		auto const nodes = element.childNodes();
		if (nodes.size() <= index) {
			std::cerr << "Assumption Error: Node was expected to have enough children, but it did not. Line: " << element.lineNumber() << ", Column: " << element.columnNumber() << std::endl;
			throw;
		}
		auto const node = nodes.at(index);
		if (node.nodeName().compare(nodeName, Qt::CaseInsensitive) != 0) {
			std::cerr << "Assumption Error: Node was expected to have child of type '" << nodeName.toStdString() << "', but it did not. Line: " << element.lineNumber() << ", Column: " << element.columnNumber() << std::endl;
			throw;
		}
		return node;
	}

	template<typename T>
	inline QDomNode getChildByType(T const& element, QString const& nodeType) const {
		auto const nodes = element.childNodes();
		for (int index = 0; index < nodes.size(); ++index) {
			auto const node = nodes.at(index);
			if (node.nodeName().compare(nodeType, Qt::CaseInsensitive) == 0) {
				return node;
			}
		}
		
		std::cerr << "Assumption Error: Node was expected to have a child of type '" << nodeType.toStdString() << "', but it did not. Line: " << element.lineNumber() << ", Column: " << element.columnNumber() << std::endl;
		for (int index = 0; index < nodes.size(); ++index) {
			std::cerr << "Child #" << index << "has type '" << nodes.at(index).nodeName().toStdString() << "'" << std::endl;
		}
		throw;
	}

	inline QDomElement ensureIsElement(QDomNode const& node) const {
		if (!node.isElement()) {
			std::cerr << "Assumption Error: Node was expected to be an element, but it was not. Line: " << node.lineNumber() << ", Column: " << node.columnNumber() << std::endl;
			throw;
		}
		return node.toElement();
	}

	std::vector<Trackpoint> ParseRunningTrackpoints(QDomDocument const& doc, bool doDebugOutput) const {
		std::vector<Trackpoint> result;
		
		auto const trainingCenterDatabase = ensureIsElement(getChildAtIndex(doc, 1, "TrainingCenterDatabase"));
		auto const activities = ensureIsElement(getOnlyChild(trainingCenterDatabase, "Activities"));
		auto const activity = ensureIsElement(getOnlyChild(activities, "Activity"));
		if (!activity.hasAttribute("Sport")) {
			std::cerr << "Error: Expected Activity to have a 'Sport' attribute, but it did not!" << std::endl;
			throw;
		}
		else if (activity.attribute("Sport").compare("Running") != 0) {
			std::cerr << "Error: Expected Activity.Sport to be 'Running', but it is '" << activity.attribute("Sport").toStdString() << "'!" << std::endl;
			throw;
		}

		auto const lap = ensureIsElement(getChildByType(activity, "Lap"));
		auto const track = ensureIsElement(getChildByType(lap, "Track"));
		
		auto const trackpointNodes = track.childNodes();
		double lastDistanceInMeters = 0.0;
		for (int i = 0; i < trackpointNodes.size(); ++i) {
			auto const trackpointNode = ensureIsElement(trackpointNodes.at(i));

			auto const children = trackpointNode.childNodes();
			if (children.size() != 5) {
				std::cerr << "Assumption Error: Expected Trackpoint to have 5 children, but it has " << children.size() << "!" << std::endl;
				throw;
			}

			Trackpoint tp;

			auto const childTime = ensureIsElement(getChildByType(trackpointNode, "Time"));
			tp.dateTime = QDateTime::fromString(childTime.text(), Qt::ISODateWithMs);

			if (tp.dateTime.toString("zzz").compare("000") != 0) {
				if (doDebugOutput) std::cerr << "Warning: Ignoring trackpoint #" << i << " not on second boundary!" << std::endl;
				continue;
			}
			
			auto const childPosition = ensureIsElement(getChildByType(trackpointNode, "Position"));
			auto const lat = ensureIsElement(getChildAtIndex(childPosition, 0, "LatitudeDegrees"));
			auto const lon = ensureIsElement(getChildAtIndex(childPosition, 1, "LongitudeDegrees"));
			tp.latitudeDegrees = std::stod(lat.text().toStdString());
			tp.longitudeDegrees = std::stod(lon.text().toStdString());


			auto const childAltitude = ensureIsElement(getChildByType(trackpointNode, "AltitudeMeters"));
			tp.altitudeMeters = std::stod(childAltitude.text().toStdString());

			auto const childDistance = ensureIsElement(getChildByType(trackpointNode, "DistanceMeters"));
			tp.distanceMeters = std::stod(childDistance.text().toStdString());

			if (tp.distanceMeters < lastDistanceInMeters) {
				if (doDebugOutput) std::cerr << "Warning: Fixing distance on point #" << i << "!" << std::endl;
				tp.distanceMeters = lastDistanceInMeters;
			}
			lastDistanceInMeters = tp.distanceMeters;

			auto const childHeartRate = ensureIsElement(getChildByType(trackpointNode, "HeartRateBpm"));
			auto const value = ensureIsElement(getChildAtIndex(childHeartRate, 0, "Value"));
			tp.heartRateBpm = std::stoi(value.text().toStdString());

			result.push_back(tp);
		}

		return result;
	}
};
