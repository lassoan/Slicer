/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, and Cancer
  Care Ontario.

==============================================================================*/

// Qt includes
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QMessageBox>
#include <QMimeData>
#include <QTimer>

// qMRML includes
#include "qMRMLMarkupsControlPointsModel_p.h"

// Slicer includes
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>
#include <qSlicerAbstractCoreModule.h>

// MRML includes
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLMarkupsNode.h>

// Terminology includes
#include "qSlicerTerminologyItemDelegate.h"
#include "vtkSlicerTerminologiesModuleLogic.h"
#include "vtkSlicerTerminologyEntry.h"
#include "vtkSlicerTerminologyCategory.h"
#include "vtkSlicerTerminologyType.h"

// Polyseg includes
#include "vtkSegment.h"

// Segmentations includes
#include "qMRMLMarkupsControlPointsTableView.h"

// Segmentations logic includes
#include "vtkSlicerMarkupsLogic.h"

//------------------------------------------------------------------------------
qMRMLMarkupsControlPointsModelPrivate::qMRMLMarkupsControlPointsModelPrivate(qMRMLMarkupsControlPointsModel& object)
  : q_ptr(&object)
  , UpdatingItemFromControlPoint(false)
  , MarkupsNode(nullptr)
{
  this->CallBack = vtkSmartPointer<vtkCallbackCommand>::New();

  this->HiddenIcon = QIcon(":Icons/VisibleOff.png");
  this->VisibleIcon = QIcon(":Icons/VisibleOn.png");

  this->LockIcon = QIcon(":Icons/Medium/SlicerLock.png");
  this->UnlockIcon = QIcon(":Icons/Medium/SlicerUnlock.png");

  this->PositionDefinedIcon = QIcon(":/Icons/XSmall/MarkupsDefined.png");
  this->PositionPreviewIcon = QIcon(":/Icons/XSmall/MarkupsInProgress.png");
  this->PositionMissingIcon = QIcon(":/Icons/XSmall/MarkupsMissing.png");
  this->PositionUndefinedIcon = QIcon(":/Icons/XSmall/MarkupsUndefined.png");

  qRegisterMetaType<QStandardItem*>("QStandardItem*");
}

//------------------------------------------------------------------------------
qMRMLMarkupsControlPointsModelPrivate::~qMRMLMarkupsControlPointsModelPrivate()
{
  if (this->MarkupsNode)
    {
    this->MarkupsNode->RemoveObserver(this->CallBack);
    }
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModelPrivate::init()
{
  Q_Q(qMRMLMarkupsControlPointsModel);
  this->CallBack->SetClientData(q);
  this->CallBack->SetCallback(qMRMLMarkupsControlPointsModel::onEvent);

  QObject::connect(q, SIGNAL(itemChanged(QStandardItem*)), q, SLOT(onItemChanged(QStandardItem*)));

  QStringList columnLabels;
  columnLabels << "" /* selected icon */ << "" /* locked icon */ << "" /* visibility icon */
    << qMRMLMarkupsControlPointsModel::tr("Label") << qMRMLMarkupsControlPointsModel::tr("Description")
    << qMRMLMarkupsControlPointsModel::tr("R") << qMRMLMarkupsControlPointsModel::tr("A") << qMRMLMarkupsControlPointsModel::tr("S")
    << "" /* position state icon */;
  q->setHorizontalHeaderLabels(columnLabels);

  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::SelectedColumn)->setToolTip(qMRMLMarkupsControlPointsModel::tr("Selected"));
  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::SelectedColumn)->setIcon(QIcon(":/Icons/MarkupsSelectedOrUnselected.png"));

  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::LockedColumn)->setToolTip(qMRMLMarkupsControlPointsModel::tr("Locked"));
  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::LockedColumn)->setIcon(QIcon(":/Icons/Small/SlicerLockUnlock.png"));

  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::VisibilityColumn)->setToolTip(qMRMLMarkupsControlPointsModel::tr("Visibility"));
  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::VisibilityColumn)->setIcon(QIcon(":/Icons/Small/SlicerVisibleInvisible.png"));

  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::PositionStatusColumn)->setToolTip(qMRMLMarkupsControlPointsModel::tr("Position status"));
  q->horizontalHeaderItem(qMRMLMarkupsControlPointsModel::PositionStatusColumn)->setIcon(QIcon(":/Icons/Large/MarkupsPositionStatus.png"));
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModelPrivate::updateControlPoint(int controlPointIndex)
{
  Q_Q(qMRMLMarkupsControlPointsModel);

  QList<QStandardItem*> items;
  for (int col = 0; col < q->columnCount(); ++col)
    {
    QStandardItem* item = q->itemFromControlPointIndex(controlPointIndex, col);
    if (!item)
      {
      qWarning() << Q_FUNC_INFO << " error: failed to find table item for column " << col;
      continue;
      }
    q->updateItemFromControlPoint(item, controlPointIndex, col);
    }
}

