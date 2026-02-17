// src/ui/EditPanel.h
//
// EditPanel -- right sidebar with 2 tabs: annotation and task.
//
// Design: all technical parameters (CaptureConfig, PlanOptions) are
// hard-coded with sensible defaults.  The UI exposes only visual
// operations (view point info, delete, plan, execute).

#pragma once

#include "core/Types.h"

#include <QWidget>

class QTabWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QListWidget;
class QScrollArea;
class QHBoxLayout;

/// \brief Right dock panel with annotation and task tabs.
class EditPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditPanel(QWidget* parent = nullptr);

    // Tab 1 -- Annotation
    void showTargetDetails(const hmi::InspectionTarget& target);
    void clearTargetDetails();
    void setPointCount(int count);

    // Tab 2 -- Task
    void showPlanResult(const hmi::PlanResponse& response);
    void updateTaskStatus(const hmi::TaskStatus& status);
    void addEvent(const hmi::InspectionEvent& event);

    /// Hard-coded default capture configuration for API calls.
    static hmi::CaptureConfig defaultCaptureConfig();

    /// Hard-coded default plan options for API calls.
    static hmi::PlanOptions defaultPlanOptions();

signals:
    void targetDeleteRequested(int32_t pointId);
    void planRequested(QString taskName);
    void startRequested(QString planId, bool dryRun);
    void pauseRequested();
    void resumeRequested();
    void stopRequested();

private:
    QWidget* createAnnotationTab();
    QWidget* createTaskTab();

    QTabWidget* m_tabs = nullptr;

    // -----------------------------------------------------------------------
    // Tab 1 -- Annotation (read-only point details)
    // -----------------------------------------------------------------------
    QLabel*      m_pointIdLabel    = nullptr;
    QLabel*      m_positionLabel   = nullptr;
    QLabel*      m_normalLabel     = nullptr;
    QPushButton* m_deleteBtn       = nullptr;
    QLabel*      m_pointCountLabel = nullptr;

    int32_t m_currentPointId = -1;

    // -----------------------------------------------------------------------
    // Tab 2 -- Task (plan + execute + monitor)
    // -----------------------------------------------------------------------

    // -- Plan section
    QLineEdit*   m_taskNameEdit    = nullptr;
    QPushButton* m_planBtn         = nullptr;
    QLabel*      m_planStatsLabel  = nullptr;

    // -- Execute section
    QPushButton* m_startBtn        = nullptr;
    QPushButton* m_pauseBtn        = nullptr;
    QPushButton* m_resumeBtn       = nullptr;
    QPushButton* m_stopBtn         = nullptr;
    QProgressBar* m_progressBar    = nullptr;
    QLabel*      m_actionLabel     = nullptr;
    QLabel*      m_waypointLabel   = nullptr;

    // -- Status section
    QLabel*      m_agvStatusLabel  = nullptr;
    QLabel*      m_armStatusLabel  = nullptr;
    QLabel*      m_interlockLabel  = nullptr;

    // -- Events section
    QListWidget* m_eventList       = nullptr;

    QString m_currentPlanId;
    QString m_currentTaskId;
};
