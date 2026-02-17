// src/ui/ProjectPanel.h
//
// ProjectPanel – left sidebar with model tree and inspection target list.

#pragma once

#include "core/Types.h"

#include <QWidget>
#include <QMap>

class QTreeWidget;
class QTreeWidgetItem;
class QListWidget;
class QListWidgetItem;
class QLabel;

/// \brief Left dock panel showing model info and point list.
class ProjectPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectPanel(QWidget* parent = nullptr);

    // Model management
    void setModelInfo(const QString& filename, const QString& modelId);
    void clearModel();

    // Target list management
    void addTarget(const hmi::InspectionTarget& target);
    void removeTarget(int32_t pointId);
    void updateTarget(const hmi::InspectionTarget& target);
    void clearTargets();
    void selectTarget(int32_t pointId);

    // Path display
    void setPath(const hmi::InspectionPath& path);
    void clearPath();

signals:
    void targetSelected(int32_t pointId);
    void targetDeleteRequested(int32_t pointId);

private:
    void setupUi();
    QListWidgetItem* findPointItem(int32_t pointId);

    QTreeWidget* m_modelTree      = nullptr;
    QListWidget* m_pointList      = nullptr;
    QLabel*      m_pointCountLabel = nullptr;

    QString m_modelId;

    // pointId -> QListWidgetItem*
    QMap<int32_t, QListWidgetItem*> m_pointItems;
};
