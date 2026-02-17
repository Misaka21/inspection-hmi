// src/scene/QVTKWidget.cpp

#include "QVTKWidget.h"

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkGenericRenderWindowInteractor.h>
#include <vtkRendererCollection.h>
#include <vtkRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QSurfaceFormat>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

QVTKWidget::QVTKWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_renderWindow(nullptr)
    , m_interactor(nullptr)
    , m_initialized(false)
    , m_renderPending(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

QVTKWidget::~QVTKWidget()
{
    // Destroy all QTimers first
    for (auto it = m_timers.begin(); it != m_timers.end(); ++it) {
        it.value()->stop();
        delete it.value();
    }
    m_timers.clear();

    if (m_interactor) {
        m_interactor->TerminateApp();
    }

    makeCurrent();

    if (m_renderWindow) {
        m_renderWindow->Finalize();
    }

    doneCurrent();
}

// ---------------------------------------------------------------------------
// VTK render window management
// ---------------------------------------------------------------------------

void QVTKWidget::setRenderWindow(vtkGenericOpenGLRenderWindow* win)
{
    if (m_renderWindow == win) {
        return;
    }

    m_renderWindow = win;

    if (m_renderWindow) {
        // Create a VTK interactor
        auto* iren = vtkGenericRenderWindowInteractor::New();
        m_interactor.TakeReference(iren);
        m_interactor->SetRenderWindow(m_renderWindow);

        // Use trackball camera style (left-drag=rotate, middle-drag=pan, wheel=zoom)
        auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        m_interactor->SetInteractorStyle(style);

        // Prevent the interactor from calling Render() on the window directly.
        // All rendering goes through Qt's paintGL().
        iren->EnableRenderOff();

        // Qt handles the buffer swap, not VTK
        m_renderWindow->SetSwapBuffers(false);

        // VTK should not render on its own; rendering only happens in paintGL()
        m_renderWindow->SetReadyForRendering(false);

        // ------------------------------------------------------------------
        // Timer bridge: vtkGenericRenderWindowInteractor fires
        // CreateTimerEvent / DestroyTimerEvent instead of calling
        // InternalCreateTimer / InternalDestroyTimer. We observe these
        // events and route them to QTimer.
        // ------------------------------------------------------------------
        auto createTimerCb = vtkSmartPointer<vtkCallbackCommand>::New();
        createTimerCb->SetClientData(this);
        createTimerCb->SetCallback([](vtkObject* caller, unsigned long, void* clientData, void* callData) {
            auto* self = static_cast<QVTKWidget*>(clientData);
            auto* iren = static_cast<vtkRenderWindowInteractor*>(caller);
            int timerId = *static_cast<int*>(callData);

            // vtkGenericRenderWindowInteractor uses TimerDuration for interval
            int interval = iren->GetTimerDuration(timerId);
            bool repeating = iren->IsOneShotTimer(timerId) == 0;

            int qtTimerId = self->createVtkTimer(interval > 0 ? interval : 10, repeating);
            // Map VTK's platform timer ID to our QTimer ID (they happen to be the same
            // if CreateTimerEvent callData is the platform timer ID)
            Q_UNUSED(qtTimerId);
        });
        m_interactor->AddObserver(vtkCommand::CreateTimerEvent, createTimerCb);

        auto destroyTimerCb = vtkSmartPointer<vtkCallbackCommand>::New();
        destroyTimerCb->SetClientData(this);
        destroyTimerCb->SetCallback([](vtkObject*, unsigned long, void* clientData, void* callData) {
            auto* self = static_cast<QVTKWidget*>(clientData);
            int timerId = *static_cast<int*>(callData);
            self->destroyVtkTimer(timerId);
        });
        m_interactor->AddObserver(vtkCommand::DestroyTimerEvent, destroyTimerCb);

        // Multi-sampling
        if (format().samples() > 0) {
            m_renderWindow->SetMultiSamples(format().samples());
        }
    }
}

vtkGenericOpenGLRenderWindow* QVTKWidget::renderWindow() const
{
    return m_renderWindow;
}

vtkRenderWindowInteractor* QVTKWidget::interactor() const
{
    return m_interactor;
}

QSurfaceFormat QVTKWidget::defaultFormat()
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 2);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    return fmt;
}

// ---------------------------------------------------------------------------
// Timer support
// ---------------------------------------------------------------------------

int QVTKWidget::createVtkTimer(int interval, bool repeating)
{
    int id = m_nextTimerId++;
    auto* timer = new QTimer(this);
    timer->setInterval(interval);
    timer->setSingleShot(!repeating);
    connect(timer, &QTimer::timeout, this, [this, id]() {
        if (m_interactor) {
            int timerId = id;  // mutable copy for InvokeEvent(void*)
            m_interactor->InvokeEvent(vtkCommand::TimerEvent, &timerId);
            scheduleRender();
        }
    });
    m_timers.insert(id, timer);
    timer->start();
    return id;
}

void QVTKWidget::destroyVtkTimer(int id)
{
    auto it = m_timers.find(id);
    if (it != m_timers.end()) {
        it.value()->stop();
        delete it.value();
        m_timers.erase(it);
    }
}

void QVTKWidget::scheduleRender()
{
    if (!m_renderPending) {
        m_renderPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_renderPending = false;
            if (m_initialized) {
                update();  // triggers paintGL()
            }
        });
    }
}

// ---------------------------------------------------------------------------
// OpenGL lifecycle
// ---------------------------------------------------------------------------

