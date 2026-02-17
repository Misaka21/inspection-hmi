// src/scene/CadScene.h
//
// CadScene – owns the VTK renderer and CAD model scene graph.
//
// Thread safety: all methods must be called from the Qt GUI thread.
// The renderer is created externally (by SceneViewport) and injected via
// setRenderer(); CadScene does not create a vtkRenderWindow itself.

#pragma once

#include <QObject>
#include <QString>

// VTK forward declarations – avoid pulling all VTK headers into every TU
// that includes this header.
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkActor;
class vtkPolyData;
class vtkOrientationMarkerWidget;
class vtkRenderWindowInteractor;

/// \brief Manages the VTK scene for CAD model visualisation.
///
/// CadScene is a thin owner of the VTK scene graph: it holds the model
/// geometry (vtkPolyData), the rendered actor, and helpers such as lighting
/// and orientation axes.  It does not own the render window or interactor –
/// those live in SceneViewport.
class CadScene : public QObject
{
    Q_OBJECT

public:
    explicit CadScene(QObject* parent = nullptr);
    ~CadScene() override;

    // -----------------------------------------------------------------------
    // Renderer binding
    // -----------------------------------------------------------------------

    /// Attach an externally owned renderer.
    /// Must be called before loadModel() or any camera/rendering method.
    void setRenderer(vtkRenderer* renderer);

    /// Return the renderer (may be nullptr if not yet set).
    vtkRenderer* renderer() const;

    // -----------------------------------------------------------------------
    // Model loading
    // -----------------------------------------------------------------------

    /// Load a CAD model from \a filePath.
    /// Supports .stl, .obj, .ply (case-insensitive).
    /// Returns \c true on success; emits modelLoaded() or errorOccurred().
    bool loadModel(const QString& filePath);

    /// Remove the current model from the scene.
    void clearModel();

    /// \c true if a model is currently loaded.
    bool hasModel() const;

    /// File path of the currently loaded model, or empty string.
    QString modelFilePath() const;

    // -----------------------------------------------------------------------
    // Access to loaded model data
    // -----------------------------------------------------------------------

    /// Raw poly-data (may be nullptr).  Used by PointAnnotator for picking.
    vtkPolyData* modelPolyData() const;

    /// The VTK actor representing the model in the renderer (may be nullptr).
    vtkActor* modelActor() const;

    // -----------------------------------------------------------------------
    // Camera control
    // -----------------------------------------------------------------------

    /// Fit the camera to the loaded model.
    void resetCamera();

    /// Standard orthogonal view: +Y looking toward −Y.
    void setViewFront();

    /// Standard orthogonal view: −Z looking toward +Z (top-down).
    void setViewTop();

    /// Standard orthogonal view: +X looking toward −X.
    void setViewRight();

    /// Isometric view (equal perspective on three principal axes).
    void setViewIsometric();

    // -----------------------------------------------------------------------
    // Rendering
    // -----------------------------------------------------------------------

    /// Request a single render frame.  Safe to call even if no render window
    /// is attached yet.
    void render();

    // -----------------------------------------------------------------------
    // Orientation widget
    // -----------------------------------------------------------------------

    /// Attach the orientation marker widget to an interactor so it can receive
    /// events.  Called by SceneViewport after the render window is ready.
    void initOrientationWidget(vtkRenderWindowInteractor* interactor);

signals:
    /// Emitted after a model has been successfully loaded.
    void modelLoaded(const QString& filePath);

    /// Emitted after the model has been removed from the scene.
    void modelCleared();

    /// Emitted when a non-fatal or fatal error occurs (e.g. unsupported file).
    void errorOccurred(const QString& error);

private:
    vtkSmartPointer<vtkRenderer>                m_renderer;
    vtkSmartPointer<vtkActor>                   m_modelActor;
    vtkSmartPointer<vtkPolyData>                m_modelData;
    vtkSmartPointer<vtkOrientationMarkerWidget> m_orientationWidget;
    QString                                     m_modelFilePath;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /// Read geometry from \a filePath; returns nullptr on failure.
    vtkSmartPointer<vtkPolyData> readFile(const QString& filePath);

    /// Ensure per-cell or per-point normals exist on \a pd.
    void ensureNormals(vtkSmartPointer<vtkPolyData>& pd);

    /// Build the default 3-point light rig for the loaded model.
    void setupDefaultLighting();

    /// Create and add the orientation axes actor.
    void setupAxes();

    /// Apply the standard neutral-grey material to the model actor.
    void applyMaterial();

    /// Apply a view preset defined by eye, focal and up vectors.
    /// Calls resetCamera() afterwards so the model fills the viewport.
    void applyViewPreset(double ex, double ey, double ez,
                         double fx, double fy, double fz,
                         double ux, double uy, double uz);
};
