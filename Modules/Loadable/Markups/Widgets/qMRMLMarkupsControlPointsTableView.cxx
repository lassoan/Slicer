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

// Segmentations includes
#include "qMRMLMarkupsControlPointsModel.h"
#include "qMRMLMarkupsControlPointsTableView.h"
#include "qMRMLMarkupsControlPointsSortFilterProxyModel.h"
#include "ui_qMRMLMarkupsControlPointsTableView.h"
#include "vtkMRMLMarkupsNode.h"
#include "vtkSegmentation.h"
#include "vtkSegment.h"

// Markups includes
#include <vtkSlicerMarkupsLogic.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLLabelMapVolumeNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>

// Slicer includes
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerAbstractCoreModule.h>
#include <qMRMLItemDelegate.h>
#include <qMRMLSliceWidget.h>

// VTK includes
#include <vtkWeakPointer.h>

// CTK includes
#include <ctkPimpl.h>

// Qt includes
#include <QAction>
#include <QContextMenuEvent>
#include <QDebug>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QStringList>
#include <QToolButton>

#define ID_PROPERTY "ID"
#define VISIBILITY_PROPERTY "Visible"
#define STATUS_PROPERTY "Status"

//-----------------------------------------------------------------------------
struct SegmentListFilterParameters
{
  bool ShowPositionStatusFilter[vtkMRMLMarkupsNode::PositionStatus_Last];
  QString TextFilter;

  const char AttributeSeparator = ';';
  const char KeyValueSeparator = ':';
  const char ValueSeparator = ',';

  const char* TextFilterKey = "text";
  const char* StatusFilterKey = "status";

  SegmentListFilterParameters()
  {
    this->init();
  }

  void init()
  {
    this->TextFilter = "";
    for (int i = 0; i < vtkMRMLMarkupsNode::PositionStatus_Last; ++i)
      {
      this->ShowPositionStatusFilter[i] = false;
      }
  }

  /// Returns a string representation of the data in the SegmentListFilterParameters
  QString serializeStatusFilter()
  {
    std::stringstream statusFilterStringStream;
    statusFilterStringStream << this->TextFilterKey << this->KeyValueSeparator << this->TextFilter.toStdString() << this->AttributeSeparator;
    statusFilterStringStream << this->StatusFilterKey << this->KeyValueSeparator;
    bool firstElement = true;
    for (int i = 0; i < vtkMRMLMarkupsNode::PositionStatus_Last; ++i)
      {
      if (!this->ShowPositionStatusFilter[i])
        {
        continue;
        }

      if (!firstElement)
        {
        statusFilterStringStream << this->ValueSeparator;
        }
      firstElement = false;

      statusFilterStringStream << vtkMRMLMarkupsNode::GetPositionStatusAsString(i);
      }
    return QString::fromStdString(statusFilterStringStream.str());
  };

  /// Converts a string representation to the underlying values, and sets them in the SegmentListFilterParameters
  void deserializeStatusFilter(QString filterString)
  {
    this->init();

    QStringList attributes = filterString.split(this->AttributeSeparator);
    for (QString attribute : attributes)
      {
      QStringList keyValue = attribute.split(this->KeyValueSeparator);
      if (keyValue.size() != 2)
        {
        continue;
        }
      QString key = keyValue[0];
      QString value = keyValue[1];

      if (key == this->TextFilterKey)
        {
        this->TextFilter = value;
        }
      else if(key == this->StatusFilterKey)
        {
        QStringList statusFilters = value.split(this->ValueSeparator);
        for (QString statusString : statusFilters)
          {
          int status = vtkMRMLMarkupsNode::GetPositionStatusFromString(statusString.toStdString().c_str());
          if (status < 0 || status >= vtkMRMLMarkupsNode::PositionStatus_Last)
            {
            continue;
            }
          this->ShowPositionStatusFilter[status] = true;
          }
        }
      }
  }
};

