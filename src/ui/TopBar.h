// src/ui/TopBar.h
//
// TopBar – QToolBar with grouped actions for project/model/connection/mode.

#pragma once

#include <QToolBar>

class QAction;
class QLineEdit;
class QLabel;

/// \brief Application toolbar with file, connection, and mode-switching controls.
class TopBar : public QToolBar
{
    Q_OBJECT

public:
    explicit TopBar(QWidget* parent = nullptr);

    void setConnectionState(bool connected);
    void setModelLoaded(bool loaded, const QString& filename = {});

signals:
    void newProjectRequested();
    void importCadRequested();
    void connectRequested(const QString& address);
    void disconnectRequested();
    void switchModeRequested();

private:
    QAction*   m_actNewProject  = nullptr;
    QAction*   m_actImportCad   = nullptr;
    QAction*   m_actConnect     = nullptr;
    QAction*   m_actDisconnect  = nullptr;
    QAction*   m_actSwitchMode  = nullptr;
    QLineEdit* m_addressEdit    = nullptr;
    QLabel*    m_connectionStatus = nullptr;
    QLabel*    m_modelLabel       = nullptr;

    void setupActions();
};
