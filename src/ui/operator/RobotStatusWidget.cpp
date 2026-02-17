// src/ui/operator/RobotStatusWidget.cpp

#include "RobotStatusWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

RobotStatusWidget::RobotStatusWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void RobotStatusWidget::setupUi()
{
    QVBoxLayout* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(6);

    vlay->addWidget(createAgvSection());
    vlay->addWidget(createArmSection());
    vlay->addWidget(createInterlockSection());
    vlay->addStretch();
}

QWidget* RobotStatusWidget::createAgvSection()
{
    QGroupBox* box = new QGroupBox(QStringLiteral("AGV 状态"), this);
    QFormLayout* form = new QFormLayout(box);
    form->setContentsMargins(6, 6, 6, 6);
    form->setSpacing(4);

    m_agvConnLabel = new QLabel(QStringLiteral("● 未连接"), box);
    setConnectionIndicator(m_agvConnLabel, false);
    form->addRow(QStringLiteral("连接:"), m_agvConnLabel);

    m_agvStateLabel = new QLabel(QStringLiteral("--"), box);
    form->addRow(QStringLiteral("状态:"), m_agvStateLabel);

    m_agvPoseLabel = new QLabel(QStringLiteral("x:-- y:-- θ:--"), box);
    m_agvPoseLabel->setWordWrap(true);
    form->addRow(QStringLiteral("位姿:"), m_agvPoseLabel);

    // Battery row with progress bar
    QHBoxLayout* batteryRow = new QHBoxLayout;
    batteryRow->setSpacing(4);
    m_agvBatteryLabel = new QLabel(QStringLiteral("0%"), box);
    m_agvBatteryLabel->setMinimumWidth(40);
    m_batteryBar = new QProgressBar(box);
    m_batteryBar->setRange(0, 100);
    m_batteryBar->setValue(0);
    m_batteryBar->setTextVisible(false);
    m_batteryBar->setMaximumHeight(16);
    batteryRow->addWidget(m_agvBatteryLabel);
    batteryRow->addWidget(m_batteryBar, 1);
    form->addRow(QStringLiteral("电量:"), batteryRow);

    m_agvVelocityLabel = new QLabel(QStringLiteral("线速:-- 角速:--"), box);
    m_agvVelocityLabel->setWordWrap(true);
    form->addRow(QStringLiteral("速度:"), m_agvVelocityLabel);

    m_agvLocQualityLabel = new QLabel(QStringLiteral("--"), box);
    form->addRow(QStringLiteral("定位质量:"), m_agvLocQualityLabel);

    return box;
}

QWidget* RobotStatusWidget::createArmSection()
{
    QGroupBox* box = new QGroupBox(QStringLiteral("机械臂状态"), this);
    QFormLayout* form = new QFormLayout(box);
    form->setContentsMargins(6, 6, 6, 6);
    form->setSpacing(4);

    m_armConnLabel = new QLabel(QStringLiteral("● 未连接"), box);
    setConnectionIndicator(m_armConnLabel, false);
    form->addRow(QStringLiteral("连接:"), m_armConnLabel);

    m_armStateLabel = new QLabel(QStringLiteral("--"), box);
    form->addRow(QStringLiteral("状态:"), m_armStateLabel);

    m_armJointsLabel = new QLabel(QStringLiteral("--"), box);
    m_armJointsLabel->setWordWrap(true);
    form->addRow(QStringLiteral("关节:"), m_armJointsLabel);

    m_armManipLabel = new QLabel(QStringLiteral("--"), box);
    form->addRow(QStringLiteral("可操作度:"), m_armManipLabel);

    m_armTcpLabel = new QLabel(QStringLiteral("--"), box);
    m_armTcpLabel->setWordWrap(true);
    form->addRow(QStringLiteral("TCP:"), m_armTcpLabel);

    return box;
}

QWidget* RobotStatusWidget::createInterlockSection()
{
    QGroupBox* box = new QGroupBox(QStringLiteral("联锁"), this);
    QVBoxLayout* vlay = new QVBoxLayout(box);
    vlay->setContentsMargins(6, 6, 6, 6);

    m_interlockLabel = new QLabel(QStringLiteral("● 未知"), box);
    m_interlockLabel->setWordWrap(true);
    vlay->addWidget(m_interlockLabel);

    return box;
}

// ---------------------------------------------------------------------------
// Data updates
// ---------------------------------------------------------------------------

