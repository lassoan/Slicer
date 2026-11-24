/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Qt includes
#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QStringBuilder>

// C++ includes
#include <cmath>

// Slicer includes
#include "qSlicerTransformsModuleWidget.h"
#include "ui_qSlicerTransformsModuleWidget.h"

// vtkSlicerLogic includes
#include "vtkSlicerTransformLogic.h"

// Transforms widgets includes
#include "qMRMLTransformAbstractOptionsWidget.h"
#include "qMRMLTransformOptionsWidgetsFactory.h"

// MRMLWidgets includes
#include <qMRMLUtils.h>

// MRML includes
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSubjectHierarchyNode.h"
#include "vtkMRMLTransformNode.h"
#include "vtkMRMLTransformDisplayNode.h"
#include "vtkMRMLVectorVolumeNode.h"

// VTK includes
#include <vtkSmartPointer.h>

//-----------------------------------------------------------------------------
class qSlicerTransformsModuleWidgetPrivate : public Ui_qSlicerTransformsModuleWidget
{
  Q_DECLARE_PUBLIC(qSlicerTransformsModuleWidget);

protected:
  qSlicerTransformsModuleWidget* const q_ptr;

public:
  qSlicerTransformsModuleWidgetPrivate(qSlicerTransformsModuleWidget& object);
  static QList<vtkSmartPointer<vtkMRMLTransformableNode>> getSelectedNodes(qMRMLTreeView* tree);
  vtkSlicerTransformLogic* logic() const;
  vtkMRMLTransformNode* MRMLTransformNode;

  /// Create an instance of each options widget that is registered in qMRMLTransformOptionsWidgetsFactory
  /// and add it to the section that it is shown in, in the order of registration
  void updateTransformOptionsWidgets();
  /// Set the transform node in the options widgets and show those that can manage it
  void updateTransformOptionsWidgetsFromMRML(bool transformNodeChanged);
  /// Notify the options widgets that the transform of the node is about to be replaced (set to identity or inverted)
  void transformAboutToBeReplaced();
  QList<qMRMLTransformAbstractOptionsWidget*> TransformOptionsWidgets;
};

