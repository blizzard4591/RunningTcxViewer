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

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m_selectedFile()
{
	ui->setupUi(this);

	if (!QObject::connect(ui->action_Open, SIGNAL(triggered()), this, SLOT(SelectNewFile()))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up connection for windowSize slider!");
		throw;
	}
	if (!QObject::connect(ui->gbox_avgSpeed, SIGNAL(optionsChanged(DataOptions*)), this, SLOT(OnDataOptionsChanged(DataOptions*)))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up connection for data options #1!");
		throw;
	}
	if (!QObject::connect(ui->gbox_avgSpeedKmh, SIGNAL(optionsChanged(DataOptions*)), this, SLOT(OnDataOptionsChanged(DataOptions*)))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up connection for data options #2!");
		throw;
	}
	if (!QObject::connect(ui->gbox_heartRate, SIGNAL(optionsChanged(DataOptions*)), this, SLOT(OnDataOptionsChanged(DataOptions*)))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up connection for data options #3!");
		throw;
	}
	if (!QObject::connect(ui->gbox_pace, SIGNAL(optionsChanged(DataOptions*)), this, SLOT(OnDataOptionsChanged(DataOptions*)))) {
		QMessageBox::critical(this, "Internal Error", "Failed to set up connection for data options #4!");
		throw;
	}

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

std::vector<std::tuple<Trackpoint, std::optional<double>, std::optional<double>>> GetMovingAverageOfVectorOld(std::vector<std::tuple<Trackpoint, std::optional<double>>> const& input, std::size_t windowSize) {
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

template<typename Callable, typename... OPT_DBL>
auto GetMovingAverageOfVector(std::vector<std::tuple<Trackpoint, OPT_DBL...>> const& input, Callable valueExtractor, DataOptions::Data const& data) {
	std::vector<std::tuple<Trackpoint, OPT_DBL..., std::optional<double>>> result;

	for (std::size_t i = 0; i < input.size(); ++i) {
		std::size_t lastExistingIndex = i;
		double sum = 0.0;
		std::size_t summands = 0;
		for (std::size_t j = 0; j < data.windowSize; ++j) {
			if ((i + j) < input.size()) {
				lastExistingIndex = (i + j);
			}
			auto const val = valueExtractor(input.at(lastExistingIndex));
			if (val.has_value() && val.value() >= data.cutoffMin && val.value() <= data.cutoffMax) {
				sum += val.value();
				++summands;
			}
		}

		if (summands > 0) {
			result.push_back(std::tuple_cat(input.at(i), std::make_tuple(std::optional<double>(sum / summands))));
		}
		else {
			result.push_back(std::tuple_cat(input.at(i), std::make_tuple(std::nullopt)));
		}
	}

	return result;
}

template<typename Callable, typename... OPT_DBL>
auto Transform(std::vector<std::tuple<Trackpoint, OPT_DBL...>> const& input, Callable valueExtractor) {
	std::vector<std::tuple<Trackpoint, OPT_DBL..., std::optional<double>>> result;

	for (auto const& i : input) {
		auto val = valueExtractor(i);
		result.push_back(std::tuple_cat(i, std::make_tuple(val)));
	}

	return result;
}

void MainWindow::UpdateChart() {
	if (m_selectedFile.empty())
		return;

	auto const timeStart = std::chrono::steady_clock::now();
	if (!m_trackpoints.has_value()) {
		if (!std::filesystem::exists(m_selectedFile)) {
			if (DO_DEBUG) std::cerr << "Error: Input file " << m_selectedFile << " does not exist!" << std::endl;
			ui->statusbar->showMessage(QString("Error: Input file '%1' does not exist!").arg(QString::fromStdString(m_selectedFile)));
			return;
		}

		Parser parser(m_selectedFile, DO_DEBUG);
		m_trackpoints = parser.GetTrackpoints();
		if (DO_DEBUG) std::cout << "Got " << m_trackpoints.value().size() << " trackpoints from input file." << std::endl;
		ui->statusbar->showMessage(QString("Got %1 trackpoints from input file.").arg(m_trackpoints.value().size()));
	}

	auto const timeTps = std::chrono::steady_clock::now();

	auto const data0 = GetSpeedFromTrackpoints(m_trackpoints.value());
	auto const data1 = GetMovingAverageOfVector(data0, [](decltype(data0)::value_type const& v) { return std::get<1>(v); }, ui->gbox_avgSpeed->getData());
	auto const data2 = GetMovingAverageOfVector(data1, [](decltype(data1)::value_type const& v) { return std::optional<double>(std::get<0>(v).heartRateBpm); }, ui->gbox_heartRate->getData());
	auto const data3 = Transform(data2, [&](decltype(data2)::value_type const& v) -> std::optional<double>{
		auto const& e = std::get<2>(v);
		if (!e.has_value()) return std::nullopt;
		if (std::abs(e.value()) <= 0.01) return std::nullopt;
		double const pace = 1.0 / (METERS_PER_SECOND_TO_KILOMETERS_PER_HOUR(e.value()) / 60.0);
		return std::optional<double>(pace);
	});
	auto const data4 = GetMovingAverageOfVector(data3, [](decltype(data3)::value_type const& v) { return std::get<4>(v); }, ui->gbox_pace->getData());
	auto const data5 = Transform(data4, [&](decltype(data4)::value_type const& v) -> std::optional<double> {
		auto const& e = std::get<1>(v);
		if (!e.has_value()) return std::nullopt;
		return std::optional<double>(METERS_PER_SECOND_TO_KILOMETERS_PER_HOUR(e.value()));
		});
	auto const data6 = GetMovingAverageOfVector(data5, [](decltype(data5)::value_type const& v) { return std::get<6>(v); }, ui->gbox_avgSpeedKmh->getData());

	auto const timeEnd = std::chrono::steady_clock::now();
	ui->statusbar->showMessage(QString("Parsing %1 points from file took %2ms (%3ms in XML).").arg(m_trackpoints.value().size()).arg(std::chrono::duration_cast<std::chrono::milliseconds>(timeEnd - timeStart).count()).arg(std::chrono::duration_cast<std::chrono::milliseconds>(timeTps - timeStart).count()));

	// Chart
	bool const haveAvgSpeedInMs = ui->gbox_avgSpeed->getData().show;
	bool const haveAvgSpeedInKmh = ui->gbox_avgSpeedKmh->getData().show;
	bool const haveAvgHeartRate = ui->gbox_heartRate->getData().show;
	bool const haveAvgPace = ui->gbox_pace->getData().show;

	QLineSeries* seriesAvgSpeedInMs = new QLineSeries();
	QLineSeries* seriesAvgSpeedInKmh = new QLineSeries();
	QLineSeries* seriesAvgPace = new QLineSeries();
	QLineSeries* seriesAvgHeartBeat = new QLineSeries();
	for (auto const& [tp, speed, avgSpeed, avgHeartBeat, pace, avgPace, speedInKmh, avgSpeedInKmh] : data6) {
		qreal const time = tp.dateTime.toMSecsSinceEpoch();
		if (avgSpeed.has_value()) {
			seriesAvgSpeedInMs->append(time, avgSpeed.value());
		}
		if (avgSpeedInKmh.has_value()) {
			seriesAvgSpeedInKmh->append(time, avgSpeedInKmh.value());
		}
		if (avgPace.has_value()) {
			seriesAvgPace->append(time, avgPace.value());
		}
		if (avgHeartBeat.has_value()) {
			seriesAvgHeartBeat->append(time, avgHeartBeat.value());
		}
	}

	QChart* chart = new QChart();
	chart->addSeries(seriesAvgSpeedInMs);
	chart->addSeries(seriesAvgSpeedInKmh);
	chart->addSeries(seriesAvgHeartBeat);
	chart->addSeries(seriesAvgPace);

	QDateTimeAxis* valueAxisTime = new QDateTimeAxis(chart);
	valueAxisTime->setFormat("dd.MM.yyyy'\r\n'hh:mm:ss");
	chart->addAxis(valueAxisTime, Qt::AlignBottom);

	QValueAxis* valueAxisAvgSpeed = new QValueAxis(chart);
	valueAxisAvgSpeed->setLabelFormat("%.2f");
	valueAxisAvgSpeed->setTitleText("Avg. Speed in m/s");
	chart->addAxis(valueAxisAvgSpeed, Qt::AlignLeft);

	QValueAxis* valueAxisAvgSpeedInKmh = new QValueAxis(chart);
	valueAxisAvgSpeedInKmh->setLabelFormat("%.2f");
	valueAxisAvgSpeedInKmh->setTitleText("Avg. Speed in km/h");
	chart->addAxis(valueAxisAvgSpeedInKmh, Qt::AlignLeft);

	QValueAxis* valueAxisHeartBeat = new QValueAxis(chart);
	valueAxisHeartBeat->setLabelFormat("%i");
	valueAxisHeartBeat->setTitleText("Avg. Heartrate in BPM");
	chart->addAxis(valueAxisHeartBeat, Qt::AlignRight);

	QValueAxis* valueAxisPace = new QValueAxis(chart);
	valueAxisPace->setLabelFormat("%.2f");
	valueAxisPace->setTitleText("Avg. Pace in min/km");
	chart->addAxis(valueAxisPace, Qt::AlignRight);

	seriesAvgSpeedInMs->attachAxis(valueAxisTime);
	seriesAvgSpeedInMs->attachAxis(valueAxisAvgSpeed);
	seriesAvgSpeedInMs->setName("Avg. Speed in m/s");
	seriesAvgSpeedInMs->setVisible(haveAvgSpeedInMs);

	seriesAvgSpeedInKmh->attachAxis(valueAxisTime);
	seriesAvgSpeedInKmh->attachAxis(valueAxisAvgSpeedInKmh);
	seriesAvgSpeedInKmh->setName("Avg. Speed in km/h");
	seriesAvgSpeedInKmh->setVisible(haveAvgSpeedInKmh);

	seriesAvgHeartBeat->attachAxis(valueAxisTime);
	seriesAvgHeartBeat->attachAxis(valueAxisHeartBeat);
	seriesAvgHeartBeat->setName("Avg. Heartrate in BPM");
	seriesAvgHeartBeat->setVisible(haveAvgHeartRate);

	seriesAvgPace->attachAxis(valueAxisTime);
	seriesAvgPace->attachAxis(valueAxisPace);
	seriesAvgPace->setName("Avg. Pace in min/km");
	seriesAvgPace->setVisible(haveAvgPace);

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
}

void MainWindow::OnDataOptionsChanged(DataOptions*) {
	UpdateChart();
}

void MainWindow::OnNewValuesUnderMouse() {
	if (m_lastChartView != nullptr) {
		auto const& values = m_lastChartView->getValuesUnderMouse();
		ui->statusbar->showMessage(QString("Time %1, Avg. Speed %2, Heatrate %3").arg(QDateTime::fromMSecsSinceEpoch(values.at(0)).toString("dd.MM.yyyy hh:mm:ss")).arg(values.at(1), 0, 'f', 2).arg(values.at(3)));
	}
}