//-----------------------------------------------------------------------------
class qMRMLMarkupsControlPointsTableViewPrivate: public Ui_qMRMLMarkupsControlPointsTableView
{
  Q_DECLARE_PUBLIC(qMRMLMarkupsControlPointsTableView);

protected:
  qMRMLMarkupsControlPointsTableView* const q_ptr;
public:
  qMRMLMarkupsControlPointsTableViewPrivate(qMRMLMarkupsControlPointsTableView& object);
  void init();

  /// Sets table message and takes care of the visibility of the label
  void setMessage(const QString& message);

public:
  /// Segmentation MRML node containing shown MarkupsControlPoints
  vtkWeakPointer<vtkMRMLMarkupsNode> MarkupsNode;

  /// Flag determining whether the long-press per-view segment visibility options are available
  bool AdvancedSegmentVisibility;

  bool IsFilterBarVisible;

  qMRMLMarkupsControlPointsModel* Model;
  qMRMLMarkupsControlPointsSortFilterProxyModel* SortFilterModel;

  QIcon StatusIcons[vtkMRMLMarkupsNode::PositionStatus_Last];
  QPushButton* ShowStatusButtons[vtkMRMLMarkupsNode::PositionStatus_Last];

  bool JumpToSelectedEnabled;
  bool CenteredJump = true;
  int ViewGroup = -1;

  /// When the model is being reset, the blocking state and selected segment IDs are stored here.
  bool WasBlockingTableSignalsBeforeReset;
  QList<int> SelectedControlPointIndicesBeforeReset;

  vtkWeakPointer<vtkSlicerMarkupsLogic> MarkupsLogic;
};