void RobotStatusWidget::updateAgvStatus(const hmi::AgvStatus& status)
{
    // Connection
    setConnectionIndicator(m_agvConnLabel, status.connected);

    // State
    QStringList stateFlags;
    if (status.arrived)  stateFlags << QStringLiteral("已到达");
    if (status.moving)   stateFlags << QStringLiteral("运动中");
    if (status.stopped)  stateFlags << QStringLiteral("已停止");
    QString stateText = stateFlags.isEmpty() ? QStringLiteral("--") : stateFlags.join(", ");
    m_agvStateLabel->setText(stateText);

    // State label color
    if (status.moving) {
        m_agvStateLabel->setStyleSheet(QStringLiteral("QLabel { color: #007bff; }"));  // blue
    } else if (status.arrived) {
        m_agvStateLabel->setStyleSheet(QStringLiteral("QLabel { color: #28a745; }"));  // green
    } else {
        m_agvStateLabel->setStyleSheet(QString());
    }

    // Pose
    m_agvPoseLabel->setText(
        QStringLiteral("x:%.2f y:%.2f θ:%.2f°")
            .arg(status.currentPose.x)
            .arg(status.currentPose.y)
            .arg(status.currentPose.yaw * 180.0 / 3.14159265)
    );

    // Battery
    int batteryPct = static_cast<int>(status.batteryPercent);
    m_agvBatteryLabel->setText(QStringLiteral("%1%").arg(batteryPct));
    m_batteryBar->setValue(batteryPct);

    QColor batteryColor;
    if (batteryPct > 50) {
        batteryColor = QColor("#28a745");  // green
    } else if (batteryPct > 20) {
        batteryColor = QColor("#ffc107");  // yellow
    } else {
        batteryColor = QColor("#dc3545");  // red
    }
    m_batteryBar->setStyleSheet(QStringLiteral(
        "QProgressBar::chunk { background-color: %1; }"
    ).arg(batteryColor.name()));

    // Velocity
    m_agvVelocityLabel->setText(
        QStringLiteral("线速:%.2f m/s 角速:%.2f r/s")
            .arg(static_cast<double>(status.linearVelocityMps))
            .arg(static_cast<double>(status.angularVelocityRps))
    );

    // Localization quality
    m_agvLocQualityLabel->setText(
        QStringLiteral("%.1f%%").arg(static_cast<double>(status.localizationQuality * 100.0f))
    );
}

void RobotStatusWidget::updateArmStatus(const hmi::ArmStatus& status)
{
    // Connection
    setConnectionIndicator(m_armConnLabel, status.connected);

    // State
    QStringList stateFlags;
    if (status.arrived) stateFlags << QStringLiteral("已到达");
    if (status.moving)  stateFlags << QStringLiteral("运动中");
    if (status.servoEnabled) stateFlags << QStringLiteral("伺服使能");
    QString stateText = stateFlags.isEmpty() ? QStringLiteral("--") : stateFlags.join(", ");
    m_armStateLabel->setText(stateText);

    // State label color
    if (status.moving) {
        m_armStateLabel->setStyleSheet(QStringLiteral("QLabel { color: #007bff; }"));  // blue
    } else if (status.arrived) {
        m_armStateLabel->setStyleSheet(QStringLiteral("QLabel { color: #28a745; }"));  // green
    } else {
        m_armStateLabel->setStyleSheet(QString());
    }

    // Joints (abbreviated)
    QString jointsText = QStringLiteral("[%.1f, %.1f, %.1f, ...]")
        .arg(status.currentJoints[0] * 180.0 / 3.14159265)
        .arg(status.currentJoints[1] * 180.0 / 3.14159265)
        .arg(status.currentJoints[2] * 180.0 / 3.14159265);
    m_armJointsLabel->setText(jointsText);

    // Manipulability
    m_armManipLabel->setText(QStringLiteral("%.3f").arg(status.manipulability));

    // TCP pose (abbreviated)
    QString tcpText = QStringLiteral("xyz:(%.2f, %.2f, %.2f)")
        .arg(static_cast<double>(status.tcpPose.position.x()))
        .arg(static_cast<double>(status.tcpPose.position.y()))
        .arg(static_cast<double>(status.tcpPose.position.z()));
    m_armTcpLabel->setText(tcpText);
}

void RobotStatusWidget::updateInterlockStatus(bool ok, const QString& message)
{
    QString text;
    QString style;
    if (ok) {
        text = QStringLiteral("● 联锁正常");
        style = QStringLiteral("QLabel { color: #28a745; font-weight: bold; }");  // green
    } else {
        text = QStringLiteral("● 联锁异常: %1").arg(message.isEmpty() ? QStringLiteral("--") : message);
        style = QStringLiteral("QLabel { color: #dc3545; font-weight: bold; }");  // red
    }
    m_interlockLabel->setText(text);
    m_interlockLabel->setStyleSheet(style);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void RobotStatusWidget::setConnectionIndicator(QLabel* label, bool connected)
{
    if (connected) {
        label->setText(QStringLiteral("● 已连接"));
        label->setStyleSheet(QStringLiteral("QLabel { color: #28a745; font-weight: bold; }"));  // green
    } else {
        label->setText(QStringLiteral("● 未连接"));
        label->setStyleSheet(QStringLiteral("QLabel { color: #dc3545; font-weight: bold; }"));  // red
    }
}
