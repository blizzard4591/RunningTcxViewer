#pragma once

#include <string>
#include <optional>
#include <vector>

#include <QMainWindow>

#include "DataOptions.hpp"
#include "Trackpoint.hpp"

namespace Ui {
class MainWindow;
}

class ChartView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void SelectNewFile();

    void UpdateChart();

    void OnDataOptionsChanged(DataOptions* options);
    void OnNewValuesUnderMouse();

private:
    Ui::MainWindow *ui;

    std::string m_selectedFile;
    ChartView* m_lastChartView = nullptr;
    std::optional<std::vector<Trackpoint>> m_trackpoints = std::nullopt;
};

