// src/ui/operator/ControlPanel.h
//
// ControlPanel – task execution control buttons (start / pause / resume / stop).

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QString>

#include "core/Types.h"

/// \brief Task execution control panel with start/pause/resume/stop buttons.
///
/// Button states are automatically updated based on the current TaskPhase:
///  - Idle/Completed/Failed/Stopped: Start enabled, others disabled
///  - Executing: Pause + Stop enabled
///  - Paused: Resume + Stop enabled
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget* parent = nullptr);

    void setTaskPhase(hmi::TaskPhase phase);
    void setTaskId(const QString& taskId);
    void setPlanId(const QString& planId);

signals:
    void startRequested(const QString& planId, bool dryRun);
    void pauseRequested(const QString& taskId, const QString& reason);
    void resumeRequested(const QString& taskId, const QString& reason);
    void stopRequested(const QString& taskId, const QString& reason);

private:
    void setupUi();
    void updateButtonStates(hmi::TaskPhase phase);

    QPushButton* m_startBtn  = nullptr;
    QPushButton* m_pauseBtn  = nullptr;
    QPushButton* m_resumeBtn = nullptr;
    QPushButton* m_stopBtn   = nullptr;
    QCheckBox*   m_dryRunCheck = nullptr;

    QString m_taskId;
    QString m_planId;
};
