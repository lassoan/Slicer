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

  This file was developed with funding from the Children's Hospital of
  Philadelphia Frontier Program.

==============================================================================*/

// SubjectHierarchy Plugins includes
#include "qSlicerSubjectHierarchyPluginHandler.h"
#include "qSlicerSubjectHierarchyVectorFieldsPlugin.h"

// Slicer includes
#include "qSlicerAbstractModuleWidget.h"

// VectorFields includes
#include "vtkMRMLVectorFieldDisplayNode.h"
#include "vtkSlicerVectorFieldsLogic.h"

// MRML includes
#include <vtkMRMLDisplayableNode.h>
#include <vtkMRMLSubjectHierarchyNode.h>

// Qt includes
#include <QAction>
#include <QDebug>

//-----------------------------------------------------------------------------
class qSlicerSubjectHierarchyVectorFieldsPluginPrivate : public QObject
{
  Q_DECLARE_PUBLIC(qSlicerSubjectHierarchyVectorFieldsPlugin);

protected:
  qSlicerSubjectHierarchyVectorFieldsPlugin* const q_ptr;

public:
  qSlicerSubjectHierarchyVectorFieldsPluginPrivate(qSlicerSubjectHierarchyVectorFieldsPlugin& object);
  ~qSlicerSubjectHierarchyVectorFieldsPluginPrivate() override;
  void init();

  /// The node of an item if it has a vector field
  vtkMRMLDisplayableNode* vectorFieldNode(vtkIdType itemID) const;

public:
  QAction* ToggleVectorFieldAction{ nullptr };
  QAction* VectorFieldOptionsAction{ nullptr };
};

