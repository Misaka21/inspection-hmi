// src/scene/PointAnnotator.cpp

#include "PointAnnotator.h"
#include "CadScene.h"

#include <QString>
#include <QVector3D>

#include <cmath>

// VTK – rendering
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkCamera.h>
#include <vtkSmartPointer.h>

// VTK – actors / mappers / properties
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>

// VTK – geometry sources
#include <vtkSphereSource.h>
#include <vtkArrowSource.h>
#include <vtkLineSource.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyLine.h>

// VTK – transforms
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

// VTK – picking
#include <vtkCellPicker.h>

// VTK – text / labels
#include <vtkBillboardTextActor3D.h>
#include <vtkTextProperty.h>
#include <vtkVectorText.h>
#include <vtkFollower.h>

// VTK – append / combine geometry
#include <vtkAppendPolyData.h>
#include <vtkPolyData.h>

// VTK – data arrays
#include <vtkPointData.h>
#include <vtkDoubleArray.h>

// ============================================================================
// Local helper functions (file-scope)
// ============================================================================

namespace {

/// Convert a QVector3D to a double[3] array.
void toArray(const QVector3D& v, double out[3])
{
    out[0] = static_cast<double>(v.x());
    out[1] = static_cast<double>(v.y());
    out[2] = static_cast<double>(v.z());
}

/// Normalise a double[3] vector.  Returns false if the vector is near-zero.
bool normalise3(double v[3])
{
    const double len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len < 1e-12) return false;
    v[0] /= len; v[1] /= len; v[2] /= len;
    return true;
}

/// Compute a rotation matrix R that rotates the +X axis to the direction \a dir.
/// \a dir must be a unit vector.  If \a dir ≈ +X we fall back to +Z as the
/// "up" reference to avoid degenerate cross products.
///
/// The resulting 4x4 matrix is written into \a mat in column-major VTK order.
void buildAlignXToDir(const double dir[3], vtkTransform* xf)
{
    // VTK ArrowSource points along +X.  We want it along dir[].
    // Strategy: rotate +X to dir using an angle-axis rotation.

    static const double xAxis[3] = {1.0, 0.0, 0.0};

    // Cross product: axis = +X × dir
    double axis[3] = {
        xAxis[1]*dir[2] - xAxis[2]*dir[1],
        xAxis[2]*dir[0] - xAxis[0]*dir[2],
        xAxis[0]*dir[1] - xAxis[1]*dir[0]
    };

    double angle = 0.0;

    const double axisLen = std::sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
    const double cosA    = xAxis[0]*dir[0] + xAxis[1]*dir[1] + xAxis[2]*dir[2];

    if (axisLen < 1e-10) {
        // dir ≈ +X (cosA ≈ 1) → identity, or dir ≈ -X (cosA ≈ -1) → 180°.
        if (cosA < 0.0) {
            // Rotate 180° around +Z.
            xf->RotateWXYZ(180.0, 0.0, 0.0, 1.0);
        }
        // else: no rotation needed
        return;
    }

    // angle in degrees
    angle = std::atan2(axisLen, cosA) * (180.0 / M_PI);
    axis[0] /= axisLen; axis[1] /= axisLen; axis[2] /= axisLen;

    xf->RotateWXYZ(angle, axis[0], axis[1], axis[2]);
}

