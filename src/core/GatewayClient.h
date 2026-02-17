// src/core/GatewayClient.h
//
// GatewayClient is a QObject that wraps the gRPC InspectionGateway stub and
// exposes a completely Qt-friendly async API (signals / slots) to the rest of
// the HMI.
//
// Design notes
// ------------
// * Unary RPCs execute on a dedicated std::thread per call so the Qt main
//   thread is never blocked.  Signals are emitted back to the main thread via
//   QMetaObject::invokeMethod with Qt::QueuedConnection.
//
// * Server-streaming RPCs (SubscribeSystemState, SubscribeInspectionEvents,
//   DownloadMedia) each run their Read loop on a dedicated std::thread.  The
//   associated grpc::ClientContext is stored in a member that can be cancelled
//   via stopSubscriptions() / disconnectFromGateway().
//
// * Client-streaming RPC (UploadCad) reads the given file in 64 KB chunks on
//   a worker thread, streams them to the server, and emits uploadCadProgress
//   periodically followed by uploadCadFinished.
//
// * Connection state is polled on a separate thread and surfaced through the
//   connectionStateChanged signal.
//
// Thread safety
// -------------
// All public slots may be called from any thread; internally they post work
// to worker threads.  Member variables that are shared between threads are
// protected by m_mutex.

#pragma once

#include "Types.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// gRPC headers required for member declarations.
#include <grpcpp/grpcpp.h>
#include "inspection_gateway.grpc.pb.h"

namespace hmi {

class GatewayClient : public QObject {
    Q_OBJECT

public:
    /// Construct with an optional initial gateway address.
    /// Call connectToGateway() to (re-)connect at any time.
    explicit GatewayClient(const QString& address = {}, QObject* parent = nullptr);
    ~GatewayClient() override;

    // Non-copyable, non-movable (QObject semantics).
    GatewayClient(const GatewayClient&)            = delete;
    GatewayClient& operator=(const GatewayClient&) = delete;
    GatewayClient(GatewayClient&&)                 = delete;
    GatewayClient& operator=(GatewayClient&&)      = delete;

    // -----------------------------------------------------------------------
    // Query helpers
    // -----------------------------------------------------------------------
    [[nodiscard]] bool isConnected() const noexcept;
    [[nodiscard]] QString currentAddress() const;

signals:
    // -----------------------------------------------------------------------
    // Signals – emitted on the Qt main thread (QueuedConnection from workers)
    // -----------------------------------------------------------------------

    // Connection lifecycle
    void connectionStateChanged(bool connected);
    void errorOccurred(QString error);

    // UploadCad (client-streaming)
    void uploadCadProgress(int percent);
    void uploadCadFinished(hmi::Result result, QString modelId);

    // SetInspectionTargets
    void setTargetsFinished(hmi::Result result, uint32_t totalTargets);

    // PlanInspection
    void planInspectionFinished(hmi::PlanResponse response);

    // GetPlan
    void getPlanFinished(hmi::GetPlanResponse response);

    // StartInspection
    void startInspectionFinished(hmi::Result result, QString taskId);

    // PauseInspection / ResumeInspection / StopInspection
    void controlTaskFinished(hmi::Result result);

    // GetTaskStatus
    void taskStatusReceived(hmi::TaskStatus status);

    // SubscribeSystemState (server-streaming)
    void systemStateReceived(hmi::TaskStatus status);

    // SubscribeInspectionEvents (server-streaming)
    void inspectionEventReceived(hmi::InspectionEvent event);

    // GetNavMap
    void navMapReceived(hmi::Result result, hmi::NavMapInfo mapInfo);

    // ListCaptures
    void capturesReceived(hmi::Result result, QVector<hmi::CaptureRecord> captures);

    // DownloadMedia (server-streaming reassembled)
    void mediaDownloaded(QString mediaId, QByteArray data);

public slots:
    // -----------------------------------------------------------------------
    // Connection management
    // -----------------------------------------------------------------------
    void connectToGateway(const QString& address);
    void disconnectFromGateway();

    // -----------------------------------------------------------------------
    // RPCs
    // -----------------------------------------------------------------------

    /// Upload a CAD file to the gateway.  Reads filePath in 64 KB chunks.
    void uploadCad(const QString& filePath);

    /// Set the list of inspection targets for modelId.
    void setInspectionTargets(const QString& modelId,
                              const QVector<hmi::InspectionTarget>& targets,
                              const hmi::CaptureConfig& config,
                              const QString& operatorId);

    /// Ask the planner to generate an inspection path.
    void planInspection(const QString& modelId,
                        const QString& taskName,
                        const hmi::PlanOptions& options);

    /// Retrieve a previously generated plan by ID.
    void getPlan(const QString& planId);

    /// Start executing an inspection plan.
    void startInspection(const QString& planId, bool dryRun = false);

    /// Pause the running task.
    void pauseInspection(const QString& taskId, const QString& reason = {});

    /// Resume a paused task.
    void resumeInspection(const QString& taskId, const QString& reason = {});

    /// Stop and cancel the task.
    void stopInspection(const QString& taskId, const QString& reason = {});

    /// One-shot poll of task status.
    void getTaskStatus(const QString& taskId);

    /// Start a server-streaming subscription to system state updates.
    /// taskId empty → all tasks.
    void subscribeSystemState(const QString& taskId = {});

    /// Start a server-streaming subscription to inspection events.
    /// taskId empty → all tasks.
    void subscribeInspectionEvents(const QString& taskId = {});

    /// Retrieve navigation map info (and optional image thumbnail).
    void getNavMap(const QString& mapId = {});

    /// List all capture records for a task.  pointId == 0 → all points.
    void listCaptures(const QString& taskId, int32_t pointId = 0);

    /// Download a binary media blob by ID.  Reassembles all chunks then emits
    /// mediaDownloaded with the full payload.
    void downloadMedia(const QString& mediaId);

    /// Cancel all active streaming subscriptions (system-state, events,
    /// download).  Does NOT disconnect the channel.
    void stopSubscriptions();

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------
    void startConnectionMonitor();
    void stopConnectionMonitor();
    void joinAllWorkers();
    void cancelAllContexts();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex m_mutex;

    QString m_address;
    std::shared_ptr<grpc::Channel> m_channel;

    // The stub is created once per channel; access under m_mutex.
    std::unique_ptr<inspection::gateway::v1::InspectionGateway::Stub> m_stub;

    // -----------------------------------------------------------------------
    // Worker threads
    // -----------------------------------------------------------------------
    std::vector<std::thread> m_workers;   ///< One-shot RPC threads.

    // Long-lived streaming threads.
    std::thread m_sysStateThread;
    std::thread m_eventsThread;
    std::thread m_downloadThread;
    std::thread m_connMonitorThread;

    // -----------------------------------------------------------------------
    // Cancellation tokens for streaming contexts
    // -----------------------------------------------------------------------
    std::unique_ptr<grpc::ClientContext> m_sysStateCtx;
    std::unique_ptr<grpc::ClientContext> m_eventsCtx;
    std::unique_ptr<grpc::ClientContext> m_downloadCtx;

    // -----------------------------------------------------------------------
    // Flags
    // -----------------------------------------------------------------------
    std::atomic<bool> m_stopMonitor{false};
    std::atomic<bool> m_connected{false};
};

} // namespace hmi
