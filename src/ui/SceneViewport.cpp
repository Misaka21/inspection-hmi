// src/ui/SceneViewport.cpp

#include "SceneViewport.h"
#include "scene/CadScene.h"
#include "scene/PointAnnotator.h"
#include "scene/QVTKWidget.h"

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>

#include <QEvent>
#include <QMouseEvent>
#include <QToolBar>
#include <QVBoxLayout>

SceneViewport::SceneViewport(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupInteraction();
}

SceneViewport::~SceneViewport() = default;

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

CadScene*       SceneViewport::cadScene()  const { return m_cadScene; }
PointAnnotator* SceneViewport::annotator() const { return m_annotator; }
QVTKWidget*     SceneViewport::vtkWidget() const { return m_vtkWidget; }

// ---------------------------------------------------------------------------
// Model loading
// ---------------------------------------------------------------------------

bool SceneViewport::loadModel(const QString& filePath)
{
    if (!m_cadScene) return false;
    return m_cadScene->loadModel(filePath);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void SceneViewport::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create VTK widget
    m_vtkWidget = new QVTKWidget(this);

    // Create render window
    auto* renderWindow = vtkGenericOpenGLRenderWindow::New();
    m_vtkWidget->setRenderWindow(renderWindow);
    renderWindow->Delete();

    // Create renderer
    auto* renderer = vtkRenderer::New();
    renderer->SetBackground(0.15, 0.15, 0.18);
    renderWindow->AddRenderer(renderer);

    // Create CadScene and attach renderer
    m_cadScene = new CadScene(this);
    m_cadScene->setRenderer(renderer);
    renderer->Delete();

    // Initialize orientation widget with the interactor
    m_cadScene->initOrientationWidget(m_vtkWidget->interactor());

    // Create PointAnnotator
    m_annotator = new PointAnnotator(m_cadScene, this);

    // View control toolbar
    createViewToolbar();

    // Layout
    layout->addWidget(m_viewToolbar);
    layout->addWidget(m_vtkWidget, 1);
}

void SceneViewport::createViewToolbar()
{
    m_viewToolbar = new QToolBar(this);
    m_viewToolbar->setObjectName("ViewToolbar");
    m_viewToolbar->setMovable(false);
    m_viewToolbar->setFloatable(false);
    m_viewToolbar->setIconSize(QSize(16, 16));
    m_viewToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto* actFront = m_viewToolbar->addAction(tr("前视图"));
    connect(actFront, &QAction::triggered, this, [this]() {
        if (m_cadScene) { m_cadScene->setViewFront(); m_vtkWidget->scheduleRender(); }
    });

    auto* actTop = m_viewToolbar->addAction(tr("俯视图"));
    connect(actTop, &QAction::triggered, this, [this]() {
        if (m_cadScene) { m_cadScene->setViewTop(); m_vtkWidget->scheduleRender(); }
    });

    auto* actRight = m_viewToolbar->addAction(tr("右视图"));
    connect(actRight, &QAction::triggered, this, [this]() {
        if (m_cadScene) { m_cadScene->setViewRight(); m_vtkWidget->scheduleRender(); }
    });

    auto* actIso = m_viewToolbar->addAction(tr("等轴测"));
    connect(actIso, &QAction::triggered, this, [this]() {
        if (m_cadScene) { m_cadScene->setViewIsometric(); m_vtkWidget->scheduleRender(); }
    });

    auto* actReset = m_viewToolbar->addAction(tr("复位"));
    connect(actReset, &QAction::triggered, this, [this]() {
        if (m_cadScene) { m_cadScene->resetCamera(); m_vtkWidget->scheduleRender(); }
    });

    // Spacer
    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_viewToolbar->addWidget(spacer);
}

void SceneViewport::setupInteraction()
{
    // Install event filter on vtkWidget to intercept mouse clicks for picking
    m_vtkWidget->installEventFilter(this);
}

// ---------------------------------------------------------------------------
// Event filter – surface picking
// ---------------------------------------------------------------------------

bool SceneViewport::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_vtkWidget && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && (mouseEvent->modifiers() & Qt::ControlModifier)) {
            // Ctrl + Left Click -> pick surface for annotation
            const int x = static_cast<int>(mouseEvent->position().x());
            const int y = static_cast<int>(mouseEvent->position().y());
            auto result = m_annotator->pickSurface(x, y);
            if (result.has_value()) {
                emit surfaceClicked(result.value());
            }
            return true;  // consume the event so VTK doesn't rotate
        }
    }
    return QWidget::eventFilter(obj, event);
}