//-----------------------------------------------------------------------------
qMRMLMarkupsControlPointsTableViewPrivate::qMRMLMarkupsControlPointsTableViewPrivate(qMRMLMarkupsControlPointsTableView& object)
  : q_ptr(&object)
  , MarkupsNode(nullptr)
  , AdvancedSegmentVisibility(false)
  , IsFilterBarVisible(false)
  , Model(nullptr)
  , SortFilterModel(nullptr)
  , JumpToSelectedEnabled(false)
  , WasBlockingTableSignalsBeforeReset(false)
{
  for (int status = 0; status < vtkMRMLMarkupsNode::PositionStatus_Last; ++status)
    {
    this->ShowStatusButtons[status] = nullptr;
    }
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableViewPrivate::init()
{
  Q_Q(qMRMLMarkupsControlPointsTableView);

  this->MarkupsLogic = vtkSlicerMarkupsLogic::SafeDownCast(q->moduleLogic("Markups"));
  if (!this->MarkupsLogic)
    {
    qCritical() << Q_FUNC_INFO << ": Markups module is not found, some markup manipulation features will not be available";
    }

  this->setupUi(q);

  this->Model = new qMRMLMarkupsControlPointsModel(this->MarkupsControlPointsTable);
  this->SortFilterModel = new qMRMLMarkupsControlPointsSortFilterProxyModel(this->MarkupsControlPointsTable);
  this->SortFilterModel->setSourceModel(this->Model);
  this->MarkupsControlPointsTable->setModel(this->SortFilterModel);

  for (int status = 0; status < vtkMRMLMarkupsNode::PositionStatus_Last; ++status)
    {
    switch (status)
      {
      case vtkMRMLMarkupsNode::PositionUndefined:
        this->ShowStatusButtons[status] = this->ShowPositionUndefinedButton;
        this->StatusIcons[status] = QIcon(":/Icons/XSmall/MarkupsUndefined.png");
        break;
      case vtkMRMLMarkupsNode::PositionPreview:
        this->ShowStatusButtons[status] = this->ShowPositionPreviewButton;
        this->StatusIcons[status] = QIcon(":/Icons/XSmall/MarkupsInProgress.png");
        break;
      case vtkMRMLMarkupsNode::PositionDefined:
        this->ShowStatusButtons[status] = this->ShowPositionDefinedButton;
        this->StatusIcons[status] = QIcon(":/Icons/XSmall/MarkupsDefined.png");
        break;
      case vtkMRMLMarkupsNode::PositionMissing:
        this->ShowStatusButtons[status] = this->ShowPositionMissingButton;
        this->StatusIcons[status] = QIcon(":/Icons/XSmall/MarkupsMissing.png");
        break;
      default:
        this->ShowStatusButtons[status] = nullptr;
        this->StatusIcons[status] = QIcon();
      }
    if (this->ShowStatusButtons[status])
      {
      this->ShowStatusButtons[status]->setProperty(STATUS_PROPERTY, status);
      }
    }

  // Hide filter bar to simplify default GUI. User can enable to handle many MarkupsControlPoints
  q->setFilterBarVisible(false);

  this->setMessage(QString());

  this->MarkupsControlPointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  /*
  this->MarkupsControlPointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  this->MarkupsControlPointsTable->horizontalHeader()->setSectionResizeMode(qMRMLMarkupsControlPointsModel::LabelColumn, QHeaderView::Stretch);
  this->MarkupsControlPointsTable->horizontalHeader()->setStretchLastSection(false);
  this->MarkupsControlPointsTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  */

  // Select rows
  this->MarkupsControlPointsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Unset read-only by default (edit triggers are double click and edit key press)
  q->setReadOnly(false);

  // Make connections
  QObject::connect(this->MarkupsControlPointsTable->selectionModel(), &QItemSelectionModel::selectionChanged,
    q, &qMRMLMarkupsControlPointsTableView::onMarkupsControlPointselectionChanged);
  QObject::connect(this->Model, &QAbstractItemModel::modelAboutToBeReset, q, &qMRMLMarkupsControlPointsTableView::modelAboutToBeReset);
  QObject::connect(this->Model, &QAbstractItemModel::modelReset, q, &qMRMLMarkupsControlPointsTableView::modelReset);
  QObject::connect(this->MarkupsControlPointsTable, &QTableView::clicked,
    q, &qMRMLMarkupsControlPointsTableView::onMarkupsControlPointsTableClicked);
  QObject::connect(this->FilterLineEdit, &ctkSearchBox::textEdited,
    this->SortFilterModel, &qMRMLMarkupsControlPointsSortFilterProxyModel::setTextFilter);
  for (QPushButton* button : this->ShowStatusButtons)
    {
    if (!button)
      {
      continue;
      }
    QObject::connect(button, &QToolButton::clicked, q, &qMRMLMarkupsControlPointsTableView::onShowPositionStatusButtonClicked);
    }
  QObject::connect(this->SortFilterModel, &qMRMLMarkupsControlPointsSortFilterProxyModel::filterModified,
    q, &qMRMLMarkupsControlPointsTableView::onMarkupsControlPointsFilterModified);

  // Set item delegate to handle color and opacity changes
  this->MarkupsControlPointsTable->installEventFilter(q);
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableViewPrivate::setMessage(const QString& message)
{
  this->MarkupsControlPointsTableMessageLabel->setVisible(!message.isEmpty());
  this->MarkupsControlPointsTableMessageLabel->setText(message);
}

CTK_GET_CPP(qMRMLMarkupsControlPointsTableView, bool, jumpToSelectedEnabled, JumpToSelectedEnabled);
CTK_SET_CPP(qMRMLMarkupsControlPointsTableView, bool, setJumpToSelectedEnabled, JumpToSelectedEnabled);

//-----------------------------------------------------------------------------
// qMRMLMarkupsControlPointsTableView methods

//-----------------------------------------------------------------------------
qMRMLMarkupsControlPointsTableView::qMRMLMarkupsControlPointsTableView(QWidget* _parent)
  : qSlicerWidget(_parent)
  , d_ptr(new qMRMLMarkupsControlPointsTableViewPrivate(*this))
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->init();
}

//-----------------------------------------------------------------------------
qMRMLMarkupsControlPointsTableView::~qMRMLMarkupsControlPointsTableView() = default;

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setMarkupsNode(vtkMRMLNode* node)
{
  Q_D(qMRMLMarkupsControlPointsTableView);

  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(node);
  d->MarkupsNode = markupsNode;
  d->Model->setMarkupsNode(d->MarkupsNode);
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::onShowPositionStatusButtonClicked()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  QPushButton* button = qobject_cast<QPushButton*>(sender());
  if (!button)
    {
    return;
    }
  int status = button->property(STATUS_PROPERTY).toInt();
  d->SortFilterModel->setShowPositionStatus(status, button->isChecked());
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::onMarkupsControlPointsFilterModified()
{
  Q_D(qMRMLMarkupsControlPointsTableView);

  QString textFilter = d->SortFilterModel->textFilter();
  if (d->FilterLineEdit->text() != textFilter)
    {
    d->FilterLineEdit->setText(textFilter);
    }

  for (int status = 0; status < vtkMRMLMarkupsNode::PositionStatus_Last; ++status)
    {
    QPushButton* button = d->ShowStatusButtons[status];
    if (!button)
      {
      continue;
      }
    button->setChecked(d->SortFilterModel->showPositionStatus(status));
    }
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::onMarkupsControlPointsTableClicked(const QModelIndex& modelIndex)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  int controlPointIndex = d->SortFilterModel->controlPointIndexFromModelIndex(modelIndex);
  if (!d->MarkupsNode || controlPointIndex<0)
    {
    return;
    }

  Qt::ItemFlags flags = d->SortFilterModel->flags(modelIndex);
  if (!flags.testFlag(Qt::ItemIsSelectable))
    {
    return;
    }

  if (modelIndex.column() == qMRMLMarkupsControlPointsModel::SelectedColumn)
    {
    int selected = d->SortFilterModel->data(modelIndex, Qt::UserRole).toBool();
    d->MarkupsNode->SetNthControlPointSelected(controlPointIndex, !selected);
    }
  else if (modelIndex.column() == qMRMLMarkupsControlPointsModel::LockedColumn)
    {
    int locked = d->SortFilterModel->data(modelIndex, Qt::UserRole).toBool();
    d->MarkupsNode->SetNthControlPointLocked(controlPointIndex, !locked);
    }
  else if (modelIndex.column() == qMRMLMarkupsControlPointsModel::VisibilityColumn)
    {
    int visible = d->SortFilterModel->data(modelIndex, Qt::UserRole).toBool();
    d->MarkupsNode->SetNthControlPointVisibility(controlPointIndex, !visible);
    }
  else if (modelIndex.column() == qMRMLMarkupsControlPointsModel::PositionStatusColumn)
    {
    int positionStatus = d->SortFilterModel->data(modelIndex, Qt::UserRole).toInt();
    // TODO    d->MarkupsNode->SetNthControlPointVisibility(controlPointIndex, positionStatus);
    }
}

//---------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setMRMLScene(vtkMRMLScene* newScene)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (newScene == this->mrmlScene())
    {
    return;
    }

  if (d->MarkupsNode && newScene != d->MarkupsNode->GetScene())
    {
    this->setMarkupsNode(nullptr);
    }

  Superclass::setMRMLScene(newScene);
}

//-----------------------------------------------------------------------------
vtkMRMLNode* qMRMLMarkupsControlPointsTableView::markupsNode()
{
  Q_D(qMRMLMarkupsControlPointsTableView);

  return d->MarkupsNode;
}

//--------------------------------------------------------------------------
qMRMLMarkupsControlPointsSortFilterProxyModel* qMRMLMarkupsControlPointsTableView::sortFilterProxyModel()const
{
  Q_D(const qMRMLMarkupsControlPointsTableView);
  if (!d->SortFilterModel)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid sort filter proxy model";
    return nullptr;
    }
  return d->SortFilterModel;
}

