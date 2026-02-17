// src/ui/MainWindow.h
//
// MainWindow – top-level QMainWindow for Engineer mode.
//
// Layout
// ------
//   TopBar      : addToolBar(Qt::TopToolBarArea)
//   ProjectPanel: QDockWidget, left
//   SceneViewport: setCentralWidget
//   EditPanel   : QDockWidget, right
//   StatusLog   : QDockWidget, bottom

#pragma once

#include <QMainWindow>

// Forward declarations – keep compile times short.
class TopBar;
class ProjectPanel;
class SceneViewport;
class EditPanel;
class StatusLog;

namespace hmi {
class GatewayClient;
} // namespace hmi

/// \brief Top-level application window for Engineer mode.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Sub-component accessors
    TopBar*        topBar()        const;
    ProjectPanel*  projectPanel()  const;
    SceneViewport* sceneViewport() const;
    EditPanel*     editPanel()     const;
    StatusLog*     statusLog()     const;

    // Gateway client wiring
    void setGatewayClient(hmi::GatewayClient* client);
    hmi::GatewayClient* gatewayClient() const;

    /// Application-level state that drives toolbar / button enable states.
    enum class AppState {
        Idle,
        ModelLoaded,
        Editing,
        Planning,
        Ready,
        Running,
        Paused,
        Finished
    };
    Q_ENUM(AppState)

    AppState appState() const;
    void     setAppState(AppState state);

signals:
    void appStateChanged(AppState state);
    void switchToOperatorMode();

private:
    void setupUi();
    void setupDocks();
    void connectSignals();
    void updateUiForState(AppState state);

    TopBar*        m_topBar        = nullptr;
    ProjectPanel*  m_projectPanel  = nullptr;
    SceneViewport* m_sceneViewport = nullptr;
    EditPanel*     m_editPanel     = nullptr;
    StatusLog*     m_statusLog     = nullptr;

    hmi::GatewayClient* m_client   = nullptr;
    AppState            m_appState = AppState::Idle;
    QString             m_currentTaskId;

    // Dock wrappers
    QDockWidget* m_projectDock  = nullptr;
    QDockWidget* m_editDock     = nullptr;
    QDockWidget* m_statusDock   = nullptr;
};
