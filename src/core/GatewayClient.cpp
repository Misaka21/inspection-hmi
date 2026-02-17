// src/core/GatewayClient.cpp
//
// Full implementation of GatewayClient – every RPC, every conversion helper.
// See GatewayClient.h for design notes.

#include "GatewayClient.h"

// Generated protobuf headers (in build/proto_gen/).
#include "inspection_gateway.pb.h"

// gRPC runtime (grpcpp.h already included via header).
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

// Qt.
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QString>
#include <QUuid>

// std.
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// Namespace aliases
// ---------------------------------------------------------------------------
namespace proto = inspection::gateway::v1;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

namespace hmi {

// ===========================================================================
// Proto  ->  hmi conversion helpers
//
// Each helper is a free function defined in the anonymous namespace so it is
// translation-unit-private.
// ===========================================================================
namespace {

// ---------------------------------------------------------------------------
// Timestamp conversion
// ---------------------------------------------------------------------------
QDateTime fromTimestamp(const google::protobuf::Timestamp& ts)
{
    if (ts.seconds() == 0 && ts.nanos() == 0) {
        return QDateTime{};
    }
    QDateTime dt;
    dt.setSecsSinceEpoch(static_cast<qint64>(ts.seconds()));
    return dt;
}

// ---------------------------------------------------------------------------
// ErrorCode
// ---------------------------------------------------------------------------
hmi::ErrorCode fromProtoError(proto::ErrorCode ec)
{
    switch (ec) {
    case proto::OK:               return hmi::ErrorCode::Ok;
    case proto::INVALID_ARGUMENT: return hmi::ErrorCode::InvalidArgument;
    case proto::NOT_FOUND:        return hmi::ErrorCode::NotFound;
    case proto::TIMEOUT:          return hmi::ErrorCode::Timeout;
    case proto::BUSY:             return hmi::ErrorCode::Busy;
    case proto::INTERNAL:         return hmi::ErrorCode::Internal;
    case proto::UNAVAILABLE:      return hmi::ErrorCode::Unavailable;
    case proto::CONFLICT:         return hmi::ErrorCode::Conflict;
    default:                      return hmi::ErrorCode::Unspecified;
    }
}

hmi::Result fromProtoResult(const proto::Result& r)
{
    hmi::Result out;
    out.code    = fromProtoError(r.code());
    out.message = QString::fromStdString(r.message());
    return out;
}

// gRPC Status -> hmi::Result (for transport-level errors).
hmi::Result fromGrpcStatus(const Status& st)
{
    hmi::Result out;
    if (st.ok()) {
        out.code = hmi::ErrorCode::Ok;
    } else {
        out.code    = hmi::ErrorCode::Internal;
        out.message = QString::fromStdString(st.error_message());
    }
    return out;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
hmi::Pose2D fromProtoPose2D(const proto::Pose2D& p)
{
    return hmi::Pose2D{
        p.x(),
        p.y(),
        p.yaw(),
        QString::fromStdString(p.frame_id())
    };
}

proto::Pose2D toProtoPose2D(const hmi::Pose2D& p)
{
    proto::Pose2D out;
    out.set_x(p.x);
    out.set_y(p.y);
    out.set_yaw(p.yaw);
    out.set_frame_id(p.frameId.toStdString());
    return out;
}

hmi::Pose3D fromProtoPose3D(const proto::Pose3D& p)
{
    hmi::Pose3D out;
    if (p.has_position()) {
        out.position = QVector3D(
            static_cast<float>(p.position().x()),
            static_cast<float>(p.position().y()),
            static_cast<float>(p.position().z()));
    }
    if (p.has_orientation()) {
        // QQuaternion(scalar, x, y, z)
        out.orientation = QQuaternion(
            static_cast<float>(p.orientation().w()),
            static_cast<float>(p.orientation().x()),
            static_cast<float>(p.orientation().y()),
            static_cast<float>(p.orientation().z()));
    }
    out.frameId = QString::fromStdString(p.frame_id());
    return out;
}

proto::Pose3D toProtoPose3D(const hmi::Pose3D& p)
{
    proto::Pose3D out;
    auto* pos = out.mutable_position();
    pos->set_x(static_cast<double>(p.position.x()));
    pos->set_y(static_cast<double>(p.position.y()));
    pos->set_z(static_cast<double>(p.position.z()));
    auto* ori = out.mutable_orientation();
    ori->set_x(static_cast<double>(p.orientation.x()));
    ori->set_y(static_cast<double>(p.orientation.y()));
    ori->set_z(static_cast<double>(p.orientation.z()));
    ori->set_w(static_cast<double>(p.orientation.scalar()));
    out.set_frame_id(p.frameId.toStdString());
    return out;
}

hmi::SurfacePoint fromProtoSurfacePoint(const proto::SurfacePoint& sp)
{
    hmi::SurfacePoint out;
    if (sp.has_position()) {
        out.position = QVector3D(
            static_cast<float>(sp.position().x()),
            static_cast<float>(sp.position().y()),
            static_cast<float>(sp.position().z()));
    }
    if (sp.has_normal()) {
        out.normal = QVector3D(
            static_cast<float>(sp.normal().x()),
            static_cast<float>(sp.normal().y()),
            static_cast<float>(sp.normal().z()));
    }
    out.frameId   = QString::fromStdString(sp.frame_id());
    out.faceIndex = sp.face_index();
    return out;
}

proto::SurfacePoint toProtoSurfacePoint(const hmi::SurfacePoint& sp)
{
    proto::SurfacePoint out;
    auto* pos = out.mutable_position();
    pos->set_x(static_cast<double>(sp.position.x()));
    pos->set_y(static_cast<double>(sp.position.y()));
    pos->set_z(static_cast<double>(sp.position.z()));
    auto* norm = out.mutable_normal();
    norm->set_x(static_cast<double>(sp.normal.x()));
    norm->set_y(static_cast<double>(sp.normal.y()));
    norm->set_z(static_cast<double>(sp.normal.z()));
    out.set_frame_id(sp.frameId.toStdString());
    out.set_face_index(sp.faceIndex);
    return out;
}

hmi::ViewHint fromProtoViewHint(const proto::ViewHint& vh)
{
    hmi::ViewHint out;
    if (vh.has_view_direction()) {
        out.viewDirection = QVector3D(
            static_cast<float>(vh.view_direction().x()),
            static_cast<float>(vh.view_direction().y()),
            static_cast<float>(vh.view_direction().z()));
    }
    out.rollDeg = vh.roll_deg();
    return out;
}

proto::ViewHint toProtoViewHint(const hmi::ViewHint& vh)
{
    proto::ViewHint out;
    auto* vd = out.mutable_view_direction();
    vd->set_x(static_cast<double>(vh.viewDirection.x()));
    vd->set_y(static_cast<double>(vh.viewDirection.y()));
    vd->set_z(static_cast<double>(vh.viewDirection.z()));
    out.set_roll_deg(vh.rollDeg);
    return out;
}

// ---------------------------------------------------------------------------
// Media
// ---------------------------------------------------------------------------
hmi::MediaRef fromProtoMediaRef(const proto::MediaRef& m)
{
    hmi::MediaRef out;
    out.mediaId   = QString::fromStdString(m.media_id());
    out.mimeType  = QString::fromStdString(m.mime_type());
    out.sha256    = QString::fromStdString(m.sha256());
    out.url       = QString::fromStdString(m.url());
    out.sizeBytes = m.size_bytes();
    return out;
}

hmi::ImageRef fromProtoImageRef(const proto::ImageRef& img)
{
    hmi::ImageRef out;
    if (img.has_media()) {
        out.media = fromProtoMediaRef(img.media());
    }
    out.width         = img.width();
    out.height        = img.height();
    const auto& thumb = img.thumbnail_jpeg();
    out.thumbnailJpeg = QByteArray(thumb.data(), static_cast<int>(thumb.size()));
    return out;
}

// ---------------------------------------------------------------------------
// Defect
// ---------------------------------------------------------------------------
hmi::BoundingBox2D fromProtoBbox(const proto::BoundingBox2D& bb)
{
    return hmi::BoundingBox2D{bb.x(), bb.y(), bb.w(), bb.h()};
}

hmi::DefectResult fromProtoDefectResult(const proto::DefectResult& dr)
{
    hmi::DefectResult out;
    out.hasDefect  = dr.has_defect();
    out.defectType = QString::fromStdString(dr.defect_type());
    out.confidence = dr.confidence();
    if (dr.has_bbox()) {
        out.bbox = fromProtoBbox(dr.bbox());
    }
    return out;
}

// ---------------------------------------------------------------------------
// InspectionTarget  (hmi -> proto, used in SetInspectionTargets)
// ---------------------------------------------------------------------------
proto::InspectionTarget toProtoTarget(const hmi::InspectionTarget& t)
{
    proto::InspectionTarget out;
    out.set_point_id(t.pointId);
    out.set_group_id(t.groupId.toStdString());
    *out.mutable_surface() = toProtoSurfacePoint(t.surface);
    *out.mutable_view()    = toProtoViewHint(t.view);
    return out;
}

// ---------------------------------------------------------------------------
// CaptureConfig  (hmi -> proto)
// ---------------------------------------------------------------------------
proto::CaptureConfig toProtoCaptureConfig(const hmi::CaptureConfig& cc)
{
    proto::CaptureConfig out;
    out.set_camera_id(cc.cameraId.toStdString());
    out.set_focus_distance_m(cc.focusDistanceM);
    out.set_fov_h_deg(cc.fovHDeg);
    out.set_fov_v_deg(cc.fovVDeg);
    out.set_max_tilt_from_normal_deg(cc.maxTiltFromNormalDeg);
    return out;
}

// ---------------------------------------------------------------------------
// PlanOptions  (hmi -> proto)
// ---------------------------------------------------------------------------
proto::PlanOptions toProtoPlanOptions(const hmi::PlanOptions& po)
{
    proto::PlanOptions out;
    out.set_candidate_radius_m(po.candidateRadiusM);
    out.set_candidate_yaw_step_deg(po.candidateYawStepDeg);
    out.set_enable_collision_check(po.enableCollisionCheck);
    out.set_enable_tsp_optimization(po.enableTspOptimization);
    out.set_ik_solver(po.ikSolver.toStdString());
    auto* w = out.mutable_weights();
    w->set_w_agv_distance(po.weights.wAgvDistance);
    w->set_w_joint_delta(po.weights.wJointDelta);
    w->set_w_manipulability(po.weights.wManipulability);
    w->set_w_view_error(po.weights.wViewError);
    w->set_w_joint_limit(po.weights.wJointLimit);
    return out;
}

hmi::PlanOptions fromProtoPlanOptions(const proto::PlanOptions& po)
{
    hmi::PlanOptions out;
    out.candidateRadiusM      = po.candidate_radius_m();
    out.candidateYawStepDeg   = po.candidate_yaw_step_deg();
    out.enableCollisionCheck  = po.enable_collision_check();
    out.enableTspOptimization = po.enable_tsp_optimization();
    out.ikSolver              = QString::fromStdString(po.ik_solver());
    if (po.has_weights()) {
        out.weights.wAgvDistance    = po.weights().w_agv_distance();
        out.weights.wJointDelta     = po.weights().w_joint_delta();
        out.weights.wManipulability = po.weights().w_manipulability();
        out.weights.wViewError      = po.weights().w_view_error();
        out.weights.wJointLimit     = po.weights().w_joint_limit();
    }
    return out;
}

// ---------------------------------------------------------------------------
// InspectionPoint / InspectionPath / PlanningStatistics
// ---------------------------------------------------------------------------
hmi::InspectionPoint fromProtoInspectionPoint(const proto::InspectionPoint& ip)
{
    hmi::InspectionPoint out;
    out.pointId        = ip.point_id();
    out.groupId        = QString::fromStdString(ip.group_id());
    if (ip.has_agv_pose())    { out.agvPose    = fromProtoPose2D(ip.agv_pose());    }
    if (ip.has_arm_pose())    { out.armPose    = fromProtoPose3D(ip.arm_pose());    }
    if (ip.has_tcp_pose_goal()){ out.tcpPoseGoal = fromProtoPose3D(ip.tcp_pose_goal()); }
    if (ip.has_camera_pose()) { out.cameraPose = fromProtoPose3D(ip.camera_pose()); }
    out.expectedQuality = ip.expected_quality();
    out.planningCost    = ip.planning_cost();
    out.cameraId        = QString::fromStdString(ip.camera_id());

    // arm_joint_goal is a repeated double; copy up to 6 values.
    const int nj = std::min(ip.arm_joint_goal_size(), 6);
    for (int i = 0; i < nj; ++i) {
        out.armJointGoal[static_cast<std::size_t>(i)] = ip.arm_joint_goal(i);
    }
    return out;
}

hmi::InspectionPath fromProtoInspectionPath(const proto::InspectionPath& path)
{
    hmi::InspectionPath out;
    out.totalPoints        = path.total_points();
    out.estimatedDistanceM = path.estimated_distance_m();
    out.estimatedDurationS = path.estimated_duration_s();
    out.waypoints.reserve(path.waypoints_size());
    for (const auto& wp : path.waypoints()) {
        out.waypoints.append(fromProtoInspectionPoint(wp));
    }
    return out;
}

hmi::PlanningStatistics fromProtoPlanningStats(const proto::PlanningStatistics& ps)
{
    hmi::PlanningStatistics out;
    out.candidatePoseCount     = ps.candidate_pose_count();
    out.ikSuccessCount         = ps.ik_success_count();
    out.collisionFilteredCount = ps.collision_filtered_count();
    out.planningTimeMs         = ps.planning_time_ms();
    return out;
}

// ---------------------------------------------------------------------------
// TaskPhase
// ---------------------------------------------------------------------------
hmi::TaskPhase fromProtoTaskPhase(proto::TaskPhase ph)
{
    switch (ph) {
    case proto::IDLE:        return hmi::TaskPhase::Idle;
    case proto::LOCALIZING:  return hmi::TaskPhase::Localizing;
    case proto::PLANNING:    return hmi::TaskPhase::Planning;
    case proto::EXECUTING:   return hmi::TaskPhase::Executing;
    case proto::PAUSED:      return hmi::TaskPhase::Paused;
    case proto::COMPLETED:   return hmi::TaskPhase::Completed;
    case proto::FAILED:      return hmi::TaskPhase::Failed;
    case proto::STOPPED:     return hmi::TaskPhase::Stopped;
    default:                 return hmi::TaskPhase::Unspecified;
    }
}

// ---------------------------------------------------------------------------
// AgvStatus / ArmStatus / TaskStatus
// ---------------------------------------------------------------------------
hmi::AgvStatus fromProtoAgvStatus(const proto::AgvStatus& a)
{
    hmi::AgvStatus out;
    out.connected           = a.connected();
    out.arrived             = a.arrived();
    out.moving              = a.moving();
    out.stopped             = a.stopped();
    if (a.has_current_pose()) { out.currentPose = fromProtoPose2D(a.current_pose()); }
    out.batteryPercent      = a.battery_percent();
    out.errorCode           = QString::fromStdString(a.error_code());
    out.linearVelocityMps   = a.linear_velocity_mps();
    out.angularVelocityRps  = a.angular_velocity_rps();
    if (a.has_goal_pose())    { out.goalPose = fromProtoPose2D(a.goal_pose()); }
    out.mapId               = QString::fromStdString(a.map_id());
    out.localizationQuality = a.localization_quality();
    return out;
}

hmi::ArmStatus fromProtoArmStatus(const proto::ArmStatus& a)
{
    hmi::ArmStatus out;
    out.connected      = a.connected();
    out.arrived        = a.arrived();
    out.moving         = a.moving();
    out.manipulability = a.manipulability();
    out.errorCode      = QString::fromStdString(a.error_code());
    out.servoEnabled   = a.servo_enabled();
    if (a.has_tcp_pose())  { out.tcpPose  = fromProtoPose3D(a.tcp_pose());  }
    if (a.has_base_pose()) { out.basePose = fromProtoPose3D(a.base_pose()); }

    const int nj = std::min(a.current_joints_size(), 6);
    for (int i = 0; i < nj; ++i) {
        out.currentJoints[static_cast<std::size_t>(i)] = a.current_joints(i);
    }
    return out;
}

hmi::TaskStatus fromProtoTaskStatus(const proto::TaskStatus& ts)
{
    hmi::TaskStatus out;
    out.taskId          = QString::fromStdString(ts.task_id());
    out.phase           = fromProtoTaskPhase(ts.phase());
    out.progressPercent = ts.progress_percent();
    out.currentAction   = QString::fromStdString(ts.current_action());
    out.errorMessage    = QString::fromStdString(ts.error_message());
    if (ts.has_agv())   { out.agv = fromProtoAgvStatus(ts.agv()); }
    if (ts.has_arm())   { out.arm = fromProtoArmStatus(ts.arm()); }
    if (ts.has_updated_at())  { out.updatedAt  = fromTimestamp(ts.updated_at());  }
    if (ts.has_started_at())  { out.startedAt  = fromTimestamp(ts.started_at());  }
    if (ts.has_finished_at()) { out.finishedAt = fromTimestamp(ts.finished_at()); }
    out.planId                  = QString::fromStdString(ts.plan_id());
    out.taskName                = QString::fromStdString(ts.task_name());
    out.currentWaypointIndex    = ts.current_waypoint_index();
    out.currentPointId          = ts.current_point_id();
    out.totalWaypoints          = ts.total_waypoints();
    out.interlockOk             = ts.interlock_ok();
    out.interlockMessage        = QString::fromStdString(ts.interlock_message());
    out.remainingTimeEstS       = ts.remaining_time_est_s();
    return out;
}

// ---------------------------------------------------------------------------
// InspectionEventType
// ---------------------------------------------------------------------------
hmi::InspectionEventType fromProtoEventType(proto::InspectionEventType et)
{
    switch (et) {
    case proto::INFO:         return hmi::InspectionEventType::Info;
    case proto::WARN:         return hmi::InspectionEventType::Warn;
    case proto::ERROR:        return hmi::InspectionEventType::Error;
    case proto::CAPTURED:     return hmi::InspectionEventType::Captured;
    case proto::DEFECT_FOUND: return hmi::InspectionEventType::DefectFound;
    default:                  return hmi::InspectionEventType::Unspecified;
    }
}

// ---------------------------------------------------------------------------
// InspectionEvent
// ---------------------------------------------------------------------------
hmi::InspectionEvent fromProtoInspectionEvent(const proto::InspectionEvent& ev)
{
    hmi::InspectionEvent out;
    out.taskId    = QString::fromStdString(ev.task_id());
    out.pointId   = ev.point_id();
    out.type      = fromProtoEventType(ev.type());
    out.message   = QString::fromStdString(ev.message());
    if (ev.has_defect())    { out.defect    = fromProtoDefectResult(ev.defect()); }
    if (ev.has_timestamp()) { out.timestamp = fromTimestamp(ev.timestamp()); }
    out.captureId = QString::fromStdString(ev.capture_id());
    out.cameraId  = QString::fromStdString(ev.camera_id());
    if (ev.has_image())      { out.image     = fromProtoImageRef(ev.image()); }
    if (ev.has_camera_pose()){ out.cameraPose = fromProtoPose3D(ev.camera_pose()); }
    for (const auto& d : ev.defects()) {
        out.defects.append(fromProtoDefectResult(d));
    }
    return out;
}

// ---------------------------------------------------------------------------
// CaptureRecord
// ---------------------------------------------------------------------------
hmi::CaptureRecord fromProtoCaptureRecord(const proto::CaptureRecord& cr)
{
    hmi::CaptureRecord out;
    out.taskId    = QString::fromStdString(cr.task_id());
    out.pointId   = cr.point_id();
    out.captureId = QString::fromStdString(cr.capture_id());
    out.cameraId  = QString::fromStdString(cr.camera_id());
    if (cr.has_image())      { out.image     = fromProtoImageRef(cr.image()); }
    if (cr.has_captured_at()){ out.capturedAt = fromTimestamp(cr.captured_at()); }
    for (const auto& d : cr.defects()) {
        out.defects.append(fromProtoDefectResult(d));
    }
    return out;
}

// ---------------------------------------------------------------------------
// NavMapInfo
// ---------------------------------------------------------------------------
hmi::NavMapInfo fromProtoNavMapInfo(const proto::NavMapInfo& nm)
{
    hmi::NavMapInfo out;
    out.mapId              = QString::fromStdString(nm.map_id());
    out.name               = QString::fromStdString(nm.name());
    out.resolutionMPerPixel = nm.resolution_m_per_pixel();
    out.width              = nm.width();
    out.height             = nm.height();
    if (nm.has_origin()) { out.origin = fromProtoPose2D(nm.origin()); }
    if (nm.has_image())  { out.image  = fromProtoImageRef(nm.image()); }
    if (nm.has_updated_at()) { out.updatedAt = fromTimestamp(nm.updated_at()); }
    return out;
}

// ---------------------------------------------------------------------------
// Deadline helper: absolute deadline N seconds from now.
// ---------------------------------------------------------------------------
std::chrono::system_clock::time_point deadlineFromNow(int seconds)
{
    return std::chrono::system_clock::now() + std::chrono::seconds(seconds);
}

} // anonymous namespace

// ===========================================================================
// GatewayClient – constructor / destructor
// ===========================================================================

GatewayClient::GatewayClient(const QString& address, QObject* parent)
    : QObject(parent)
{
    // Register Qt metatypes so they cross thread boundaries in queued signals.
    qRegisterMetaType<hmi::Result>();
    qRegisterMetaType<hmi::TaskStatus>();
    qRegisterMetaType<hmi::InspectionEvent>();
    qRegisterMetaType<hmi::PlanResponse>();
    qRegisterMetaType<hmi::GetPlanResponse>();
    qRegisterMetaType<hmi::NavMapInfo>();
    qRegisterMetaType<hmi::CaptureRecord>();
    qRegisterMetaType<QVector<hmi::CaptureRecord>>();
    qRegisterMetaType<QVector<hmi::DefectResult>>();
    qRegisterMetaType<QVector<hmi::InspectionTarget>>();

    if (!address.isEmpty()) {
        connectToGateway(address);
    }
}

GatewayClient::~GatewayClient()
{
    disconnectFromGateway();
}

// ===========================================================================
// Query helpers
// ===========================================================================

bool GatewayClient::isConnected() const noexcept
{
    return m_connected.load(std::memory_order_relaxed);
}

QString GatewayClient::currentAddress() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_address;
}

// ===========================================================================
// Connection management
// ===========================================================================

void GatewayClient::connectToGateway(const QString& address)
{
    // Tear down any existing connection first.
    disconnectFromGateway();

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_address = address;
        m_channel = grpc::CreateChannel(
            address.toStdString(),
            grpc::InsecureChannelCredentials());
        m_stub = proto::InspectionGateway::NewStub(m_channel);
    }

