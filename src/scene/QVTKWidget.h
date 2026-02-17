// src/scene/QVTKWidget.h
//
// QVTKWidget – Qt6-compatible VTK OpenGL widget.
//
// Replaces the system QVTKOpenGLNativeWidget (built against Qt5).
// Integrates vtkGenericOpenGLRenderWindow with Qt6's QOpenGLWidget and
// provides VTK timer support via QTimer.

#pragma once

#include <QMap>
#include <QOpenGLWidget>
#include <QTimer>
#include <vtkSmartPointer.h>

class vtkGenericOpenGLRenderWindow;
class vtkRenderWindowInteractor;

class QVTKWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit QVTKWidget(QWidget* parent = nullptr);
    ~QVTKWidget() override;

    void setRenderWindow(vtkGenericOpenGLRenderWindow* win);
    vtkGenericOpenGLRenderWindow* renderWindow() const;
    vtkRenderWindowInteractor* interactor() const;

    static QSurfaceFormat defaultFormat();

    // Called by the custom interactor to create/destroy timers via QTimer
    int createVtkTimer(int interval, bool repeating);
    void destroyVtkTimer(int id);

    // Schedule a render (non-reentrant — avoids infinite loops)
    void scheduleRender();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    QMap<int, QTimer*> m_timers;
    int m_nextTimerId = 1;
    bool m_initialized = false;
    bool m_renderPending = false;
};
