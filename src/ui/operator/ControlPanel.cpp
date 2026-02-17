// src/ui/operator/ControlPanel.cpp

#include "ControlPanel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    updateButtonStates(hmi::TaskPhase::Idle);
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void ControlPanel::setupUi()
{
    QGroupBox* box = new QGroupBox(QStringLiteral("任务控制"), this);
    QVBoxLayout* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->addWidget(box);

    QVBoxLayout* boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(8, 8, 8, 8);
    boxLayout->setSpacing(6);

    // Dry-run checkbox
    m_dryRunCheck = new QCheckBox(QStringLiteral("试运行模式（不实际执行拍照）"), box);
    m_dryRunCheck->setChecked(false);
    boxLayout->addWidget(m_dryRunCheck);

    // Buttons row
    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);

    m_startBtn = new QPushButton(QStringLiteral("开始"), box);
    m_startBtn->setMinimumHeight(40);
    m_startBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #28a745;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #218838;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #6c757d;"
        "}"
    ));
    btnRow->addWidget(m_startBtn);

    m_pauseBtn = new QPushButton(QStringLiteral("暂停"), box);
    m_pauseBtn->setMinimumHeight(40);
    m_pauseBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #ffc107;"
        "  color: #212529;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #e0a800;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #6c757d;"
        "  color: white;"
        "}"
    ));
    btnRow->addWidget(m_pauseBtn);

    m_resumeBtn = new QPushButton(QStringLiteral("继续"), box);
    m_resumeBtn->setMinimumHeight(40);
    m_resumeBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #17a2b8;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #138496;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #6c757d;"
        "}"
    ));
    btnRow->addWidget(m_resumeBtn);

    m_stopBtn = new QPushButton(QStringLiteral("停止"), box);
    m_stopBtn->setMinimumHeight(40);
    m_stopBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #dc3545;"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #c82333;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #6c757d;"
        "}"
    ));
    btnRow->addWidget(m_stopBtn);

    boxLayout->addLayout(btnRow);

    // Connect signals
    connect(m_startBtn, &QPushButton::clicked, this, [this]() {
        bool dryRun = m_dryRunCheck->isChecked();
        emit startRequested(m_planId, dryRun);
    });

    connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QString reason = QInputDialog::getText(
            this,
            QStringLiteral("暂停任务"),
            QStringLiteral("请输入暂停原因（可选）:"),
            QLineEdit::Normal,
            QString(),
            &ok
        );
        if (ok) {
            emit pauseRequested(m_taskId, reason);
        }
    });

    connect(m_resumeBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        QString reason = QInputDialog::getText(
            this,
            QStringLiteral("继续任务"),
            QStringLiteral("请输入继续原因（可选）:"),
            QLineEdit::Normal,
            QString(),
            &ok
        );
        if (ok) {
            emit resumeRequested(m_taskId, reason);
        }
    });

    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            QStringLiteral("停止任务"),
            QStringLiteral("确定要停止当前任务吗？此操作不可恢复。"),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            bool ok = false;
            QString reason = QInputDialog::getText(
                this,
                QStringLiteral("停止任务"),
                QStringLiteral("请输入停止原因（可选）:"),
                QLineEdit::Normal,
                QString(),
                &ok
            );
            if (ok) {
                emit stopRequested(m_taskId, reason);
            }
        }
    });
}

// ---------------------------------------------------------------------------
// Data updates
// ---------------------------------------------------------------------------

void ControlPanel::setTaskPhase(hmi::TaskPhase phase)
{
    updateButtonStates(phase);
}

void ControlPanel::setTaskId(const QString& taskId)
{
    m_taskId = taskId;
}

void ControlPanel::setPlanId(const QString& planId)
{
    m_planId = planId;
}

void ControlPanel::updateButtonStates(hmi::TaskPhase phase)
{
    // Button enable states based on phase:
    // - Idle/Completed/Failed/Stopped: Start enabled, others disabled
    // - Executing: Pause + Stop enabled, Start + Resume disabled
    // - Paused: Resume + Stop enabled, Start + Pause disabled
    // - Planning/Localizing: all disabled (intermediate states)

    switch (phase) {
        case hmi::TaskPhase::Idle:
        case hmi::TaskPhase::Completed:
        case hmi::TaskPhase::Failed:
        case hmi::TaskPhase::Stopped:
            m_startBtn->setEnabled(true);
            m_pauseBtn->setEnabled(false);
            m_resumeBtn->setEnabled(false);
            m_stopBtn->setEnabled(false);
            break;

        case hmi::TaskPhase::Executing:
            m_startBtn->setEnabled(false);
            m_pauseBtn->setEnabled(true);
            m_resumeBtn->setEnabled(false);
            m_stopBtn->setEnabled(true);
            break;

        case hmi::TaskPhase::Paused:
            m_startBtn->setEnabled(false);
            m_pauseBtn->setEnabled(false);
            m_resumeBtn->setEnabled(true);
            m_stopBtn->setEnabled(true);
            break;

        case hmi::TaskPhase::Localizing:
        case hmi::TaskPhase::Planning:
            // Intermediate states: disable all controls
            m_startBtn->setEnabled(false);
            m_pauseBtn->setEnabled(false);
            m_resumeBtn->setEnabled(false);
            m_stopBtn->setEnabled(true);  // Allow stop during planning
            break;

        default:
            m_startBtn->setEnabled(false);
            m_pauseBtn->setEnabled(false);
            m_resumeBtn->setEnabled(false);
            m_stopBtn->setEnabled(false);
            break;
    }
}
