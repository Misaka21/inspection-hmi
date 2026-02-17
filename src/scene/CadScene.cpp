// src/scene/CadScene.cpp

#include "CadScene.h"

#include <QFileInfo>
#include <QString>

// VTK – rendering
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCamera.h>
#include <vtkLight.h>
#include <vtkLightCollection.h>

// VTK – actors / mappers
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>

// VTK – geometry
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPointData.h>
#include <vtkCellData.h>

// VTK – file readers
#include <vtkSTLReader.h>
#include <vtkOBJReader.h>
#include <vtkPLYReader.h>

// VTK – axes / orientation widget
#include <vtkAxesActor.h>
#include <vtkOrientationMarkerWidget.h>

// VTK – smart pointer
#include <vtkSmartPointer.h>

// ============================================================================
// Construction / destruction
// ============================================================================

CadScene::CadScene(QObject* parent)
    : QObject(parent)
{}

CadScene::~CadScene() = default;

// ============================================================================
// Renderer binding
// ============================================================================

void CadScene::setRenderer(vtkRenderer* renderer)
{
    m_renderer = renderer;
}

vtkRenderer* CadScene::renderer() const
{
    return m_renderer.Get();
}

// ============================================================================
// Model loading
// ============================================================================

bool CadScene::loadModel(const QString& filePath)
{
    if (!m_renderer) {
        emit errorOccurred(QStringLiteral("Cannot load model: no renderer set."));
        return false;
    }

    // Remove the previous model (if any) from the scene.
    clearModel();

    auto pd = readFile(filePath);
    if (!pd) {
        emit errorOccurred(QStringLiteral("Failed to read file: %1").arg(filePath));
        return false;
    }

    ensureNormals(pd);
    m_modelData = pd;

    // Create mapper + actor.
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(m_modelData);
    mapper->ScalarVisibilityOff();

    m_modelActor = vtkSmartPointer<vtkActor>::New();
    m_modelActor->SetMapper(mapper);
    applyMaterial();

    m_renderer->AddActor(m_modelActor);

    // Remove any lights added by a previous loadModel call, then rebuild.
    m_renderer->RemoveAllLights();
    setupDefaultLighting();

    m_modelFilePath = filePath;
    resetCamera();

    emit modelLoaded(filePath);
    return true;
}

void CadScene::clearModel()
{
    if (m_modelActor && m_renderer) {
        m_renderer->RemoveActor(m_modelActor);
    }
    m_modelActor  = nullptr;
    m_modelData   = nullptr;
    m_modelFilePath.clear();

    emit modelCleared();
}

bool CadScene::hasModel() const
{
    return m_modelData != nullptr;
}

QString CadScene::modelFilePath() const
{
    return m_modelFilePath;
}

// ============================================================================
// Access to loaded model data
// ============================================================================

vtkPolyData* CadScene::modelPolyData() const
{
    return m_modelData.Get();
}

vtkActor* CadScene::modelActor() const
{
    return m_modelActor.Get();
}

// ============================================================================
// Camera control
// ============================================================================

void CadScene::resetCamera()
{
    if (!m_renderer) return;
    m_renderer->ResetCamera();
    render();
}

void CadScene::setViewFront()
{
    // Camera looks along −Y, up is +Z.
    // Focal point placed at the model bounds centre by applyViewPreset.
    applyViewPreset( 0.0,  1.0, 0.0,   // eye (will be scaled by ResetCamera)
                     0.0,  0.0, 0.0,   // focal
                     0.0,  0.0, 1.0);  // up
}

void CadScene::setViewTop()
{
    // Camera looks along −Z (down), up is +Y.
    applyViewPreset( 0.0, 0.0,  1.0,
                     0.0, 0.0,  0.0,
                     0.0, 1.0,  0.0);
}

void CadScene::setViewRight()
{
    // Camera looks along −X, up is +Z.
    applyViewPreset( 1.0, 0.0, 0.0,
                     0.0, 0.0, 0.0,
                     0.0, 0.0, 1.0);
}

void CadScene::setViewIsometric()
{
    // Classic 3-axis isometric: camera at (+1,+1,+1) relative to origin.
    applyViewPreset( 1.0, 1.0, 1.0,
                     0.0, 0.0, 0.0,
                     0.0, 0.0, 1.0);
}

// ============================================================================
// Rendering
// ============================================================================

void CadScene::render()
{
    if (!m_renderer) return;
    auto* rw = m_renderer->GetRenderWindow();
    if (rw) {
        rw->Render();
    }
}

// ============================================================================
// Orientation widget
// ============================================================================

void CadScene::initOrientationWidget(vtkRenderWindowInteractor* interactor)
{
    if (!interactor || !m_renderer) return;

    auto axes = vtkSmartPointer<vtkAxesActor>::New();
    axes->SetTotalLength(1.0, 1.0, 1.0);
    axes->SetShaftTypeToCylinder();
    axes->SetCylinderRadius(0.03);
    axes->SetConeRadius(0.15);

    m_orientationWidget = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    m_orientationWidget->SetOrientationMarker(axes);
    m_orientationWidget->SetInteractor(interactor);
    m_orientationWidget->SetViewport(0.0, 0.0, 0.15, 0.15);
    m_orientationWidget->SetEnabled(1);
    m_orientationWidget->InteractiveOff();
}

// ============================================================================
// Private helpers
// ============================================================================

