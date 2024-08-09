#include "ChartView.hpp"

#include <iostream>

#include <QMouseEvent>
#include <QLineSeries>
#include <QValueAxis>

static bool constexpr DO_DEBUG = false;

ChartView::ChartView(QChart* chart, QWidget* parent)
	: QChartView(chart, parent)
	, m_chart(chart)
{
	setRubberBand(QChartView::RectangleRubberBand);
}

ChartView::~ChartView() {
	if (m_chart != nullptr) {
		delete m_chart;
		m_chart = nullptr;
	}
}

bool ChartView::viewportEvent(QEvent* event)
{
	if (event->type() == QEvent::TouchBegin) {
		// By default touch events are converted to mouse events. So
		// after this event we will get a mouse event also but we want
		// to handle touch events as gestures only. So we need this safeguard
		// to block mouse events that are actually generated from touch.
		m_isTouching = true;

		// Turn off animations when handling gestures they
		// will only slow us down.
		chart()->setAnimationOptions(QChart::NoAnimation);
	}
	return QChartView::viewportEvent(event);
}

void ChartView::mousePressEvent(QMouseEvent* event)
{
	if (m_isTouching)
		return;
	QChartView::mousePressEvent(event);
}

void ChartView::mouseMoveEvent(QMouseEvent* event)
{
	if (m_isTouching)
		return;

	QPointF const scene_position = mapToScene(event->pos());
	QPointF const chart_position = chart()->mapFromScene(scene_position);
	auto value_at_position = chart()->mapToValue(chart_position);
	auto const axes = chart()->axes(Qt::Horizontal);
	if (axes.size() < 1) {
		throw;
	}
	QValueAxis* xAxis = static_cast<QValueAxis*>(axes.at(0));
	if (xAxis->min() < value_at_position.x() && value_at_position.x() < xAxis->max()) {
		m_cursorPos = scene_position;
		// update();
		scene()->update();
	}
	else {
		m_cursorPos = std::nullopt;
		scene()->update();
	}
	//emit newValuesUnderMouse();

	QChartView::mouseMoveEvent(event);
}

void ChartView::mouseReleaseEvent(QMouseEvent* event)
{
	if (m_isTouching)
		m_isTouching = false;

	// Because we disabled animations when touch event was detected
	// we must put them back on.
	chart()->setAnimationOptions(QChart::SeriesAnimations);

	QChartView::mouseReleaseEvent(event);
}

//![1]
void ChartView::keyPressEvent(QKeyEvent* event)
{
	switch (event->key()) {
	case Qt::Key_Plus:
		chart()->zoomIn();
		break;
	case Qt::Key_Minus:
		chart()->zoomOut();
		break;
		//![1]
	case Qt::Key_Left:
		chart()->scroll(-10, 0);
		break;
	case Qt::Key_Right:
		chart()->scroll(10, 0);
		break;
	case Qt::Key_Up:
		chart()->scroll(0, 10);
		break;
	case Qt::Key_Down:
		chart()->scroll(0, -10);
		break;
	default:
		QGraphicsView::keyPressEvent(event);
		break;
	}
}