/// Build a wireframe frustum polydata (8 edges) representing the camera FOV.
///
/// The frustum apex is at the origin.  The far-plane rectangle is at distance
/// \a depth along +X.  Half-widths at the far plane are determined by the
/// horizontal and vertical FOV angles.
vtkSmartPointer<vtkPolyData> buildFrustumPolyData(double depth,
                                                   double fovHDeg,
                                                   double fovVDeg)
{
    const double tanH = std::tan((fovHDeg * 0.5) * (M_PI / 180.0));
    const double tanV = std::tan((fovVDeg * 0.5) * (M_PI / 180.0));
    const double hw   = depth * tanH;   // half-width at far plane
    const double hh   = depth * tanV;   // half-height at far plane

    // 5 vertices: apex (0) + 4 far-plane corners (1..4)
    auto pts = vtkSmartPointer<vtkPoints>::New();
    pts->SetNumberOfPoints(5);
    pts->SetPoint(0, 0.0,  0.0,  0.0);   // apex
    pts->SetPoint(1, depth, -hw, -hh);   // bottom-left
    pts->SetPoint(2, depth,  hw, -hh);   // bottom-right
    pts->SetPoint(3, depth,  hw,  hh);   // top-right
    pts->SetPoint(4, depth, -hw,  hh);   // top-left

    // 8 line segments: 4 from apex to corners, 4 connecting corners.
    auto lines = vtkSmartPointer<vtkCellArray>::New();

    // Apex -> corners
    for (vtkIdType c = 1; c <= 4; ++c) {
        vtkIdType ids[2] = {0, c};
        lines->InsertNextCell(2, ids);
    }
    // Far-plane rectangle
    {
        vtkIdType ids[2];
        ids[0] = 1; ids[1] = 2; lines->InsertNextCell(2, ids);
        ids[0] = 2; ids[1] = 3; lines->InsertNextCell(2, ids);
        ids[0] = 3; ids[1] = 4; lines->InsertNextCell(2, ids);
        ids[0] = 4; ids[1] = 1; lines->InsertNextCell(2, ids);
    }

    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(pts);
    pd->SetLines(lines);
    return pd;
}

} // anonymous namespace

// ============================================================================
// Construction / destruction
// ============================================================================

PointAnnotator::PointAnnotator(CadScene* scene, QObject* parent)
    : QObject(parent)
    , m_scene(scene)
{
    m_picker = vtkSmartPointer<vtkCellPicker>::New();
    m_picker->SetTolerance(0.005);
}

PointAnnotator::~PointAnnotator()
{
    // Remove all annotation actors from the renderer before this object dies.
    clearTargets();
    clearPath();
}

// ============================================================================
// Annotation mode
// ============================================================================

void PointAnnotator::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool PointAnnotator::isEnabled() const
{
    return m_enabled;
}

// ============================================================================
// Capture configuration
// ============================================================================

void PointAnnotator::setCaptureConfig(const hmi::CaptureConfig& config)
{
    m_captureConfig = config;

    // Rebuild the frustum actors for all existing visuals.
    vtkRenderer* ren = m_scene ? m_scene->renderer() : nullptr;

    for (auto it = m_visuals.begin(); it != m_visuals.end(); ++it) {
        auto& vis = it.value();
        if (vis.frustumActor && ren) {
            ren->RemoveActor(vis.frustumActor);
        }
        vis.frustumActor = createFrustumActor(vis.target.surface, vis.target.view);
        if (vis.frustumActor && ren) {
            ren->AddActor(vis.frustumActor);
        }
    }

    if (ren) {
        auto* rw = ren->GetRenderWindow();
        if (rw) rw->Render();
    }
}

// ============================================================================
// Target management
// ============================================================================

void PointAnnotator::addTarget(const hmi::InspectionTarget& target)
{
    if (m_visuals.contains(target.pointId)) {
        // Already present – treat as update.
        updateTarget(target);
        return;
    }

    AnnotationVisual vis = createVisual(target);
    vtkRenderer* ren = m_scene ? m_scene->renderer() : nullptr;
    if (ren) {
        if (vis.sphereActor)   ren->AddActor(vis.sphereActor);
        if (vis.arrowActor)    ren->AddActor(vis.arrowActor);
        if (vis.frustumActor)  ren->AddActor(vis.frustumActor);
        if (vis.labelActor) {
            // vtkFollower needs the camera to be set before adding to renderer.
            auto* follower = vtkFollower::SafeDownCast(vis.labelActor.Get());
            if (follower && ren->GetActiveCamera()) {
                follower->SetCamera(ren->GetActiveCamera());
            }
            ren->AddActor(vis.labelActor);
        }
    }

    m_visuals.insert(target.pointId, std::move(vis));
    emit targetAdded(target.pointId);
}

void PointAnnotator::removeTarget(int32_t pointId)
{
    auto it = m_visuals.find(pointId);
    if (it == m_visuals.end()) return;

    removeVisualFromRenderer(it.value());
    m_visuals.erase(it);

    if (m_selectedId == pointId) {
        m_selectedId = -1;
    }

    if (m_scene && m_scene->renderer()) {
        auto* rw = m_scene->renderer()->GetRenderWindow();
        if (rw) rw->Render();
    }

    emit targetRemoved(pointId);
}