//--------------------------------------------------------------------------
qMRMLMarkupsControlPointsModel* qMRMLMarkupsControlPointsTableView::model()const
{
  Q_D(const qMRMLMarkupsControlPointsTableView);
  if (!d->Model)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid data model";
    return nullptr;
    }
  return d->Model;
}

//-----------------------------------------------------------------------------
QTableView* qMRMLMarkupsControlPointsTableView::tableWidget()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  return d->MarkupsControlPointsTable;
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::onMarkupsControlPointselectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (d->JumpToSelectedEnabled)
    {
    this->jumpSlices();
    }
  if (d->MarkupsControlPointsTable->signalsBlocked())
    {
    return;
    }
  emit selectionChanged(selected, deselected);
}

//-----------------------------------------------------------------------------
int qMRMLMarkupsControlPointsTableView::controlPointCount() const
{
  Q_D(const qMRMLMarkupsControlPointsTableView);

  return d->Model->rowCount();
}

//-----------------------------------------------------------------------------
QList<int> qMRMLMarkupsControlPointsTableView::selectedControlPointIndices()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (!d->MarkupsControlPointsTable->selectionModel()->hasSelection())
    {
    return QList<int>();
    }

  QList<int> selectedControlPointIndices;
  for (int row = 0; row < d->SortFilterModel->rowCount(); ++row)
    {
    if (!d->MarkupsControlPointsTable->selectionModel()->isRowSelected(row, QModelIndex()))
      {
      continue;
      }
    selectedControlPointIndices << d->SortFilterModel->controlPointIndexFromModelIndex(d->SortFilterModel->index(row, 0));
    }
  return selectedControlPointIndices;
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setSelectedControlPointIndices(const QList<int>& indices)
{
  Q_D(qMRMLMarkupsControlPointsTableView);

  if (!d->MarkupsNode && !indices.empty())
    {
    qCritical() << Q_FUNC_INFO << " failed: segmentation node is not set";
    return;
    }
  if (indices == this->selectedControlPointIndices())
    {
    return;
    }

  bool validSelection = false;
  MRMLNodeModifyBlocker blocker(d->MarkupsNode);
  // First segment selection should also clear other selections
  QItemSelectionModel::SelectionFlag itemSelectionFlag = QItemSelectionModel::ClearAndSelect;
  for (int controlPointIndex : indices)
    {
    QModelIndex index = d->SortFilterModel->modelIndexFromControlPointIndex(controlPointIndex);
    if (!index.isValid())
      {
      continue;
      }
    validSelection = true;
    QItemSelectionModel::QItemSelectionModel::SelectionFlags flags = QFlags<QItemSelectionModel::SelectionFlag>();
    flags.setFlag(itemSelectionFlag);
    flags.setFlag(QItemSelectionModel::Rows);
    d->MarkupsControlPointsTable->selectionModel()->select(index, flags);
    // Afther the first segment, we append to the current selection
    itemSelectionFlag = QItemSelectionModel::Select;
    }

  if (!validSelection)
    {
    // The list of segment IDs was either empty, or all IDs were invalid.
    d->MarkupsControlPointsTable->selectionModel()->clearSelection();
    }
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::clearSelection()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->MarkupsControlPointsTable->clearSelection();
}