void ChartView::drawForeground(QPainter* painter, QRectF const&) {
	if (!m_cursorPos.has_value())
		return;
	painter->save();

	QPen pen = QPen(QColor("indigo"));
	pen.setWidth(1);
	painter->setPen(pen);

	auto p = m_cursorPos.value();
	auto r = chart()->plotArea();

	QPointF const p_x_top = QPointF(p.x(), r.top());
	QPointF const p_x_bottom = QPointF(p.x(), r.bottom());
	painter->drawLine(p_x_top, p_x_bottom);

	QPointF const p_left_y = QPointF(r.left(), p.y());
	QPointF const p_right_y = QPointF(r.right(), p.y());
	painter->drawLine(p_left_y, p_right_y);

	auto const series = chart()->series();
	if (m_values.size() != (static_cast<std::size_t>(series.size()) * 2)) {
		m_values.resize(series.size() * 2);
	}

	QPointF const chart_position = chart()->mapFromScene(p);
	std::size_t index = 0;
	for (auto const& series_i : series) {
		if (DO_DEBUG) std::cerr << "Working on series " << series_i->name().toStdString() << "..." << std::endl;
		QPen pen2 = QPen(QColor("black"));
		pen2.setWidth(8);
		painter->setPen(pen2);

		QPointF const value_at_position = chart()->mapToValue(chart_position, series_i);

		// find the nearest points
		qreal min_distance_left = std::numeric_limits<qreal>::max();
		qreal min_distance_right = std::numeric_limits<qreal>::max();
		std::optional<QPointF> nearest_point_left = std::nullopt;
		std::optional<QPointF> nearest_point_right = std::nullopt;
		std::optional<QPointF> exact_point = std::nullopt;
		QLineSeries* ls = static_cast<QLineSeries*>(series_i);
		QPointF valuePoint;
		for (auto const& p_i : ls->points()) {
			if (p_i.x() > value_at_position.x()) {
				if (p_i.x() - value_at_position.x() < min_distance_right) {
					min_distance_right = p_i.x() - value_at_position.x();
					nearest_point_right = p_i;
				}
			}
			else if (p_i.x() < value_at_position.x()) {
				if (value_at_position.x() - p_i.x() < min_distance_right) {
					min_distance_left = value_at_position.x() - p_i.x();
					nearest_point_left = p_i;
					valuePoint = p_i;
				}
				else {
					exact_point = p_i;
					valuePoint = p_i;
					nearest_point_left = std::nullopt;
					nearest_point_right = std::nullopt;
					break;
				}
			}
		}

		if (exact_point.has_value()) {
			QPointF const mappedPoint = chart()->mapToScene(chart()->mapToPosition(exact_point.value(), series_i));
			painter->drawPoint(mappedPoint);
			if (DO_DEBUG) std::cerr << "Found exact point." << std::endl;
			m_values[index++] = valuePoint.x();
			m_values[index++] = valuePoint.y();
		}
		else if (nearest_point_right.has_value() && nearest_point_left.has_value()) {
			// do linear interpolated by my self
			qreal const k = ((nearest_point_right.value().y() - nearest_point_left.value().y()) / (nearest_point_right.value().x() - nearest_point_left.value().x()));
			qreal const point_interpolated_y = nearest_point_left.value().y() + k * (value_at_position.x() - nearest_point_left.value().x());
			qreal const point_interpolated_x = value_at_position.x();

			QPointF const interpolatedPoint = QPointF(point_interpolated_x, point_interpolated_y);

			QPointF const mappedPoint = chart()->mapToScene(chart()->mapToPosition(interpolatedPoint, series_i));
			painter->drawPoint(mappedPoint);
			if (DO_DEBUG) std::cerr << "Found interpolated point at (" << interpolatedPoint.x() << ", " << interpolatedPoint.y() << ") mapped to (" << mappedPoint.x() << ", " << mappedPoint.y() << ")." << std::endl;
			m_values[index++] = valuePoint.x();
			m_values[index++] = valuePoint.y();

			pen2.setWidth(1);
			painter->setPen(pen2);
			QFont font = painter->font();
			font.setPointSizeF(12.0f);
			painter->setFont(font);
			bool const isLeftSide = mappedPoint.x() > (chart()->scene()->width() / 2.0f);
			int flags = 0;
			QRectF rect;
			if (isLeftSide) {
				flags = Qt::AlignRight;
				rect = QRectF(mappedPoint.x() - 100.f - 10.f, mappedPoint.y(), 100.f, 50.f);
			}
			else {
				flags = Qt::AlignLeft;
				rect = QRectF(mappedPoint.x() + 10.f, mappedPoint.y(), 100.f, 50.f);
			}
			QRectF boundingRect;
			painter->drawText(rect, flags, QString::number(valuePoint.y()), &boundingRect);
			painter->fillRect(boundingRect, QBrush(QColor(255, 255, 255, 255)));
			painter->drawText(rect, flags, QString::number(valuePoint.y()));
		}
		else {
			if (DO_DEBUG) std::cerr << "Found NO point for series!" << std::endl;
		}
	}
	painter->restore();

	emit newValuesUnderMouse();
}