void QVTKWidget::initializeGL()
{
    if (!m_renderWindow) {
        return;
    }

    m_renderWindow->OpenGLInit();
    m_renderWindow->SetIsCurrent(true);

    const qreal dpr = devicePixelRatio();
    m_renderWindow->SetDPI(96.0 * dpr);

    if (m_interactor) {
        m_interactor->Initialize();
        m_interactor->Enable();
    }

    // GL context is now valid — allow Render() calls from any code path
    // (CadScene, PointAnnotator, etc.) to actually produce output.
    m_renderWindow->SetReadyForRendering(true);
    m_initialized = true;
}

void QVTKWidget::paintGL()
{
    if (!m_renderWindow || !m_initialized) {
        return;
    }

    m_renderWindow->SetIsCurrent(true);
    m_renderWindow->Render();
    m_renderWindow->SetIsCurrent(false);
}

void QVTKWidget::resizeGL(int w, int h)
{
    if (!m_renderWindow) {
        return;
    }

    const qreal dpr = devicePixelRatio();
    const int pw = static_cast<int>(w * dpr);
    const int ph = static_cast<int>(h * dpr);

    m_renderWindow->SetSize(pw, ph);

    if (m_interactor) {
        m_interactor->SetSize(pw, ph);
        m_interactor->ConfigureEvent();
    }
}

// ---------------------------------------------------------------------------
// Mouse event translation
// ---------------------------------------------------------------------------

void QVTKWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_interactor) {
        QOpenGLWidget::mousePressEvent(event);
        return;
    }

    const int x = static_cast<int>(event->position().x());
    const int y = height() - static_cast<int>(event->position().y()) - 1;

    m_interactor->SetEventInformation(
        x, y,
        (event->modifiers() & Qt::ControlModifier) != 0,
        (event->modifiers() & Qt::ShiftModifier) != 0
    );

    switch (event->button()) {
        case Qt::LeftButton:
            m_interactor->LeftButtonPressEvent();
            break;
        case Qt::MiddleButton:
            m_interactor->MiddleButtonPressEvent();
            break;
        case Qt::RightButton:
            m_interactor->RightButtonPressEvent();
            break;
        default:
            QOpenGLWidget::mousePressEvent(event);
            return;
    }

    event->accept();
    scheduleRender();
}

void QVTKWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_interactor) {
        QOpenGLWidget::mouseReleaseEvent(event);
        return;
    }

    const int x = static_cast<int>(event->position().x());
    const int y = height() - static_cast<int>(event->position().y()) - 1;

    m_interactor->SetEventInformation(
        x, y,
        (event->modifiers() & Qt::ControlModifier) != 0,
        (event->modifiers() & Qt::ShiftModifier) != 0
    );

    switch (event->button()) {
        case Qt::LeftButton:
            m_interactor->LeftButtonReleaseEvent();
            break;
        case Qt::MiddleButton:
            m_interactor->MiddleButtonReleaseEvent();
            break;
        case Qt::RightButton:
            m_interactor->RightButtonReleaseEvent();
            break;
        default:
            QOpenGLWidget::mouseReleaseEvent(event);
            return;
    }

    event->accept();
    scheduleRender();
}

void QVTKWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_interactor) {
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    const int x = static_cast<int>(event->position().x());
    const int y = height() - static_cast<int>(event->position().y()) - 1;

    m_interactor->SetEventInformation(
        x, y,
        (event->modifiers() & Qt::ControlModifier) != 0,
        (event->modifiers() & Qt::ShiftModifier) != 0
    );

    m_interactor->MouseMoveEvent();

    event->accept();
    scheduleRender();
}

void QVTKWidget::wheelEvent(QWheelEvent* event)
{
    if (!m_interactor) {
        QOpenGLWidget::wheelEvent(event);
        return;
    }

    const int x = static_cast<int>(event->position().x());
    const int y = height() - static_cast<int>(event->position().y()) - 1;

    m_interactor->SetEventInformation(
        x, y,
        (event->modifiers() & Qt::ControlModifier) != 0,
        (event->modifiers() & Qt::ShiftModifier) != 0
    );

    const int delta = event->angleDelta().y();
    if (delta > 0) {
        m_interactor->MouseWheelForwardEvent();
    } else if (delta < 0) {
        m_interactor->MouseWheelBackwardEvent();
    }

    event->accept();
    scheduleRender();
}

// ---------------------------------------------------------------------------
// Keyboard event translation
// ---------------------------------------------------------------------------

void QVTKWidget::keyPressEvent(QKeyEvent* event)
{
    if (!m_interactor) {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }

    m_interactor->SetKeyEventInformation(
        (event->modifiers() & Qt::ControlModifier) != 0,
        (event->modifiers() & Qt::ShiftModifier) != 0,
        event->key(),
        event->count(),
        event->text().toLatin1().constData()
    );

    m_interactor->KeyPressEvent();
    m_interactor->CharEvent();

    event->accept();
    scheduleRender();
}

void QVTKWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (!m_interactor) {
        QOpenGLWidget::keyReleaseEvent(event);
        return;
    }

    m_interactor->SetKeyEventInformation(
        (event->modifiers() & Qt::ControlModifier) != 0,
        (event->modifiers() & Qt::ShiftModifier) != 0,
        event->key(),
        event->count(),
        event->text().toLatin1().constData()
    );

    m_interactor->KeyReleaseEvent();

    event->accept();
    scheduleRender();
}

// ---------------------------------------------------------------------------
// Enter/leave events
// ---------------------------------------------------------------------------

void QVTKWidget::enterEvent(QEnterEvent* event)
{
    if (m_interactor) {
        m_interactor->EnterEvent();
    }
    QOpenGLWidget::enterEvent(event);
}

void QVTKWidget::leaveEvent(QEvent* event)
{
    if (m_interactor) {
        m_interactor->LeaveEvent();
    }
    QOpenGLWidget::leaveEvent(event);
}
