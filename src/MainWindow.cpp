#include "MainWindow.hpp"
#include "ui_mainwindow.h"

#include <algorithm>
#include <iostream>
#include <tuple>
#include <vector>

#include <QChart>
#include <QDateTimeAxis>
#include <QFileDialog>
#include <QLineSeries>
#include <QMessageBox>
#include <QTimer>
#include <QValueAxis>

#include "ChartView.hpp"
#include "Parser.hpp"

static bool constexpr DO_DEBUG = false;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_selectedFile()
    , m_windowSize(15)
{
    ui->setupUi(this);

	if (!QObject::connect(ui->windowSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(OnSliderValueChanged(int)))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up connection for windowSize slider!");
		throw;
	}
	if (!QObject::connect(ui->action_Open, SIGNAL(triggered()), this, SLOT(SelectNewFile()))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up connection for windowSize slider!");
		throw;
	}

	ui->windowSizeLabel->setText("15");

	QTimer::singleShot(250, this, SLOT(SelectNewFile()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::SelectNewFile() {
    QString const filename = QFileDialog::getOpenFileName(this, "Select TCX file to display", QString(), "Trackpoints (*.tcx)");

    if (!filename.isNull()) {
        m_selectedFile = filename.toStdString();
    }
    else {
        m_selectedFile = "";
    }

    UpdateChart();
}

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
			if (DO_DEBUG) std::cerr << "Ignoring starting point with invalid time jump!" << std::endl;
			result.push_back(std::make_tuple(tpA, std::nullopt));
			continue;
		}
		else if (speedInMetersPerSecond <= KILOMETERS_PER_HOUR_TO_METERS_PER_SECOND(3.6)) {
			if (DO_DEBUG) std::cerr << "Ignoring point #" << i << " with low speed!" << std::endl;
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

void MainWindow::UpdateChart() {
	if (m_selectedFile.empty())
		return;

	if (!std::filesystem::exists(m_selectedFile)) {
		if (DO_DEBUG) std::cerr << "Error: Input file " << m_selectedFile << " does not exist!" << std::endl;
		return;
	}

	auto const timeStart = std::chrono::steady_clock::now();
	if (!m_trackpoints.has_value()) {
		Parser parser(m_selectedFile, DO_DEBUG);
		m_trackpoints = parser.GetTrackpoints();
		if (DO_DEBUG) std::cout << "Got " << m_trackpoints.value().size() << " trackpoints from input file." << std::endl;
	}
	
	auto const timeTps = std::chrono::steady_clock::now();

	auto const speeds = GetSpeedFromTrackpoints(m_trackpoints.value());
	auto const avgSpeeds = GetMovingAverageOfVector(speeds, m_windowSize);

	auto const timeEnd = std::chrono::steady_clock::now();
	ui->statusbar->showMessage(QString("Parsing %1 points from file took %2ms (%3ms in XML).").arg(m_trackpoints.value().size()).arg(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count()).arg(std::chrono::duration_cast<std::chrono::milliseconds>(timeTps - timeStart).count()));

	// Chart
	QLineSeries* seriesAvgSpeed = new QLineSeries();
	QLineSeries* seriesHeartBeat = new QLineSeries();
	for (auto const& [tp, speed, avgSpeed] : avgSpeeds) {
		qreal const time = tp.dateTime.toMSecsSinceEpoch();
		if (avgSpeed.has_value()) {
			seriesAvgSpeed->append(time, avgSpeed.value());
		}
		seriesHeartBeat->append(time, tp.heartRateBpm);
	}

	QChart* chart = new QChart();
	chart->addSeries(seriesAvgSpeed);
	chart->addSeries(seriesHeartBeat);

	QDateTimeAxis* valueAxisTime = new QDateTimeAxis(chart);
	valueAxisTime->setFormat("dd.MM.yyyy'\r\n'hh:mm:ss");
	chart->addAxis(valueAxisTime, Qt::AlignBottom);

	QValueAxis* valueAxisAvgSpeed = new QValueAxis(chart);
	valueAxisAvgSpeed->setLabelFormat("%.2f");
	valueAxisAvgSpeed->setTitleText("Avg. Speed in m/s");
	chart->addAxis(valueAxisAvgSpeed, Qt::AlignLeft);

	QValueAxis* valueAxisHeartBeat = new QValueAxis(chart);
	valueAxisHeartBeat->setLabelFormat("%i");
	valueAxisHeartBeat->setTitleText("Heartrate in BPM");
	chart->addAxis(valueAxisHeartBeat, Qt::AlignRight);

	seriesAvgSpeed->attachAxis(valueAxisTime);
	seriesAvgSpeed->attachAxis(valueAxisAvgSpeed);
	seriesAvgSpeed->setName("Avg. Speed in m/s");

	seriesHeartBeat->attachAxis(valueAxisTime);
	seriesHeartBeat->attachAxis(valueAxisHeartBeat);
	seriesHeartBeat->setName("Heartrate in BPM");
	
	ChartView* chartView = new ChartView(chart, nullptr);
	if (!QObject::connect(chartView, SIGNAL(newValuesUnderMouse()), this, SLOT(OnNewValuesUnderMouse()))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up signal connection to ChartView!");
		throw;
	}
	chartView->setRenderHint(QPainter::Antialiasing);

	if (m_lastChartView != nullptr) {
		ui->verticalLayout->removeWidget(m_lastChartView);
		delete m_lastChartView;
		m_lastChartView = nullptr;
	}

	m_lastChartView = chartView;
	ui->verticalLayout->addWidget(chartView);

	// CSV
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
}

void MainWindow::OnSliderValueChanged(int value) {
    if (value > 0 && value <= 300) {
        m_windowSize = value;
		ui->windowSizeLabel->setText(QString::number(value));

        UpdateChart();
    }
    else {
        ui->windowSizeSlider->setValue(15);
		ui->windowSizeLabel->setText("15");
    }
}

void MainWindow::OnNewValuesUnderMouse() {
	if (m_lastChartView != nullptr) {
		auto const& values = m_lastChartView->getValuesUnderMouse();
		ui->statusbar->showMessage(QString("Time %1, Avg. Speed %2, Heatrate %3").arg(QDateTime::fromMSecsSinceEpoch(values.at(0)).toString("dd.MM.yyyy hh:mm:ss")).arg(values.at(1), 0, 'f', 2).arg(values.at(3)));
	}
}