//------------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsTableView::eventFilter(QObject* target, QEvent* event)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (target == d->MarkupsControlPointsTable)
    {
    // Prevent giving the focus to the previous/next widget if arrow keys are used
    // at the edge of the table (without this: if the current cell is in the top
    // row and user press the Up key, the focus goes from the table to the previous
    // widget in the tab order)
    if (event->type() == QEvent::KeyPress)
      {
      QKeyEvent* keyEvent = static_cast<QKeyEvent *>(event);
      QAbstractItemModel* model = d->MarkupsControlPointsTable->model();
      QModelIndex currentIndex = d->MarkupsControlPointsTable->currentIndex();

      if (model && (
        (keyEvent->key() == Qt::Key_Left && currentIndex.column() == 0)
        || (keyEvent->key() == Qt::Key_Up && currentIndex.row() == 0)
        || (keyEvent->key() == Qt::Key_Right && currentIndex.column() == model->columnCount() - 1)
        || (keyEvent->key() == Qt::Key_Down && currentIndex.row() == model->rowCount() - 1)))
        {
        return true;
        }
      }
    }
  return this->QWidget::eventFilter(target, event);
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::endProcessing()
{
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setSelectionMode(int mode)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->MarkupsControlPointsTable->setSelectionMode(static_cast<QAbstractItemView::SelectionMode>(mode));
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setHeaderVisible(bool visible)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->MarkupsControlPointsTable->horizontalHeader()->setVisible(visible);
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setReadOnly(bool aReadOnly)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (aReadOnly)
    {
    d->MarkupsControlPointsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
  else
    {
    d->MarkupsControlPointsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    }
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setFilterBarVisible(bool visible)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->FilterBar->setVisible(visible);
  d->IsFilterBarVisible = visible;
  d->SortFilterModel->setFilterEnabled(visible);
}

//------------------------------------------------------------------------------
int qMRMLMarkupsControlPointsTableView::selectionMode()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  return d->MarkupsControlPointsTable->selectionMode();
}