    startConnectionMonitor();
}

void GatewayClient::disconnectFromGateway()
{
    stopSubscriptions();
    stopConnectionMonitor();
    joinAllWorkers();

    std::lock_guard<std::mutex> lk(m_mutex);
    m_stub.reset();
    m_channel.reset();
    m_address.clear();

    if (m_connected.exchange(false)) {
        QMetaObject::invokeMethod(this, [this]() {
            emit connectionStateChanged(false);
        }, Qt::QueuedConnection);
    }
}

// ---------------------------------------------------------------------------
// stopSubscriptions – cancel all long-running streaming contexts.
// ---------------------------------------------------------------------------
void GatewayClient::stopSubscriptions()
{
    cancelAllContexts();

    if (m_sysStateThread.joinable())  { m_sysStateThread.join(); }
    if (m_eventsThread.joinable())    { m_eventsThread.join(); }
    if (m_downloadThread.joinable())  { m_downloadThread.join(); }
}

// ---------------------------------------------------------------------------
// Internal: cancelAllContexts
// ---------------------------------------------------------------------------
void GatewayClient::cancelAllContexts()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_sysStateCtx)  { m_sysStateCtx->TryCancel(); }
    if (m_eventsCtx)    { m_eventsCtx->TryCancel(); }
    if (m_downloadCtx)  { m_downloadCtx->TryCancel(); }
}

