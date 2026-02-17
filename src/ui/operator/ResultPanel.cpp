// src/ui/operator/ResultPanel.cpp

#include "ResultPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QPainter>
#include <QDateTime>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ResultPanel::ResultPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

// ---------------------------------------------------------------------------
// UI setup
// ---------------------------------------------------------------------------

void ResultPanel::setupUi()
{
    QVBoxLayout* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createGalleryTab(), QStringLiteral("抓拍结果"));
    m_tabs->addTab(createTimelineTab(), QStringLiteral("事件时间轴"));
    vlay->addWidget(m_tabs);
}

QWidget* ResultPanel::createGalleryTab()
{
    QWidget* tab = new QWidget(this);
    QHBoxLayout* hlay = new QHBoxLayout(tab);
    hlay->setContentsMargins(4, 4, 4, 4);
    hlay->setSpacing(4);

    // Left: thumbnail list (icon mode)
    m_thumbnailList = new QListWidget(tab);
    m_thumbnailList->setViewMode(QListWidget::IconMode);
    m_thumbnailList->setIconSize(QSize(120, 90));
    m_thumbnailList->setResizeMode(QListWidget::Adjust);
    m_thumbnailList->setSpacing(8);
    m_thumbnailList->setUniformItemSizes(false);
    hlay->addWidget(m_thumbnailList, 1);

    // Right: detail view
    QWidget* detailPane = new QWidget(tab);
    QVBoxLayout* detailLayout = new QVBoxLayout(detailPane);
    detailLayout->setContentsMargins(4, 4, 4, 4);

    QLabel* detailTitle = new QLabel(QStringLiteral("详细信息"), detailPane);
    QFont titleFont = detailTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    detailTitle->setFont(titleFont);
    detailLayout->addWidget(detailTitle);

    m_detailImage = new QLabel(detailPane);
    m_detailImage->setAlignment(Qt::AlignCenter);
    m_detailImage->setMinimumSize(200, 150);
    m_detailImage->setScaledContents(false);
    m_detailImage->setStyleSheet(QStringLiteral(
        "QLabel { background-color: #2c3e50; border: 1px solid #bdc3c7; }"
    ));
    detailLayout->addWidget(m_detailImage, 1);

    m_defectInfoLabel = new QLabel(QStringLiteral("点击缩略图查看详细信息"), detailPane);
    m_defectInfoLabel->setWordWrap(true);
    m_defectInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailLayout->addWidget(m_defectInfoLabel, 0);

    hlay->addWidget(detailPane, 0);

    // Connect thumbnail click to detail view
    connect(m_thumbnailList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString captureId = item->data(Qt::UserRole).toString();
        if (!m_captures.contains(captureId)) {
            return;
        }

        const CaptureInfo& info = m_captures[captureId];
        emit captureSelected(captureId);

        // Show full image if available, otherwise thumbnail
        QPixmap displayImage = info.fullImage.isNull() ? info.thumbnail : info.fullImage;
        if (!displayImage.isNull()) {
            // Scale to fit label while keeping aspect ratio
            QPixmap scaled = displayImage.scaled(
                m_detailImage->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
            m_detailImage->setPixmap(scaled);
        }

        // Show defect info
        QString defectText;
        if (info.defects.isEmpty()) {
            defectText = QStringLiteral("点位 %1\n未发现缺陷").arg(info.pointId);
        } else {
            defectText = QStringLiteral("点位 %1\n发现 %2 个缺陷:\n")
                .arg(info.pointId)
                .arg(info.defects.size());
            for (int i = 0; i < info.defects.size(); ++i) {
                const auto& defect = info.defects[i];
                defectText += QStringLiteral("\n%1. 类型: %2, 置信度: %3%")
                    .arg(i + 1)
                    .arg(defect.defectType)
                    .arg(static_cast<int>(defect.confidence * 100));
            }
        }
        m_defectInfoLabel->setText(defectText);

        // If full image not yet loaded, request download
        if (info.fullImage.isNull() && !info.captureId.isEmpty()) {
            // Assume the media ID is embedded in the event; for now we'll just use captureId
            // In a real implementation, you'd extract the mediaId from the ImageRef
            // emit downloadImageRequested(info.image.media.mediaId);
        }
    });

    return tab;
}

QWidget* ResultPanel::createTimelineTab()
{
    QWidget* tab = new QWidget(this);
    QVBoxLayout* vlay = new QVBoxLayout(tab);
    vlay->setContentsMargins(4, 4, 4, 4);

    m_eventTimeline = new QListWidget(tab);
    m_eventTimeline->setAlternatingRowColors(true);
    vlay->addWidget(m_eventTimeline);

    return tab;
}

// ---------------------------------------------------------------------------
// Data updates
// ---------------------------------------------------------------------------

void ResultPanel::addCaptureEvent(const hmi::InspectionEvent& event)
{
    if (event.captureId.isEmpty()) {
        return;
    }

    // Create or update capture info
    CaptureInfo info;
    info.captureId = event.captureId;
    info.pointId = event.pointId;
    info.defects = event.defects;

    // Render thumbnail with defects
    if (!event.image.thumbnailJpeg.isEmpty()) {
        info.thumbnail = renderThumbnailWithDefects(
            event.image.thumbnailJpeg,
            event.defects,
            static_cast<int>(event.image.width),
            static_cast<int>(event.image.height)
        );
    }

    m_captures[event.captureId] = info;

    // Add to thumbnail list
    QListWidgetItem* item = new QListWidgetItem(m_thumbnailList);
    item->setText(QStringLiteral("点位 %1").arg(event.pointId));
    if (!info.thumbnail.isNull()) {
        item->setIcon(QIcon(info.thumbnail));
    }
    item->setData(Qt::UserRole, event.captureId);

    // Color code by defect presence
    if (!event.defects.isEmpty()) {
        item->setBackground(QBrush(QColor("#ffe5e5")));  // light red
    }
}

