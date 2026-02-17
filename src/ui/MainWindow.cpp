// src/ui/MainWindow.cpp

#include "MainWindow.h"
#include "TopBar.h"
#include "ProjectPanel.h"
#include "SceneViewport.h"
#include "EditPanel.h"
#include "StatusLog.h"

#include "core/GatewayClient.h"
#include "core/Types.h"
#include "scene/CadScene.h"
#include "scene/PointAnnotator.h"
#include "scene/QVTKWidget.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("工程师模式 – 巡检 HMI"));
    setMinimumSize(1280, 720);

    setupUi();
    setupDocks();
    connectSignals();
    updateUiForState(m_appState);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Sub-component accessors
// ---------------------------------------------------------------------------

TopBar*        MainWindow::topBar()        const { return m_topBar; }
ProjectPanel*  MainWindow::projectPanel()  const { return m_projectPanel; }
SceneViewport* MainWindow::sceneViewport() const { return m_sceneViewport; }
EditPanel*     MainWindow::editPanel()     const { return m_editPanel; }
StatusLog*     MainWindow::statusLog()     const { return m_statusLog; }

// ---------------------------------------------------------------------------
// Gateway client
// ---------------------------------------------------------------------------

void MainWindow::setGatewayClient(hmi::GatewayClient* client)
{
    if (m_client == client) return;
    m_client = client;
    if (!m_client) return;

    // Connection state -> TopBar
    connect(m_client, &hmi::GatewayClient::connectionStateChanged,
            this, [this](bool connected) {
                m_topBar->setConnectionState(connected);
                if (connected)
                    m_statusLog->logInfo(tr("已连接到网关"));
                else
                    m_statusLog->logWarning(tr("与网关断开连接"));
            });

    // Upload progress
    connect(m_client, &hmi::GatewayClient::uploadCadProgress,
            this, [this](int pct) {
                m_statusLog->logInfo(tr("上传 CAD 模型: %1%").arg(pct));
            });

    connect(m_client, &hmi::GatewayClient::uploadCadFinished,
            this, [this](hmi::Result result, QString modelId) {
                if (result.ok()) {
                    m_statusLog->logInfo(tr("CAD 模型上传完成，模型ID: %1").arg(modelId));
                    setAppState(AppState::ModelLoaded);
                } else {
                    m_statusLog->logError(tr("CAD 模型上传失败: %1").arg(result.message));
                }
            });

    // Plan finished
    connect(m_client, &hmi::GatewayClient::planInspectionFinished,
            this, [this](hmi::PlanResponse response) {
                if (response.result.ok()) {
                    m_editPanel->showPlanResult(response);
                    m_projectPanel->setPath(response.path);
                    m_statusLog->logInfo(
                        tr("规划完成: %1 个点位, 距离 %.2f m")
                            .arg(response.path.totalPoints)
                            .arg(response.path.estimatedDistanceM));
                    setAppState(AppState::Ready);
                } else {
                    m_statusLog->logError(tr("规划失败: %1").arg(response.result.message));
                    setAppState(AppState::Editing);
                }
            });

    // Task status streaming
    connect(m_client, &hmi::GatewayClient::systemStateReceived,
            this, [this](hmi::TaskStatus status) {
                m_editPanel->updateTaskStatus(status);
            });

    // Inspection events
    connect(m_client, &hmi::GatewayClient::inspectionEventReceived,
            this, [this](hmi::InspectionEvent event) {
                m_editPanel->addEvent(event);
                m_statusLog->logInfo(
                    tr("[事件] 点%1: %2").arg(event.pointId).arg(event.message));
            });

    // Generic errors
    connect(m_client, &hmi::GatewayClient::errorOccurred,
            this, [this](QString error) {
                m_statusLog->logError(error);
            });

    // SetInspectionTargets finished
    connect(m_client, &hmi::GatewayClient::setTargetsFinished,
            this, [this](hmi::Result result, uint32_t totalTargets) {
                if (result.ok()) {
                    m_statusLog->logInfo(
                        tr("已提交 %1 个检测目标").arg(totalTargets));
                } else {
                    m_statusLog->logError(
                        tr("提交目标失败: %1").arg(result.message));
                    setAppState(AppState::Editing);
                }
            });

    // Control task (pause/resume/stop) finished
    connect(m_client, &hmi::GatewayClient::controlTaskFinished,
            this, [this](hmi::Result result) {
                if (!result.ok()) {
                    m_statusLog->logError(
                        tr("任务控制失败: %1").arg(result.message));
                }
            });

    // Start inspection finished
    connect(m_client, &hmi::GatewayClient::startInspectionFinished,
            this, [this](hmi::Result result, QString taskId) {
                if (result.ok()) {
                    m_currentTaskId = taskId;
                    m_statusLog->logInfo(tr("任务已启动: %1").arg(taskId));
                    setAppState(AppState::Running);
                    m_client->subscribeSystemState(taskId);
                    m_client->subscribeInspectionEvents(taskId);
                } else {
                    m_statusLog->logError(tr("启动失败: %1").arg(result.message));
                }
            });
}

