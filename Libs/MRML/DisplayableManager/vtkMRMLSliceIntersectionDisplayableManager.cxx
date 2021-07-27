/*==============================================================================

  Program: 3D Slicer

  Portions (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All Rights Reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

==============================================================================*/


// MRMLDisplayableManager includes
#include "vtkMRMLSliceIntersectionDisplayableManager.h"
#include <vtkMRMLSliceIntersectionInteractionWidget.h>

// MRMLDM includes
#include <vtkMRMLInteractionEventData.h>

// MRML includes
#include <vtkMRMLProceduralColorNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformDisplayNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkColorTransferFunction.h>
#include <vtkEventBroker.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkWeakPointer.h>
#include <vtkPointLocator.h>

// STD includes
#include <algorithm>
#include <cassert>
#include <set>
#include <map>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLSliceIntersectionDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLSliceIntersectionDisplayableManager::vtkInternal
{
public:

  vtkInternal(vtkMRMLSliceIntersectionDisplayableManager* external);
  ~vtkInternal();

  typedef std::map < vtkMRMLSliceNode*, vtkSmartPointer<vtkMRMLSliceIntersectionInteractionWidget> > InteractionPipelinesCacheType;
  InteractionPipelinesCacheType InteractionPipelines;

  // Slice Nodes
  void SetSliceNode(vtkMRMLSliceNode* sliceNode);
  void AddSliceNode(vtkMRMLSliceNode* sliceNode);
  void RemoveSliceNode(vtkMRMLSliceNode* sliceNode);

  vtkWeakPointer<vtkMRMLSliceIntersectionInteractionWidget> LastActiveWidget;
  vtkMRMLSliceIntersectionInteractionWidget* FindClosestWidget(vtkMRMLInteractionEventData* callData, double& closestDistance2);

private:
  vtkMRMLSliceIntersectionDisplayableManager* External;
  bool AddingTransformNode;
  vtkSmartPointer<vtkMRMLSliceNode> SliceNode;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::vtkInternal(vtkMRMLSliceIntersectionDisplayableManager* external)
  : External(external)
  , AddingTransformNode(false)
{
}

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::~vtkInternal()
{
  this->SliceNode = nullptr;
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::SetSliceNode(vtkMRMLSliceNode* sliceNode)
{
  if (!sliceNode || this->SliceNode == sliceNode)
  {
    return;
  }
  this->SliceNode = sliceNode;
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::RemoveSliceNode(vtkMRMLSliceNode* sliceNode)
{
  this->InteractionPipelines.erase(sliceNode);
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::AddSliceNode(vtkMRMLSliceNode* sliceNode)
{
  if (!sliceNode)
  {
    return;
  }

  // Do not add the slice node if it is already associated with a pipeline object.
  InteractionPipelinesCacheType::iterator it;
  it = this->InteractionPipelines.find(sliceNode);
  if (it != this->InteractionPipelines.end())
  {
    return;
  }

  vtkNew<vtkMRMLSliceIntersectionInteractionWidget> interactionWidget;
  interactionWidget->CreateDefaultRepresentation(sliceNode, this->External->GetMRMLSliceNode(), this->External->GetRenderer());
  this->InteractionPipelines.insert(std::make_pair(sliceNode, interactionWidget.GetPointer()));
}

//---------------------------------------------------------------------------
// vtkMRMLSliceIntersectionDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionDisplayableManager::vtkMRMLSliceIntersectionDisplayableManager()
{
  this->Internal = new vtkInternal(this);
}

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionDisplayableManager::~vtkMRMLSliceIntersectionDisplayableManager()
{
  delete this->Internal;
  this->Internal = nullptr;
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "vtkMRMLSliceIntersectionDisplayableManager: " << this->GetClassName() << "\n";
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!node->IsA("vtkMRMLSliceNode"))
  {
    return;
  }

  // Escape if the scene a scene is being closed, imported or connected
  if (this->GetMRMLScene()->IsBatchProcessing())
  {
    this->SetUpdateFromMRMLRequested(true);
    return;
  }

  this->Internal->AddSliceNode(vtkMRMLSliceNode::SafeDownCast(node));
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  if (node && (!node->IsA("vtkMRMLSliceNode")))
  {
    return;
  }

  vtkMRMLSliceNode* sliceNode = nullptr;

  bool modified = false;
  if ((sliceNode = vtkMRMLSliceNode::SafeDownCast(node)))
  {
    this->Internal->RemoveSliceNode(sliceNode);
    modified = true;
  }
  if (modified)
  {
    this->RequestRender();
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  vtkMRMLScene* scene = this->GetMRMLScene();

  if (scene == nullptr || scene->IsBatchProcessing())
  {
    return;
  }

  if (vtkMRMLSliceNode::SafeDownCast(caller))
  {
    this->RequestRender();

    for (auto interactionPipeline : this->Internal->InteractionPipelines)
    {
      interactionPipeline.second->UpdateFromMRML(this->GetMRMLSliceNode(), event, callData);
    }
  }
  else
  {
    this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::Create()
{
  //this->Internal->SetSliceNode(this->GetMRMLSliceNode());
  this->Internal->AddSliceNode(this->GetMRMLSliceNode());
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::OnMRMLDisplayableNodeModifiedEvent(vtkObject* vtkNotUsed(caller))
{
  vtkErrorMacro("vtkMRMLSliceIntersectionDisplayableManager::OnMRMLDisplayableNodeModifiedEvent: Not implemented!");
}

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionWidget* vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::FindClosestWidget(vtkMRMLInteractionEventData* callData,
  double& closestDistance2)
{
  vtkMRMLSliceIntersectionInteractionWidget* closestWidget = nullptr;
  closestDistance2 = VTK_DOUBLE_MAX;

  for (auto widgetIterator : this->InteractionPipelines)
  {
    vtkMRMLSliceIntersectionInteractionWidget* widget = widgetIterator.second;
    if (!widget)
    {
      continue;
    }
    double distance2FromWidget = VTK_DOUBLE_MAX;
    if (widget->CanProcessInteractionEvent(callData, distance2FromWidget))
    {
      if (!closestWidget || distance2FromWidget < closestDistance2)
      {
        closestDistance2 = distance2FromWidget;
        closestWidget = widget;
      }
    }
  }
  return closestWidget;
}

//---------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionDisplayableManager::CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& closestDistance2)
{
  // New point can be placed anywhere
  int eventid = eventData->GetType();

  if (eventid == vtkCommand::LeaveEvent && this->Internal->LastActiveWidget != nullptr)
  {
    if (this->Internal->LastActiveWidget->GetSliceNode()
      && this->Internal->LastActiveWidget->GetSliceNode()->GetActiveInteractionType() > vtkMRMLSliceIntersectionInteractionWidget::InteractionNone)
    {
      // this widget has active component, therefore leave event is relevant
      closestDistance2 = 0.0;
      return this->Internal->LastActiveWidget;
    }
  }

  // Other interactions
  bool canProcess = (this->Internal->FindClosestWidget(eventData, closestDistance2) != nullptr);

  if (!canProcess && this->Internal->LastActiveWidget != nullptr
    && (eventid == vtkCommand::MouseMoveEvent || eventid == vtkCommand::Move3DEvent))
  {
    // TODO: handle multiple contexts
    this->Internal->LastActiveWidget->Leave(eventData);
    this->Internal->LastActiveWidget = nullptr;
  }

  return canProcess;
}

//---------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionDisplayableManager::ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData)
{
  int eventid = eventData->GetType();

  if (eventid == vtkCommand::LeaveEvent)
  {
    if (this->Internal->LastActiveWidget != nullptr)
    {
      this->Internal->LastActiveWidget->Leave(eventData);
      this->Internal->LastActiveWidget = nullptr;
    }
  }

  // Find/create active widget
  vtkMRMLSliceIntersectionInteractionWidget* activeWidget = nullptr;
  double closestDistance2 = VTK_DOUBLE_MAX;
  activeWidget = this->Internal->FindClosestWidget(eventData, closestDistance2);

  // Deactivate previous widget
  if (this->Internal->LastActiveWidget != nullptr && this->Internal->LastActiveWidget != activeWidget)
  {
    this->Internal->LastActiveWidget->Leave(eventData);
  }
  this->Internal->LastActiveWidget = activeWidget;
  if (!activeWidget)
  {
    // deactivate widget if we move far from it
    if (eventid == vtkCommand::MouseMoveEvent && this->Internal->LastActiveWidget != nullptr)
    {
      this->Internal->LastActiveWidget->Leave(eventData);
      this->Internal->LastActiveWidget = nullptr;
    }
    return false;
  }

  // Pass on the interaction event to the active widget
  return activeWidget->ProcessInteractionEvent(eventData);
}