// ---------------------------------------------------------------------------
// Internal: joinAllWorkers  – wait for all one-shot RPC threads.
// ---------------------------------------------------------------------------
void GatewayClient::joinAllWorkers()
{
    std::vector<std::thread> workers;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        workers = std::move(m_workers);
    }
    for (auto& t : workers) {
        if (t.joinable()) { t.join(); }
    }
}

// ---------------------------------------------------------------------------
// Internal: connection state monitor thread.
//
// Polls the gRPC channel state every 500 ms and emits connectionStateChanged
// when it transitions between READY and anything else.
// ---------------------------------------------------------------------------
void GatewayClient::startConnectionMonitor()
{
    m_stopMonitor.store(false);
    m_connMonitorThread = std::thread([this]() {
        bool lastReady = false;
        while (!m_stopMonitor.load(std::memory_order_relaxed)) {
            std::shared_ptr<grpc::Channel> ch;
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                ch = m_channel;
            }
            if (!ch) { break; }

            grpc_connectivity_state state = ch->GetState(/*try_to_connect=*/true);
            bool nowReady = (state == GRPC_CHANNEL_READY);

            if (nowReady != lastReady) {
                lastReady = nowReady;
                m_connected.store(nowReady, std::memory_order_relaxed);
                QMetaObject::invokeMethod(this, [this, nowReady]() {
                    emit connectionStateChanged(nowReady);
                }, Qt::QueuedConnection);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
}

void GatewayClient::stopConnectionMonitor()
{
    m_stopMonitor.store(true);
    if (m_connMonitorThread.joinable()) {
        m_connMonitorThread.join();
    }
}

// ===========================================================================
// Helpers: spawn a one-shot worker thread and track it.
// ===========================================================================
namespace {

// Clean up finished workers from the vector (best-effort, non-blocking).
void reapFinishedWorkers(std::vector<std::thread>& workers)
{
    workers.erase(
        std::remove_if(workers.begin(), workers.end(),
            [](std::thread& t) -> bool {
                // If the thread has no OS thread yet, it is default-constructed
                // (already joined/detached).
                return !t.joinable();
            }),
        workers.end());
}

} // anonymous namespace

// ===========================================================================
// RPC – UploadCad (client-streaming)
// ===========================================================================

void GatewayClient::uploadCad(const QString& filePath)
{
    std::shared_ptr<grpc::Channel> ch;
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        ch   = m_channel;
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r;
        r.code    = hmi::ErrorCode::Unavailable;
        r.message = QStringLiteral("Not connected");
        QMetaObject::invokeMethod(this, [this, r]() {
            emit uploadCadFinished(r, {});
        }, Qt::QueuedConnection);
        return;
    }

    // Capture what we need for the thread by value.
    QString path = filePath;

    std::thread worker([this, path, stub]() {
        // Build upload session id.
        const std::string uploadId =
            QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        const std::string filename =
            QFileInfo(path).fileName().toStdString();

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(300)); // 5-minute max upload

        proto::UploadCadResponse response;
        auto writer = stub->UploadCad(&ctx, &response);

        // Open the file.
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            hmi::Result r;
            r.code    = hmi::ErrorCode::InvalidArgument;
            r.message = QStringLiteral("Cannot open file: ") + path;
            QMetaObject::invokeMethod(this, [this, r]() {
                emit uploadCadFinished(r, {});
            }, Qt::QueuedConnection);
            return;
        }

        constexpr qint64 CHUNK_SIZE = 64 * 1024; // 64 KB
        const qint64 totalBytes = file.size();
        qint64 bytesSent = 0;
        uint32_t chunkIndex = 0;
        int lastPercent = -1;

        while (!file.atEnd()) {
            QByteArray buf = file.read(CHUNK_SIZE);
            if (buf.isEmpty()) { break; }

            proto::UploadCadChunk chunk;
            chunk.set_upload_id(uploadId);
            chunk.set_filename(filename);
            chunk.set_data(buf.constData(), static_cast<std::size_t>(buf.size()));
            chunk.set_chunk_index(chunkIndex++);

            bytesSent += buf.size();
            const bool isLast = file.atEnd();
            chunk.set_eof(isLast);

            if (!writer->Write(chunk)) {
                // Stream broken.
                hmi::Result r;
                r.code    = hmi::ErrorCode::Internal;
                r.message = QStringLiteral("Stream write failed at chunk %1").arg(chunkIndex - 1);
                QMetaObject::invokeMethod(this, [this, r]() {
                    emit uploadCadFinished(r, {});
                }, Qt::QueuedConnection);
                return;
            }

            // Progress notification.
            if (totalBytes > 0) {
                int pct = static_cast<int>((bytesSent * 100) / totalBytes);
                if (pct != lastPercent) {
                    lastPercent = pct;
                    QMetaObject::invokeMethod(this, [this, pct]() {
                        emit uploadCadProgress(pct);
                    }, Qt::QueuedConnection);
                }
            }
        }

        writer->WritesDone();
        Status st = writer->Finish();

        hmi::Result r;
        QString modelId;
        if (st.ok()) {
            r       = fromProtoResult(response.result());
            modelId = QString::fromStdString(response.model_id());
        } else {
            r = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(this, [this, r, modelId]() {
            emit uploadCadFinished(r, modelId);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPC – SetInspectionTargets (unary)
// ===========================================================================

void GatewayClient::setInspectionTargets(
    const QString& modelId,
    const QVector<hmi::InspectionTarget>& targets,
    const hmi::CaptureConfig& config,
    const QString& operatorId)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r{ hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, r]() {
            emit setTargetsFinished(r, 0);
        }, Qt::QueuedConnection);
        return;
    }

    std::thread worker([this, stub, modelId, targets, config, operatorId]() {
        proto::SetInspectionTargetsRequest req;
        req.set_model_id(modelId.toStdString());
        req.set_operator_id(operatorId.toStdString());
        *req.mutable_capture() = toProtoCaptureConfig(config);
        for (const auto& t : targets) {
            *req.add_targets() = toProtoTarget(t);
        }

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(60));

        proto::SetInspectionTargetsResponse resp;
        Status st = stub->SetInspectionTargets(&ctx, req, &resp);

        hmi::Result r;
        uint32_t total = 0;
        if (st.ok()) {
            r     = fromProtoResult(resp.result());
            total = resp.total_targets();
        } else {
            r = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(this, [this, r, total]() {
            emit setTargetsFinished(r, total);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPC – PlanInspection (unary)
// ===========================================================================

void GatewayClient::planInspection(const QString& modelId,
                                   const QString& taskName,
                                   const hmi::PlanOptions& options)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::PlanResponse resp;
        resp.result = { hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, resp]() {
            emit planInspectionFinished(resp);
        }, Qt::QueuedConnection);
        return;
    }

    std::thread worker([this, stub, modelId, taskName, options]() {
        proto::PlanInspectionRequest req;
        req.set_model_id(modelId.toStdString());
        req.set_task_name(taskName.toStdString());
        *req.mutable_options() = toProtoPlanOptions(options);

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(120)); // planning can take a while

        proto::PlanInspectionResponse resp;
        Status st = stub->PlanInspection(&ctx, req, &resp);

        hmi::PlanResponse out;
        if (st.ok()) {
            out.result = fromProtoResult(resp.result());
            out.planId = QString::fromStdString(resp.plan_id());
            if (resp.has_path())  { out.path  = fromProtoInspectionPath(resp.path()); }
            if (resp.has_stats()) { out.stats = fromProtoPlanningStats(resp.stats()); }
        } else {
            out.result = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(this, [this, out]() {
            emit planInspectionFinished(out);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPC – GetPlan (unary)
// ===========================================================================

void GatewayClient::getPlan(const QString& planId)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::GetPlanResponse r;
        r.result = { hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, r]() {
            emit getPlanFinished(r);
        }, Qt::QueuedConnection);
        return;
    }

    std::thread worker([this, stub, planId]() {
        proto::GetPlanRequest req;
        req.set_plan_id(planId.toStdString());

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(30));

        proto::GetPlanResponse resp;
        Status st = stub->GetPlan(&ctx, req, &resp);

        hmi::GetPlanResponse out;
        if (st.ok()) {
            out.result    = fromProtoResult(resp.result());
            out.planId    = QString::fromStdString(resp.plan_id());
            out.modelId   = QString::fromStdString(resp.model_id());
            out.taskName  = QString::fromStdString(resp.task_name());
            if (resp.has_options())  { out.options = fromProtoPlanOptions(resp.options()); }
            if (resp.has_path())     { out.path    = fromProtoInspectionPath(resp.path()); }
            if (resp.has_stats())    { out.stats   = fromProtoPlanningStats(resp.stats()); }
            if (resp.has_created_at()){ out.createdAt = fromTimestamp(resp.created_at()); }
        } else {
            out.result = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(this, [this, out]() {
            emit getPlanFinished(out);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPC – StartInspection (unary)
// ===========================================================================

void GatewayClient::startInspection(const QString& planId, bool dryRun)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r{ hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, r]() {
            emit startInspectionFinished(r, {});
        }, Qt::QueuedConnection);
        return;
    }

    std::thread worker([this, stub, planId, dryRun]() {
        proto::StartInspectionRequest req;
        req.set_plan_id(planId.toStdString());
        req.set_dry_run(dryRun);

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(30));

        proto::StartInspectionResponse resp;
        Status st = stub->StartInspection(&ctx, req, &resp);

        hmi::Result r;
        QString taskId;
        if (st.ok()) {
            r      = fromProtoResult(resp.result());
            taskId = QString::fromStdString(resp.task_id());
        } else {
            r = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(this, [this, r, taskId]() {
            emit startInspectionFinished(r, taskId);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPCs – PauseInspection / ResumeInspection / StopInspection (unary, shared)
// ===========================================================================

// Internal helper to reduce code duplication for the three control RPCs.
namespace {

using ControlRpcFn = std::function<Status(ClientContext*,
                                          const proto::ControlTaskRequest&,
                                          proto::ControlTaskResponse*)>;

void runControlRpc(GatewayClient* self,
                   proto::InspectionGateway::Stub* stub,
                   const QString& taskId,
                   const QString& reason,
                   ControlRpcFn fn)
{
    std::thread worker([self, stub, taskId, reason, fn = std::move(fn)]() {
        proto::ControlTaskRequest req;
        req.set_task_id(taskId.toStdString());
        req.set_reason(reason.toStdString());

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(30));

        proto::ControlTaskResponse resp;
        Status st = fn(&ctx, req, &resp);

        hmi::Result r;
        if (st.ok()) {
            r = fromProtoResult(resp.result());
        } else {
            r = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(self, [self, r]() {
            emit self->controlTaskFinished(r);
        }, Qt::QueuedConnection);
    });

    // Note: we cannot access m_workers here (different translation unit scope
    // for the anonymous namespace).  Detach; the task is short-lived.
    worker.detach();
}

} // anonymous namespace

void GatewayClient::pauseInspection(const QString& taskId, const QString& reason)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r{ hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, r]() { emit controlTaskFinished(r); },
                                  Qt::QueuedConnection);
        return;
    }
    runControlRpc(this, stub, taskId, reason,
        [stub](ClientContext* ctx,
               const proto::ControlTaskRequest& req,
               proto::ControlTaskResponse* resp) {
            return stub->PauseInspection(ctx, req, resp);
        });
}

void GatewayClient::resumeInspection(const QString& taskId, const QString& reason)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r{ hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, r]() { emit controlTaskFinished(r); },
                                  Qt::QueuedConnection);
        return;
    }
    runControlRpc(this, stub, taskId, reason,
        [stub](ClientContext* ctx,
               const proto::ControlTaskRequest& req,
               proto::ControlTaskResponse* resp) {
            return stub->ResumeInspection(ctx, req, resp);
        });
}

void GatewayClient::stopInspection(const QString& taskId, const QString& reason)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r{ hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, r]() { emit controlTaskFinished(r); },
                                  Qt::QueuedConnection);
        return;
    }
    runControlRpc(this, stub, taskId, reason,
        [stub](ClientContext* ctx,
               const proto::ControlTaskRequest& req,
               proto::ControlTaskResponse* resp) {
            return stub->StopInspection(ctx, req, resp);
        });
}

// ===========================================================================
// RPC – GetTaskStatus (unary)
// ===========================================================================

void GatewayClient::getTaskStatus(const QString& taskId)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("GetTaskStatus: not connected"));
        }, Qt::QueuedConnection);
        return;
    }

    std::thread worker([this, stub, taskId]() {
        proto::GetTaskStatusRequest req;
        req.set_task_id(taskId.toStdString());

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(15));

        proto::GetTaskStatusResponse resp;
        Status st = stub->GetTaskStatus(&ctx, req, &resp);

        if (!st.ok()) {
            QString err = QString::fromStdString(st.error_message());
            QMetaObject::invokeMethod(this, [this, err]() {
                emit errorOccurred(QStringLiteral("GetTaskStatus: ") + err);
            }, Qt::QueuedConnection);
            return;
        }

        hmi::TaskStatus ts = fromProtoTaskStatus(resp.status());
        QMetaObject::invokeMethod(this, [this, ts]() {
            emit taskStatusReceived(ts);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPC – SubscribeSystemState (server-streaming)
// ===========================================================================

void GatewayClient::subscribeSystemState(const QString& taskId)
{
    // Cancel any existing subscription.
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_sysStateCtx) {
            m_sysStateCtx->TryCancel();
        }
    }
    if (m_sysStateThread.joinable()) {
        m_sysStateThread.join();
    }

    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
        m_sysStateCtx = std::make_unique<ClientContext>();
    }
    if (!stub) {
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("SubscribeSystemState: not connected"));
        }, Qt::QueuedConnection);
        return;
    }

    m_sysStateThread = std::thread([this, stub, taskId]() {
        proto::SubscribeRequest req;
        req.set_task_id(taskId.toStdString());
        req.set_include_snapshot(true);

        ClientContext* ctx = nullptr;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            ctx = m_sysStateCtx.get();
        }
        if (!ctx) { return; }

        auto reader = stub->SubscribeSystemState(ctx, req);

        proto::SystemStateEvent ev;
        while (reader->Read(&ev)) {
            hmi::TaskStatus ts = fromProtoTaskStatus(ev.status());
            QMetaObject::invokeMethod(this, [this, ts]() {
                emit systemStateReceived(ts);
            }, Qt::QueuedConnection);
        }

        // Stream ended (cancelled, server closed, or error).
        Status st = reader->Finish();
        if (!st.ok() &&
            st.error_code() != grpc::StatusCode::CANCELLED)
        {
            QString err = QString::fromStdString(st.error_message());
            QMetaObject::invokeMethod(this, [this, err]() {
                emit errorOccurred(QStringLiteral("SubscribeSystemState ended: ") + err);
            }, Qt::QueuedConnection);
        }
    });
}