hmi::GatewayClient* MainWindow::gatewayClient() const { return m_client; }

// ---------------------------------------------------------------------------
// App state
// ---------------------------------------------------------------------------

MainWindow::AppState MainWindow::appState() const { return m_appState; }

void MainWindow::setAppState(AppState state)
{
    if (m_appState == state) return;
    m_appState = state;
    updateUiForState(state);
    emit appStateChanged(state);
}

// ---------------------------------------------------------------------------
// Private – UI construction
// ---------------------------------------------------------------------------

void MainWindow::setupUi()
{
    // Create all sub-components
    m_topBar        = new TopBar(this);
    m_projectPanel  = new ProjectPanel(this);
    m_sceneViewport = new SceneViewport(this);
    m_editPanel     = new EditPanel(this);
    m_statusLog     = new StatusLog(this);

    // Central widget is the 3-D viewport
    setCentralWidget(m_sceneViewport);

    // Toolbar at the top
    addToolBar(Qt::TopToolBarArea, m_topBar);
}

void MainWindow::setupDocks()
{
    setCorner(Qt::BottomLeftCorner,  Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // Left dock – project/model panel
    m_projectDock = new QDockWidget(tr("项目"), this);
    m_projectDock->setObjectName("ProjectDock");
    m_projectDock->setWidget(m_projectPanel);
    m_projectDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    // Right dock – edit / planning / monitor
    m_editDock = new QDockWidget(tr("编辑"), this);
    m_editDock->setObjectName("EditDock");
    m_editDock->setWidget(m_editPanel);
    m_editDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_editDock);

    // Bottom dock – status log
    m_statusDock = new QDockWidget(tr("日志"), this);
    m_statusDock->setObjectName("StatusDock");
    m_statusDock->setWidget(m_statusLog);
    m_statusDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_statusDock);

    // Reasonable initial sizes
    resizeDocks({m_projectDock}, {260}, Qt::Horizontal);
    resizeDocks({m_editDock},    {320}, Qt::Horizontal);
    resizeDocks({m_statusDock},  {160}, Qt::Vertical);
}

