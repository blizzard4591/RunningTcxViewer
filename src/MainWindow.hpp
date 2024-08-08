#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QMainWindow>

#include "ChartView.hpp"
#include "Trackpoint.hpp"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void SelectNewFile();

    void UpdateChart();

    void OnSliderValueChanged(int value);
    void OnNewValuesUnderMouse();
private:
    Ui::MainWindow *ui;

    std::filesystem::path m_selectedFile;
    int m_windowSize;
    ChartView* m_lastChartView = nullptr;
    std::optional<std::vector<Trackpoint>> m_trackpoints = std::nullopt;
};

