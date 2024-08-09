#pragma once

#include <QGroupBox>
#include <QLineSeries>

namespace Ui {
class DataOptions;
}

class DataOptions : public QGroupBox
{
    Q_OBJECT

public:
    explicit DataOptions(QWidget* parent = nullptr);
    virtual ~DataOptions();

    struct Data {
        bool show;
        int windowSize;
        double cutoffMin;
        double cutoffMax;

        Data() : show(false), windowSize(1), cutoffMin(0.0), cutoffMax(999.0) {}
    };
    Data const& getData() const {
        return m_data;
    }

public slots:
    void OnCBoxCheckStateChanged(Qt::CheckState state);
    void OnSBoxValueChanged(int value);
    void OnDBoxMinValueChanged(double value);
    void OnDBoxMaxValueChanged(double value);

signals:
    void optionsChanged(DataOptions* options);

private:
    Ui::DataOptions *ui;
    Data m_data;

    void updateData();
};