//------------------------------------------------------------------------------
QStandardItem* qMRMLMarkupsControlPointsModelPrivate::insertControlPoint(int controlPointIndex)
{
  Q_Q(qMRMLMarkupsControlPointsModel);
  QStandardItem* item = q->itemFromControlPointIndex(controlPointIndex);
  if (item)
    {
    // It is possible that the item has been already added if it is the parent of a child item already inserted
    return item;
    }

  QList<QStandardItem*> items;
  for (int col = 0; col < q->columnCount(); ++col)
    {
    QStandardItem* newItem = new QStandardItem();
    q->updateItemFromControlPoint(newItem, controlPointIndex, col);
    items.append(newItem);
    }

  int row = controlPointIndex;
  q->insertRow(row, items);

  item = items[0];
  if (q->itemFromControlPointIndex(controlPointIndex) != item)
    {
    qCritical() << Q_FUNC_INFO << ": Item mismatch when inserting control point " << controlPointIndex;
    return nullptr;
    }
  return item;
}

//------------------------------------------------------------------------------
// qMRMLMarkupsControlPointsModel
//------------------------------------------------------------------------------
qMRMLMarkupsControlPointsModel::qMRMLMarkupsControlPointsModel(QObject *_parent)
  :QStandardItemModel(_parent)
  , d_ptr(new qMRMLMarkupsControlPointsModelPrivate(*this))
{
  Q_D(qMRMLMarkupsControlPointsModel);
  d->init();
}

//------------------------------------------------------------------------------
qMRMLMarkupsControlPointsModel::qMRMLMarkupsControlPointsModel(qMRMLMarkupsControlPointsModelPrivate* pimpl, QObject* parent)
  : QStandardItemModel(parent)
  , d_ptr(pimpl)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  d->init();
}

//------------------------------------------------------------------------------
qMRMLMarkupsControlPointsModel::~qMRMLMarkupsControlPointsModel() = default;

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::setMarkupsNode(vtkMRMLMarkupsNode* markupsNode)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  if (markupsNode == d->MarkupsNode)
    {
    return;
    }

  if (d->MarkupsNode)
    {
    d->MarkupsNode->RemoveObserver(d->CallBack);
    }
  d->MarkupsNode = markupsNode;

  // Update all MarkupsControlPoints
  this->rebuildFromMarkupsControlPoints();

  if (d->MarkupsNode)
    {
    d->MarkupsNode->AddObserver(vtkMRMLMarkupsNode::PointAddedEvent, d->CallBack, 10.0);
    d->MarkupsNode->AddObserver(vtkMRMLMarkupsNode::PointRemovedEvent, d->CallBack, 10.0);
    d->MarkupsNode->AddObserver(vtkMRMLMarkupsNode::PointModifiedEvent, d->CallBack, -10.0);
    }
}

//------------------------------------------------------------------------------
vtkMRMLMarkupsNode* qMRMLMarkupsControlPointsModel::markupsNode()const
{
  Q_D(const qMRMLMarkupsControlPointsModel);
  return d->MarkupsNode;
}

//------------------------------------------------------------------------------
QStandardItem* qMRMLMarkupsControlPointsModel::itemFromControlPointIndex(int controlPointIndex, int column/*=0*/)const
{
  QModelIndex index = this->indexFromControlPointIndex(controlPointIndex, column);
  QStandardItem* item = this->itemFromIndex(index);
  return item;
}

//------------------------------------------------------------------------------
QModelIndex qMRMLMarkupsControlPointsModel::indexFromControlPointIndex(int controlPointIndex, int column/*=0*/)const
{
  Q_D(const qMRMLMarkupsControlPointsModel);
  QModelIndex itemIndex;
  if (controlPointIndex >= 0 && controlPointIndex < this->rowCount()
    && column >=0 && column < this->columnCount())
    {
    itemIndex = this->index(controlPointIndex, column);
    }
  return itemIndex;
}