//------------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsTableView::headerVisible()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  return d->MarkupsControlPointsTable->horizontalHeader()->isVisible();
}

//------------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsTableView::readOnly()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  return (d->MarkupsControlPointsTable->editTriggers() == QAbstractItemView::NoEditTriggers);
}

//------------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsTableView::filterBarVisible()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  return d->FilterBar->isVisible();
}

//------------------------------------------------------------------------------
QString qMRMLMarkupsControlPointsTableView::textFilter()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  return d->SortFilterModel->textFilter();
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setTextFilter(QString filter)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->SortFilterModel->setTextFilter(filter);
}

//------------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsTableView::positionStatusShown(int status)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (status < 0 || status >= vtkMRMLMarkupsNode::PositionStatus_Last)
    {
    return false;
    }

  QPushButton* button = d->ShowStatusButtons[status];
  if (!button)
    {
    return false;
    }
  return button->isChecked();
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setPositionStatusShown(int status, bool shown)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (status < 0 || status >= vtkMRMLMarkupsNode::PositionStatus_Last)
    {
    return;
    }

  QPushButton* button = d->ShowStatusButtons[status];
  if (!button)
    {
    return;
    }
  button->setChecked(shown);
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::contextMenuEvent(QContextMenuEvent* event)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  /*TODO

  QMenu* contextMenu = new QMenu(this);

  QStringList selectedSegmentIDs = this->selectedSegmentIDs();

  if (selectedSegmentIDs.size() > 0)
    {
    QAction* showOnlySelectedAction = new QAction("Show only selected MarkupsControlPoints", this);
    QObject::connect(showOnlySelectedAction, SIGNAL(triggered()), this, SLOT(showOnlySelectedMarkupsControlPoints()));
    contextMenu->addAction(showOnlySelectedAction);

    contextMenu->addSeparator();

    QAction* jumpSlicesAction = new QAction("Jump slices", this);
    QObject::connect(jumpSlicesAction, SIGNAL(triggered()), this, SLOT(jumpSlices()));
    contextMenu->addAction(jumpSlicesAction);

    contextMenu->addSeparator();

    QAction* moveUpAction = new QAction("Move selected MarkupsControlPoints up", this);
    QObject::connect(moveUpAction, SIGNAL(triggered()), this, SLOT(moveSelectedMarkupsControlPointsUp()));
    contextMenu->addAction(moveUpAction);

    QAction* moveDownAction = new QAction("Move selected MarkupsControlPoints down", this);
    QObject::connect(moveDownAction, SIGNAL(triggered()), this, SLOT(moveSelectedMarkupsControlPointsDown()));
    contextMenu->addAction(moveDownAction);
    }

  contextMenu->addSeparator();

  QAction* showFilterAction = new QAction("Show filter bar", this);
  showFilterAction->setCheckable(true);
  showFilterAction->setChecked(d->FilterBar->isVisible());
  QObject::connect(showFilterAction, SIGNAL(triggered(bool)), this, SLOT(setFilterBarVisible(bool)));
  contextMenu->addAction(showFilterAction);

  QAction* showLayerColumnAction = new QAction("Show layer column", this);
  showLayerColumnAction->setCheckable(true);
  showLayerColumnAction->setChecked(this->layerColumnVisible());
  QObject::connect(showLayerColumnAction, SIGNAL(triggered(bool)), this, SLOT(setLayerColumnVisible(bool)));
  contextMenu->addAction(showLayerColumnAction);

  QModelIndex index = d->MarkupsControlPointsTable->indexAt(d->MarkupsControlPointsTable->viewport()->mapFromGlobal(event->globalPos()));
  if (d->AdvancedSegmentVisibility && index.isValid())
    {
    QString segmentID = d->SortFilterModel->segmentIDFromIndex(index);

    // Get segment display properties
    vtkMRMLSegmentationDisplayNode::SegmentDisplayProperties properties;
    vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(d->MarkupsNode->GetDisplayNode());
    if (displayNode)
      {
      displayNode->GetSegmentDisplayProperties(segmentID.toUtf8().constData(), properties);
      }

    contextMenu->addSeparator();

    QAction* visibility3DAction = new QAction("Show in 3D", this);
    visibility3DAction->setCheckable(true);
    visibility3DAction->setChecked(properties.Visible3D);
    visibility3DAction->setProperty(ID_PROPERTY, segmentID);
    QObject::connect(visibility3DAction, SIGNAL(triggered(bool)), this, SLOT(onVisibility3DActionToggled(bool)));
    contextMenu->addAction(visibility3DAction);

    QAction* visibility2DFillAction = new QAction("Show in 2D as fill", this);
    visibility2DFillAction->setCheckable(true);
    visibility2DFillAction->setChecked(properties.Visible2DFill);
    visibility2DFillAction->setProperty(ID_PROPERTY, segmentID);
    connect(visibility2DFillAction, SIGNAL(triggered(bool)), this, SLOT(onVisibility2DFillActionToggled(bool)));
    contextMenu->addAction(visibility2DFillAction);

    QAction* visibility2DOutlineAction = new QAction("Show in 2D as outline", this);
    visibility2DOutlineAction->setCheckable(true);
    visibility2DOutlineAction->setChecked(properties.Visible2DOutline);
    visibility2DOutlineAction->setProperty(ID_PROPERTY, segmentID);
    connect(visibility2DOutlineAction, SIGNAL(triggered(bool)), this, SLOT(onVisibility2DOutlineActionToggled(bool)));
    contextMenu->addAction(visibility2DOutlineAction);
    }

  if (selectedSegmentIDs.size() > 0)
    {
    contextMenu->addSeparator();
    for (int status = 0; status < vtkMRMLMarkupsNode::PositionStatus_Last; ++status)
      {
      QString name = vtkSlicerSegmentationsModuleLogic::GetMarkupsControlPointstatusAsHumanReadableString(status);
      QIcon icon = d->StatusIcons[status];

      QAction* setStatusAction = new QAction(name);
      setStatusAction->setIcon(icon);
      setStatusAction->setProperty(STATUS_PROPERTY, status);
      QObject::connect(setStatusAction, SIGNAL(triggered()), this, SLOT(setSelectedMarkupsControlPointsStatus()));
      contextMenu->addAction(setStatusAction);
      }

    contextMenu->addSeparator();
    QAction* clearSelectedSegmentAction = new QAction("Clear selected MarkupsControlPoints", this);
    QObject::connect(clearSelectedSegmentAction, SIGNAL(triggered()), this, SLOT(clearSelectedMarkupsControlPoints()));
    contextMenu->addAction(clearSelectedSegmentAction);
    }

  contextMenu->popup(event->globalPos());
  */
}

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setSelectedControlPointsPositionStatus(int aStatus)
{
  Q_D(qMRMLMarkupsControlPointsTableView);

  int status = aStatus;
  if (status == -1)
    {
    QAction* setStatusAction = qobject_cast<QAction*>(sender());
    Q_ASSERT(setStatusAction);
    if (!setStatusAction)
      {
      return;
      }
    status = setStatusAction->property(STATUS_PROPERTY).toInt();
    }

  if (!d->MarkupsNode)
    {
    qCritical() << Q_FUNC_INFO << "Invalid segmentation node";
    return;
    }

  MRMLNodeModifyBlocker blocker(d->MarkupsNode);
  QList<int> selectedControlPointIndices = this->selectedControlPointIndices();
  for (int controlPointIndex : selectedControlPointIndices)
    {
    // TODO: d->MarkupsNode->SetNthControlPointPositionStatus(controlPointIndex, status);
    }
}