//-----------------------------------------------------------------------------
qSlicerTransformsModuleWidgetPrivate::qSlicerTransformsModuleWidgetPrivate(qSlicerTransformsModuleWidget& object)
  : q_ptr(&object)
{
  this->MRMLTransformNode = nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetPrivate::updateTransformOptionsWidgets()
{
  Q_Q(qSlicerTransformsModuleWidget);
  for (qMRMLTransformAbstractOptionsWidget* widget : this->TransformOptionsWidgets)
  {
    delete widget;
  }
  this->TransformOptionsWidgets.clear();

  // The widgets of the Edit section are at the top of the section, above the buttons, in the order of registration.
  // The Transforms module registers the widget of linear transforms (matrix and sliders) first.
  int editSectionIndex = 0;
  qMRMLTransformOptionsWidgetsFactory* factory = qMRMLTransformOptionsWidgetsFactory::instance();
  for (const QString& widgetClassName : factory->registeredOptionsWidgetsClassNames())
  {
    qMRMLTransformAbstractOptionsWidget* widget = factory->createWidget(widgetClassName);
    if (!widget)
    {
      continue;
    }
    this->TransformOptionsWidgets.append(widget);
    this->verticalLayout_3->insertWidget(editSectionIndex++, widget);
    // Shortcuts of the widget (for example, copy and paste) work anywhere in the module
    q->addActions(widget->actions());
    widget->setVisible(false);
    widget->setMRMLScene(q->mrmlScene());
    QObject::connect(q, SIGNAL(mrmlSceneChanged(vtkMRMLScene*)), widget, SLOT(setMRMLScene(vtkMRMLScene*)));
  }
  this->updateTransformOptionsWidgetsFromMRML(true);
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetPrivate::updateTransformOptionsWidgetsFromMRML(bool transformNodeChanged)
{
  for (qMRMLTransformAbstractOptionsWidget* widget : this->TransformOptionsWidgets)
  {
    // A widget may be able to manage the node only while the node has a certain kind of transform,
    // so this is checked each time the transform is modified, too
    bool canManage = widget->canManageMRMLTransformNode(this->MRMLTransformNode);
    if (transformNodeChanged || widget->mrmlTransformNode() != this->MRMLTransformNode)
    {
      widget->setMRMLTransformNode(this->MRMLTransformNode);
    }
    // Do not show/hide the widgets when clicking on a folder or scene node, to reduce the GUI layout jumping around
    // (a widget that remains visible gets no node, so it is expected to disable itself).
    // This method may be called very frequently (when the transform is changing in real time)
    // and setVisible calls take time even if the visibility state does not change,
    // therefore it is only called if the visibility has to be changed.
    if (this->MRMLTransformNode && canManage == widget->isHidden())
    {
      widget->setVisible(canManage);
    }
    if (canManage)
    {
      widget->updateWidgetFromMRML();
    }
  }
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetPrivate::transformAboutToBeReplaced()
{
  for (qMRMLTransformAbstractOptionsWidget* widget : this->TransformOptionsWidgets)
  {
    widget->transformAboutToBeReplaced();
  }
}

//-----------------------------------------------------------------------------
vtkSlicerTransformLogic* qSlicerTransformsModuleWidgetPrivate::logic() const
{
  Q_Q(const qSlicerTransformsModuleWidget);
  return vtkSlicerTransformLogic::SafeDownCast(q->logic());
}

//-----------------------------------------------------------------------------
QList<vtkSmartPointer<vtkMRMLTransformableNode>> qSlicerTransformsModuleWidgetPrivate::getSelectedNodes(qMRMLTreeView* tree)
{
  QModelIndexList selectedIndexes = tree->selectionModel()->selectedRows();
  selectedIndexes = qMRMLTreeView::removeChildren(selectedIndexes);

  // Return the list of nodes
  QList<vtkSmartPointer<vtkMRMLTransformableNode>> selectedNodes;
  for (const QModelIndex& selectedIndex : selectedIndexes)
  {
    vtkMRMLTransformableNode* node = vtkMRMLTransformableNode::SafeDownCast(tree->sortFilterProxyModel()->mrmlNodeFromIndex(selectedIndex));
    Q_ASSERT(node);
    selectedNodes << node;
  }
  return selectedNodes;
}

//-----------------------------------------------------------------------------
qSlicerTransformsModuleWidget::qSlicerTransformsModuleWidget(QWidget* _parentWidget)
  : Superclass(_parentWidget)
  , d_ptr(new qSlicerTransformsModuleWidgetPrivate(*this))
{
}

//-----------------------------------------------------------------------------
qSlicerTransformsModuleWidget::~qSlicerTransformsModuleWidget() = default;

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::setup()
{
  Q_D(qSlicerTransformsModuleWidget);
  this->Superclass::setup();
  d->setupUi(this);

  // Connect invert button
  this->connect(d->InvertPushButton, SIGNAL(clicked()), SLOT(invert()));

  // Connect split button
  this->connect(d->SplitPushButton, SIGNAL(clicked()), SLOT(split()));

  // Connect node selector with module itself
  this->connect(d->TransformNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)), SLOT(onNodeSelected(vtkMRMLNode*)));

  // Transform nodes connection
  this->connect(d->TransformToolButton, SIGNAL(clicked()), SLOT(transformSelectedNodes()));
  this->connect(d->UntransformToolButton, SIGNAL(clicked()), SLOT(untransformSelectedNodes()));
  this->connect(d->HardenToolButton, SIGNAL(clicked()), SLOT(hardenSelectedNodes()));

  // Observe display section, if opened, then add display node
  this->connect(d->DisplayCollapsibleButton, SIGNAL(clicked(bool)), SLOT(onDisplaySectionClicked(bool)));

  // Observe Apply transform section to maintain a nice layout
  // even when the section is closed.
  this->connect(d->TransformedCollapsibleButton, SIGNAL(clicked(bool)), SLOT(onTransformableSectionClicked(bool)));

  // Icons
  QIcon rightIcon = QApplication::style()->standardIcon(QStyle::SP_ArrowRight);
  d->TransformToolButton->setIcon(rightIcon);

  QIcon leftIcon = QApplication::style()->standardIcon(QStyle::SP_ArrowLeft);
  d->UntransformToolButton->setIcon(leftIcon);

  // Connect convert button
  this->connect(d->ConvertPushButton, SIGNAL(clicked()), SLOT(convert()));

  // Connect node convert input/output node selectors
  this->connect(d->ConvertReferenceVolumeNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)), SLOT(updateConvertButtonState()));
  this->connect(d->ConvertOutputDisplacementFieldNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)), SLOT(updateConvertButtonState()));

  // Options widgets: of linear transforms (the Transforms module registers it),
  // and of transform types that other modules (for example, of extensions) provide
  d->updateTransformOptionsWidgets();
  this->connect(qMRMLTransformOptionsWidgetsFactory::instance(), SIGNAL(optionsWidgetRegistered()), SLOT(onUpdateTransformOptionsWidgets()));
  this->connect(qMRMLTransformOptionsWidgetsFactory::instance(), SIGNAL(optionsWidgetUnregistered()), SLOT(onUpdateTransformOptionsWidgets()));

  this->onTransformableSectionClicked(d->TransformedCollapsibleButton->isChecked());
  this->onNodeSelected(nullptr);
  this->updateConvertButtonState();
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::enter()
{
  Q_D(qSlicerTransformsModuleWidget);

  // Connect SH item modified event so that widget state is updated when a display node is created
  // on the currently selected item (when color is set to a folder)
  vtkMRMLSubjectHierarchyNode* shNode = d->TransformNodeSelector->subjectHierarchyNode();
  if (shNode)
  {
    qvtkConnect(shNode, vtkMRMLSubjectHierarchyNode::SubjectHierarchyItemDisplayModifiedEvent, this, SLOT(onSubjectHierarchyItemModified(vtkObject*, void*)));
  }
  else
  {
    qCritical() << Q_FUNC_INFO << ": Invalid subject hierarchy";
  }

  // If no node is selected then select the first displayed node to save the user a click
  if (!d->TransformNodeSelector->currentNode())
  {
    vtkMRMLNode* node = d->TransformNodeSelector->findFirstNodeByClass("vtkMRMLTransformNode");
    if (node)
    {
      d->TransformNodeSelector->setCurrentNode(node);
    }
  }

  this->Superclass::enter();
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::exit()
{
  Q_D(qSlicerTransformsModuleWidget);

  // Disconnect SH node modified when module is not active
  vtkMRMLSubjectHierarchyNode* shNode = d->TransformNodeSelector->subjectHierarchyNode();
  if (shNode)
  {
    qvtkDisconnect(shNode, vtkMRMLSubjectHierarchyNode::SubjectHierarchyItemDisplayModifiedEvent, this, SLOT(onSubjectHierarchyItemModified(vtkObject*, void*)));
  }
  else
  {
    qCritical() << Q_FUNC_INFO << ": Invalid subject hierarchy";
  }

  this->Superclass::exit();
}

//---------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::onSubjectHierarchyItemModified(vtkObject* vtkNotUsed(caller), void* vtkNotUsed(callData))
{
  Q_D(qSlicerTransformsModuleWidget);
  // Refresh the display node section. This is necessary because it is possible that the display node did not exist yet
  // when the TransformDisplayNodeWidget was set in the widget.
  d->TransformDisplayNodeWidget->setMRMLTransformNode(d->MRMLTransformNode);
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::onNodeSelected(vtkMRMLNode* node)
{
  Q_D(qSlicerTransformsModuleWidget);

  vtkMRMLTransformNode* transformNode = vtkMRMLTransformNode::SafeDownCast(node);

  this->qvtkReconnect(d->MRMLTransformNode, transformNode, vtkMRMLTransformableNode::TransformModifiedEvent, this, SLOT(onMRMLTransformNodeModified(vtkObject*)));

  if (d->MRMLTransformNode == nullptr && transformNode != nullptr)
  {
    d->TransformedCollapsibleButton->setCollapsed(false);
  }

  d->MRMLTransformNode = transformNode;
  d->updateTransformOptionsWidgetsFromMRML(true);

  bool isTransform = (transformNode != nullptr);
  bool isCompositeTransform = (transformNode != nullptr && transformNode->IsComposite());

  // Enable/Disable the invert button and the sections that need a transform
  // (the options widgets are updated in updateTransformOptionsWidgetsFromMRML)

  d->InvertPushButton->setEnabled(isTransform);
  d->TransformToolButton->setEnabled(isTransform);
  d->UntransformToolButton->setEnabled(isTransform);
  d->HardenToolButton->setEnabled(isTransform);
  d->TransformableTreeView->setEnabled(isTransform);
  d->TransformedTreeView->setEnabled(isTransform);
  d->ConvertCollapsibleButton->setEnabled(isTransform);

  d->SplitPushButton->setVisible(isCompositeTransform);

  d->TransformDisplayNodeWidget->setMRMLTransformNode(transformNode);
  d->TransformInfoWidget->setMRMLTransformNode(transformNode);

  QStringList nodeTypes;
  // If no transform node, it would show the entire scene, lets shown none
  // instead.
  if (transformNode == nullptr)
  {
    nodeTypes << QString("vtkMRMLNotANode");
  }
  d->TransformedTreeView->setNodeTypes(nodeTypes);

  // Filter the current node in the transformed tree view
  d->TransformedTreeView->setRootNode(transformNode);

  // Hide the current node in the transformable tree view
  QStringList hiddenNodeIDs;
  if (transformNode)
  {
    hiddenNodeIDs << QString(transformNode->GetID());
  }
  d->TransformableTreeView->sortFilterProxyModel()->setHiddenNodeIDs(hiddenNodeIDs);

  // If there is no display node then collapse the display section.
  // This allows creation of transform display nodes on request:
  // the display node is created if the user expands the display section.
  vtkMRMLTransformDisplayNode* dispNode = nullptr;
  if (transformNode)
  {
    dispNode = vtkMRMLTransformDisplayNode::SafeDownCast(transformNode->GetDisplayNode());
  }
  if (dispNode == nullptr)
  {
    d->DisplayCollapsibleButton->setCollapsed(true);
  }
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::onUpdateTransformOptionsWidgets()
{
  Q_D(qSlicerTransformsModuleWidget);
  d->updateTransformOptionsWidgets();
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::onMRMLTransformNodeModified(vtkObject* caller)
{
  Q_D(qSlicerTransformsModuleWidget);

  vtkMRMLTransformNode* transformNode = vtkMRMLTransformNode::SafeDownCast(caller);
  if (!transformNode)
  {
    return;
  }
  Q_ASSERT(d->MRMLTransformNode == transformNode);

  bool isCompositeTransform = transformNode->IsComposite();

  // This method may be called very frequently (when transform is changing
  // in real time). Due to some reason setVisible calls take time,
  // even if the visibility state does not change.
  // To save time, only call the set function if the visibility has to be changed.
  if (isCompositeTransform != d->SplitPushButton->isVisible())
  {
    d->SplitPushButton->setVisible(isCompositeTransform);
  }

  d->updateTransformOptionsWidgetsFromMRML(false);
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::invert()
{
  Q_D(qSlicerTransformsModuleWidget);

  if (!d->MRMLTransformNode)
  {
    return;
  }

  d->transformAboutToBeReplaced();

  MRMLNodeModifyBlocker blocker(d->MRMLTransformNode);
  d->MRMLTransformNode->Inverse();
  d->MRMLTransformNode->InverseName();
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::split()
{
  Q_D(qSlicerTransformsModuleWidget);

  if (d->MRMLTransformNode == nullptr)
  {
    return;
  }

  d->MRMLTransformNode->Split();
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerTransformsModuleWidget);
  this->Superclass::setMRMLScene(scene);
  // If the root index is set before the scene, it will show the scene as
  // top-level item. Setting the root index to be the scene makes the nodes
  // top-level, and this can only be done once the scene is set.
  d->TransformableTreeView->setRootIndex(d->TransformableTreeView->sortFilterProxyModel()->mrmlSceneIndex());
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::transformSelectedNodes()
{
  Q_D(qSlicerTransformsModuleWidget);
  QList<vtkSmartPointer<vtkMRMLTransformableNode>> nodesToTransform = qSlicerTransformsModuleWidgetPrivate::getSelectedNodes(d->TransformableTreeView);
  for (const vtkSmartPointer<vtkMRMLTransformableNode>& node : nodesToTransform)
  {
    node->SetAndObserveTransformNodeID(d->MRMLTransformNode->GetID());
  }
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::untransformSelectedNodes()
{
  Q_D(qSlicerTransformsModuleWidget);
  QList<vtkSmartPointer<vtkMRMLTransformableNode>> nodesToTransform = qSlicerTransformsModuleWidgetPrivate::getSelectedNodes(d->TransformedTreeView);
  for (const vtkSmartPointer<vtkMRMLTransformableNode>& node : nodesToTransform)
  {
    node->SetAndObserveTransformNodeID(nullptr);
  }
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::hardenSelectedNodes()
{
  Q_D(qSlicerTransformsModuleWidget);
  QList<vtkSmartPointer<vtkMRMLTransformableNode>> nodesToTransform = qSlicerTransformsModuleWidgetPrivate::getSelectedNodes(d->TransformedTreeView);
  QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
  for (const vtkSmartPointer<vtkMRMLTransformableNode>& node : nodesToTransform)
  {
    d->logic()->hardenTransform(vtkMRMLTransformableNode::SafeDownCast(node));
  }
  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::onDisplaySectionClicked(bool clicked)
{
  Q_D(qSlicerTransformsModuleWidget);
  // If the display section is opened and there is no display node then create one
  if (!clicked)
  {
    return;
  }
  if (d->MRMLTransformNode == nullptr)
  {
    return;
  }
  if (vtkMRMLTransformDisplayNode::SafeDownCast(d->MRMLTransformNode->GetDisplayNode()) == nullptr)
  {
    d->MRMLTransformNode->CreateDefaultDisplayNodes();
    // Refresh the display node section
    d->TransformDisplayNodeWidget->setMRMLTransformNode(d->MRMLTransformNode);
  }
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::onTransformableSectionClicked(bool clicked)
{
  Q_D(qSlicerTransformsModuleWidget);
  if (clicked)
  {
    // the transformable section is open, so no need for spacer
    d->BottomSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
  }
  else
  {
    // the transformable section is open, add spacer to prevent stretching of
    // the remaining sections
    d->BottomSpacer->changeSize(1, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
  }
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::convert()
{
  Q_D(qSlicerTransformsModuleWidget);
  if (d->MRMLTransformNode == nullptr)
  {
    qWarning("qSlicerTransformsModuleWidget::convert failed: MRMLTransformNode is invalid");
    return;
  }
  if (d->ConvertReferenceVolumeNodeComboBox->currentNode() == nullptr)
  {
    qWarning("qSlicerTransformsModuleWidget::convert failed: reference volume node is invalid");
    return;
  }
  if (d->ConvertOutputDisplacementFieldNodeComboBox->currentNode() == nullptr)
  {
    qWarning("qSlicerTransformsModuleWidget::convert failed: reference volume node is invalid");
    return;
  }
  vtkMRMLScalarVolumeNode* scalarOutputVolumeNode = vtkMRMLScalarVolumeNode::SafeDownCast(d->ConvertOutputDisplacementFieldNodeComboBox->currentNode());
  vtkMRMLVectorVolumeNode* vectorOutputVolumeNode = vtkMRMLVectorVolumeNode::SafeDownCast(d->ConvertOutputDisplacementFieldNodeComboBox->currentNode());
  vtkMRMLTransformNode* outputTransformNode = vtkMRMLTransformNode::SafeDownCast(d->ConvertOutputDisplacementFieldNodeComboBox->currentNode());
  vtkMRMLVolumeNode* referenceVolumeNode = vtkMRMLVolumeNode::SafeDownCast(d->ConvertReferenceVolumeNodeComboBox->currentNode());
  QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
  vtkMRMLNode* resultNode = nullptr;
  if (vectorOutputVolumeNode)
  {
    // this must be checked before scalarOutputVolumeNode, as vtkMRMLVectorVolumeNode is a vtkMRMLScalarVolumeNode as well
    resultNode = d->logic()->CreateDisplacementVolumeFromTransform(d->MRMLTransformNode, referenceVolumeNode, false /*magnitudeOnly*/, vectorOutputVolumeNode);
  }
  else if (scalarOutputVolumeNode)
  {
    resultNode = d->logic()->CreateDisplacementVolumeFromTransform(d->MRMLTransformNode, referenceVolumeNode, true /*magnitudeOnly*/, scalarOutputVolumeNode);
  }
  else if (outputTransformNode)
  {
    resultNode = d->logic()->ConvertToGridTransform(d->MRMLTransformNode, referenceVolumeNode, outputTransformNode);
  }
  else
  {
    qWarning("qSlicerTransformsModuleWidget::convert failed: invalid output node type");
  }
  QApplication::restoreOverrideCursor();
  if (resultNode == nullptr)
  {
    QMessageBox::warning(this, tr("Conversion failed"), tr("Failed to convert transform. See application log for details."));
  }
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModuleWidget::updateConvertButtonState()
{
  Q_D(qSlicerTransformsModuleWidget);
  bool enableConvert = (d->MRMLTransformNode != nullptr                                    //
                        && d->ConvertReferenceVolumeNodeComboBox->currentNode() != nullptr //
                        && d->ConvertOutputDisplacementFieldNodeComboBox->currentNode() != nullptr);
  d->ConvertPushButton->setEnabled(enableConvert);
}

//-----------------------------------------------------------
bool qSlicerTransformsModuleWidget::setEditedNode(vtkMRMLNode* node, QString role /* = QString()*/, QString context /* = QString()*/)
{
  Q_D(qSlicerTransformsModuleWidget);
  Q_UNUSED(role);
  Q_UNUSED(context);
  if (vtkMRMLTransformNode::SafeDownCast(node))
  {
    d->TransformNodeSelector->setCurrentNode(node);
    return true;
  }

  if (vtkMRMLTransformDisplayNode::SafeDownCast(node))
  {
    vtkMRMLTransformDisplayNode* displayNode = vtkMRMLTransformDisplayNode::SafeDownCast(node);
    vtkMRMLTransformNode* displayableNode = vtkMRMLTransformNode::SafeDownCast(displayNode->GetDisplayableNode());
    if (!displayableNode)
    {
      return false;
    }
    d->TransformNodeSelector->setCurrentNode(displayableNode);
    return true;
  }

  return false;
}