void PointAnnotator::updateTarget(const hmi::InspectionTarget& target)
{
    auto it = m_visuals.find(target.pointId);
    if (it == m_visuals.end()) {
        addTarget(target);
        return;
    }

    // Remove old actors.
    removeVisualFromRenderer(it.value());

    // Rebuild.
    const bool wasSelected = (m_selectedId == target.pointId);
    AnnotationVisual vis = createVisual(target);
    updateVisualAppearance(vis, wasSelected);

    vtkRenderer* ren = m_scene ? m_scene->renderer() : nullptr;
    if (ren) {
        if (vis.sphereActor)   ren->AddActor(vis.sphereActor);
        if (vis.arrowActor)    ren->AddActor(vis.arrowActor);
        if (vis.frustumActor)  ren->AddActor(vis.frustumActor);
        if (vis.labelActor) {
            // vtkFollower needs the camera to be set before adding to renderer.
            auto* follower = vtkFollower::SafeDownCast(vis.labelActor.Get());
            if (follower && ren->GetActiveCamera()) {
                follower->SetCamera(ren->GetActiveCamera());
            }
            ren->AddActor(vis.labelActor);
        }
    }

    *it = std::move(vis);

    if (ren) {
        auto* rw = ren->GetRenderWindow();
        if (rw) rw->Render();
    }
}

void PointAnnotator::clearTargets()
{
    for (auto it = m_visuals.begin(); it != m_visuals.end(); ++it) {
        removeVisualFromRenderer(it.value());
    }
    m_visuals.clear();
    m_selectedId = -1;

    if (m_scene && m_scene->renderer()) {
        auto* rw = m_scene->renderer()->GetRenderWindow();
        if (rw) rw->Render();
    }
}

QVector<hmi::InspectionTarget> PointAnnotator::targets() const
{
    QVector<hmi::InspectionTarget> result;
    result.reserve(m_visuals.size());
    for (auto it = m_visuals.begin(); it != m_visuals.end(); ++it) {
        result.append(it.value().target);
    }
    return result;
}

// ============================================================================
// Selection
// ============================================================================

void PointAnnotator::selectTarget(int32_t pointId)
{
    // Deselect previous.
    if (m_selectedId != -1 && m_selectedId != pointId) {
        auto prev = m_visuals.find(m_selectedId);
        if (prev != m_visuals.end()) {
            updateVisualAppearance(prev.value(), false);
        }
    }

    m_selectedId = pointId;

    auto it = m_visuals.find(pointId);
    if (it != m_visuals.end()) {
        updateVisualAppearance(it.value(), true);
    }

    if (m_scene && m_scene->renderer()) {
        auto* rw = m_scene->renderer()->GetRenderWindow();
        if (rw) rw->Render();
    }

    emit targetSelected(pointId);
}

void PointAnnotator::clearSelection()
{
    if (m_selectedId != -1) {
        auto it = m_visuals.find(m_selectedId);
        if (it != m_visuals.end()) {
            updateVisualAppearance(it.value(), false);
        }
        m_selectedId = -1;
        if (m_scene && m_scene->renderer()) {
            auto* rw = m_scene->renderer()->GetRenderWindow();
            if (rw) rw->Render();
        }
    }
}

int32_t PointAnnotator::selectedTargetId() const
{
    return m_selectedId;
}

// ============================================================================
// Path visualisation
// ============================================================================

void PointAnnotator::showPath(const hmi::InspectionPath& path)
{
    clearPath();

    if (path.waypoints.isEmpty()) return;

    vtkRenderer* ren = m_scene ? m_scene->renderer() : nullptr;

    // Main connecting polyline.
    auto lineActor = createPathLineActor(path);
    if (lineActor) {
        if (ren) ren->AddActor(lineActor);
        m_pathActors.append(lineActor);
    }

    // Small cyan sphere at each waypoint position (AGV XY position).
    for (const auto& wp : path.waypoints) {
        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetRadius(0.03);   // 3 cm waypoint dot
        sphere->SetThetaResolution(10);
        sphere->SetPhiResolution(10);
        sphere->SetCenter(wp.agvPose.x, wp.agvPose.y, 0.01);
        sphere->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());
        mapper->ScalarVisibilityOff();

        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        actor->GetProperty()->SetColor(0.0, 0.9, 0.4);
        actor->GetProperty()->SetOpacity(0.85);

        if (ren) ren->AddActor(actor);
        m_pathActors.append(actor);
    }

    if (ren) {
        auto* rw = ren->GetRenderWindow();
        if (rw) rw->Render();
    }
}

