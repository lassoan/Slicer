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

// Segmentations includes
#include "qMRMLMarkupsControlPointsModel.h"
#include "qMRMLMarkupsControlPointsSortFilterProxyModel.h"

// MRML include
#include "vtkMRMLMarkupsNode.h"

// Qt includes
#include <QDebug>
#include <QStandardItem>

// -----------------------------------------------------------------------------
// qMRMLMarkupsControlPointsSortFilterProxyModelPrivate

// -----------------------------------------------------------------------------
/// \ingroup Slicer_MRMLWidgets
class qMRMLMarkupsControlPointsSortFilterProxyModelPrivate
{
public:
  qMRMLMarkupsControlPointsSortFilterProxyModelPrivate();

  bool FilterEnabled{false};
  QString LabelFilter;
  QString TextFilter;
  bool ShowPositionStatus[vtkMRMLMarkupsNode::PositionStatus_Last];
  QList<int> HideMarkupsControlPoints;
};

// -----------------------------------------------------------------------------
qMRMLMarkupsControlPointsSortFilterProxyModelPrivate::qMRMLMarkupsControlPointsSortFilterProxyModelPrivate()
{
  for (int i = 0; i < vtkMRMLMarkupsNode::PositionStatus_Last; ++i)
    {
    this->ShowPositionStatus[i] = false;
    }
}

// -----------------------------------------------------------------------------
// qMRMLMarkupsControlPointsSortFilterProxyModel

// -----------------------------------------------------------------------------
CTK_GET_CPP(qMRMLMarkupsControlPointsSortFilterProxyModel, bool, filterEnabled, FilterEnabled);
CTK_GET_CPP(qMRMLMarkupsControlPointsSortFilterProxyModel, QString, labelFilter, LabelFilter);
CTK_GET_CPP(qMRMLMarkupsControlPointsSortFilterProxyModel, QString, textFilter, TextFilter);

//------------------------------------------------------------------------------
qMRMLMarkupsControlPointsSortFilterProxyModel::qMRMLMarkupsControlPointsSortFilterProxyModel(QObject *vparent)
 : QSortFilterProxyModel(vparent)
 , d_ptr(new qMRMLMarkupsControlPointsSortFilterProxyModelPrivate)
{
  this->setDynamicSortFilter(true);
}

//------------------------------------------------------------------------------
qMRMLMarkupsControlPointsSortFilterProxyModel::~qMRMLMarkupsControlPointsSortFilterProxyModel() = default;

