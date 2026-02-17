// src/ui/operator/ResultPanel.h
//
// ResultPanel – shows capture results, defect thumbnails, and event timeline.

#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QLabel>
#include <QMap>
#include <QPixmap>
#include <QByteArray>

#include "core/Types.h"

/// \brief Displays inspection results: capture thumbnails and event timeline.
///
/// Two tabs:
///  - Gallery: thumbnail grid showing all captures with defect bounding boxes overlaid
///  - Timeline: chronological list of all inspection events with icons/colors
///
/// Clicking a thumbnail shows a detail view with defect information.
class ResultPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ResultPanel(QWidget* parent = nullptr);

    /// Add a capture event (Captured or DefectFound) to the gallery.
    void addCaptureEvent(const hmi::InspectionEvent& event);

    /// Batch-set all capture records (e.g., from GetCaptureRecords RPC).
    void setCaptureRecords(const QVector<hmi::CaptureRecord>& records);

    /// Append an event to the timeline.
    void addEvent(const hmi::InspectionEvent& event);

    /// Clear all captures and events.
    void clear();

    /// When a full image is downloaded, update the detail view.
    void setFullImage(const QString& captureId, const QByteArray& imageData);

signals:
    /// Emitted when the user clicks a thumbnail to request the full image download.
    void downloadImageRequested(const QString& mediaId);

    /// Emitted when a capture is selected in the gallery.
    void captureSelected(const QString& captureId);

private:
    void setupUi();
    QWidget* createGalleryTab();
    QWidget* createTimelineTab();

    /// Render a thumbnail with red bounding boxes for defects.
    QPixmap renderThumbnailWithDefects(const QByteArray& jpegData,
                                       const QVector<hmi::DefectResult>& defects,
                                       int imgW, int imgH);

    /// Get event type icon/text/color
    static QString eventTypeIcon(hmi::InspectionEventType type);
    static QString eventTypeColor(hmi::InspectionEventType type);

    QTabWidget*   m_tabs         = nullptr;

    // Gallery tab
    QListWidget*  m_thumbnailList = nullptr;

    // Timeline tab
    QListWidget*  m_eventTimeline = nullptr;

    // Detail view (shared between tabs)
    QLabel*       m_detailImage      = nullptr;
    QLabel*       m_defectInfoLabel  = nullptr;

    struct CaptureInfo {
        QString captureId;
        int32_t pointId = 0;
        QVector<hmi::DefectResult> defects;
        QPixmap thumbnail;
        QPixmap fullImage;
    };

    QMap<QString, CaptureInfo> m_captures;
};