// ===========================================================================
// RPC – SubscribeInspectionEvents (server-streaming)
// ===========================================================================

void GatewayClient::subscribeInspectionEvents(const QString& taskId)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_eventsCtx) {
            m_eventsCtx->TryCancel();
        }
    }
    if (m_eventsThread.joinable()) {
        m_eventsThread.join();
    }

    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
        m_eventsCtx = std::make_unique<ClientContext>();
    }
    if (!stub) {
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("SubscribeInspectionEvents: not connected"));
        }, Qt::QueuedConnection);
        return;
    }

    m_eventsThread = std::thread([this, stub, taskId]() {
        proto::SubscribeRequest req;
        req.set_task_id(taskId.toStdString());
        req.set_include_snapshot(true);

        ClientContext* ctx = nullptr;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            ctx = m_eventsCtx.get();
        }
        if (!ctx) { return; }

        auto reader = stub->SubscribeInspectionEvents(ctx, req);

        proto::InspectionEvent ev;
        while (reader->Read(&ev)) {
            hmi::InspectionEvent out = fromProtoInspectionEvent(ev);
            QMetaObject::invokeMethod(this, [this, out]() {
                emit inspectionEventReceived(out);
            }, Qt::QueuedConnection);
        }

        Status st = reader->Finish();
        if (!st.ok() &&
            st.error_code() != grpc::StatusCode::CANCELLED)
        {
            QString err = QString::fromStdString(st.error_message());
            QMetaObject::invokeMethod(this, [this, err]() {
                emit errorOccurred(QStringLiteral("SubscribeInspectionEvents ended: ") + err);
            }, Qt::QueuedConnection);
        }
    });
}

