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

#ifndef __qMRMLMarkupsControlPointsModel_h
#define __qMRMLMarkupsControlPointsModel_h

// Qt includes
#include <QStandardItemModel>

// CTK includes
#include <ctkPimpl.h>
#include <ctkVTKObject.h>

// MarkupsControlPoints includes
#include "qSlicerMarkupsModuleWidgetsExport.h"

class qMRMLMarkupsControlPointsModelPrivate;
class vtkMRMLMarkupsNode;
class vtkMRMLNode;
class vtkMRMLScene;

/// \brief Item model for MarkupsControlPoints
///
/// Associated with a vtkMRMLMarkupsNode. This model creates one model item for each control point.
class Q_SLICER_MODULE_MARKUPS_WIDGETS_EXPORT qMRMLMarkupsControlPointsModel : public QStandardItemModel
{
  Q_OBJECT
  QVTK_OBJECT

public:
  enum
  {
    SelectedColumn = 0,
    LockedColumn,
    VisibilityColumn,
    LabelColumn,
    DescriptionColumn,
    CoordinateRColumn,
    CoordinateAColumn,
    CoordinateSColumn,
    PositionStatusColumn,
    NumberOfColumns // add columns above this line
  };

  /*
  enum SegmentTableItemDataRole
  {
    SelectedRole = Qt::UserRole + 1,
    LockedRole,
    VisibilityRole,
    NameRole,
    DescriptionRole,
    CoordinateRRole,
    CoordinateARole,
    CoordinateSRole,
    PositionStatusRole,
  };
  */

  typedef QStandardItemModel Superclass;
  qMRMLMarkupsControlPointsModel(QObject *parent=nullptr);
  ~qMRMLMarkupsControlPointsModel() override;

/*  int LabelColumn()const;
  void setLabelColumn(int column);
  int visibilityColumn()const;
  void setVisibilityColumn(int column);
  int colorColumn()const;
  void setColorColumn(int column);
  int opacityColumn()const;
  void setOpacityColumn(int column);
  int statusColumn()const;
  void setStatusColumn(int column);
  int layerColumn()const;
  void setLayerColumn(int layer);

  /// Returns the segment ID for the given index
  QString segmentIDFromIndex(const QModelIndex &index)const;
  // Returns the segment ID for the given item
  QString segmentIDFromItem(QStandardItem* item)const;
  */

  // Returns the index for the given segment ID
  QModelIndex indexFromControlPointIndex(int controlPointIndex, int column=0)const;

  // Returns the item for the given segment ID
  QStandardItem* itemFromControlPointIndex(int controlPointIndex, int column=0)const;

  /// Return all the QModelIndexes (all the columns) for a given segment ID
  QModelIndexList indexes(int controlPointIndex)const;

  /// The segmentation node that is used to populate the model
  vtkMRMLMarkupsNode* markupsNode()const;
  virtual void setMarkupsNode(vtkMRMLMarkupsNode* markupsNode);

  virtual Qt::ItemFlags controlPointFlags(int controlPointIndex, int column)const;

signals:
  /// Signal requesting selecting items in the tree
  void requestSelectItems(QList<int> controlPointIndices);

protected slots:
  /// Invoked when an item in the model is changed
  virtual void onItemChanged(QStandardItem* item);

protected:
  qMRMLMarkupsControlPointsModel(qMRMLMarkupsControlPointsModelPrivate* pimpl, QObject *parent=nullptr);

  /// Removes all items and regenerates the model from the MarkupsControlPoints in the segmentation node
  virtual void rebuildFromMarkupsControlPoints();
  /// Updates all items from the MarkupsControlPoints in the segmentation model
  virtual void updateFromMarkupsControlPoints();

  /// Update QStandardItem associated using segmentID and column
  virtual void updateItemFromControlPoint(QStandardItem* item, int controlPointIndex, int column);
  /// Update QStandardItem data associated using segmentID and column
  virtual void updateItemDataFromControlPoint(QStandardItem* item, int controlPointIndex, int column);
  /// Update a segment in the MRML node using the associated QStandardItem data
  virtual void updateControlPointFromItem(QStandardItem* item);
  /// Update all of the the QStandardItem associated with a column
  void updateItemsFromColumnIndex(int column);
  /// Update all of the the QStandardItem associated with a control point
  void updateItemsFromSegmentID(int controlPointIndex);

  /// Rearrange the order of the rows to match the indices of the MarkupsControlPoints in the MRML node
  void reorderItems();

  /// Invoked when the segmentation node is modified with one of these events:
  /// vtkMRMLMarkupsNode::PointAddedEvent,
  /// vtkMRMLMarkupsNode::PointRemovedEvent,
  /// vtkMRMLMarkupsNode::PointModifiedEvent
  static void onEvent(vtkObject* caller, unsigned long event, void* clientData, void* callData);

  /// Called when a control point is added to the markups node
  virtual void onControlPointAdded(int controlPointIndex);
  /// Called when a control point is removed from the markups node
  virtual void onControlPointRemoved(int controlPointIndex);
  /// Called when a control point is modified in the markups node
  virtual void onControlPointModified(int controlPointIndex);

protected:
  QScopedPointer<qMRMLMarkupsControlPointsModelPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLMarkupsControlPointsModel);
  Q_DISABLE_COPY(qMRMLMarkupsControlPointsModel);
};

#endif
