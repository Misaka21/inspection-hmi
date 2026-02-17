// src/ui/operator/OperatorWindow.h
//
// OperatorWindow – top-level QMainWindow for Operator mode.
//
// Layout (vertical, compact for small screens)
// -----------------------------------------------
//   Toolbar        : "切换到工程师模式" action
//   TaskCardWidget : task summary bar at the top
//   QSplitter      : NavMapWidget (left) | RobotStatusWidget (right)
//   ControlPanel   : start / pause / resume / stop
//   ResultPanel    : capture gallery + event timeline

#pragma once

#include <QMainWindow>
#include <QAction>
#include <QSplitter>
#include <QVBoxLayout>

#include "core/Types.h"

// Forward declarations
class TaskCardWidget;
class NavMapWidget;
class RobotStatusWidget;
class ControlPanel;
class ResultPanel;

/// \brief Top-level application window for Operator mode.
///
/// Provides a compact, touch-friendly vertical layout suitable for
/// small-screen industrial HMI panels.
class OperatorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit OperatorWindow(QWidget* parent = nullptr);
    ~OperatorWindow() override;

    // Sub-component accessors
    TaskCardWidget*   taskCard()     const;
    NavMapWidget*     navMap()       const;
    RobotStatusWidget* robotStatus() const;
    ControlPanel*     controlPanel() const;
    ResultPanel*      resultPanel()  const;

    /// Push a full TaskStatus snapshot to all child widgets.
    void updateTaskStatus(const hmi::TaskStatus& status);

    /// Append an inspection event to the result panel.
    void addEvent(const hmi::InspectionEvent& event);

signals:
    /// Emitted when the user activates "切换到工程师模式".
    void switchToEngineerMode();

private:
    void setupUi();

    TaskCardWidget*    m_taskCard     = nullptr;
    NavMapWidget*      m_navMap       = nullptr;
    RobotStatusWidget* m_robotStatus  = nullptr;
    ControlPanel*      m_controlPanel = nullptr;
    ResultPanel*       m_resultPanel  = nullptr;
    QAction*           m_switchModeAction = nullptr;
};
