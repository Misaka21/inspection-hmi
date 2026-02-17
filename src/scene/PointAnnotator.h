// src/scene/PointAnnotator.h
//
// PointAnnotator – manages inspection-point actors on the CAD model surface.
//
// Each InspectionTarget is visualised as:
//   - a sphere marker at the surface position
//   - an arrow along the surface normal
//   - a wireframe camera frustum
//   - a billboard text label showing the point ID
//
// Thread safety: all methods must be called from the Qt GUI thread.

#pragma once

#include <QMap>
#include <QObject>
#include <QVector>
#include <optional>
#include <cstdint>

// VTK forward declarations only – full headers in the .cpp
#include <vtkSmartPointer.h>

class vtkActor;
class vtkCellPicker;

// Pull in the HMI domain types (hmi_core exposes src/core/ as a PUBLIC
// include directory, so the header is reachable as "Types.h").
#include "Types.h"

// Forward declaration – avoids pulling all of CadScene into every consumer.
class CadScene;

/// \brief Handles interactive point annotation on a CadScene CAD model.
///
/// PointAnnotator sits above CadScene in the ownership hierarchy.  It holds
/// a non-owning pointer to the scene, creates VTK actors for every
/// InspectionTarget, and handles surface picking via vtkCellPicker.
class PointAnnotator : public QObject
{
    Q_OBJECT

public:
    explicit PointAnnotator(CadScene* scene, QObject* parent = nullptr);
    ~PointAnnotator() override;

    // -----------------------------------------------------------------------
    // Annotation mode
    // -----------------------------------------------------------------------

    /// Enable / disable annotation mode.
    /// When enabled, pickSurface() creates new targets on each call.
    void setEnabled(bool enabled);

    /// Return the current enabled state.
    bool isEnabled() const;

    // -----------------------------------------------------------------------
    // Capture configuration
    // -----------------------------------------------------------------------

    /// Set capture config used to size the frustum visualisation.
    void setCaptureConfig(const hmi::CaptureConfig& config);

    // -----------------------------------------------------------------------
    // Target management
    // -----------------------------------------------------------------------

    /// Add a new target and create its actors.
    void addTarget(const hmi::InspectionTarget& target);

    /// Remove the target with the given \a pointId and destroy its actors.
    void removeTarget(int32_t pointId);

    /// Replace an existing target's data and rebuild its actors.
    void updateTarget(const hmi::InspectionTarget& target);

    /// Remove all targets.
    void clearTargets();

    /// Return a copy of all current targets.
    QVector<hmi::InspectionTarget> targets() const;

    // -----------------------------------------------------------------------
    // Selection
    // -----------------------------------------------------------------------

    /// Highlight the target with \a pointId as selected.
    void selectTarget(int32_t pointId);

    /// Deselect the currently selected target.
    void clearSelection();

    /// Return the ID of the currently selected target, or -1 if none.
    int32_t selectedTargetId() const;

    // -----------------------------------------------------------------------
    // Planned path visualisation
    // -----------------------------------------------------------------------

    /// Display a green polyline connecting all waypoint positions.
    void showPath(const hmi::InspectionPath& path);

    /// Remove the path polyline actors.
    void clearPath();

    /// Highlight the waypoint at \a index in the path (e.g. during execution).
    void highlightWaypoint(int index);

    // -----------------------------------------------------------------------
    // Surface picking
    // -----------------------------------------------------------------------

    /// Cast a ray from screen coordinates into the CAD model.
    /// Returns the hit SurfacePoint, or std::nullopt if no model was hit.
    /// The face index stored in SurfacePoint::faceIndex is the VTK cell id.
    std::optional<hmi::SurfacePoint> pickSurface(int screenX, int screenY);

signals:
    /// Emitted after a target has been added.
    void targetAdded(int32_t pointId);

    /// Emitted after a target has been removed.
    void targetRemoved(int32_t pointId);

    /// Emitted when the selection changes.
    void targetSelected(int32_t pointId);

    /// Emitted when a surface point was successfully picked.
    void surfacePicked(hmi::SurfacePoint point);

private:
    // -----------------------------------------------------------------------
    // Internal visual bundle
    // -----------------------------------------------------------------------

    struct AnnotationVisual {
        hmi::InspectionTarget                   target;
        vtkSmartPointer<vtkActor>               sphereActor;   ///< point marker
        vtkSmartPointer<vtkActor>               arrowActor;    ///< normal direction
        vtkSmartPointer<vtkActor>               frustumActor;  ///< camera frustum
        vtkSmartPointer<vtkActor>               labelActor;    ///< billboard text
    };

    // -----------------------------------------------------------------------
    // Data members
    // -----------------------------------------------------------------------

    CadScene*                             m_scene    = nullptr;
    bool                                  m_enabled  = false;
    hmi::CaptureConfig                    m_captureConfig;
    int32_t                               m_selectedId   = -1;
    int32_t                               m_nextPointId  =  1;

    QMap<int32_t, AnnotationVisual>       m_visuals;
    QVector<vtkSmartPointer<vtkActor>>    m_pathActors;

    vtkSmartPointer<vtkCellPicker>        m_picker;

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /// Build the full AnnotationVisual bundle for \a target.
    AnnotationVisual createVisual(const hmi::InspectionTarget& target);

    /// Update actor colours / sizes to reflect whether \a visual is selected.
    void updateVisualAppearance(AnnotationVisual& visual, bool selected);

    /// Remove all actors belonging to \a visual from the renderer.
    void removeVisualFromRenderer(AnnotationVisual& visual);

    /// Build the wireframe frustum actor for a surface point + view hint.
    vtkSmartPointer<vtkActor> createFrustumActor(const hmi::SurfacePoint& surface,
                                                  const hmi::ViewHint&    view);

    /// Build a polyline actor connecting all waypoint agv_pose positions.
    /// (Renders in the XY plane; Z set to a small offset above the ground.)
    vtkSmartPointer<vtkActor> createPathLineActor(const hmi::InspectionPath& path);
};