//-----------------------------------------------------------------------------
qSlicerSubjectHierarchyVectorFieldsPluginPrivate::qSlicerSubjectHierarchyVectorFieldsPluginPrivate(qSlicerSubjectHierarchyVectorFieldsPlugin& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
qSlicerSubjectHierarchyVectorFieldsPluginPrivate::~qSlicerSubjectHierarchyVectorFieldsPluginPrivate() = default;

//------------------------------------------------------------------------------
void qSlicerSubjectHierarchyVectorFieldsPluginPrivate::init()
{
  Q_Q(qSlicerSubjectHierarchyVectorFieldsPlugin);

  this->ToggleVectorFieldAction = new QAction(qSlicerSubjectHierarchyVectorFieldsPlugin::tr("Show as vector field"), q);
  this->ToggleVectorFieldAction->setObjectName("ToggleVectorFieldAction");
  this->ToggleVectorFieldAction->setToolTip(
    qSlicerSubjectHierarchyVectorFieldsPlugin::tr("Show the vector field with glyphs, a grid, contours or streamlines. While this is checked, the eye icon shows and hides the "
                                                  "vector field with the node."));
  this->ToggleVectorFieldAction->setCheckable(true);
  QObject::connect(this->ToggleVectorFieldAction, SIGNAL(toggled(bool)), q, SLOT(toggleVectorFieldForCurrentItem(bool)));

  this->VectorFieldOptionsAction = new QAction(qSlicerSubjectHierarchyVectorFieldsPlugin::tr("Vector field display settings..."), q);
  this->VectorFieldOptionsAction->setObjectName("VectorFieldOptionsAction");
  this->VectorFieldOptionsAction->setToolTip(qSlicerSubjectHierarchyVectorFieldsPlugin::tr("Switch to the Vector Fields module to edit how the vector field is shown"));
  QObject::connect(this->VectorFieldOptionsAction, SIGNAL(triggered()), q, SLOT(showVectorFieldOptionsForCurrentItem()));
}

//-----------------------------------------------------------------------------
vtkMRMLDisplayableNode* qSlicerSubjectHierarchyVectorFieldsPluginPrivate::vectorFieldNode(vtkIdType itemID) const
{
  vtkMRMLSubjectHierarchyNode* shNode = qSlicerSubjectHierarchyPluginHandler::instance()->subjectHierarchyNode();
  if (!shNode || !itemID)
  {
    return nullptr;
  }
  vtkMRMLDisplayableNode* node = vtkMRMLDisplayableNode::SafeDownCast(shNode->GetItemDataNode(itemID));
  return (vtkSlicerVectorFieldsLogic::HasVectorField(node) ? node : nullptr);
}

//-----------------------------------------------------------------------------
qSlicerSubjectHierarchyVectorFieldsPlugin::qSlicerSubjectHierarchyVectorFieldsPlugin(QObject* parent)
  : Superclass(parent)
  , d_ptr(new qSlicerSubjectHierarchyVectorFieldsPluginPrivate(*this))
{
  this->m_Name = QString("VectorFields");

  Q_D(qSlicerSubjectHierarchyVectorFieldsPlugin);
  d->init();
}

//-----------------------------------------------------------------------------
qSlicerSubjectHierarchyVectorFieldsPlugin::~qSlicerSubjectHierarchyVectorFieldsPlugin() = default;

//-----------------------------------------------------------------------------
QList<QAction*> qSlicerSubjectHierarchyVectorFieldsPlugin::visibilityContextMenuActions() const
{
  Q_D(const qSlicerSubjectHierarchyVectorFieldsPlugin);
  QList<QAction*> actions;
  actions << d->ToggleVectorFieldAction << d->VectorFieldOptionsAction;
  return actions;
}

//---------------------------------------------------------------------------
void qSlicerSubjectHierarchyVectorFieldsPlugin::showVisibilityContextMenuActionsForItem(vtkIdType itemID)
{
  Q_D(qSlicerSubjectHierarchyVectorFieldsPlugin);
  if (!d->vectorFieldNode(itemID))
  {
    return;
  }
  bool wasBlocked = d->ToggleVectorFieldAction->blockSignals(true);
  d->ToggleVectorFieldAction->setChecked(this->isVectorFieldShown(itemID));
  d->ToggleVectorFieldAction->blockSignals(wasBlocked);
  d->ToggleVectorFieldAction->setVisible(true);
  d->VectorFieldOptionsAction->setVisible(true);
}

//---------------------------------------------------------------------------
bool qSlicerSubjectHierarchyVectorFieldsPlugin::isVectorFieldShown(vtkIdType itemID)
{
  Q_D(qSlicerSubjectHierarchyVectorFieldsPlugin);
  // Shown with the node, also while the eye icon has hidden it
  for (vtkMRMLVectorFieldDisplayNode* displayNode : vtkSlicerVectorFieldsLogic::GetVectorFieldDisplayNodes(d->vectorFieldNode(itemID)))
  {
    if (displayNode->GetShowMode() == vtkMRMLDisplayNode::ShowDefault)
    {
      return true;
    }
  }
  return false;
}

//---------------------------------------------------------------------------
bool qSlicerSubjectHierarchyVectorFieldsPlugin::showVectorField(bool show, vtkIdType itemID)
{
  Q_D(qSlicerSubjectHierarchyVectorFieldsPlugin);
  vtkMRMLDisplayableNode* node = d->vectorFieldNode(itemID);
  if (!node)
  {
    return false;
  }
  std::vector<vtkMRMLVectorFieldDisplayNode*> displayNodes = vtkSlicerVectorFieldsLogic::GetVectorFieldDisplayNodes(node);
  if (show && displayNodes.empty())
  {
    // Added shown
    return (vtkSlicerVectorFieldsLogic::AddVectorFieldDisplayNode(node) != nullptr);
  }
  for (vtkMRMLVectorFieldDisplayNode* displayNode : displayNodes)
  {
    vtkSlicerVectorFieldsLogic::SetVectorFieldDisplayNodeShown(displayNode, show);
  }
  return true;
}

//---------------------------------------------------------------------------
void qSlicerSubjectHierarchyVectorFieldsPlugin::toggleVectorFieldForCurrentItem(bool show)
{
  vtkIdType currentItemID = qSlicerSubjectHierarchyPluginHandler::instance()->currentItem();
  if (!currentItemID)
  {
    qCritical() << Q_FUNC_INFO << ": Invalid current item";
    return;
  }
  this->showVectorField(show, currentItemID);
}

//---------------------------------------------------------------------------
void qSlicerSubjectHierarchyVectorFieldsPlugin::showVectorFieldOptionsForCurrentItem()
{
  Q_D(qSlicerSubjectHierarchyVectorFieldsPlugin);
  vtkMRMLDisplayableNode* node = d->vectorFieldNode(qSlicerSubjectHierarchyPluginHandler::instance()->currentItem());
  if (!node)
  {
    qCritical() << Q_FUNC_INFO << ": Current item has no vector field";
    return;
  }
  qSlicerAbstractModuleWidget* moduleWidget = qSlicerSubjectHierarchyAbstractPlugin::switchToModule("VectorFields");
  if (moduleWidget)
  {
    moduleWidget->setEditedNode(node);
  }
}
