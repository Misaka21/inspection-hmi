// src/ui/operator/TaskCardWidget.h
//
// TaskCardWidget – displays task summary information in a compact card.
//
// Shows: task name, phase, progress bar, current waypoint, elapsed/remaining time.

#pragma once

#include <QFrame>
#include <QLabel>
#include <QProgressBar>

#include "core/Types.h"

/// \brief A compact card widget that displays task summary.
///
/// The card uses a colored badge to indicate the current task phase (Idle,
/// Executing, Paused, Completed, Failed, etc.) and shows real-time progress.
class TaskCardWidget : public QFrame
{
    Q_OBJECT

public:
    explicit TaskCardWidget(QWidget* parent = nullptr);

    /// Push a full TaskStatus snapshot to update all displayed fields.
    void updateStatus(const hmi::TaskStatus& status);

    /// Reset the card to an empty "no task" state.
    void clear();

private:
    void setupUi();

    static QString phaseToString(hmi::TaskPhase phase);
    static QString phaseToColor(hmi::TaskPhase phase);

    QLabel*       m_taskNameLabel  = nullptr;
    QLabel*       m_phaseLabel     = nullptr;
    QLabel*       m_progressLabel  = nullptr;
    QProgressBar* m_progressBar    = nullptr;
    QLabel*       m_waypointLabel  = nullptr;   // "当前点位: 3/15"
    QLabel*       m_timeLabel      = nullptr;   // elapsed / remaining
};
