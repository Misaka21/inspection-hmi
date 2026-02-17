// src/ui/StatusLog.h
//
// StatusLog – bottom dock log panel with color-coded output.

#pragma once

#include <QWidget>

class QPlainTextEdit;
class QLabel;
class QPushButton;

/// \brief Bottom status log widget with info/warning/error color coding.
class StatusLog : public QWidget
{
    Q_OBJECT

public:
    explicit StatusLog(QWidget* parent = nullptr);

    void logInfo(const QString& message);
    void logWarning(const QString& message);
    void logError(const QString& message);
    void clear();

    void setStatusText(const QString& text);

private:
    void setupUi();
    void appendLog(const QString& level, const QString& message);

    QPlainTextEdit* m_logText     = nullptr;
    QLabel*         m_statusLabel = nullptr;
    QPushButton*    m_clearBtn    = nullptr;
};