void PointAnnotator::clearPath()
{
    vtkRenderer* ren = m_scene ? m_scene->renderer() : nullptr;
    for (auto& actor : m_pathActors) {
        if (ren && actor) ren->RemoveActor(actor);
    }
    m_pathActors.clear();

    if (ren) {
        auto* rw = ren->GetRenderWindow();
        if (rw) rw->Render();
    }
}

void PointAnnotator::highlightWaypoint(int index)
{
    // Path actors layout: [0] = polyline, [1..N] = sphere per waypoint.
    // The waypoint sphere is at index (index + 1).
    const int sphereIdx = index + 1;
    if (sphereIdx < 0 || sphereIdx >= m_pathActors.size()) return;

    // Reset all waypoint spheres to default colour.
    for (int i = 1; i < m_pathActors.size(); ++i) {
        if (m_pathActors[i]) {
            m_pathActors[i]->GetProperty()->SetColor(0.0, 0.9, 0.4);
            m_pathActors[i]->GetProperty()->SetOpacity(0.85);
        }
    }

    // Highlight the requested waypoint.
    if (m_pathActors[sphereIdx]) {
        m_pathActors[sphereIdx]->GetProperty()->SetColor(1.0, 1.0, 0.0);
        m_pathActors[sphereIdx]->GetProperty()->SetOpacity(1.0);
    }

    if (m_scene && m_scene->renderer()) {
        auto* rw = m_scene->renderer()->GetRenderWindow();
        if (rw) rw->Render();
    }
}

// ============================================================================
// Surface picking
// ============================================================================

std::optional<hmi::SurfacePoint> PointAnnotator::pickSurface(int screenX, int screenY)
{
    if (!m_scene || !m_scene->renderer()) return std::nullopt;
    if (!m_scene->modelActor())           return std::nullopt;

    vtkRenderer* ren = m_scene->renderer();

    // Restrict the picker to the model actor to avoid picking annotation spheres.
    m_picker->InitializePickList();
    m_picker->AddPickList(m_scene->modelActor());
    m_picker->PickFromListOn();

    const int result = m_picker->Pick(
        static_cast<double>(screenX),
        static_cast<double>(screenY),
        0.0,
        ren
    );

    if (result == 0) return std::nullopt;

    double worldPos[3];
    m_picker->GetPickPosition(worldPos);

    double normal[3] = {0.0, 0.0, 1.0};
    m_picker->GetPickNormal(normal);

    hmi::SurfacePoint sp;
    sp.position  = QVector3D(static_cast<float>(worldPos[0]),
                             static_cast<float>(worldPos[1]),
                             static_cast<float>(worldPos[2]));
    sp.normal    = QVector3D(static_cast<float>(normal[0]),
                             static_cast<float>(normal[1]),
                             static_cast<float>(normal[2]));
    sp.frameId   = QStringLiteral("cad");
    sp.faceIndex = static_cast<uint32_t>(m_picker->GetCellId());

    emit surfacePicked(sp);
    return sp;
}

// ============================================================================
// Private helpers
// ============================================================================