/*
//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::showOnlySelectedMarkupsControlPoints()
{
  QStringList selectedSegmentIDs = this->selectedSegmentIDs();
  if (selectedSegmentIDs.size() == 0)
    {
    qWarning() << Q_FUNC_INFO << ": No segment selected";
    return;
    }

  Q_D(qMRMLMarkupsControlPointsTableView);
  if (!d->MarkupsNode)
    {
    qCritical() << Q_FUNC_INFO << ": No current segmentation node";
    return;
    }
  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
    d->MarkupsNode->GetDisplayNode() );
  if (!displayNode)
    {
    qCritical() << Q_FUNC_INFO << ": No display node for segmentation " << d->MarkupsNode->GetName();
    return;
    }

  vtkSegmentation* segmentation = d->MarkupsNode->GetSegmentation();
  if (!segmentation)
    {
    qCritical() << Q_FUNC_INFO << ": No segmentation";
    return;
    }

  std::vector<std::string> displayedSegmentIDs;
  segmentation->GetSegmentIDs(displayedSegmentIDs);

  QStringList hiddenSegmentIDs = d->SortFilterModel->hideMarkupsControlPoints();

  // Hide all MarkupsControlPoints except the selected ones
  MRMLNodeModifyBlocker blocker(displayNode);
  for (std::string displayedID : displayedSegmentIDs)
    {
    QString segmentID = QString::fromStdString(displayedID);
    if (hiddenSegmentIDs.contains(segmentID))
      {
      continue;
      }

    bool visible = false;
    if (selectedSegmentIDs.contains(segmentID))
      {
      visible = true;
      }

    displayNode->SetSegmentVisibility(displayedID, visible);
    }
}

*/

