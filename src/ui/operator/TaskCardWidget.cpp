// src/ui/operator/TaskCardWidget.cpp

#include "TaskCardWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TaskCardWidget::TaskCardWidget(QWidget* parent)
    : QFrame(parent)
{
    setupUi();
    clear();
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void TaskCardWidget::setupUi()
{
    // Style: rounded border, light background
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setStyleSheet(QStringLiteral(
        "TaskCardWidget {"
        "  background-color: #f8f9fa;"
        "  border: 1px solid #dee2e6;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "}"
    ));

    QVBoxLayout* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(8, 8, 8, 8);
    vlay->setSpacing(4);

    // Top row: task name + phase badge
    QHBoxLayout* topRow = new QHBoxLayout;
    topRow->setSpacing(8);

    m_taskNameLabel = new QLabel(QStringLiteral("任务: --"), this);
    QFont taskFont = m_taskNameLabel->font();
    taskFont.setPointSize(taskFont.pointSize() + 2);
    taskFont.setBold(true);
    m_taskNameLabel->setFont(taskFont);
    topRow->addWidget(m_taskNameLabel, 1);

    m_phaseLabel = new QLabel(QStringLiteral("空闲"), this);
    m_phaseLabel->setAlignment(Qt::AlignCenter);
    m_phaseLabel->setMinimumWidth(80);
    m_phaseLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: #6c757d;"  // default gray
        "  color: white;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-weight: bold;"
        "}"
    ));
    topRow->addWidget(m_phaseLabel, 0);

    vlay->addLayout(topRow);

    // Middle row: progress percentage + bar
    QHBoxLayout* progressRow = new QHBoxLayout;
    progressRow->setSpacing(8);

    m_progressLabel = new QLabel(QStringLiteral("进度: 0%"), this);
    m_progressLabel->setMinimumWidth(80);
    progressRow->addWidget(m_progressLabel, 0);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setMaximumHeight(18);
    progressRow->addWidget(m_progressBar, 1);

    vlay->addLayout(progressRow);

    // Bottom row: waypoint + time
    QHBoxLayout* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);

    m_waypointLabel = new QLabel(QStringLiteral("当前点位: --/--"), this);
    bottomRow->addWidget(m_waypointLabel, 1);

    m_timeLabel = new QLabel(QStringLiteral("用时: -- / 剩余: --"), this);
    m_timeLabel->setAlignment(Qt::AlignRight);
    bottomRow->addWidget(m_timeLabel, 1);

    vlay->addLayout(bottomRow);
}

// ---------------------------------------------------------------------------
// Data update
// ---------------------------------------------------------------------------

void TaskCardWidget::updateStatus(const hmi::TaskStatus& status)
{
    // Task name
    if (status.taskName.isEmpty()) {
        m_taskNameLabel->setText(QStringLiteral("任务: %1").arg(status.taskId.left(8)));
    } else {
        m_taskNameLabel->setText(QStringLiteral("任务: %1").arg(status.taskName));
    }

    // Phase badge
    QString phaseText = phaseToString(status.phase);
    QString phaseColor = phaseToColor(status.phase);
    m_phaseLabel->setText(phaseText);
    m_phaseLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: %1;"
        "  color: white;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-weight: bold;"
        "}"
    ).arg(phaseColor));

    // Progress
    int progressInt = static_cast<int>(status.progressPercent);
    m_progressLabel->setText(QStringLiteral("进度: %1%").arg(progressInt));
    m_progressBar->setValue(progressInt);

    // Waypoint
    m_waypointLabel->setText(
        QStringLiteral("当前点位: %1 / %2")
            .arg(status.currentWaypointIndex)
            .arg(status.totalWaypoints)
    );

    // Time
    QString elapsedStr = QStringLiteral("--");
    QString remainingStr = QStringLiteral("--");

    if (status.startedAt.isValid()) {
        qint64 elapsedSec = status.startedAt.secsTo(QDateTime::currentDateTime());
        int elapsedMin = static_cast<int>(elapsedSec / 60);
        elapsedStr = QStringLiteral("%1 分").arg(elapsedMin);
    }

    if (status.remainingTimeEstS > 0.0) {
        int remainingMin = static_cast<int>(status.remainingTimeEstS / 60.0);
        remainingStr = QStringLiteral("%1 分").arg(remainingMin);
    }

    m_timeLabel->setText(QStringLiteral("用时: %1 / 剩余: %2").arg(elapsedStr, remainingStr));
}

void TaskCardWidget::clear()
{
    m_taskNameLabel->setText(QStringLiteral("任务: --"));
    m_phaseLabel->setText(QStringLiteral("空闲"));
    m_phaseLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: #6c757d;"
        "  color: white;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-weight: bold;"
        "}"
    ));
    m_progressLabel->setText(QStringLiteral("进度: 0%"));
    m_progressBar->setValue(0);
    m_waypointLabel->setText(QStringLiteral("当前点位: --/--"));
    m_timeLabel->setText(QStringLiteral("用时: -- / 剩余: --"));
}

// ---------------------------------------------------------------------------
// Phase helpers
// ---------------------------------------------------------------------------

QString TaskCardWidget::phaseToString(hmi::TaskPhase phase)
{
    switch (phase) {
        case hmi::TaskPhase::Idle:        return QStringLiteral("空闲");
        case hmi::TaskPhase::Localizing:  return QStringLiteral("定位中");
        case hmi::TaskPhase::Planning:    return QStringLiteral("规划中");
        case hmi::TaskPhase::Executing:   return QStringLiteral("执行中");
        case hmi::TaskPhase::Paused:      return QStringLiteral("已暂停");
        case hmi::TaskPhase::Completed:   return QStringLiteral("已完成");
        case hmi::TaskPhase::Failed:      return QStringLiteral("失败");
        case hmi::TaskPhase::Stopped:     return QStringLiteral("已停止");
        default:                          return QStringLiteral("未知");
    }
}

QString TaskCardWidget::phaseToColor(hmi::TaskPhase phase)
{
    switch (phase) {
        case hmi::TaskPhase::Idle:        return QStringLiteral("#6c757d");  // gray
        case hmi::TaskPhase::Localizing:  return QStringLiteral("#17a2b8");  // cyan
        case hmi::TaskPhase::Planning:    return QStringLiteral("#007bff");  // blue
        case hmi::TaskPhase::Executing:   return QStringLiteral("#28a745");  // green
        case hmi::TaskPhase::Paused:      return QStringLiteral("#ffc107");  // amber
        case hmi::TaskPhase::Completed:   return QStringLiteral("#20c997");  // teal
        case hmi::TaskPhase::Failed:      return QStringLiteral("#dc3545");  // red
        case hmi::TaskPhase::Stopped:     return QStringLiteral("#6c757d");  // gray
        default:                          return QStringLiteral("#6c757d");
    }
}