PointAnnotator::AnnotationVisual
PointAnnotator::createVisual(const hmi::InspectionTarget& target)
{
    AnnotationVisual vis;
    vis.target = target;

    const double px = static_cast<double>(target.surface.position.x());
    const double py = static_cast<double>(target.surface.position.y());
    const double pz = static_cast<double>(target.surface.position.z());

    // ----------------------------------------------------------------
    // 1. Sphere marker (red, radius ≈ 3 mm for a typical meter-scale model)
    // ----------------------------------------------------------------
    {
        auto sphere = vtkSmartPointer<vtkSphereSource>::New();
        sphere->SetRadius(0.003);
        sphere->SetCenter(px, py, pz);
        sphere->SetThetaResolution(16);
        sphere->SetPhiResolution(16);
        sphere->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphere->GetOutputPort());
        mapper->ScalarVisibilityOff();

        vis.sphereActor = vtkSmartPointer<vtkActor>::New();
        vis.sphereActor->SetMapper(mapper);
        vis.sphereActor->GetProperty()->SetColor(0.9, 0.1, 0.1);  // red
        vis.sphereActor->GetProperty()->SetAmbient(0.3);
        vis.sphereActor->GetProperty()->SetDiffuse(0.7);
    }

    // ----------------------------------------------------------------
    // 2. Normal arrow (blue, length ≈ 15 mm)
    // ----------------------------------------------------------------
    {
        double nx = static_cast<double>(target.surface.normal.x());
        double ny = static_cast<double>(target.surface.normal.y());
        double nz = static_cast<double>(target.surface.normal.z());
        double nDir[3] = {nx, ny, nz};
        if (!normalise3(nDir)) {
            nDir[0] = 0.0; nDir[1] = 0.0; nDir[2] = 1.0;
        }

        const double arrowLen = 0.015;  // 15 mm

        auto arrow = vtkSmartPointer<vtkArrowSource>::New();
        arrow->SetTipLength(0.25);
        arrow->SetTipRadius(0.05);
        arrow->SetShaftRadius(0.02);
        arrow->SetTipResolution(12);
        arrow->SetShaftResolution(12);
        arrow->Update();

        // Transform: scale to arrowLen, rotate +X → normal, translate to surface pos.
        auto xf = vtkSmartPointer<vtkTransform>::New();
        xf->Translate(px, py, pz);
        buildAlignXToDir(nDir, xf);
        xf->Scale(arrowLen, arrowLen, arrowLen);

        auto xfFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
        xfFilter->SetInputConnection(arrow->GetOutputPort());
        xfFilter->SetTransform(xf);
        xfFilter->Update();

        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(xfFilter->GetOutputPort());
        mapper->ScalarVisibilityOff();

        vis.arrowActor = vtkSmartPointer<vtkActor>::New();
        vis.arrowActor->SetMapper(mapper);
        vis.arrowActor->GetProperty()->SetColor(0.1, 0.3, 0.9);  // blue
        vis.arrowActor->GetProperty()->SetAmbient(0.3);
        vis.arrowActor->GetProperty()->SetDiffuse(0.7);
    }

    // ----------------------------------------------------------------
    // 3. Camera frustum (wireframe, based on CaptureConfig + ViewHint)
    // ----------------------------------------------------------------
    vis.frustumActor = createFrustumActor(target.surface, target.view);

    // ----------------------------------------------------------------
    // 4. Billboard text label showing the point ID
    // ----------------------------------------------------------------
    {
        auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
        label->SetInput(QString::number(target.pointId).toLocal8Bit().constData());
        label->SetPosition(px, py, pz + 0.005);  // slight offset above the sphere

        vtkTextProperty* tp = label->GetTextProperty();
        tp->SetFontSize(14);
        tp->SetColor(1.0, 1.0, 1.0);           // white text
        tp->SetBackgroundColor(0.0, 0.0, 0.0);
        tp->SetBackgroundOpacity(0.55);
        tp->SetFontFamilyToCourier();
        tp->BoldOn();

        vis.labelActor = vtkSmartPointer<vtkActor>::New();

        // vtkBillboardTextActor3D is a vtkActor2D subclass, but we store it
        // through vtkActor for interface simplicity.  We add it to the
        // renderer explicitly below using a raw pointer cast.
        // Actually, store as vtkActor2D-compatible by adding separately.
        // We'll cast back when adding to renderer.
        // Store the label as a separate member using a union trick:
        // We reuse the labelActor field by casting — but vtkBillboardTextActor3D
        // IS-A vtkActor2D, not vtkActor.  We store it as vtkActor using
        // vtkSmartPointer<vtkActor> only if we downcast.
        //
        // VTK design: vtkBillboardTextActor3D inherits vtkActor2D, NOT vtkActor.
        // We handle this by storing it directly via vtkActor2D and adding it to
        // the renderer with AddActor2D.  The AnnotationVisual struct uses
        // vtkSmartPointer<vtkActor> for labelActor; we set it to nullptr and
        // store the text actor separately.
        //
        // Solution: store via a separate map – but to keep the struct clean we
        // store the text actor pointer in a parallel map keyed by pointId.
        // The actor is still owned by a vtkSmartPointer inside the struct; we
        // just need the vtk2dLabel_ approach.
        //
        // Simpler solution chosen: use vtkFollower (IS-A vtkActor) with
        // vtkVectorText instead.  This renders text as 3-D geometry, is a proper
        // vtkActor subclass, and the camera parameter makes it always face the viewer.

        // We will implement a vtkFollower+vtkVectorText text label in a
        // separate block below.  The billboard label variable goes unused.
        (void)label;
        vis.labelActor = nullptr;  // will be replaced by vtkFollower below
    }

    // ----------------------------------------------------------------
    // 4b. 3-D text label via vtkVectorText + vtkFollower
    //     vtkFollower IS-A vtkActor and always faces the active camera.
    // ----------------------------------------------------------------
    {
        auto vtext = vtkSmartPointer<vtkVectorText>::New();
        vtext->SetText(QString::number(target.pointId).toLocal8Bit().constData());
        vtext->Update();

        // Scale text to ≈2 mm height and position it just above the sphere.
        const double textScale = 0.004;  // 4 mm per unit (VTK text is ~1 unit tall)

        auto textMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        textMapper->SetInputConnection(vtext->GetOutputPort());
        textMapper->ScalarVisibilityOff();

        auto follower = vtkSmartPointer<vtkFollower>::New();
        follower->SetMapper(textMapper);
        follower->SetScale(textScale, textScale, textScale);
        follower->SetPosition(px, py, pz + 0.008);
        follower->GetProperty()->SetColor(1.0, 1.0, 0.2);  // yellow text
        follower->GetProperty()->SetAmbient(1.0);
        follower->GetProperty()->SetDiffuse(0.0);

        vis.labelActor = follower;
    }

    return vis;
}

