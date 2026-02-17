// src/ui/operator/NavMapWidget.cpp

#include "NavMapWidget.h"

#include <QVBoxLayout>
#include <QGraphicsRectItem>
#include <QPen>
#include <QBrush>
#include <QPainterPath>
#include <QTransform>
#include <QWheelEvent>
#include <cmath>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NavMapWidget::NavMapWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void NavMapWidget::setupUi()
{
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QBrush(QColor("#2c3e50")));

    m_view = new QGraphicsView(m_scene, this);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);

    QVBoxLayout* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->addWidget(m_view);

    // Enable mouse wheel zoom
    m_view->setInteractive(true);
    m_view->viewport()->installEventFilter(this);
}

// Override eventFilter to handle mouse wheel zoom
bool NavMapWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_view->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        double scaleFactor = 1.15;
        if (wheelEvent->angleDelta().y() > 0) {
            m_view->scale(scaleFactor, scaleFactor);
        } else {
            m_view->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// Map loading
// ---------------------------------------------------------------------------

void NavMapWidget::setNavMap(const hmi::NavMapInfo& mapInfo, const QImage& mapImage)
{
    m_mapInfo = mapInfo;

    // Clear old map
    if (m_mapItem) {
        m_scene->removeItem(m_mapItem);
        delete m_mapItem;
        m_mapItem = nullptr;
    }

    // Create new map pixmap
    if (!mapImage.isNull()) {
        QPixmap pix = QPixmap::fromImage(mapImage);
        m_mapItem = m_scene->addPixmap(pix);
        m_mapItem->setZValue(0);
        m_scene->setSceneRect(m_mapItem->boundingRect());
        m_view->fitInView(m_mapItem, Qt::KeepAspectRatio);
    }
}

// ---------------------------------------------------------------------------
// AGV pose update
// ---------------------------------------------------------------------------

void NavMapWidget::updateAgvPose(const hmi::Pose2D& pose)
{
    if (!m_mapItem) {
        return;  // No map loaded
    }

    QPointF pixelPos = worldToPixel(pose.x, pose.y);

    // Create triangle pointing in the direction of yaw
    // Triangle: base = 12px, height = 20px
    QPolygonF triangle;
    triangle << QPointF(0, -10) << QPointF(-6, 6) << QPointF(6, 6);

    if (!m_agvItem) {
        m_agvItem = m_scene->addPolygon(triangle, QPen(Qt::NoPen), QBrush(QColor("#ff5722")));
        m_agvItem->setZValue(10);
    } else {
        m_agvItem->setPolygon(triangle);
    }

    // Position and rotate
    m_agvItem->setPos(pixelPos);
    // yaw is in radians, convert to degrees
    double yawDeg = pose.yaw * 180.0 / M_PI;
    // Since the map's Y-axis is inverted (screen coordinates), we need to negate the rotation
    m_agvItem->setRotation(-yawDeg);
}

// ---------------------------------------------------------------------------
// Path display
// ---------------------------------------------------------------------------

void NavMapWidget::setPath(const hmi::InspectionPath& path)
{
    m_currentPath = path;
    m_highlightedIndex = -1;
    updatePathDisplay();
}

void NavMapWidget::highlightWaypoint(int index)
{
    m_highlightedIndex = index;
    updatePathDisplay();
}

void NavMapWidget::clearPath()
{
    m_currentPath.waypoints.clear();
    m_highlightedIndex = -1;
    updatePathDisplay();
}

void NavMapWidget::updatePathDisplay()
{
    // Remove old path
    if (m_pathItem) {
        m_scene->removeItem(m_pathItem);
        delete m_pathItem;
        m_pathItem = nullptr;
    }

    // Remove old waypoint markers
    for (auto* marker : m_waypointMarkers) {
        m_scene->removeItem(marker);
        delete marker;
    }
    m_waypointMarkers.clear();

    if (m_currentPath.waypoints.isEmpty() || !m_mapItem) {
        return;
    }

    // Draw path as green polyline
    QPainterPath polyline;
    bool first = true;
    for (const auto& wp : m_currentPath.waypoints) {
        QPointF pt = worldToPixel(wp.agvPose.x, wp.agvPose.y);
        if (first) {
            polyline.moveTo(pt);
            first = false;
        } else {
            polyline.lineTo(pt);
        }
    }

    QPen pathPen(QColor("#00e676"), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    m_pathItem = m_scene->addPath(polyline, pathPen);
    m_pathItem->setZValue(5);

    // Draw waypoint markers
    for (int i = 0; i < m_currentPath.waypoints.size(); ++i) {
        const auto& wp = m_currentPath.waypoints[i];
        QPointF pt = worldToPixel(wp.agvPose.x, wp.agvPose.y);

        bool isHighlighted = (i == m_highlightedIndex);
        double radius = isHighlighted ? 6.0 : 3.0;
        QColor color = isHighlighted ? QColor("#ffeb3b") : QColor("#4caf50");

        QGraphicsEllipseItem* marker = m_scene->addEllipse(
            pt.x() - radius, pt.y() - radius, 2 * radius, 2 * radius,
            QPen(Qt::NoPen),
            QBrush(color)
        );
        marker->setZValue(6);
        m_waypointMarkers.append(marker);
    }
}

// ---------------------------------------------------------------------------
// Coordinate transformation
// ---------------------------------------------------------------------------

QPointF NavMapWidget::worldToPixel(double x, double y) const
{
    if (m_mapInfo.resolutionMPerPixel <= 0.0) {
        return QPointF(0, 0);
    }

    // From NavMapInfo documentation:
    //   u = (x - origin.x) / resolution
    //   v = (origin.y - y) / resolution
    double u = (x - m_mapInfo.origin.x) / m_mapInfo.resolutionMPerPixel;
    double v = (m_mapInfo.origin.y - y) / m_mapInfo.resolutionMPerPixel;

    return QPointF(u, v);
}