//-----------------------------------------------------------------------------
vtkMRMLMarkupsNode* qMRMLMarkupsControlPointsSortFilterProxyModel::markupsNode()const
{
  qMRMLMarkupsControlPointsModel* model = qobject_cast<qMRMLMarkupsControlPointsModel*>(this->sourceModel());
  if (!model)
    {
    return nullptr;
    }
  return model->markupsNode();
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsSortFilterProxyModel::setFilterEnabled(bool filterEnabled)
{
  Q_D(qMRMLMarkupsControlPointsSortFilterProxyModel);
  if (d->FilterEnabled == filterEnabled)
  {
    return;
  }
  d->FilterEnabled = filterEnabled;
  this->invalidateFilter();
  emit filterModified();
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsSortFilterProxyModel::setLabelFilter(QString filter)
{
  Q_D(qMRMLMarkupsControlPointsSortFilterProxyModel);
  if (d->LabelFilter == filter)
    {
    return;
    }
  d->LabelFilter = filter;
  this->invalidateFilter();
  emit filterModified();
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsSortFilterProxyModel::setTextFilter(QString filter)
{
  Q_D(qMRMLMarkupsControlPointsSortFilterProxyModel);
  if (d->TextFilter == filter)
    {
    return;
    }
  d->TextFilter = filter;
  this->invalidateFilter();
  emit filterModified();
}

//-----------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsSortFilterProxyModel::showPositionStatus(int status) const
{
  Q_D(const qMRMLMarkupsControlPointsSortFilterProxyModel);
  if (status < 0 || status >= vtkMRMLMarkupsNode::PositionStatus_Last)
    {
    return false;
    }
  return d->ShowPositionStatus[status];
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsSortFilterProxyModel::setShowPositionStatus(int status, bool shown)
{
  Q_D(qMRMLMarkupsControlPointsSortFilterProxyModel);
  if (status < 0 || status >= vtkMRMLMarkupsNode::PositionStatus_Last)
    {
    return;
    }
  if (d->ShowPositionStatus[status] == shown)
    {
    return;
    }

  d->ShowPositionStatus[status] = shown;
  this->invalidateFilter();
  emit filterModified();
}

//-----------------------------------------------------------------------------
QModelIndex qMRMLMarkupsControlPointsSortFilterProxyModel::modelIndexFromControlPointIndex(int controlPointIndex, int column/*=0*/)const
{
  qMRMLMarkupsControlPointsModel* markupsControlPointsModel = qobject_cast<qMRMLMarkupsControlPointsModel*>(this->sourceModel());
  return this->mapFromSource(markupsControlPointsModel->indexFromControlPointIndex(controlPointIndex, column));
}

//-----------------------------------------------------------------------------
int qMRMLMarkupsControlPointsSortFilterProxyModel::controlPointIndexFromModelIndex(QModelIndex modelIndex)const
{
  QModelIndex sourceModelIndex = this->mapToSource(modelIndex);
  if (!sourceModelIndex.isValid())
    {
    return -1;
    }
  return sourceModelIndex.row();
}

//-----------------------------------------------------------------------------
QStandardItem* qMRMLMarkupsControlPointsSortFilterProxyModel::sourceItem(const QModelIndex& sourceIndex)const
{
  qMRMLMarkupsControlPointsModel* model = qobject_cast<qMRMLMarkupsControlPointsModel*>(this->sourceModel());
  if (!model)
    {
    return nullptr;
    }
  return sourceIndex.isValid() ? model->itemFromIndex(sourceIndex) : model->invisibleRootItem();
}

//------------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent)const
{
  QStandardItem* parentItem = this->sourceItem(sourceParent);
  if (!parentItem)
    {
    return false;
    }
  QStandardItem* item = nullptr;

  // Sometimes the row is not complete (DnD), search for a non null item
  for (int childIndex=0; childIndex < parentItem->columnCount(); ++childIndex)
    {
    item = parentItem->child(sourceRow, childIndex);
    if (item)
      {
      break;
      }
    }
  if (item == nullptr)
    {
    return false;
    }

  /*
  qMRMLMarkupsControlPointsModel* model = qobject_cast<qMRMLMarkupsControlPointsModel*>(this->sourceModel());
  QString segmentID = model->segmentIDFromItem(item);
  return this->filterAcceptsItem(segmentID);
  */
  int controlPointIndex = sourceRow;
  return this->filterAcceptsItem(controlPointIndex);
}

//------------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsSortFilterProxyModel::filterAcceptsItem(int controlPointIndex)const
{
  Q_D(const qMRMLMarkupsControlPointsSortFilterProxyModel);

  // Filter if segment is hidden
  if (d->HideMarkupsControlPoints.contains(controlPointIndex))
    {
    return false;
    }

  if (!d->FilterEnabled)
    {
    return true;
    }

  qMRMLMarkupsControlPointsModel* model = qobject_cast<qMRMLMarkupsControlPointsModel*>(this->sourceModel());
  if (!model)
    {
    return false;
    }
  vtkMRMLMarkupsNode* markupsNode = model->markupsNode();
  if (!markupsNode)
    {
    return false;
    }
  vtkMRMLMarkupsNode::ControlPoint* controlPoint = markupsNode->GetNthControlPoint(controlPointIndex);
  if (!controlPoint)
    {
    return false;
    }

  // Filter by segment label
  if (!d->LabelFilter.isEmpty())
    {
    QString label = QString::fromStdString(controlPoint->Label);
    if (!label.contains(d->LabelFilter, Qt::CaseInsensitive))
      {
      return false;
      }
    }

  // Filter by segment text (label and description)
  if (!d->TextFilter.isEmpty())
    {
    bool matchFound = false;
    QString label = QString::fromStdString(controlPoint->Label);
    if (label.contains(d->TextFilter, Qt::CaseInsensitive))
      {
      matchFound = true;
      }
    if (!matchFound)
      {
      QString description = QString::fromStdString(controlPoint->Description);
      if (description.contains(d->TextFilter, Qt::CaseInsensitive))
        {
        matchFound = true;
        }
      }
    if (!matchFound)
      {
      return false;
      }
    }

  // Filter if segment state does not match one of the shown states
  bool positionStatusFilterEnabled = false;
  for (int i = 0; i < vtkMRMLMarkupsNode::PositionStatus_Last; ++i)
    {
    positionStatusFilterEnabled = d->ShowPositionStatus[i];
    if (positionStatusFilterEnabled)
      {
      break;
      }
    }

  if (positionStatusFilterEnabled)
    {
    int status = controlPoint->PositionStatus;
    if (status >= 0 && status < vtkMRMLMarkupsNode::PositionStatus_Last && !d->ShowPositionStatus[status])
      {
      return false;
      }
    }

  // All criteria were met
  return true;
}

//------------------------------------------------------------------------------
Qt::ItemFlags qMRMLMarkupsControlPointsSortFilterProxyModel::flags(const QModelIndex & index)const
{
  qMRMLMarkupsControlPointsModel* markupsControlPointsModel = qobject_cast<qMRMLMarkupsControlPointsModel*>(this->sourceModel());
  int controlPointIndex = -1; // not used for now
  int column = index.column();
  Qt::ItemFlags flags = markupsControlPointsModel->controlPointFlags(controlPointIndex, column);

  /*
  QString segmentID = this->segmentIDFromIndex(index);
  bool isSelectable = this->filterAcceptsItem(segmentID);
  QStandardItem* item = markupsControlPointsModel->itemFromControlPointIndex(segmentID, index.column());
  if (!item)
    {
    return Qt::ItemFlags();
    }

  QFlags<Qt::ItemFlag> flags = item->flags();
  if (isSelectable)
    {
    return flags | Qt::ItemIsSelectable;
    }
  else
    {
    return flags & ~Qt::ItemIsSelectable;
    }
  */
  return flags;
}

// --------------------------------------------------------------------------
void qMRMLMarkupsControlPointsSortFilterProxyModel::setHideMarkupsControlPoints(const QList<int>& segmentIDs)
{
  Q_D(qMRMLMarkupsControlPointsSortFilterProxyModel);
  d->HideMarkupsControlPoints = segmentIDs;
  this->invalidateFilter();
  emit filterModified();
}

// --------------------------------------------------------------------------
const QList<int>& qMRMLMarkupsControlPointsSortFilterProxyModel::hideMarkupsControlPoints()const
{
  Q_D(const qMRMLMarkupsControlPointsSortFilterProxyModel);
  return d->HideMarkupsControlPoints;
}
