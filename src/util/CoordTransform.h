// src/util/CoordTransform.h
//
// Header-only coordinate transformation utilities for the Inspection HMI.
//
// Provides conversion helpers between:
//   - World coordinates <-> pixel coordinates (navigation map)
//   - Pose2D <-> Pose3D conversions
//   - Yaw angles <-> quaternions
//   - Camera/view geometry computations
//   - Formatting utilities for display
//
// All functions are inline for zero-overhead abstraction. This header depends
// on core/Types.h for Pose2D/Pose3D definitions.

#pragma once

#include "Types.h"

#include <QPointF>
#include <QQuaternion>
#include <QString>
#include <QStringList>
#include <QVector3D>
#include <QtMath>

#include <array>
#include <cmath>

namespace hmi {
namespace coord {

// ---------------------------------------------------------------------------
// Navigation map world <-> pixel conversion
// ---------------------------------------------------------------------------

/// Convert world coordinates (x, y) to pixel coordinates (u, v) using the
/// navigation map convention:
///   u = (x - origin.x) / resolution
///   v = (origin.y - y) / resolution
///
/// \param worldX       World X coordinate (metres).
/// \param worldY       World Y coordinate (metres).
/// \param originX      Map origin X (metres).
/// \param originY      Map origin Y (metres).
/// \param resolution   Map resolution (metres per pixel).
/// \return             Pixel coordinates (u, v).
inline QPointF worldToPixel(double worldX, double worldY,
                            double originX, double originY,
                            double resolution) noexcept
{
    double u = (worldX - originX) / resolution;
    double v = (originY - worldY) / resolution;
    return QPointF(u, v);
}

/// Convert pixel coordinates (u, v) back to world coordinates (x, y).
///
/// \param u            Pixel U coordinate.
/// \param v            Pixel V coordinate.
/// \param originX      Map origin X (metres).
/// \param originY      Map origin Y (metres).
/// \param resolution   Map resolution (metres per pixel).
/// \return             World coordinates (x, y).
inline QPointF pixelToWorld(double u, double v,
                            double originX, double originY,
                            double resolution) noexcept
{
    double x = u * resolution + originX;
    double y = originY - v * resolution;
    return QPointF(x, y);
}

// ---------------------------------------------------------------------------
// Yaw <-> Quaternion conversion
// ---------------------------------------------------------------------------

/// Convert a yaw angle (radians) to a quaternion representing rotation
/// around the Z axis.
///
/// \param yawRad  Yaw angle in radians.
/// \return        Quaternion (x, y, z, w).
inline QQuaternion yawToQuaternion(double yawRad) noexcept
{
    // QQuaternion::fromAxisAndAngle expects angle in degrees.
    return QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1),
                                         qRadiansToDegrees(yawRad));
}

/// Extract the yaw angle (radians) from a quaternion.
/// Assumes the quaternion represents a rotation primarily around Z.
///
/// \param q  Input quaternion.
/// \return   Yaw angle in radians.
inline double quaternionToYaw(const QQuaternion& q) noexcept
{
    // Extract yaw from quaternion:
    // yaw = atan2(2*(qw*qz + qx*qy), 1 - 2*(qy*qy + qz*qz))
    float qw = q.scalar();
    float qx = q.x();
    float qy = q.y();
    float qz = q.z();
    return std::atan2(2.0 * (qw * qz + qx * qy),
                      1.0 - 2.0 * (qy * qy + qz * qz));
}

// ---------------------------------------------------------------------------
// Pose2D <-> Pose3D conversion
// ---------------------------------------------------------------------------

/// Convert a 2D pose (x, y, yaw) to a 3D pose (x, y, 0, quaternion).
///
/// \param p2d  Input 2D pose.
/// \return     Corresponding 3D pose with Z=0.
inline Pose3D pose2Dto3D(const Pose2D& p2d) noexcept
{
    Pose3D p3d;
    p3d.position = QVector3D(static_cast<float>(p2d.x),
                             static_cast<float>(p2d.y),
                             0.0f);
    p3d.orientation = yawToQuaternion(p2d.yaw);
    p3d.frameId = p2d.frameId;
    return p3d;
}

/// Project a 3D pose onto the XY plane, extracting (x, y, yaw).
///
/// \param p3d  Input 3D pose.
/// \return     Corresponding 2D pose.
inline Pose2D pose3Dto2D(const Pose3D& p3d) noexcept
{
    Pose2D p2d;
    p2d.x = p3d.position.x();
    p2d.y = p3d.position.y();
    p2d.yaw = quaternionToYaw(p3d.orientation);
    p2d.frameId = p3d.frameId;
    return p2d;
}

// ---------------------------------------------------------------------------
// Camera view geometry helpers
// ---------------------------------------------------------------------------

/// Compute the default view direction from a surface normal.
/// The camera looks directly at the surface (opposite of normal).
///
/// \param surfaceNormal  Surface normal vector.
/// \return               View direction (unit vector).
inline QVector3D defaultViewDirection(const QVector3D& surfaceNormal) noexcept
{
    return -surfaceNormal.normalized();
}

