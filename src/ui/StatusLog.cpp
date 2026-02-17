// src/ui/StatusLog.cpp

#include "StatusLog.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

StatusLog::StatusLog(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void StatusLog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top bar with status label and clear button
    auto* topBar = new QWidget(this);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);

    m_statusLabel = new QLabel(tr("就绪"), topBar);
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; }");
    topLayout->addWidget(m_statusLabel, 1);

    m_clearBtn = new QPushButton(tr("清空"), topBar);
    m_clearBtn->setMaximumWidth(80);
    connect(m_clearBtn, &QPushButton::clicked, this, &StatusLog::clear);
    topLayout->addWidget(m_clearBtn);

    mainLayout->addWidget(topBar);

    // Log text area
    m_logText = new QPlainTextEdit(this);
    m_logText->setReadOnly(true);
    m_logText->setMaximumBlockCount(500);  // limit history
    mainLayout->addWidget(m_logText);
}

void StatusLog::logInfo(const QString& message)
{
    appendLog(tr("信息"), message);
}

void StatusLog::logWarning(const QString& message)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString html = QString(
        "<span style='color: orange;'>[%1] <b>警告</b>: %2</span>")
                             .arg(ts)
                             .arg(message.toHtmlEscaped());
    m_logText->appendHtml(html);
}

void StatusLog::logError(const QString& message)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString html = QString(
        "<span style='color: red;'>[%1] <b>错误</b>: %2</span>")
                             .arg(ts)
                             .arg(message.toHtmlEscaped());
    m_logText->appendHtml(html);
}

void StatusLog::clear()
{
    m_logText->clear();
}

void StatusLog::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}

void StatusLog::appendLog(const QString& level, const QString& message)
{
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString line = QString("[%1] %2: %3").arg(ts).arg(level).arg(message);
    m_logText->appendPlainText(line);
}
