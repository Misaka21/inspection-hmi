// src/core/Types.h
//
// Qt-friendly domain data types that mirror the inspection_gateway.proto
// message definitions.  These types are used throughout the HMI (signals,
// slots, view-models) and are deliberately decoupled from generated protobuf
// headers so that no consumer of Types.h needs to pull in heavy proto
// machinery.
//
// Conversion from/to proto is handled exclusively inside GatewayClient.cpp.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMetaType>
#include <QPointF>
#include <QString>
#include <QQuaternion>
#include <QVector>
#include <QVector3D>
#include <array>
#include <cstdint>

namespace hmi {

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

/// Mirrors proto ErrorCode enum values.
enum class ErrorCode : int32_t {
    Unspecified     = 0,
    Ok              = 1,
    InvalidArgument = 2,
    NotFound        = 3,
    Timeout         = 4,
    Busy            = 5,
    Internal        = 6,
    Unavailable     = 7,
    Conflict        = 8,
};

/// Lightweight result holder returned by every RPC completion signal.
struct Result {
    ErrorCode code    = ErrorCode::Unspecified;
    QString   message;

    /// Returns true when the RPC completed successfully.
    [[nodiscard]] bool ok() const noexcept { return code == ErrorCode::Ok; }
};

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

struct Pose2D {
    double  x       = 0.0;
    double  y       = 0.0;
    double  yaw     = 0.0;
    QString frameId;
};

struct Pose3D {
    QVector3D    position;
    QQuaternion  orientation;   ///< (x, y, z, w)
    QString      frameId;
};

struct SurfacePoint {
    QVector3D position;
    QVector3D normal;           ///< Unit vector in SurfacePoint::frameId.
    QString   frameId;
    uint32_t  faceIndex = 0;    ///< Optional: for debug / CAD round-trip.
};

struct ViewHint {
    QVector3D viewDirection;    ///< Camera forward direction unit vector.
    double    rollDeg = 0.0;    ///< Rotation around viewDirection.
};

// ---------------------------------------------------------------------------
// Media references
// ---------------------------------------------------------------------------

struct MediaRef {
    QString  mediaId;
    QString  mimeType;
    QString  sha256;
    QString  url;
    uint64_t sizeBytes = 0;
};

struct ImageRef {
    MediaRef    media;
    uint32_t    width  = 0;
    uint32_t    height = 0;
    QByteArray  thumbnailJpeg;  ///< Optional small preview for UI.
};

// ---------------------------------------------------------------------------
// Defect / detection
// ---------------------------------------------------------------------------

struct BoundingBox2D {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct DefectResult {
    bool         hasDefect   = false;
    QString      defectType;
    float        confidence  = 0.0f;
    BoundingBox2D bbox;
};

// ---------------------------------------------------------------------------
// Capture configuration
// ---------------------------------------------------------------------------

struct CaptureConfig {
    QString cameraId;
    double  focusDistanceM        = 0.0;
    double  fovHDeg               = 0.0;
    double  fovVDeg               = 0.0;
    double  maxTiltFromNormalDeg  = 0.0;
};

// ---------------------------------------------------------------------------
// Inspection target / plan
// ---------------------------------------------------------------------------

struct InspectionTarget {
    int32_t       pointId = 0;
    QString       groupId;
    SurfacePoint  surface;
    ViewHint      view;
};

struct InspectionPoint {
    int32_t                pointId     = 0;
    QString                groupId;
    Pose2D                 agvPose;
    Pose3D                 armPose;
    std::array<double, 6>  armJointGoal{};
    double                 expectedQuality = 0.0;
    double                 planningCost    = 0.0;
    Pose3D                 tcpPoseGoal;
    Pose3D                 cameraPose;
    QString                cameraId;
};

struct InspectionPath {
    QVector<InspectionPoint> waypoints;
    uint32_t                 totalPoints          = 0;
    double                   estimatedDistanceM   = 0.0;
    double                   estimatedDurationS   = 0.0;
};

struct PlanningWeights {
    double wAgvDistance    = 0.0;
    double wJointDelta     = 0.0;
    double wManipulability = 0.0;
    double wViewError      = 0.0;
    double wJointLimit     = 0.0;
};

struct PlanOptions {
    double         candidateRadiusM       = 0.0;
    double         candidateYawStepDeg    = 0.0;
    bool           enableCollisionCheck   = true;
    bool           enableTspOptimization  = true;
    QString        ikSolver;
    PlanningWeights weights;
};

struct PlanningStatistics {
    uint32_t candidatePoseCount      = 0;
    uint32_t ikSuccessCount          = 0;
    uint32_t collisionFilteredCount  = 0;
    double   planningTimeMs          = 0.0;
};

// ---------------------------------------------------------------------------
// Task status
// ---------------------------------------------------------------------------

/// Mirrors proto TaskPhase enum values.
enum class TaskPhase : int32_t {
    Unspecified = 0,
    Idle        = 1,
    Localizing  = 2,
    Planning    = 3,
    Executing   = 4,
    Paused      = 5,
    Completed   = 6,
    Failed      = 7,
    Stopped     = 8,
};

struct AgvStatus {
    bool    connected            = false;
    bool    arrived              = false;
    bool    moving               = false;
    bool    stopped              = false;
    Pose2D  currentPose;
    float   batteryPercent       = 0.0f;
    QString errorCode;
    float   linearVelocityMps    = 0.0f;
    float   angularVelocityRps   = 0.0f;
    Pose2D  goalPose;
    QString mapId;
    float   localizationQuality  = 0.0f;
};

struct ArmStatus {
    bool                  connected      = false;
    bool                  arrived        = false;
    bool                  moving         = false;
    std::array<double, 6> currentJoints{};
    double                manipulability = 0.0;
    QString               errorCode;
    bool                  servoEnabled   = false;
    Pose3D                tcpPose;
    Pose3D                basePose;
};

struct TaskStatus {
    QString   taskId;
    TaskPhase phase           = TaskPhase::Unspecified;
    float     progressPercent = 0.0f;
    QString   currentAction;
    QString   errorMessage;
    AgvStatus agv;
    ArmStatus arm;
    QDateTime updatedAt;
    QString   planId;
    QString   taskName;
    uint32_t  currentWaypointIndex = 0;
    int32_t   currentPointId       = 0;
    uint32_t  totalWaypoints       = 0;
    bool      interlockOk          = false;
    QString   interlockMessage;
    double    remainingTimeEstS    = 0.0;
    QDateTime startedAt;
    QDateTime finishedAt;
};

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

/// Mirrors proto InspectionEventType enum values.
enum class InspectionEventType : int32_t {
    Unspecified  = 0,
    Info         = 1,
    Warn         = 2,
    Error        = 3,
    Captured     = 4,
    DefectFound  = 5,
};

struct InspectionEvent {
    QString               taskId;
    int32_t               pointId     = 0;
    InspectionEventType   type        = InspectionEventType::Unspecified;
    QString               message;
    DefectResult          defect;
    QDateTime             timestamp;
    QString               captureId;
    QString               cameraId;
    ImageRef              image;
    QVector<DefectResult> defects;
    Pose3D                cameraPose;
};

// ---------------------------------------------------------------------------
// Capture records
// ---------------------------------------------------------------------------

struct CaptureRecord {
    QString               taskId;
    int32_t               pointId   = 0;
    QString               captureId;
    QString               cameraId;
    ImageRef              image;
    QVector<DefectResult> defects;
    QDateTime             capturedAt;
};

// ---------------------------------------------------------------------------
// Navigation map
// ---------------------------------------------------------------------------

struct NavMapInfo {
    QString   mapId;
    QString   name;
    double    resolutionMPerPixel = 0.0;
    uint32_t  width  = 0;
    uint32_t  height = 0;
    Pose2D    origin;
    ImageRef  image;
    QDateTime updatedAt;
};

// ---------------------------------------------------------------------------
// Compound RPC response types
// ---------------------------------------------------------------------------

struct PlanResponse {
    Result          result;
    QString         planId;
    InspectionPath  path;
    PlanningStatistics stats;
};

struct GetPlanResponse {
    Result             result;
    QString            planId;
    QString            modelId;
    QString            taskName;
    PlanOptions        options;
    InspectionPath     path;
    PlanningStatistics stats;
    QDateTime          createdAt;
};

} // namespace hmi

// Register value types with Qt's meta-object system so they can travel
// through queued connections without explicit qRegisterMetaType() calls in
// each translation unit.  Q_DECLARE_METATYPE only instructs the template;
// the actual registration happens lazily the first time the type is used in
// a QVariant or through QMetaType::fromType<T>().
Q_DECLARE_METATYPE(hmi::Result)
Q_DECLARE_METATYPE(hmi::TaskStatus)
Q_DECLARE_METATYPE(hmi::InspectionEvent)
Q_DECLARE_METATYPE(hmi::PlanResponse)
Q_DECLARE_METATYPE(hmi::GetPlanResponse)
Q_DECLARE_METATYPE(hmi::NavMapInfo)
Q_DECLARE_METATYPE(hmi::CaptureRecord)
Q_DECLARE_METATYPE(QVector<hmi::CaptureRecord>)
Q_DECLARE_METATYPE(QVector<hmi::DefectResult>)
Q_DECLARE_METATYPE(QVector<hmi::InspectionTarget>)
