// src/ui/EditPanel.cpp

#include "EditPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

EditPanel::EditPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createAnnotationTab(), tr("标注"));
    m_tabs->addTab(createTaskTab(),       tr("任务"));

    layout->addWidget(m_tabs);
}

// ===========================================================================
// Tab 1 -- Annotation
// ===========================================================================

QWidget* EditPanel::createAnnotationTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget();
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // -----------------------------------------------------------------------
    // Selected point details (read-only)
    // -----------------------------------------------------------------------
    auto* detailsGroup = new QGroupBox(tr("选中点位"), content);
    auto* detailsForm = new QFormLayout(detailsGroup);

    m_pointIdLabel = new QLabel(tr("(未选择)"), detailsGroup);
    detailsForm->addRow(tr("点位 ID:"), m_pointIdLabel);

    m_positionLabel = new QLabel(tr("---"), detailsGroup);
    detailsForm->addRow(tr("位置:"), m_positionLabel);

    m_normalLabel = new QLabel(tr("---"), detailsGroup);
    detailsForm->addRow(tr("法向:"), m_normalLabel);

    mainLayout->addWidget(detailsGroup);

    // -----------------------------------------------------------------------
    // Delete button
    // -----------------------------------------------------------------------
    m_deleteBtn = new QPushButton(tr("删除点位"), content);
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentPointId >= 0) {
            emit targetDeleteRequested(m_currentPointId);
        }
    });
    mainLayout->addWidget(m_deleteBtn);

    // -----------------------------------------------------------------------
    // Point count summary
    // -----------------------------------------------------------------------
    m_pointCountLabel = new QLabel(tr("共 0 个检测点位"), content);
    m_pointCountLabel->setStyleSheet("QLabel { color: gray; }");
    mainLayout->addWidget(m_pointCountLabel);

    mainLayout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

// ===========================================================================
// Tab 2 -- Task (plan + execute + monitor merged)
// ===========================================================================

