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

#ifndef __qMRMLMarkupsControlPointsSortFilterProxyModel_h
#define __qMRMLMarkupsControlPointsSortFilterProxyModel_h

// Segmentation includes
#include "qSlicerMarkupsModuleWidgetsExport.h"

// Qt includes
#include <QSortFilterProxyModel>

// CTK includes
#include <ctkVTKObject.h>
#include <ctkPimpl.h>

class qMRMLMarkupsControlPointsSortFilterProxyModelPrivate;
class QStandardItem;
class vtkMRMLMarkupsNode;
class vtkMRMLScene;

/// \ingroup Slicer_QtModules_Segmentations
class Q_SLICER_MODULE_MARKUPS_WIDGETS_EXPORT qMRMLMarkupsControlPointsSortFilterProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT
  QVTK_OBJECT

  /// Whether the filter should be applied
  Q_PROPERTY(bool filterEnabled READ filterEnabled WRITE setFilterEnabled)
  /// Filter to show only items that contain the string in their label. Empty by default.
  Q_PROPERTY(QString labelFilter READ labelFilter WRITE setLabelFilter)
  /// Filter to show only items that contain the string in any property (label or description). Empty by default.
  Q_PROPERTY(QString textFilter READ textFilter WRITE setTextFilter)

public:
  typedef QSortFilterProxyModel Superclass;
  qMRMLMarkupsControlPointsSortFilterProxyModel(QObject *parent=nullptr);
  ~qMRMLMarkupsControlPointsSortFilterProxyModel() override;

  /// Returns the markups node in the source model
  Q_INVOKABLE vtkMRMLMarkupsNode* markupsNode()const;

  /// MarkupsControlPoints that have their indices listed in hideMarkupsControlPoints are
  /// not shown in the table.
  Q_INVOKABLE void setHideMarkupsControlPoints(const QList<int>& controlPointIndices);
  Q_INVOKABLE const QList<int>& hideMarkupsControlPoints()const;

  bool filterEnabled()const;
  QString labelFilter()const;
  QString textFilter()const;

  /// Filter to show MarkupsControlPoints with the specified state
  /// If the flags for all states are false, than no filtering is performed
  /// The list of available status is in vtkSlicerSegmentationsModuleLogic::MarkupsControlPointstatus
  /// \sa setShowStatus
  Q_INVOKABLE bool showPositionStatus(int status) const;

  /// Retrieve an index for a given a segment ID
  Q_INVOKABLE QModelIndex modelIndexFromControlPointIndex(int controlPointIndex, int column = 0)const;

  /// Retrieve an index for a given a segment ID
  Q_INVOKABLE int controlPointIndexFromModelIndex(QModelIndex modelIndex)const;

  /// Returns true if the item in the row indicated by the given sourceRow and
  /// sourceParent should be included in the model; otherwise returns false.
  /// This method tests each item via \a filterAcceptsItem
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent)const override;

  /// Filters items to decide which to display in the view
  virtual bool filterAcceptsItem(int controlPointIndex)const;

  /// Returns the flags for the current index
  Qt::ItemFlags flags(const QModelIndex & index)const override;

  /// Set filter to show MarkupsControlPoints with the specified state
  /// If the flags for all states are false, than no filtering is performed
  /// The list of available status is in vtkMRMLMarkupsModelNode.
  /// \sa showStatus
  Q_INVOKABLE void setShowPositionStatus(int status, bool shown);

public slots:
  void setFilterEnabled(bool filterEnabled);
  void setLabelFilter(QString filter);
  void setTextFilter(QString filter);

signals:
  /// Emitted when one of the filter parameters are modified
  /// \sa setShowStatus setTextFilter setLabelFilter
  void filterModified();

protected:
  QStandardItem* sourceItem(const QModelIndex& index)const;

protected:
  QScopedPointer<qMRMLMarkupsControlPointsSortFilterProxyModelPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLMarkupsControlPointsSortFilterProxyModel);
  Q_DISABLE_COPY(qMRMLMarkupsControlPointsSortFilterProxyModel);
};

#endif
