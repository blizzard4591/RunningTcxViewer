#pragma once

#include <QChartView>
#include <QRubberBand>

#include <optional>
#include <vector>

class ChartView : public QChartView {
    Q_OBJECT
public:
    ChartView(QChart* chart, QWidget* parent = nullptr);

    virtual ~ChartView();

    std::vector<qreal> const& getValuesUnderMouse() const {
        return m_values;
    }

signals:
    void newValuesUnderMouse();
protected:
    bool viewportEvent(QEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);
    
    void drawForeground(QPainter* painter, QRectF const& rect) override;
private:
    bool m_isTouching = false;
    QChart* m_chart;
    std::vector<qreal> m_values;
    std::optional<QPointF> m_cursorPos = std::nullopt;
};