void MainWindow::connectSignals()
{
    // TopBar actions
    connect(m_topBar, &TopBar::newProjectRequested, this, [this]() {
        m_projectPanel->clearModel();
        m_projectPanel->clearTargets();
        m_projectPanel->clearPath();
        m_editPanel->clearTargetDetails();
        m_editPanel->setPointCount(0);
        m_sceneViewport->cadScene()->clearModel();
        m_sceneViewport->annotator()->clearTargets();
        setAppState(AppState::Idle);
        m_statusLog->logInfo(tr("新建项目"));
    });

    connect(m_topBar, &TopBar::importCadRequested, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("导入 CAD 模型"),
            QString(),
            tr("三维模型 (*.stl *.obj *.ply);;所有文件 (*)"));
        if (path.isEmpty()) return;
        m_statusLog->logInfo(tr("正在加载模型: %1").arg(path));
        const bool ok = m_sceneViewport->loadModel(path);
        if (ok) {
            const QFileInfo fi(path);
            m_topBar->setModelLoaded(true, fi.fileName());
            m_projectPanel->setModelInfo(fi.fileName(), path);
            setAppState(AppState::ModelLoaded);
            m_statusLog->logInfo(tr("模型加载成功: %1").arg(fi.fileName()));
            if (m_client) {
                m_client->uploadCad(path);
            }
        } else {
            m_statusLog->logError(tr("模型加载失败: %1").arg(path));
        }
    });

    connect(m_topBar, &TopBar::connectRequested, this, [this](const QString& addr) {
        if (m_client) {
            m_statusLog->logInfo(tr("连接到: %1").arg(addr));
            m_client->connectToGateway(addr);
        }
    });

    connect(m_topBar, &TopBar::disconnectRequested, this, [this]() {
        if (m_client) {
            m_client->disconnectFromGateway();
            m_statusLog->logInfo(tr("已断开连接"));
        }
    });

    connect(m_topBar, &TopBar::switchModeRequested,
            this, &MainWindow::switchToOperatorMode);

    // SceneViewport surface click -> create target in annotator + ProjectPanel
    // SceneViewport.eventFilter only does pickSurface() and emits surfaceClicked.
    // MainWindow is the single place that creates InspectionTarget objects.
    connect(m_sceneViewport, &SceneViewport::surfaceClicked,
            this, [this](hmi::SurfacePoint pt) {
                static int32_t s_nextId = 1;
                hmi::InspectionTarget target;
                target.pointId            = s_nextId++;
                target.surface            = pt;
                target.view.viewDirection = -pt.normal; // camera looks at surface

                auto* annotator = m_sceneViewport->annotator();
                annotator->addTarget(target);
                m_sceneViewport->vtkWidget()->scheduleRender();

                m_projectPanel->addTarget(target);
                m_editPanel->showTargetDetails(target);
                m_editPanel->setPointCount(annotator->targets().size());
                setAppState(AppState::Editing);
                m_statusLog->logInfo(
                    tr("添加点位 %1 (%.3f, %.3f, %.3f)")
                        .arg(target.pointId)
                        .arg(static_cast<double>(pt.position.x()))
                        .arg(static_cast<double>(pt.position.y()))
                        .arg(static_cast<double>(pt.position.z())));
            });

    // ProjectPanel selection -> EditPanel detail
    connect(m_projectPanel, &ProjectPanel::targetSelected,
            this, [this](int32_t /*pointId*/) {
                // In a full implementation we'd look up the target by ID
                // and call editPanel->showTargetDetails(target)
            });

    // Helper lambda for deleting a target from all components
    auto deleteTarget = [this](int32_t pointId) {
        m_sceneViewport->annotator()->removeTarget(pointId);
        m_sceneViewport->vtkWidget()->scheduleRender();
        m_projectPanel->removeTarget(pointId);
        m_editPanel->clearTargetDetails();
        m_editPanel->setPointCount(m_sceneViewport->annotator()->targets().size());
        m_statusLog->logInfo(tr("删除点位 %1").arg(pointId));
    };

    connect(m_projectPanel, &ProjectPanel::targetDeleteRequested,
            this, deleteTarget);

    // EditPanel target delete
    connect(m_editPanel, &EditPanel::targetDeleteRequested,
            this, deleteTarget);

    // Plan request: collect targets -> SetInspectionTargets -> PlanInspection
    connect(m_editPanel, &EditPanel::planRequested,
            this, [this](QString taskName) {
                if (!m_client) {
                    m_statusLog->logError(tr("未连接到网关，无法规划"));
                    return;
                }
                const auto targets = m_sceneViewport->annotator()->targets();
                if (targets.isEmpty()) {
                    m_statusLog->logError(tr("没有标注点位，无法规划"));
                    return;
                }
                setAppState(AppState::Planning);
                m_statusLog->logInfo(
                    tr("提交 %1 个点位并开始规划: %2")
                        .arg(targets.size())
                        .arg(taskName));

                const auto captureConfig = EditPanel::defaultCaptureConfig();
                const auto planOptions   = EditPanel::defaultPlanOptions();

                // Step 1: submit targets + capture config
                m_client->setInspectionTargets(
                    QString(), targets, captureConfig, QString());

                // Step 2: request plan
                m_client->planInspection(QString(), taskName, planOptions);
            });

    // Start request
    connect(m_editPanel, &EditPanel::startRequested,
            this, [this](QString planId, bool dryRun) {
                if (!m_client) {
                    m_statusLog->logError(tr("未连接到网关，无法启动"));
                    return;
                }
                m_statusLog->logInfo(
                    tr("启动任务: planId=%1, dryRun=%2")
                        .arg(planId)
                        .arg(dryRun ? tr("是") : tr("否")));
                m_client->startInspection(planId, dryRun);
            });

    // Pause request
    connect(m_editPanel, &EditPanel::pauseRequested,
            this, [this]() {
                if (!m_client) return;
                m_statusLog->logInfo(tr("暂停任务"));
                m_client->pauseInspection(m_currentTaskId);
                setAppState(AppState::Paused);
            });

    // Resume request
    connect(m_editPanel, &EditPanel::resumeRequested,
            this, [this]() {
                if (!m_client) return;
                m_statusLog->logInfo(tr("继续任务"));
                m_client->resumeInspection(m_currentTaskId);
                setAppState(AppState::Running);
            });

    // Stop request
    connect(m_editPanel, &EditPanel::stopRequested,
            this, [this]() {
                if (!m_client) return;
                m_statusLog->logInfo(tr("停止任务"));
                m_client->stopInspection(m_currentTaskId);
                setAppState(AppState::Ready);
            });

    // CadScene error propagation
    connect(m_sceneViewport->cadScene(), &CadScene::errorOccurred,
            this, [this](const QString& err) {
                m_statusLog->logError(err);
            });
}

void MainWindow::updateUiForState(AppState state)
{
    // Update status bar text
    switch (state) {
    case AppState::Idle:
        statusBar()->showMessage(tr("空闲 – 请导入 CAD 模型"));
        break;
    case AppState::ModelLoaded:
        statusBar()->showMessage(tr("模型已加载 – 请标注巡检点"));
        break;
    case AppState::Editing:
        statusBar()->showMessage(tr("标注模式 – 点击模型表面添加巡检点"));
        break;
    case AppState::Planning:
        statusBar()->showMessage(tr("规划中..."));
        break;
    case AppState::Ready:
        statusBar()->showMessage(tr("规划完成 – 可以启动任务"));
        break;
    case AppState::Running:
        statusBar()->showMessage(tr("任务执行中"));
        break;
    case AppState::Paused:
        statusBar()->showMessage(tr("任务已暂停"));
        break;
    case AppState::Finished:
        statusBar()->showMessage(tr("任务已完成"));
        break;
    }
}