//------------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::jumpSlices()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  if (!d->MarkupsNode)
    {
    return;
    }
  int controlPointIndex = d->SortFilterModel->controlPointIndexFromModelIndex(
    d->MarkupsControlPointsTable->currentIndex());
  d->MarkupsLogic->JumpSlicesToNthPointInMarkup(d->MarkupsNode->GetID(), controlPointIndex, d->CenteredJump, d->ViewGroup);
}

/*
// --------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::setHideMarkupsControlPoints(const QStringList& segmentIDs)
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->SortFilterModel->setHideMarkupsControlPoints(segmentIDs);
}

// --------------------------------------------------------------------------
QStringList qMRMLMarkupsControlPointsTableView::hideMarkupsControlPoints()const
{
  Q_D(const qMRMLMarkupsControlPointsTableView);
  return d->SortFilterModel->hideMarkupsControlPoints();
}

// --------------------------------------------------------------------------
QStringList qMRMLMarkupsControlPointsTableView::displayedSegmentIDs()const
{
  Q_D(const qMRMLMarkupsControlPointsTableView);

  QStringList displayedSegmentIDs;
  for (int row = 0; row < d->SortFilterModel->rowCount(); ++row)
    {
    displayedSegmentIDs << d->SortFilterModel->segmentIDFromIndex(d->SortFilterModel->index(row, 0));
    }
  return displayedSegmentIDs;
}
*/

// --------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::modelAboutToBeReset()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->WasBlockingTableSignalsBeforeReset = d->MarkupsControlPointsTable->blockSignals(true);
  d->SelectedControlPointIndicesBeforeReset = this->selectedControlPointIndices();
}

// --------------------------------------------------------------------------
void qMRMLMarkupsControlPointsTableView::modelReset()
{
  Q_D(qMRMLMarkupsControlPointsTableView);
  d->MarkupsControlPointsTable->blockSignals(d->WasBlockingTableSignalsBeforeReset);
  this->setSelectedControlPointIndices(d->SelectedControlPointIndicesBeforeReset);
  d->SelectedControlPointIndicesBeforeReset.clear();
}