void ResultPanel::setCaptureRecords(const QVector<hmi::CaptureRecord>& records)
{
    m_captures.clear();
    m_thumbnailList->clear();

    for (const auto& record : records) {
        CaptureInfo info;
        info.captureId = record.captureId;
        info.pointId = record.pointId;
        info.defects = record.defects;

        if (!record.image.thumbnailJpeg.isEmpty()) {
            info.thumbnail = renderThumbnailWithDefects(
                record.image.thumbnailJpeg,
                record.defects,
                static_cast<int>(record.image.width),
                static_cast<int>(record.image.height)
            );
        }

        m_captures[record.captureId] = info;

        QListWidgetItem* item = new QListWidgetItem(m_thumbnailList);
        item->setText(QStringLiteral("点位 %1").arg(record.pointId));
        if (!info.thumbnail.isNull()) {
            item->setIcon(QIcon(info.thumbnail));
        }
        item->setData(Qt::UserRole, record.captureId);

        if (!record.defects.isEmpty()) {
            item->setBackground(QBrush(QColor("#ffe5e5")));
        }
    }
}

void ResultPanel::addEvent(const hmi::InspectionEvent& event)
{
    QString icon = eventTypeIcon(event.type);
    QString color = eventTypeColor(event.type);
    QString timeStr = event.timestamp.isValid()
        ? event.timestamp.toString(QStringLiteral("hh:mm:ss"))
        : QStringLiteral("--:--:--");

    QString text = QStringLiteral("%1 [%2] %3")
        .arg(icon)
        .arg(timeStr)
        .arg(event.message);

    QListWidgetItem* item = new QListWidgetItem(text, m_eventTimeline);
    item->setForeground(QBrush(QColor(color)));

    m_eventTimeline->scrollToBottom();
}

void ResultPanel::clear()
{
    m_captures.clear();
    m_thumbnailList->clear();
    m_eventTimeline->clear();
    m_detailImage->clear();
    m_defectInfoLabel->setText(QStringLiteral("点击缩略图查看详细信息"));
}

void ResultPanel::setFullImage(const QString& captureId, const QByteArray& imageData)
{
    if (!m_captures.contains(captureId)) {
        return;
    }

    QPixmap fullImage;
    if (fullImage.loadFromData(imageData)) {
        m_captures[captureId].fullImage = fullImage;
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

QPixmap ResultPanel::renderThumbnailWithDefects(const QByteArray& jpegData,
                                                 const QVector<hmi::DefectResult>& defects,
                                                 int imgW, int imgH)
{
    Q_UNUSED(imgW)
    Q_UNUSED(imgH)

    QPixmap pixmap;
    if (!pixmap.loadFromData(jpegData)) {
        // Fallback: create a placeholder
        pixmap = QPixmap(120, 90);
        pixmap.fill(Qt::darkGray);
        return pixmap;
    }

    // Draw bounding boxes on the pixmap
    if (!defects.isEmpty()) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        QPen pen(QColor("#ff0000"), 2, Qt::SolidLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        for (const auto& defect : defects) {
            if (defect.hasDefect) {
                QRect rect(defect.bbox.x, defect.bbox.y, defect.bbox.w, defect.bbox.h);
                painter.drawRect(rect);

                // Draw confidence label
                QString label = QStringLiteral("%1%").arg(static_cast<int>(defect.confidence * 100));
                QFont font = painter.font();
                font.setPointSize(8);
                font.setBold(true);
                painter.setFont(font);
                painter.setBrush(QBrush(QColor(255, 0, 0, 180)));
                QRect labelRect(rect.x(), rect.y() - 16, 40, 14);
                painter.fillRect(labelRect, painter.brush());
                painter.setPen(Qt::white);
                painter.drawText(labelRect, Qt::AlignCenter, label);
            }
        }
    }

    return pixmap;
}

// ---------------------------------------------------------------------------
// Event type helpers
// ---------------------------------------------------------------------------

QString ResultPanel::eventTypeIcon(hmi::InspectionEventType type)
{
    switch (type) {
        case hmi::InspectionEventType::Info:        return QStringLiteral("ℹ️");
        case hmi::InspectionEventType::Warn:        return QStringLiteral("⚠️");
        case hmi::InspectionEventType::Error:       return QStringLiteral("❌");
        case hmi::InspectionEventType::Captured:    return QStringLiteral("📷");
        case hmi::InspectionEventType::DefectFound: return QStringLiteral("🔴");
        default:                                    return QStringLiteral("•");
    }
}

QString ResultPanel::eventTypeColor(hmi::InspectionEventType type)
{
    switch (type) {
        case hmi::InspectionEventType::Info:        return QStringLiteral("#17a2b8");  // cyan
        case hmi::InspectionEventType::Warn:        return QStringLiteral("#ffc107");  // yellow
        case hmi::InspectionEventType::Error:       return QStringLiteral("#dc3545");  // red
        case hmi::InspectionEventType::Captured:    return QStringLiteral("#28a745");  // green
        case hmi::InspectionEventType::DefectFound: return QStringLiteral("#dc3545");  // red
        default:                                    return QStringLiteral("#6c757d");  // gray
    }
}