// ============================================================================

void PointAnnotator::updateVisualAppearance(AnnotationVisual& visual, bool selected)
{
    if (!visual.sphereActor) return;

    if (selected) {
        // Larger yellow sphere.
        visual.sphereActor->GetProperty()->SetColor(1.0, 0.9, 0.0);  // yellow
        visual.sphereActor->SetScale(2.0, 2.0, 2.0);
    } else {
        // Normal red sphere.
        visual.sphereActor->GetProperty()->SetColor(0.9, 0.1, 0.1);  // red
        visual.sphereActor->SetScale(1.0, 1.0, 1.0);
    }

    if (visual.frustumActor) {
        if (selected) {
            visual.frustumActor->GetProperty()->SetColor(1.0, 0.9, 0.0);
            visual.frustumActor->GetProperty()->SetOpacity(0.7);
            visual.frustumActor->GetProperty()->SetLineWidth(2.0);
        } else {
            visual.frustumActor->GetProperty()->SetColor(0.3, 0.7, 1.0);
            visual.frustumActor->GetProperty()->SetOpacity(0.45);
            visual.frustumActor->GetProperty()->SetLineWidth(1.0);
        }
    }
}

// ============================================================================

void PointAnnotator::removeVisualFromRenderer(AnnotationVisual& visual)
{
    vtkRenderer* ren = m_scene ? m_scene->renderer() : nullptr;
    if (!ren) return;

    if (visual.sphereActor)   ren->RemoveActor(visual.sphereActor);
    if (visual.arrowActor)    ren->RemoveActor(visual.arrowActor);
    if (visual.frustumActor)  ren->RemoveActor(visual.frustumActor);
    if (visual.labelActor)    ren->RemoveActor(visual.labelActor);
}

// ============================================================================

