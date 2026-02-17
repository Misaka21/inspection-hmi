// src/ui/SceneViewport.h
//
// SceneViewport – central widget wrapping VTK 3D viewport and CadScene.

#pragma once

#include "core/Types.h"

#include <QWidget>

class QVTKWidget;
class CadScene;
class PointAnnotator;
class QToolBar;

/// \brief VTK 3D viewport widget with integrated scene and annotator.
class SceneViewport : public QWidget
{
    Q_OBJECT

public:
    explicit SceneViewport(QWidget* parent = nullptr);
    ~SceneViewport() override;

    CadScene*       cadScene()   const;
    PointAnnotator* annotator()  const;
    QVTKWidget*     vtkWidget()  const;

    bool loadModel(const QString& filePath);

signals:
    void surfaceClicked(hmi::SurfacePoint point);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();
    void setupInteraction();
    void createViewToolbar();

    QVTKWidget*     m_vtkWidget  = nullptr;
    CadScene*       m_cadScene   = nullptr;
    PointAnnotator* m_annotator  = nullptr;
    QToolBar*       m_viewToolbar = nullptr;
};