/// Compute the camera position given a surface point, view direction, and
/// focus distance.
///
/// \param surfacePos      Point on the surface (metres).
/// \param viewDir         Camera view direction (unit vector).
/// \param focusDistance   Distance from surface to camera (metres).
/// \return                Camera position (metres).
inline QVector3D cameraPosition(const QVector3D& surfacePos,
                                const QVector3D& viewDir,
                                double focusDistance) noexcept
{
    // Camera is positioned at focusDistance along -viewDirection from surface.
    return surfacePos - viewDir.normalized() * static_cast<float>(focusDistance);
}

/// Compute the four corner points of the camera frustum at the focal plane.
/// Returns corners in the order: top-left, top-right, bottom-right, bottom-left.
///
/// \param cameraPos      Camera position (metres).
/// \param viewDir        Camera view direction (unit vector).
/// \param focusDistance  Distance to focal plane (metres).
/// \param fovHDeg        Horizontal field of view (degrees).
/// \param fovVDeg        Vertical field of view (degrees).
/// \return               Array of 4 corner points [TL, TR, BR, BL].
inline std::array<QVector3D, 4> frustumCorners(const QVector3D& cameraPos,
                                                const QVector3D& viewDir,
                                                double focusDistance,
                                                double fovHDeg,
                                                double fovVDeg) noexcept
{
    QVector3D forward = viewDir.normalized();

    // Find an up vector not parallel to forward.
    QVector3D worldUp(0, 0, 1);
    if (std::abs(QVector3D::dotProduct(forward, worldUp)) > 0.99f) {
        worldUp = QVector3D(0, 1, 0);
    }

    QVector3D right = QVector3D::crossProduct(forward, worldUp).normalized();
    QVector3D up = QVector3D::crossProduct(right, forward).normalized();

    double halfW = focusDistance * std::tan(qDegreesToRadians(fovHDeg / 2.0));
    double halfH = focusDistance * std::tan(qDegreesToRadians(fovVDeg / 2.0));

    QVector3D center = cameraPos + forward * static_cast<float>(focusDistance);

    return {
        center - right * static_cast<float>(halfW) + up * static_cast<float>(halfH),  // top-left
        center + right * static_cast<float>(halfW) + up * static_cast<float>(halfH),  // top-right
        center + right * static_cast<float>(halfW) - up * static_cast<float>(halfH),  // bottom-right
        center - right * static_cast<float>(halfW) - up * static_cast<float>(halfH)   // bottom-left
    };
}

// ---------------------------------------------------------------------------
// Angle unit conversion
// ---------------------------------------------------------------------------

/// Convert degrees to radians.
inline constexpr double degToRad(double deg) noexcept
{
    return qDegreesToRadians(deg);
}

/// Convert radians to degrees.
inline constexpr double radToDeg(double rad) noexcept
{
    return qRadiansToDegrees(rad);
}

// ---------------------------------------------------------------------------
// Formatting utilities for display
// ---------------------------------------------------------------------------

/// Format a 6-DOF joint configuration for display.
/// Converts radians to degrees and returns a string like:
///   "J1: 45.0°  J2: -30.5°  J3: 90.0° ..."
///
/// \param joints  Array of 6 joint angles (radians).
/// \return        Formatted string.
inline QString formatJoints(const std::array<double, 6>& joints)
{
    QStringList parts;
    parts.reserve(6);
    for (int i = 0; i < 6; ++i) {
        parts << QString("J%1: %2°")
                 .arg(i + 1)
                 .arg(qRadiansToDegrees(joints[i]), 0, 'f', 1);
    }
    return parts.join("  ");
}

/// Format a 2D pose for display.
/// Returns a string like: "(1.234, 5.678, 90.0°)"
///
/// \param p  2D pose.
/// \return   Formatted string.
inline QString formatPose2D(const Pose2D& p)
{
    return QString("(%1, %2, %3°)")
        .arg(p.x, 0, 'f', 3)
        .arg(p.y, 0, 'f', 3)
        .arg(qRadiansToDegrees(p.yaw), 0, 'f', 1);
}

/// Format a 3D vector for display.
/// Returns a string like: "(1.234, 5.678, 9.012)"
///
/// \param v  3D vector.
/// \return   Formatted string.
inline QString formatVec3(const QVector3D& v)
{
    return QString("(%1, %2, %3)")
        .arg(v.x(), 0, 'f', 3)
        .arg(v.y(), 0, 'f', 3)
        .arg(v.z(), 0, 'f', 3);
}

/// Format a quaternion for display as Euler angles (roll, pitch, yaw).
/// Returns a string like: "RPY: (0.0°, 5.0°, 90.0°)"
///
/// \param q  Quaternion.
/// \return   Formatted string.
inline QString formatQuaternion(const QQuaternion& q)
{
    // Convert quaternion to Euler angles (intrinsic XYZ order).
    QVector3D euler = q.toEulerAngles();
    return QString("RPY: (%1°, %2°, %3°)")
        .arg(euler.x(), 0, 'f', 1)
        .arg(euler.y(), 0, 'f', 1)
        .arg(euler.z(), 0, 'f', 1);
}

/// Format a 3D pose for display.
/// Returns a multi-line string with position and orientation.
///
/// \param p  3D pose.
/// \return   Formatted string.
inline QString formatPose3D(const Pose3D& p)
{
    return QString("Pos: %1\nOri: %2")
        .arg(formatVec3(p.position))
        .arg(formatQuaternion(p.orientation));
}

} // namespace coord
} // namespace hmi