vtkSmartPointer<vtkActor>
PointAnnotator::createFrustumActor(const hmi::SurfacePoint& surface,
                                    const hmi::ViewHint&     view)
{
    // Use reasonable defaults when capture config is not yet set.
    const double focusDist = (m_captureConfig.focusDistanceM > 1e-6)
                             ? m_captureConfig.focusDistanceM
                             : 0.25;
    const double fovH = (m_captureConfig.fovHDeg > 1e-6)
                        ? m_captureConfig.fovHDeg
                        : 60.0;
    const double fovV = (m_captureConfig.fovVDeg > 1e-6)
                        ? m_captureConfig.fovVDeg
                        : 45.0;

    // Build the canonical frustum pointing along +X.
    auto frustumPD = buildFrustumPolyData(focusDist, fovH, fovV);

    // View direction: from the surface point toward the camera.
    // Convention: view_direction = camera forward.  The apex of the frustum
    // is at surface + view_direction * focusDist (camera origin).
    double vd[3];
    vd[0] = static_cast<double>(view.viewDirection.x());
    vd[1] = static_cast<double>(view.viewDirection.y());
    vd[2] = static_cast<double>(view.viewDirection.z());
    if (!normalise3(vd)) {
        // Fall back to surface normal direction.
        vd[0] = -static_cast<double>(surface.normal.x());
        vd[1] = -static_cast<double>(surface.normal.y());
        vd[2] = -static_cast<double>(surface.normal.z());
        if (!normalise3(vd)) {
            vd[0] = 0.0; vd[1] = 0.0; vd[2] = 1.0;
        }
    }

    // Frustum apex position: surface + vd * focusDist.
    const double ax = static_cast<double>(surface.position.x()) + vd[0] * focusDist;
    const double ay = static_cast<double>(surface.position.y()) + vd[1] * focusDist;
    const double az = static_cast<double>(surface.position.z()) + vd[2] * focusDist;

    // The frustum geometry is built along +X.
    // We need to rotate +X to POINT AWAY from the apex (opposite of viewDirection),
    // because the frustum opens away from the camera toward the surface.
    // Camera looks along vd; so frustum opens in the direction (-vd).
    double openDir[3] = {-vd[0], -vd[1], -vd[2]};

    auto xf = vtkSmartPointer<vtkTransform>::New();
    xf->Translate(ax, ay, az);
    buildAlignXToDir(openDir, xf);
    // Roll: rotate around the (now-aligned) +X axis by rollDeg.
    if (std::abs(view.rollDeg) > 1e-6) {
        xf->RotateX(view.rollDeg);
    }

    auto xfFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
    xfFilter->SetInputData(frustumPD);
    xfFilter->SetTransform(xf);
    xfFilter->Update();

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(xfFilter->GetOutputPort());
    mapper->ScalarVisibilityOff();

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.3, 0.7, 1.0);       // light blue wireframe
    actor->GetProperty()->SetRepresentationToWireframe();
    actor->GetProperty()->SetLineWidth(1.0);
    actor->GetProperty()->SetOpacity(0.45);

    return actor;
}

// ============================================================================

vtkSmartPointer<vtkActor>
PointAnnotator::createPathLineActor(const hmi::InspectionPath& path)
{
    if (path.waypoints.isEmpty()) return nullptr;

    auto pts = vtkSmartPointer<vtkPoints>::New();
    pts->SetNumberOfPoints(path.waypoints.size());

    for (int i = 0; i < path.waypoints.size(); ++i) {
        const auto& wp = path.waypoints[i];
        pts->SetPoint(static_cast<vtkIdType>(i),
                      wp.agvPose.x, wp.agvPose.y, 0.005);
    }

    // Build a single poly-line cell connecting all points.
    auto polyLine = vtkSmartPointer<vtkPolyLine>::New();
    polyLine->GetPointIds()->SetNumberOfIds(path.waypoints.size());
    for (int i = 0; i < path.waypoints.size(); ++i) {
        polyLine->GetPointIds()->SetId(static_cast<vtkIdType>(i),
                                       static_cast<vtkIdType>(i));
    }

    auto cells = vtkSmartPointer<vtkCellArray>::New();
    cells->InsertNextCell(polyLine);

    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(pts);
    pd->SetLines(cells);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(pd);
    mapper->ScalarVisibilityOff();

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.0, 0.85, 0.3);     // green path
    actor->GetProperty()->SetLineWidth(2.5);
    actor->GetProperty()->SetOpacity(0.85);

    return actor;
}
