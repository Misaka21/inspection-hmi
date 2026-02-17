// src/ui/TopBar.cpp

#include "TopBar.h"

#include <QAction>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>

TopBar::TopBar(QWidget* parent)
    : QToolBar(tr("工具栏"), parent)
{
    setObjectName("TopBar");
    setMovable(false);
    setFloatable(false);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIconSize(QSize(20, 20));

    setupActions();
}

void TopBar::setupActions()
{
    // Project group
    m_actNewProject = addAction(tr("新建"));
    m_actNewProject->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    connect(m_actNewProject, &QAction::triggered, this, &TopBar::newProjectRequested);

    m_actImportCad = addAction(tr("导入 CAD"));
    m_actImportCad->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    connect(m_actImportCad, &QAction::triggered, this, &TopBar::importCadRequested);

    addSeparator();

    // Model label
    m_modelLabel = new QLabel(tr("未加载模型"), this);
    m_modelLabel->setStyleSheet("QLabel { color: gray; margin: 0 8px; }");
    addWidget(m_modelLabel);

    addSeparator();

    // Connection group
    QLabel* addrLabel = new QLabel(tr("网关地址:"), this);
    addrLabel->setStyleSheet("QLabel { margin: 0 4px; }");
    addWidget(addrLabel);

    m_addressEdit = new QLineEdit(this);
    m_addressEdit->setPlaceholderText("localhost:50051");
    m_addressEdit->setText("localhost:50051");
    m_addressEdit->setMinimumWidth(180);
    m_addressEdit->setMaximumWidth(220);
    addWidget(m_addressEdit);

    m_actConnect = addAction(tr("连接"));
    m_actConnect->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(m_actConnect, &QAction::triggered, this, [this]() {
        const QString addr = m_addressEdit->text().trimmed();
        if (!addr.isEmpty()) {
            emit connectRequested(addr);
        }
    });

    m_actDisconnect = addAction(tr("断开"));
    m_actDisconnect->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_actDisconnect->setEnabled(false);
    connect(m_actDisconnect, &QAction::triggered, this, &TopBar::disconnectRequested);

    m_connectionStatus = new QLabel(tr("未连接"), this);
    m_connectionStatus->setStyleSheet("QLabel { color: gray; margin: 0 8px; }");
    addWidget(m_connectionStatus);

    addSeparator();

    // Mode switcher
    m_actSwitchMode = addAction(tr("切换至操作员模式"));
    m_actSwitchMode->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(m_actSwitchMode, &QAction::triggered, this, &TopBar::switchModeRequested);

    // Push right
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);
}

void TopBar::setConnectionState(bool connected)
{
    m_actConnect->setEnabled(!connected);
    m_actDisconnect->setEnabled(connected);
    m_addressEdit->setEnabled(!connected);

    if (connected) {
        m_connectionStatus->setText(tr("已连接"));
        m_connectionStatus->setStyleSheet("QLabel { color: green; margin: 0 8px; }");
    } else {
        m_connectionStatus->setText(tr("未连接"));
        m_connectionStatus->setStyleSheet("QLabel { color: gray; margin: 0 8px; }");
    }
}

void TopBar::setModelLoaded(bool loaded, const QString& filename)
{
    if (loaded && !filename.isEmpty()) {
        m_modelLabel->setText(tr("模型: %1").arg(filename));
        m_modelLabel->setStyleSheet("QLabel { color: black; margin: 0 8px; }");
    } else if (loaded) {
        m_modelLabel->setText(tr("模型已加载"));
        m_modelLabel->setStyleSheet("QLabel { color: black; margin: 0 8px; }");
    } else {
        m_modelLabel->setText(tr("未加载模型"));
        m_modelLabel->setStyleSheet("QLabel { color: gray; margin: 0 8px; }");
    }
}
