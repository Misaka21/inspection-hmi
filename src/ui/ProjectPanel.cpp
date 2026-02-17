// src/ui/ProjectPanel.cpp

#include "ProjectPanel.h"

#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

ProjectPanel::ProjectPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ProjectPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Splitter to allow resizing between model tree and point list
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // -----------------------------------------------------------------------
    // Model tree group
    // -----------------------------------------------------------------------
    auto* modelGroup = new QGroupBox(tr("模型"), this);
    auto* modelLayout = new QVBoxLayout(modelGroup);

    m_modelTree = new QTreeWidget(modelGroup);
    m_modelTree->setHeaderHidden(true);
    m_modelTree->setRootIsDecorated(true);
    modelLayout->addWidget(m_modelTree);

    splitter->addWidget(modelGroup);

    // -----------------------------------------------------------------------
    // Point list group
    // -----------------------------------------------------------------------
    auto* pointGroup = new QGroupBox(tr("点位列表"), this);
    auto* pointLayout = new QVBoxLayout(pointGroup);

    m_pointCountLabel = new QLabel(tr("共 0 个点位"), pointGroup);
    m_pointCountLabel->setStyleSheet("QLabel { color: gray; }");
    pointLayout->addWidget(m_pointCountLabel);

    m_pointList = new QListWidget(pointGroup);
    m_pointList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_pointList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int32_t pointId = item->data(Qt::UserRole).toInt();
        emit targetSelected(pointId);
    });

    // Context menu for delete
    m_pointList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_pointList, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = m_pointList->itemAt(pos);
        if (!item) return;
        const int32_t pointId = item->data(Qt::UserRole).toInt();
        emit targetDeleteRequested(pointId);
    });

    pointLayout->addWidget(m_pointList);

    splitter->addWidget(pointGroup);

    mainLayout->addWidget(splitter);

    // Give more space to point list initially
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
}

// ---------------------------------------------------------------------------
// Model management
// ---------------------------------------------------------------------------

void ProjectPanel::setModelInfo(const QString& filename, const QString& modelId)
{
    m_modelId = modelId;

    m_modelTree->clear();

    auto* root = new QTreeWidgetItem(m_modelTree);
    root->setText(0, filename);
    root->setExpanded(true);

    auto* idItem = new QTreeWidgetItem(root);
    idItem->setText(0, tr("模型 ID: %1").arg(modelId.isEmpty() ? tr("(未上传)") : modelId));

    m_modelTree->addTopLevelItem(root);
}

void ProjectPanel::clearModel()
{
    m_modelTree->clear();
    m_modelId.clear();
}

// ---------------------------------------------------------------------------
// Target list management
// ---------------------------------------------------------------------------

void ProjectPanel::addTarget(const hmi::InspectionTarget& target)
{
    // Check if already exists
    if (m_pointItems.contains(target.pointId)) {
        updateTarget(target);
        return;
    }

    const QString text = tr("点 %1 (%2)")
                             .arg(target.pointId)
                             .arg(target.groupId.isEmpty() ? tr("未分组") : target.groupId);

    auto* item = new QListWidgetItem(text, m_pointList);
    item->setData(Qt::UserRole, target.pointId);
    m_pointList->addItem(item);
    m_pointItems[target.pointId] = item;

    m_pointCountLabel->setText(tr("共 %1 个点位").arg(m_pointItems.size()));
}

void ProjectPanel::removeTarget(int32_t pointId)
{
    QListWidgetItem* item = findPointItem(pointId);
    if (!item) return;

    const int row = m_pointList->row(item);
    m_pointList->takeItem(row);
    m_pointItems.remove(pointId);
    delete item;

    m_pointCountLabel->setText(tr("共 %1 个点位").arg(m_pointItems.size()));
}

void ProjectPanel::updateTarget(const hmi::InspectionTarget& target)
{
    QListWidgetItem* item = findPointItem(target.pointId);
    if (!item) return;

    const QString text = tr("点 %1 (%2)")
                             .arg(target.pointId)
                             .arg(target.groupId.isEmpty() ? tr("未分组") : target.groupId);
    item->setText(text);
}

void ProjectPanel::clearTargets()
{
    m_pointList->clear();
    m_pointItems.clear();
    m_pointCountLabel->setText(tr("共 0 个点位"));
}

void ProjectPanel::selectTarget(int32_t pointId)
{
    QListWidgetItem* item = findPointItem(pointId);
    if (!item) return;
    m_pointList->setCurrentItem(item);
}

// ---------------------------------------------------------------------------
// Path display
// ---------------------------------------------------------------------------

void ProjectPanel::setPath(const hmi::InspectionPath& path)
{
    // Optionally display path statistics in the model tree or a separate panel.
    // For now, we update the point count to include path info.
    m_pointCountLabel->setText(
        tr("共 %1 个点位 | 路径 %2 点, %.2f m")
            .arg(m_pointItems.size())
            .arg(path.totalPoints)
            .arg(path.estimatedDistanceM));
}

void ProjectPanel::clearPath()
{
    m_pointCountLabel->setText(tr("共 %1 个点位").arg(m_pointItems.size()));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QListWidgetItem* ProjectPanel::findPointItem(int32_t pointId)
{
    return m_pointItems.value(pointId, nullptr);
}