QWidget* EditPanel::createTaskTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget();
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // -----------------------------------------------------------------------
    // Task config group
    // -----------------------------------------------------------------------
    auto* taskGroup = new QGroupBox(tr("任务配置"), content);
    auto* taskForm = new QFormLayout(taskGroup);

    m_taskNameEdit = new QLineEdit(taskGroup);
    m_taskNameEdit->setPlaceholderText(tr("例: 巡检任务01"));
    taskForm->addRow(tr("任务名称:"), m_taskNameEdit);

    m_planBtn = new QPushButton(tr("提交并规划"), taskGroup);
    connect(m_planBtn, &QPushButton::clicked, this, [this]() {
        const QString taskName = m_taskNameEdit->text().trimmed();
        emit planRequested(taskName.isEmpty() ? tr("未命名任务") : taskName);
    });
    taskForm->addRow(QString(), m_planBtn);

    mainLayout->addWidget(taskGroup);

    // -----------------------------------------------------------------------
    // Plan result group
    // -----------------------------------------------------------------------
    auto* resultGroup = new QGroupBox(tr("规划结果"), content);
    auto* resultLayout = new QVBoxLayout(resultGroup);

    m_planStatsLabel = new QLabel(tr("尚未规划"), resultGroup);
    m_planStatsLabel->setWordWrap(true);
    m_planStatsLabel->setStyleSheet("QLabel { color: gray; }");
    resultLayout->addWidget(m_planStatsLabel);

    mainLayout->addWidget(resultGroup);

    // -----------------------------------------------------------------------
    // Execute control group
    // -----------------------------------------------------------------------
    auto* execGroup = new QGroupBox(tr("执行控制"), content);
    auto* execLayout = new QVBoxLayout(execGroup);

    // Button row
    auto* btnRow = new QHBoxLayout();

    m_startBtn = new QPushButton(tr("启动"), execGroup);
    m_startBtn->setEnabled(false);
    connect(m_startBtn, &QPushButton::clicked, this, [this]() {
        if (!m_currentPlanId.isEmpty()) {
            emit startRequested(m_currentPlanId, false);
        }
    });
    btnRow->addWidget(m_startBtn);

    m_pauseBtn = new QPushButton(tr("暂停"), execGroup);
    m_pauseBtn->setEnabled(false);
    connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
        emit pauseRequested();
    });
    btnRow->addWidget(m_pauseBtn);

    m_resumeBtn = new QPushButton(tr("继续"), execGroup);
    m_resumeBtn->setEnabled(false);
    connect(m_resumeBtn, &QPushButton::clicked, this, [this]() {
        emit resumeRequested();
    });
    btnRow->addWidget(m_resumeBtn);

    m_stopBtn = new QPushButton(tr("停止"), execGroup);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        emit stopRequested();
    });
    btnRow->addWidget(m_stopBtn);

    execLayout->addLayout(btnRow);

    // Progress bar
    m_progressBar = new QProgressBar(execGroup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    execLayout->addWidget(m_progressBar);

    // Current action label
    m_actionLabel = new QLabel(tr("空闲"), execGroup);
    m_actionLabel->setWordWrap(true);
    execLayout->addWidget(m_actionLabel);

    // Waypoint label
    m_waypointLabel = new QLabel(tr("航点: 0 / 0"), execGroup);
    execLayout->addWidget(m_waypointLabel);

    mainLayout->addWidget(execGroup);

    // -----------------------------------------------------------------------
    // System status group
    // -----------------------------------------------------------------------
    auto* statusGroup = new QGroupBox(tr("系统状态"), content);
    auto* statusLayout = new QVBoxLayout(statusGroup);

    m_agvStatusLabel = new QLabel(tr("AGV: 未连接"), statusGroup);
    statusLayout->addWidget(m_agvStatusLabel);

    m_armStatusLabel = new QLabel(tr("机械臂: 未连接"), statusGroup);
    statusLayout->addWidget(m_armStatusLabel);

    m_interlockLabel = new QLabel(tr("联锁: 未知"), statusGroup);
    statusLayout->addWidget(m_interlockLabel);

    mainLayout->addWidget(statusGroup);

    // -----------------------------------------------------------------------
    // Events group
    // -----------------------------------------------------------------------
    auto* eventsGroup = new QGroupBox(tr("事件"), content);
    auto* eventsLayout = new QVBoxLayout(eventsGroup);

    m_eventList = new QListWidget(eventsGroup);
    m_eventList->setMaximumHeight(200);
    eventsLayout->addWidget(m_eventList);

    mainLayout->addWidget(eventsGroup);

    mainLayout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

// ===========================================================================
// Tab 1 -- Annotation API
// ===========================================================================

void EditPanel::showTargetDetails(const hmi::InspectionTarget& target)
{
    m_currentPointId = target.pointId;

    m_pointIdLabel->setText(QString::number(target.pointId));
    m_positionLabel->setText(
        tr("(%.3f, %.3f, %.3f)")
            .arg(static_cast<double>(target.surface.position.x()))
            .arg(static_cast<double>(target.surface.position.y()))
            .arg(static_cast<double>(target.surface.position.z())));
    m_normalLabel->setText(
        tr("(%.3f, %.3f, %.3f)")
            .arg(static_cast<double>(target.surface.normal.x()))
            .arg(static_cast<double>(target.surface.normal.y()))
            .arg(static_cast<double>(target.surface.normal.z())));

    m_deleteBtn->setEnabled(true);
}

void EditPanel::clearTargetDetails()
{
    m_currentPointId = -1;
    m_pointIdLabel->setText(tr("(未选择)"));
    m_positionLabel->setText(tr("---"));
    m_normalLabel->setText(tr("---"));
    m_deleteBtn->setEnabled(false);
}

void EditPanel::setPointCount(int count)
{
    m_pointCountLabel->setText(tr("共 %1 个检测点位").arg(count));
}

// ===========================================================================
// Default configurations (hard-coded)
// ===========================================================================

hmi::CaptureConfig EditPanel::defaultCaptureConfig()
{
    hmi::CaptureConfig cfg;
    cfg.cameraId            = QStringLiteral("hikvision_0");
    cfg.focusDistanceM      = 0.5;
    cfg.fovHDeg             = 60.0;
    cfg.fovVDeg             = 45.0;
    cfg.maxTiltFromNormalDeg = 30.0;
    return cfg;
}

hmi::PlanOptions EditPanel::defaultPlanOptions()
{
    hmi::PlanOptions opts;
    opts.candidateRadiusM      = 1.0;
    opts.candidateYawStepDeg   = 30.0;
    opts.enableCollisionCheck  = true;
    opts.enableTspOptimization = true;
    opts.ikSolver              = QStringLiteral("TracIK");
    return opts;
}

// ===========================================================================
// Tab 2 -- Task API: Plan result
// ===========================================================================

void EditPanel::showPlanResult(const hmi::PlanResponse& response)
{
    if (!response.result.ok()) {
        m_planStatsLabel->setText(
            tr("规划失败: %1").arg(response.result.message));
        m_planStatsLabel->setStyleSheet("QLabel { color: red; }");
        m_startBtn->setEnabled(false);
        return;
    }

    m_currentPlanId = response.planId;

    const QString stats = tr(
        "✓ %1个点位 | %.2f m | %.1f ms")
                              .arg(response.path.totalPoints)
                              .arg(response.path.estimatedDistanceM)
                              .arg(response.stats.planningTimeMs);

    m_planStatsLabel->setText(stats);
    m_planStatsLabel->setStyleSheet("QLabel { color: green; }");
    m_startBtn->setEnabled(true);
}

// ===========================================================================
// Tab 2 -- Task API: Task status
// ===========================================================================

void EditPanel::updateTaskStatus(const hmi::TaskStatus& status)
{
    m_currentTaskId = status.taskId;

    // Progress
    m_progressBar->setValue(static_cast<int>(status.progressPercent));
    m_actionLabel->setText(status.currentAction);
    m_waypointLabel->setText(
        tr("航点: %1 / %2").arg(status.currentWaypointIndex).arg(status.totalWaypoints));

    // AGV status
    if (status.agv.connected) {
        QString agvState;
        if (status.agv.arrived)
            agvState = tr("到位");
        else if (status.agv.moving)
            agvState = tr("移动中");
        else
            agvState = tr("停止");

        m_agvStatusLabel->setText(
            tr("AGV: ●在线 | %1 | %2%")
                .arg(agvState)
                .arg(static_cast<int>(status.agv.batteryPercent)));
        m_agvStatusLabel->setStyleSheet("QLabel { color: green; }");
    } else {
        m_agvStatusLabel->setText(tr("AGV: ○离线"));
        m_agvStatusLabel->setStyleSheet("QLabel { color: gray; }");
    }

    // Arm status
    if (status.arm.connected) {
        QString armState;
        if (status.arm.arrived)
            armState = tr("到位");
        else if (status.arm.moving)
            armState = tr("运动中");
        else
            armState = tr("空闲");

        m_armStatusLabel->setText(tr("机械臂: ●在线 | %1").arg(armState));
        m_armStatusLabel->setStyleSheet("QLabel { color: green; }");
    } else {
        m_armStatusLabel->setText(tr("机械臂: ○离线"));
        m_armStatusLabel->setStyleSheet("QLabel { color: gray; }");
    }

    // Interlock
    if (status.interlockOk) {
        m_interlockLabel->setText(tr("联锁: ●正常"));
        m_interlockLabel->setStyleSheet("QLabel { color: green; }");
    } else {
        m_interlockLabel->setText(tr("联锁: ●异常 – %1").arg(status.interlockMessage));
        m_interlockLabel->setStyleSheet("QLabel { color: red; }");
    }

    // Update button enable states based on task phase
    using P = hmi::TaskPhase;
    switch (status.phase) {
    case P::Executing:
        m_startBtn->setEnabled(false);
        m_pauseBtn->setEnabled(true);
        m_resumeBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        m_planBtn->setEnabled(false);
        break;
    case P::Paused:
        m_startBtn->setEnabled(false);
        m_pauseBtn->setEnabled(false);
        m_resumeBtn->setEnabled(true);
        m_stopBtn->setEnabled(true);
        m_planBtn->setEnabled(false);
        break;
    case P::Completed:
    case P::Failed:
    case P::Stopped:
        m_startBtn->setEnabled(!m_currentPlanId.isEmpty());
        m_pauseBtn->setEnabled(false);
        m_resumeBtn->setEnabled(false);
        m_stopBtn->setEnabled(false);
        m_planBtn->setEnabled(true);
        break;
    default:
        // Idle, Localizing, Planning, Unspecified
        break;
    }
}

// ===========================================================================
// Tab 2 -- Task API: Events
// ===========================================================================

void EditPanel::addEvent(const hmi::InspectionEvent& event)
{
    const QString typeStr = [&event]() -> QString {
        using T = hmi::InspectionEventType;
        switch (event.type) {
        case T::Info:        return QStringLiteral("[信息]");
        case T::Warn:        return QStringLiteral("[警告]");
        case T::Error:       return QStringLiteral("[错误]");
        case T::Captured:    return QStringLiteral("[拍摄]");
        case T::DefectFound: return QStringLiteral("[缺陷]");
        default:             return QStringLiteral("[未知]");
        }
    }();

    const QString timeStr = event.timestamp.isValid()
        ? event.timestamp.toString("HH:mm")
        : QStringLiteral("--:--");

    const QString text = tr("%1 %2 点%3 %4")
                             .arg(typeStr)
                             .arg(timeStr)
                             .arg(event.pointId)
                             .arg(event.message);

    auto* item = new QListWidgetItem(text, m_eventList);

    // Color code by event type
    using T = hmi::InspectionEventType;
    switch (event.type) {
    case T::Warn:
        item->setForeground(QColor(255, 140, 0)); // orange
        break;
    case T::Error:
        item->setForeground(Qt::red);
        break;
    case T::DefectFound:
        item->setForeground(QColor(255, 0, 255)); // magenta
        break;
    default:
        break;
    }

    m_eventList->addItem(item);
    m_eventList->scrollToBottom();

    // Limit event list size to last 100 items
    while (m_eventList->count() > 100) {
        delete m_eventList->takeItem(0);
    }
}