vtkSmartPointer<vtkPolyData> CadScene::readFile(const QString& filePath)
{
    QFileInfo info(filePath);
    const QString ext = info.suffix().toLower();

    if (ext == QStringLiteral("stl")) {
        auto reader = vtkSmartPointer<vtkSTLReader>::New();
        reader->SetFileName(filePath.toLocal8Bit().constData());
        reader->Update();
        if (reader->GetOutput()->GetNumberOfPoints() == 0) {
            return nullptr;
        }
        auto pd = vtkSmartPointer<vtkPolyData>::New();
        pd->DeepCopy(reader->GetOutput());
        return pd;
    }

    if (ext == QStringLiteral("obj")) {
        auto reader = vtkSmartPointer<vtkOBJReader>::New();
        reader->SetFileName(filePath.toLocal8Bit().constData());
        reader->Update();
        if (reader->GetOutput()->GetNumberOfPoints() == 0) {
            return nullptr;
        }
        auto pd = vtkSmartPointer<vtkPolyData>::New();
        pd->DeepCopy(reader->GetOutput());
        return pd;
    }

    if (ext == QStringLiteral("ply")) {
        auto reader = vtkSmartPointer<vtkPLYReader>::New();
        reader->SetFileName(filePath.toLocal8Bit().constData());
        reader->Update();
        if (reader->GetOutput()->GetNumberOfPoints() == 0) {
            return nullptr;
        }
        auto pd = vtkSmartPointer<vtkPolyData>::New();
        pd->DeepCopy(reader->GetOutput());
        return pd;
    }

    emit errorOccurred(QStringLiteral("Unsupported file format: .%1").arg(ext));
    return nullptr;
}

void CadScene::ensureNormals(vtkSmartPointer<vtkPolyData>& pd)
{
    // Check whether normals are already present on the point data.
    bool hasPointNormals = (pd->GetPointData()->GetNormals() != nullptr);
    bool hasCellNormals  = (pd->GetCellData()->GetNormals()  != nullptr);

    if (hasPointNormals || hasCellNormals) return;

    auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputData(pd);
    normals->ComputePointNormalsOn();
    normals->ComputeCellNormalsOn();
    normals->SplittingOff();          // preserve topology for picking
    normals->ConsistencyOn();
    normals->AutoOrientNormalsOn();
    normals->Update();

    pd = normals->GetOutput();
}

void CadScene::setupDefaultLighting()
{
    if (!m_renderer) return;

    // 3-point lighting rig (key / fill / rim).
    // Intensities chosen for a neutral industrial appearance.

    // Key light – warm, upper-front-left.
    auto key = vtkSmartPointer<vtkLight>::New();
    key->SetLightTypeToSceneLight();
    key->SetPosition( 1.0, 1.0, 2.0);
    key->SetFocalPoint(0.0, 0.0, 0.0);
    key->SetColor(1.0, 0.98, 0.95);
    key->SetIntensity(1.0);
    m_renderer->AddLight(key);

    // Fill light – cool, left side.
    auto fill = vtkSmartPointer<vtkLight>::New();
    fill->SetLightTypeToSceneLight();
    fill->SetPosition(-2.0, 0.5, 0.5);
    fill->SetFocalPoint(0.0, 0.0, 0.0);
    fill->SetColor(0.85, 0.90, 1.0);
    fill->SetIntensity(0.45);
    m_renderer->AddLight(fill);

    // Rim / back light – neutral, behind.
    auto rim = vtkSmartPointer<vtkLight>::New();
    rim->SetLightTypeToSceneLight();
    rim->SetPosition(0.0, -1.5, -1.0);
    rim->SetFocalPoint(0.0, 0.0, 0.0);
    rim->SetColor(0.95, 0.95, 0.95);
    rim->SetIntensity(0.30);
    m_renderer->AddLight(rim);
}

void CadScene::setupAxes()
{
    // Axes are set up lazily in initOrientationWidget() once the interactor
    // is available.  This method is kept as a no-op hook for subclasses.
}

void CadScene::applyMaterial()
{
    if (!m_modelActor) return;
    vtkProperty* prop = m_modelActor->GetProperty();

    // Neutral light grey – good contrast with both red and blue annotation actors.
    prop->SetColor(0.78, 0.78, 0.78);
    prop->SetAmbient(0.15);
    prop->SetDiffuse(0.70);
    prop->SetSpecular(0.20);
    prop->SetSpecularPower(25.0);
    prop->SetOpacity(1.0);
    prop->BackfaceCullingOff();
}

void CadScene::applyViewPreset(double ex, double ey, double ez,
                                double /*fx*/, double /*fy*/, double /*fz*/,
                                double ux, double uy, double uz)
{
    if (!m_renderer) return;

    vtkCamera* cam = m_renderer->GetActiveCamera();
    if (!cam) return;

    // Determine a sensible distance from the model bounds diagonal.
    double bounds[6] = {0.0};
    if (m_modelActor) {
        m_modelActor->GetBounds(bounds);
    }
    const double cx = (bounds[0] + bounds[1]) * 0.5;
    const double cy = (bounds[2] + bounds[3]) * 0.5;
    const double cz = (bounds[4] + bounds[5]) * 0.5;

    const double dx = bounds[1] - bounds[0];
    const double dy = bounds[3] - bounds[2];
    const double dz = bounds[5] - bounds[4];
    const double diag = std::sqrt(dx*dx + dy*dy + dz*dz);
    const double dist = (diag > 0.0) ? diag * 1.5 : 1.0;

    // Normalise the eye direction.
    const double elen = std::sqrt(ex*ex + ey*ey + ez*ez);
    const double enx = (elen > 0.0) ? ex / elen : 1.0;
    const double eny = (elen > 0.0) ? ey / elen : 0.0;
    const double enz = (elen > 0.0) ? ez / elen : 0.0;

    cam->SetFocalPoint(cx, cy, cz);
    cam->SetPosition(cx + enx * dist,
                     cy + eny * dist,
                     cz + enz * dist);
    cam->SetViewUp(ux, uy, uz);

    m_renderer->ResetCamera();
    render();
}