// ===========================================================================
// RPC – GetNavMap (unary)
// ===========================================================================

void GatewayClient::getNavMap(const QString& mapId)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r{ hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        hmi::NavMapInfo empty;
        QMetaObject::invokeMethod(this, [this, r, empty]() {
            emit navMapReceived(r, empty);
        }, Qt::QueuedConnection);
        return;
    }

    std::thread worker([this, stub, mapId]() {
        proto::GetNavMapRequest req;
        req.set_map_id(mapId.toStdString());
        req.set_include_image_thumbnail(true);

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(30));

        proto::GetNavMapResponse resp;
        Status st = stub->GetNavMap(&ctx, req, &resp);

        hmi::Result r;
        hmi::NavMapInfo info;
        if (st.ok()) {
            r    = fromProtoResult(resp.result());
            if (resp.has_map()) {
                info = fromProtoNavMapInfo(resp.map());
            }
        } else {
            r = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(this, [this, r, info]() {
            emit navMapReceived(r, info);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPC – ListCaptures (unary)
// ===========================================================================

void GatewayClient::listCaptures(const QString& taskId, int32_t pointId)
{
    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
    }
    if (!stub) {
        hmi::Result r{ hmi::ErrorCode::Unavailable, QStringLiteral("Not connected") };
        QMetaObject::invokeMethod(this, [this, r]() {
            emit capturesReceived(r, {});
        }, Qt::QueuedConnection);
        return;
    }

    std::thread worker([this, stub, taskId, pointId]() {
        proto::ListCapturesRequest req;
        req.set_task_id(taskId.toStdString());
        req.set_point_id(pointId);
        req.set_include_thumbnails(true);

        ClientContext ctx;
        ctx.set_deadline(deadlineFromNow(30));

        proto::ListCapturesResponse resp;
        Status st = stub->ListCaptures(&ctx, req, &resp);

        hmi::Result r;
        QVector<hmi::CaptureRecord> records;
        if (st.ok()) {
            r = fromProtoResult(resp.result());
            records.reserve(resp.captures_size());
            for (const auto& cr : resp.captures()) {
                records.append(fromProtoCaptureRecord(cr));
            }
        } else {
            r = fromGrpcStatus(st);
        }

        QMetaObject::invokeMethod(this, [this, r, records]() {
            emit capturesReceived(r, records);
        }, Qt::QueuedConnection);
    });

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        reapFinishedWorkers(m_workers);
        m_workers.push_back(std::move(worker));
    }
}

// ===========================================================================
// RPC – DownloadMedia (server-streaming)
//
// Reassembles all chunks in-order into a single QByteArray, then emits
// mediaDownloaded once the stream closes with eof on the final chunk.
// ===========================================================================

void GatewayClient::downloadMedia(const QString& mediaId)
{
    // Cancel any existing download.
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_downloadCtx) {
            m_downloadCtx->TryCancel();
        }
    }
    if (m_downloadThread.joinable()) {
        m_downloadThread.join();
    }

    proto::InspectionGateway::Stub* stub = nullptr;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        stub = m_stub.get();
        m_downloadCtx = std::make_unique<ClientContext>();
    }
    if (!stub) {
        QMetaObject::invokeMethod(this, [this]() {
            emit errorOccurred(QStringLiteral("DownloadMedia: not connected"));
        }, Qt::QueuedConnection);
        return;
    }

    m_downloadThread = std::thread([this, stub, mediaId]() {
        proto::DownloadMediaRequest req;
        req.set_media_id(mediaId.toStdString());

        ClientContext* ctx = nullptr;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            ctx = m_downloadCtx.get();
        }
        if (!ctx) { return; }

        auto reader = stub->DownloadMedia(ctx, req);

        QByteArray assembled;
        proto::MediaChunk chunk;
        while (reader->Read(&chunk)) {
            const auto& data = chunk.data();
            assembled.append(data.data(), static_cast<int>(data.size()));
        }

        Status st = reader->Finish();
        if (st.ok()) {
            QMetaObject::invokeMethod(this, [this, mediaId, assembled]() {
                emit mediaDownloaded(mediaId, assembled);
            }, Qt::QueuedConnection);
        } else if (st.error_code() != grpc::StatusCode::CANCELLED) {
            QString err = QString::fromStdString(st.error_message());
            QMetaObject::invokeMethod(this, [this, err]() {
                emit errorOccurred(QStringLiteral("DownloadMedia failed: ") + err);
            }, Qt::QueuedConnection);
        }
    });
}

} // namespace hmi
