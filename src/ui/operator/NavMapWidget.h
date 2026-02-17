// src/ui/operator/NavMapWidget.h
//
// NavMapWidget – 2D navigation map visualization using QGraphicsView.
//
// Displays the occupancy grid, planned path, AGV pose, and waypoint markers.

#pragma once

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsEllipseItem>
#include <QImage>
#include <QVector>
#include <QEvent>

#include "core/Types.h"

/// \brief A 2D navigation map visualization using QGraphicsView.
///
/// Coordinate system mapping:
///   World (metric) -> Pixel:
///     u = (x - origin.x) / resolution
///     v = (origin.y - y) / resolution
///
/// AGV is rendered as a triangle marker, waypoints as circles, and the path
/// as a green polyline.  Supports mouse wheel zoom and drag pan.
class NavMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NavMapWidget(QWidget* parent = nullptr);

    /// Load and display a navigation map.
    void setNavMap(const hmi::NavMapInfo& mapInfo, const QImage& mapImage);

    /// Update the AGV triangle marker to reflect current pose.
    void updateAgvPose(const hmi::Pose2D& pose);

    /// Set the inspection path and render it as a polyline with waypoint markers.
    void setPath(const hmi::InspectionPath& path);

    /// Highlight a specific waypoint (e.g., the current target).
    void highlightWaypoint(int index);

    /// Clear the path and all waypoint markers.
    void clearPath();

protected:
    /// Event filter for mouse wheel zoom on the viewport.
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();

    /// Convert world (metric) coordinates to pixel coordinates.
    QPointF worldToPixel(double x, double y) const;

    /// Redraw the path polyline and waypoint markers.
    void updatePathDisplay();

    QGraphicsView*  m_view        = nullptr;
    QGraphicsScene* m_scene       = nullptr;

    QGraphicsPixmapItem*  m_mapItem     = nullptr;
    QGraphicsPathItem*    m_pathItem    = nullptr;
    QGraphicsPolygonItem* m_agvItem     = nullptr;   // AGV triangle
    QVector<QGraphicsEllipseItem*> m_waypointMarkers;

    hmi::NavMapInfo m_mapInfo;
    hmi::InspectionPath m_currentPath;
    int m_highlightedIndex = -1;
};