//------------------------------------------------------------------------------
QModelIndexList qMRMLMarkupsControlPointsModel::indexes(int controlPointIndex) const
{
  QModelIndexList itemIndexes;
  if (controlPointIndex >= 0 && controlPointIndex < this->rowCount())
    {
    for (int col = 0; col < NumberOfColumns; ++col)
      {
      itemIndexes << this->index(controlPointIndex, col);
      }
    }
  return itemIndexes;
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::rebuildFromMarkupsControlPoints()
{
  Q_D(qMRMLMarkupsControlPointsModel);

  this->beginResetModel();

  // Enabled so it can be interacted with
  this->invisibleRootItem()->setFlags(Qt::ItemIsEnabled);

  // Remove rows before populating
  this->removeRows(0, this->rowCount());

  if (!d->MarkupsNode)
    {
    this->endResetModel();
    return;
    }

  // Populate model with the MarkupsControlPoints
  int numberOfControlPoints = d->MarkupsNode->GetNumberOfControlPoints();
  for (int controlPointIndex = 0; controlPointIndex < numberOfControlPoints; ++controlPointIndex)
    {
    d->insertControlPoint(controlPointIndex);
    }

  this->endResetModel();
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::updateFromMarkupsControlPoints()
{
  Q_D(qMRMLMarkupsControlPointsModel);

  // Update model with the MarkupsControlPoints
  int numberOfControlPoints = d->MarkupsNode->GetNumberOfControlPoints();
  for (int controlPointIndex = 0; controlPointIndex < numberOfControlPoints; ++controlPointIndex)
    {
    d->updateControlPoint(controlPointIndex);
    }
}

//------------------------------------------------------------------------------
Qt::ItemFlags qMRMLMarkupsControlPointsModel::controlPointFlags(int controlPointIndex, int column)const
{
  Q_D(const qMRMLMarkupsControlPointsModel);
  Q_UNUSED(controlPointIndex);

  Qt::ItemFlags flags;
  flags.setFlag(Qt::ItemIsEnabled);
  flags.setFlag(Qt::ItemIsSelectable);

  if (!d->MarkupsNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid markups node";
    return flags;
    }

  if (column != SelectedColumn && column != LockedColumn
    && column != VisibilityColumn && column != PositionStatusColumn)
    {
    flags.setFlag(Qt::ItemIsEditable);
    }

  return flags;
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::updateItemFromControlPoint(QStandardItem* item, int controlPointIndex, int column)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  if (!item)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid item";
    return;
    }

  bool wasUpdating = d->UpdatingItemFromControlPoint;
  d->UpdatingItemFromControlPoint = true;

  Qt::ItemFlags flags = this->controlPointFlags(controlPointIndex, column);
  item->setFlags(flags);

  // Update item data for the current column
  this->updateItemDataFromControlPoint(item, controlPointIndex, column);

  d->UpdatingItemFromControlPoint = wasUpdating;
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::updateItemDataFromControlPoint(QStandardItem* item, int controlPointIndex, int column)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  if (!d->MarkupsNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid markups node";
    return;
    }

  if (d->UpdatingFromMRML)
    {
    return;
    }

  vtkMRMLMarkupsNode::ControlPoint* controlPoint = d->MarkupsNode->GetNthControlPoint(controlPointIndex);
  if (!controlPoint)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid control point index " << controlPointIndex;
    return;
    }

  bool wasUpdatingFromMRML = d->UpdatingFromMRML;
  d->UpdatingFromMRML = true;

  // Item data is used to prevent setting the same icon.
  // It should be fine to set the icon even if it is the same, but due
  // to a bug in Qt (http://bugreports.qt.nokia.com/browse/QTBUG-20248),
  // it would fire a superfluous itemChanged() signal.

  if (column == SelectedColumn)
    {
    item->setCheckState(controlPoint->Selected ? Qt::Checked : Qt::Unchecked);
    }
  else if (column == LockedColumn)
    {
    if (item->data(Qt::UserRole).isNull()
      || item->data(Qt::UserRole).toBool() != controlPoint->Locked)
      {
      item->setData(controlPoint->Locked, Qt::UserRole);
      item->setIcon(controlPoint->Locked ? d->LockIcon : d->UnlockIcon);
      }
    }
  else if (column == VisibilityColumn)
    {
    if (item->data(Qt::UserRole).isNull()
      || item->data(Qt::UserRole).toBool() != controlPoint->Visibility)
      {
      item->setData(controlPoint->Visibility, Qt::UserRole);
      item->setIcon(controlPoint->Visibility ? d->VisibleIcon : d->HiddenIcon);
      }
    }
  else if (column == LabelColumn)
    {
    item->setText(QString::fromStdString(controlPoint->Label));
    }
  else if (column == DescriptionColumn)
    {
    item->setText(QString::fromStdString(controlPoint->Description));
    }
  else if (column == CoordinateRColumn || column == CoordinateAColumn || column == CoordinateSColumn)
    {
    bool positionVisible = (controlPoint->PositionStatus == vtkMRMLMarkupsNode::PositionDefined ||
      controlPoint->PositionStatus == vtkMRMLMarkupsNode::PositionPreview);
    if (item->data(Qt::UserRole).isNull() ||
      item->data(Qt::UserRole).toDouble() != controlPoint->Position[column - CoordinateRColumn])
      // Only set if it changed (https://bugreports.qt-project.org/browse/QTBUG-20248)
      {
      item->setData(controlPoint->Position[column - CoordinateRColumn], Qt::UserRole);
      }
    if (item->data(Qt::UserRole+1).isNull() ||
      item->data(Qt::UserRole+1).toBool() != positionVisible)
      // Only set if it changed (https://bugreports.qt-project.org/browse/QTBUG-20248)
      {
      item->setData(positionVisible, Qt::UserRole + 1);
      }
    }
  else if (column == PositionStatusColumn)
    {
    if (item->data(Qt::UserRole).isNull() ||
        item->data(Qt::UserRole).toInt() != controlPoint->PositionStatus) // Only set if it changed (https://bugreports.qt-project.org/browse/QTBUG-20248)
      {
      QIcon statusIcon;
      QString statusTooltip;
      switch (controlPoint->PositionStatus)
        {
        case vtkMRMLMarkupsNode::PositionUndefined:
          statusIcon = d->PositionUndefinedIcon;
          statusTooltip = tr("Position undefined");
          break;
        case vtkMRMLMarkupsNode::PositionPreview:
          statusIcon = d->PositionPreviewIcon;
          statusTooltip = tr("Position preview");
          break;
        case vtkMRMLMarkupsNode::PositionDefined:
          statusIcon = d->PositionDefinedIcon;
          statusTooltip = "Position defined";
          break;
        case vtkMRMLMarkupsNode::PositionMissing:
          statusIcon = d->PositionMissingIcon;
          statusTooltip = "Position missing";
          break;
        }
      item->setData(controlPoint->PositionStatus, Qt::UserRole);
      item->setToolTip(statusTooltip);
      item->setIcon(statusIcon);
      }
    }

  d->UpdatingFromMRML = wasUpdatingFromMRML;
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::updateControlPointFromItem(QStandardItem* item)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  if (!d->MarkupsNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentation node";
    return;
    }
  if (!item)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid item";
    return;
    }

  int controlPointIndex = item->row();
  int column = item->column();

  if (column == SelectedColumn)
    {
    bool selected = item->data(Qt::UserRole).toBool();
    d->MarkupsNode->SetNthControlPointSelected(controlPointIndex, selected);
    }
  else if (column == LockedColumn)
    {
    bool locked = item->data(Qt::UserRole).toBool();
    d->MarkupsNode->SetNthControlPointSelected(controlPointIndex, locked);
    }
  else if (column == VisibilityColumn)
    {
    bool visible = item->data(Qt::UserRole).toBool();
    d->MarkupsNode->SetNthControlPointVisibility(controlPointIndex, visible);
    }
  else if (column == LabelColumn)
    {
    d->MarkupsNode->SetNthControlPointLabel(controlPointIndex, item->text().toStdString());
    }
  else if (column == DescriptionColumn)
    {
    d->MarkupsNode->SetNthControlPointDescription(controlPointIndex, item->text().toStdString());
    }
  else if (column == CoordinateRColumn || column == CoordinateAColumn || column == CoordinateSColumn)
    {
    double position[3] = { 0.0, 0.0, 0.0 };
    d->MarkupsNode->GetNthControlPointPosition(controlPointIndex, position);
    int positionStatus = d->MarkupsNode->GetNthControlPointPositionStatus(controlPointIndex);
    int componentIndex = column - CoordinateRColumn;
    double positionComponent = item->data(Qt::UserRole).toDouble();
    position[componentIndex] = positionComponent;
    d->MarkupsNode->SetNthControlPointPosition(controlPointIndex, position, positionStatus);
    }
  else if (column == PositionStatusColumn)
    {
    int positionStatus = item->data(Qt::UserRole).toInt();
    switch (positionStatus)
      {
      case vtkMRMLMarkupsNode::PositionUndefined:
        d->MarkupsNode->UnsetNthControlPointPosition(controlPointIndex);
        break;
      case vtkMRMLMarkupsNode::PositionPreview:
        d->MarkupsNode->ResetNthControlPointPosition(controlPointIndex);
        break;
      case vtkMRMLMarkupsNode::PositionDefined:
        d->MarkupsNode->RestoreNthControlPointPosition(controlPointIndex);
        break;
      case vtkMRMLMarkupsNode::PositionMissing:
        d->MarkupsNode->SetNthControlPointPositionMissing(controlPointIndex);
        break;
      }
    }
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::onEvent(
  vtkObject* caller, unsigned long event, void* clientData, void* callData)
{
  vtkMRMLMarkupsNode* markupsNode = reinterpret_cast<vtkMRMLMarkupsNode*>(caller);
  qMRMLMarkupsControlPointsModel* model = reinterpret_cast<qMRMLMarkupsControlPointsModel*>(clientData);
  if (!model || !markupsNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid event parameters";
    return;
    }

  // Get segment ID for segmentation node events
  int controlPointIndex = -1;
  if (callData)
    {
    int* controlPointIndexPtr = reinterpret_cast<int*>(callData);
    if (controlPointIndexPtr)
      {
      controlPointIndex = *controlPointIndexPtr;
      }
    }

  switch (event)
    {
    case vtkMRMLMarkupsNode::PointAddedEvent:
        model->onControlPointAdded(controlPointIndex);
      break;
    case vtkMRMLMarkupsNode::PointRemovedEvent:
        model->onControlPointRemoved(controlPointIndex);
      break;
    case vtkMRMLMarkupsNode::PointModifiedEvent:
      model->onControlPointModified(controlPointIndex);
      break;
    }
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::onControlPointAdded(int controlPointIndex)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  Q_UNUSED(controlPointIndex);
  if (!d->MarkupsNode)
    {
    return;
    }
  int numberOfControlPoints = d->MarkupsNode->GetNumberOfControlPoints();
  for (int controlPointIndex = this->rowCount(); controlPointIndex < numberOfControlPoints; ++controlPointIndex)
    {
    d->insertControlPoint(controlPointIndex);
    }
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::onControlPointRemoved(int controlPointIndex)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  if (!d->MarkupsNode)
    {
    return;
    }
  if (controlPointIndex>=0)
    {
    this->removeRow(controlPointIndex);
    return;
    }
  int numberOfControlPoints = d->MarkupsNode->GetNumberOfControlPoints();
  for (int controlPointIndex = 0; controlPointIndex < numberOfControlPoints; ++controlPointIndex)
    {
    d->updateControlPoint(controlPointIndex);
    }
  for (int controlPointIndex = this->rowCount()-1; controlPointIndex >= numberOfControlPoints; --controlPointIndex)
    {
    this->removeRow(controlPointIndex);
    }
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::onControlPointModified(int controlPointIndex)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  if (!d->MarkupsNode)
    {
    return;
    }
  if (controlPointIndex>=0)
    {
    d->updateControlPoint(controlPointIndex);
    return;
    }
  int numberOfControlPoints = d->MarkupsNode->GetNumberOfControlPoints();
  for (int controlPointIndex = 0; controlPointIndex < numberOfControlPoints; ++controlPointIndex)
    {
    d->updateControlPoint(controlPointIndex);
    }
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsModel::onItemChanged(QStandardItem* item)
{
  Q_D(qMRMLMarkupsControlPointsModel);
  if (d->UpdatingFromMRML)
    {
    // The item was changed because we are updating from MRML.
    return;
    }
  this->updateControlPointFromItem(item);
}
