/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

#ifndef __qMRMLMarkupsControlPointsTableView_h
#define __qMRMLMarkupsControlPointsTableView_h

// Segmentations includes
#include "qSlicerMarkupsModuleWidgetsExport.h"

// Slicer includes
#include "qSlicerWidget.h"

// CTK includes
#include <ctkPimpl.h>
#include <ctkVTKObject.h>

class qMRMLMarkupsControlPointsModel;
class qMRMLMarkupsControlPointsTableViewPrivate;
class qMRMLMarkupsControlPointsSortFilterProxyModel;
class QContextMenuEvent;
class QItemSelection;
class QStandardItem;
class QStringList;
class QTableWidgetItem;
class QTableView;
class vtkMRMLNode;
class vtkSegment;

/// \ingroup Slicer_QtModules_Segmentations_Widgets
class Q_SLICER_MODULE_MARKUPS_WIDGETS_EXPORT qMRMLMarkupsControlPointsTableView : public qSlicerWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  Q_PROPERTY(int selectionMode READ selectionMode WRITE setSelectionMode)
  Q_PROPERTY(bool headerVisible READ headerVisible WRITE setHeaderVisible)
  Q_PROPERTY(bool readOnly READ readOnly WRITE setReadOnly)
  Q_PROPERTY(bool filterBarVisible READ filterBarVisible WRITE setFilterBarVisible)
  Q_PROPERTY(QString textFilter READ textFilter WRITE setTextFilter)
  Q_PROPERTY(bool jumpToSelectedEnabled READ jumpToSelectedEnabled  WRITE setJumpToSelectedEnabled)

  typedef qSlicerWidget Superclass;
  /// Constructor
  explicit qMRMLMarkupsControlPointsTableView(QWidget* parent = nullptr);
  /// Destructor
  ~qMRMLMarkupsControlPointsTableView() override;

  /// Get segmentation MRML node
  Q_INVOKABLE vtkMRMLNode* markupsNode();

  /// Get access to the table widget to allow low-level customization
  Q_INVOKABLE QTableView* tableWidget();

  /// Return number of control points (rows) in the table
  int controlPointCount() const;

  /// Get the segment IDs of selected MarkupsControlPoints
  Q_INVOKABLE QList<int> selectedControlPointIndices();
  /// Select MarkupsControlPoints with specified IDs
  Q_INVOKABLE void setSelectedControlPointIndices(const QList<int>& indices);
  /// Clear segment selection
  Q_INVOKABLE void clearSelection();

  int selectionMode();
  bool headerVisible();
  bool readOnly();
  bool filterBarVisible();

  /// MarkupsControlPoints that have their ID listed in hideMarkupsControlPoints are
  /// not shown in the table.
  //Q_INVOKABLE void setHideMarkupsControlPointIndices(const QList<int>& indices);
  //Q_INVOKABLE QList<int> hideMarkupsControlPointIndices()const;

  /// Return list of visible segment IDs
  //Q_INVOKABLE QList<int> displayedControlPointIndices()const;

  Q_INVOKABLE qMRMLMarkupsControlPointsSortFilterProxyModel* sortFilterProxyModel()const;
  Q_INVOKABLE qMRMLMarkupsControlPointsModel* model()const;

  /// The text used to filter the MarkupsControlPoints in the table
  /// \sa setTextFilter
  QString textFilter();
  // If the specified position status should be shown in the table
  /// \sa setPositionStatusShown
  Q_INVOKABLE bool positionStatusShown(int status);

public slots:
  /// Set segmentation MRML node
  void setMarkupsNode(vtkMRMLNode* node);
  /// Set MRML scene
  void setMRMLScene(vtkMRMLScene* newScene) override;

  /// Set selection mode in the table. Input value is int for Python compatibility. Actual values are
  /// defined in QAbstractItemView::SelectionMode. For example, QAbstractItemView::NoSelection,
  /// QAbstractItemView::SingleSelection, QAbstractItemView::ExtendedSelection.
  void setSelectionMode(int mode);

  void setHeaderVisible(bool visible);
  void setReadOnly(bool aReadOnly);
  void setFilterBarVisible(bool visible);

  /// Enables automatic jumping to current control point when selection is changed.
  void setJumpToSelectedEnabled(bool enable);

  /// Jump position of all slice views to show the segment's center.
  void jumpSlices();

  /// Set the text used to filter the MarkupsControlPoints in the table
  /// \sa textFilter
  void setTextFilter(QString textFilter);

  /// Set if the specified position status should be shown in the table
  /// \sa statusShown
  void setPositionStatusShown(int status, bool shown);

  /// Set the status of the selected MarkupsControlPoints
  void setSelectedControlPointsPositionStatus(int status = -1);

  /*
  // TODO

  /// Show only selected MarkupsControlPoints
  void showOnlySelectedMarkupsControlPoints();

  /// Erase the contents of the selected MarkupsControlPoints and set the status to "Not started"
  void clearSelectedMarkupsControlPoints();

  */
  /// Returns true if automatic jump to current control point is enabled.
  bool jumpToSelectedEnabled()const;

signals:
  /// Emitted if selection changes
  void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

protected slots:
  /// Forwards selection changed events. In case of batch update of items, selected and deselected are empty.
  void onMarkupsControlPointselectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

  /// Handles when the filters on underlying sort model are modified
  void onMarkupsControlPointsFilterModified();
  /// Handles clicks on the show status buttons
  void onShowPositionStatusButtonClicked();

  /// Handles clicks on a table cell (visibility + state)
  void onMarkupsControlPointsTableClicked(const QModelIndex& modelIndex);

  /// Handle MRML scene event
  void endProcessing();

  /// Signals to save/restore segment ID selection when the model is reset
  void modelAboutToBeReset();
  void modelReset();

protected:

  /// To prevent accidentally moving out of the widget when pressing up/down arrows
  bool eventFilter(QObject* target, QEvent* event) override;

  /// Handle context menu events
  void contextMenuEvent(QContextMenuEvent* event) override;

protected:
  QScopedPointer<qMRMLMarkupsControlPointsTableViewPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLMarkupsControlPointsTableView);
  Q_DISABLE_COPY(qMRMLMarkupsControlPointsTableView);
};

#endif
