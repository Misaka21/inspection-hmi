// src/ui/operator/RobotStatusWidget.h
//
// RobotStatusWidget – displays AGV and Arm status in a compact card layout.
//
// Shows connection state, motion state, pose/joints, battery, interlock, etc.

#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>

#include "core/Types.h"

/// \brief Displays AGV and Arm status in a compact, two-section card.
///
/// Uses colored indicators (●) to show connection and motion states.
/// AGV section: connection, state, pose, battery, velocity, localization quality.
/// Arm section: connection, state, joints, manipulability, TCP pose.
/// Interlock section: shows interlock OK/NOK status.
class RobotStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RobotStatusWidget(QWidget* parent = nullptr);

    void updateAgvStatus(const hmi::AgvStatus& status);
    void updateArmStatus(const hmi::ArmStatus& status);
    void updateInterlockStatus(bool ok, const QString& message);

private:
    void setupUi();
    QWidget* createAgvSection();
    QWidget* createArmSection();
    QWidget* createInterlockSection();

    /// Set connection indicator: green if connected, red otherwise.
    static void setConnectionIndicator(QLabel* label, bool connected);

    // AGV section
    QLabel* m_agvConnLabel       = nullptr;
    QLabel* m_agvStateLabel      = nullptr;
    QLabel* m_agvPoseLabel       = nullptr;
    QLabel* m_agvBatteryLabel    = nullptr;
    QProgressBar* m_batteryBar   = nullptr;
    QLabel* m_agvVelocityLabel   = nullptr;
    QLabel* m_agvLocQualityLabel = nullptr;

    // Arm section
    QLabel* m_armConnLabel      = nullptr;
    QLabel* m_armStateLabel     = nullptr;
    QLabel* m_armJointsLabel    = nullptr;
    QLabel* m_armManipLabel     = nullptr;
    QLabel* m_armTcpLabel       = nullptr;

    // Interlock section
    QLabel* m_interlockLabel    = nullptr;
};
