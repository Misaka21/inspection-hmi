// src/ui/operator/OperatorWindow.cpp

#include "OperatorWindow.h"

#include "TaskCardWidget.h"
#include "NavMapWidget.h"
#include "RobotStatusWidget.h"
#include "ControlPanel.h"
#include "ResultPanel.h"

#include <QToolBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <QSizePolicy>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

OperatorWindow::OperatorWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("巡检 HMI – 操作员模式"));
    setMinimumSize(800, 600);
    setupUi();
}

OperatorWindow::~OperatorWindow() = default;

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void OperatorWindow::setupUi()
{
    // ---- Toolbar ----
    QToolBar* toolbar = addToolBar(QStringLiteral("操作员工具栏"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    m_switchModeAction = toolbar->addAction(QStringLiteral("切换到工程师模式"));
    m_switchModeAction->setToolTip(QStringLiteral("切换到工程师（调试）模式"));
    connect(m_switchModeAction, &QAction::triggered,
            this, &OperatorWindow::switchToEngineerMode);

    // ---- Central widget with vertical layout ----
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* vlay = new QVBoxLayout(central);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);

    // Top: task summary card
    m_taskCard = new TaskCardWidget(central);
    m_taskCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    vlay->addWidget(m_taskCard);

    // Middle: horizontal splitter – nav map left, robot status right
    QSplitter* midSplitter = new QSplitter(Qt::Horizontal, central);
    midSplitter->setChildrenCollapsible(false);

    m_navMap = new NavMapWidget(midSplitter);
    m_navMap->setMinimumSize(300, 250);
    midSplitter->addWidget(m_navMap);

    m_robotStatus = new RobotStatusWidget(midSplitter);
    m_robotStatus->setMinimumSize(220, 250);
    midSplitter->addWidget(m_robotStatus);

    // Give the nav map 2/3 and robot status 1/3 of the initial width
    midSplitter->setStretchFactor(0, 2);
    midSplitter->setStretchFactor(1, 1);

    vlay->addWidget(midSplitter, /*stretch=*/1);

    // Below middle: control buttons
    m_controlPanel = new ControlPanel(central);
    m_controlPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    vlay->addWidget(m_controlPanel);

    // Bottom: result panel (gallery + timeline)
    m_resultPanel = new ResultPanel(central);
    m_resultPanel->setMinimumHeight(180);
    m_resultPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    vlay->addWidget(m_resultPanel, /*stretch=*/0);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

TaskCardWidget* OperatorWindow::taskCard() const
{
    return m_taskCard;
}

NavMapWidget* OperatorWindow::navMap() const
{
    return m_navMap;
}

RobotStatusWidget* OperatorWindow::robotStatus() const
{
    return m_robotStatus;
}

ControlPanel* OperatorWindow::controlPanel() const
{
    return m_controlPanel;
}

ResultPanel* OperatorWindow::resultPanel() const
{
    return m_resultPanel;
}

// ---------------------------------------------------------------------------
// Data routing
// ---------------------------------------------------------------------------

void OperatorWindow::updateTaskStatus(const hmi::TaskStatus& status)
{
    m_taskCard->updateStatus(status);
    m_navMap->updateAgvPose(status.agv.currentPose);
    m_robotStatus->updateAgvStatus(status.agv);
    m_robotStatus->updateArmStatus(status.arm);
    m_robotStatus->updateInterlockStatus(status.interlockOk, status.interlockMessage);
    m_controlPanel->setTaskPhase(status.phase);
    m_controlPanel->setTaskId(status.taskId);
    m_controlPanel->setPlanId(status.planId);
}

void OperatorWindow::addEvent(const hmi::InspectionEvent& event)
{
    m_resultPanel->addEvent(event);
    if (event.type == hmi::InspectionEventType::Captured ||
        event.type == hmi::InspectionEventType::DefectFound)
    {
        m_resultPanel->addCaptureEvent(event);
    }
}
