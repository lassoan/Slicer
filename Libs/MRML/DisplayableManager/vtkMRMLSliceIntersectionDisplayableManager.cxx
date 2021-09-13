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

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLCrosshairNode.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLLightBoxRendererManagerProxy.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceCompositeNode.h>
#include <vtkMRMLSliceIntersectionInteractionWidget.h>
#include <vtkMRMLSliceLogic.h>
#include <vtkMRMLSliceNode.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkEventBroker.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPickingManager.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProp.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>
#include <vtkVersion.h>

// STD includes
#include <algorithm>
#include <cassert>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLSliceIntersectionDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLSliceIntersectionDisplayableManager::vtkInternal
{
  public:
    vtkInternal(vtkMRMLSliceIntersectionDisplayableManager* external);
    ~vtkInternal();

    vtkObserverManager* GetMRMLNodesObserverManager();
    void Modified();

    // Slice
    vtkMRMLSliceNode* GetSliceNode();
    void UpdateIntersectingSliceNodes();
    // Crosshair
    void SetCrosshairNode(vtkMRMLCrosshairNode* crosshairNode);

    // Actors
    void SetActor(vtkActor2D* prop) { Actor = prop; };

    // Build the crosshair representation
    void BuildCrosshair();

    // Add a line to the crosshair in display coordinates (needs to be
    // passed the points and cellArray to manipulate).
    void AddCrosshairLine(vtkPoints* pts, vtkCellArray* cellArray,
      int p1x, int p1y, int p2x, int p2y);

    // Did crosshair position change?
    bool DidCrosshairPositionChange();

    // Did crosshair property change?
    bool DidCrosshairPropertyChange();

    vtkMRMLSliceIntersectionDisplayableManager* External;
    int                                        PickState;
    int                                        ActionState;
    vtkSmartPointer<vtkActor2D>                Actor;
    vtkWeakPointer<vtkRenderer>                LightBoxRenderer;

    vtkWeakPointer<vtkMRMLCrosshairNode>       CrosshairNode;
    int CrosshairMode;
    int CrosshairThickness;
    double CrosshairPosition[3];

    vtkSmartPointer<vtkMRMLSliceIntersectionInteractionWidget> SliceIntersectionInteractionWidget;
};


//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionDisplayableManager::vtkInternal
::vtkInternal(vtkMRMLSliceIntersectionDisplayableManager* external)
{
  this->External = external;
  this->SliceIntersectionInteractionWidget = vtkSmartPointer<vtkMRMLSliceIntersectionInteractionWidget>::New();
}

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::~vtkInternal()
{
  if (this->SliceIntersectionInteractionWidget)
    {
    this->SliceIntersectionInteractionWidget->SetMRMLApplicationLogic(nullptr);
    this->SliceIntersectionInteractionWidget->SetRenderer(nullptr);
    this->SliceIntersectionInteractionWidget->SetSliceNode(nullptr);
    }
}

//---------------------------------------------------------------------------
vtkMRMLSliceNode* vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::GetSliceNode()
{
  return this->External->GetMRMLSliceNode();
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::vtkInternal::UpdateIntersectingSliceNodes()
{
  if (this->External->GetMRMLScene() == nullptr)
    {
    this->SliceIntersectionInteractionWidget->SetSliceNode(nullptr);
    return;
    }

  if (!this->SliceIntersectionInteractionWidget->GetRenderer())
    {
    vtkMRMLApplicationLogic* mrmlAppLogic = this->External->GetMRMLApplicationLogic();
    this->SliceIntersectionInteractionWidget->SetMRMLApplicationLogic(mrmlAppLogic);
    this->SliceIntersectionInteractionWidget->CreateDefaultRepresentation();
    this->SliceIntersectionInteractionWidget->SetRenderer(this->External->GetRenderer());
    this->SliceIntersectionInteractionWidget->SetSliceNode(this->GetSliceNode());
    }
  else
    {
    this->SliceIntersectionInteractionWidget->SetSliceNode(this->GetSliceNode());
    }
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
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  vtkMRMLScene* scene = this->GetMRMLScene();

  if (scene == nullptr || scene->IsBatchProcessing())
    {
    return;
    }

  vtkMRMLSliceCompositeNode* compositeNode = vtkMRMLSliceCompositeNode::SafeDownCast(caller);
  if (compositeNode && event == vtkCommand::ModifiedEvent)
    {
    this->RequestRender();
    }
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::ObserveMRMLScene()
{
  this->Superclass::ObserveMRMLScene();

  // Observe all the slice composite nodes in the scene to trigger rendering when the interactive slice intersection
  // is turned on or off.
  //TODO: This is a hack, and should be done differently. The reasons this workaround was applied are:
  // - There is no way to get the composite node for a given slice node without the slice logic, which cannot be accessed from here
  // - The composite node is known from the widget representation, but the RequestRender function in this class seems to be the only
  //   way to get re-render, and there is no way to trigger calling that function from the representation
  // - Note: The reason update works in the traditional slice intersection is that the slice model node is modified in that case,
  //   and the model slice displayable manager handles that and calls RequestRender. But for the interactive slice intersection the
  //   slice model is not modified.
  // ! A possible way to make this nicer (and not observe three nodes instead of one) could be to get the slice composite node
  //   in vtkMRMLSliceIntersectionInteractionRepresentation::SetSliceNode, get the corresponding slice node there, and observe
  //   the composite node, which would call a Modified on the slice node, which in turn can be observed from this class directly.
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkMRMLScene* scene = this->GetMRMLScene();
  std::vector<vtkMRMLNode*> sliceCompositeNodes;
  int numOfSegmentationNodes = scene->GetNodesByClass("vtkMRMLSliceCompositeNode", sliceCompositeNodes);
  for (int i = 0; i < numOfSegmentationNodes; i++)
    {
    vtkMRMLSliceCompositeNode* sliceCompositeNode = vtkMRMLSliceCompositeNode::SafeDownCast(sliceCompositeNodes[i]);
    if (!broker->GetObservationExist(sliceCompositeNode, vtkCommand::ModifiedEvent, this, this->GetMRMLNodesCallbackCommand()))
      {
      broker->AddObservation(sliceCompositeNode, vtkCommand::ModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
      }
    }
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::UpdateFromMRMLScene()
{
  this->Internal->UpdateIntersectingSliceNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::UnobserveMRMLScene()
{
  this->Superclass::UnobserveMRMLScene();

  this->Internal->SliceIntersectionInteractionWidget->SetSliceNode(nullptr);

  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkMRMLScene* scene = this->GetMRMLScene();
  std::vector<vtkMRMLNode*> sliceCompositeNodes;
  int numOfSegmentationNodes = scene->GetNodesByClass("vtkMRMLSliceCompositeNode", sliceCompositeNodes);
  for (int i = 0; i < numOfSegmentationNodes; i++)
    {
    vtkMRMLSliceCompositeNode* sliceCompositeNode = vtkMRMLSliceCompositeNode::SafeDownCast(sliceCompositeNodes[i]);
    vtkEventBroker::ObservationVector observations;
    observations = broker->GetObservations(sliceCompositeNode, vtkCommand::ModifiedEvent, this, this->GetMRMLNodesCallbackCommand());
    broker->RemoveObservations(observations);
    }
  }

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::OnMRMLNodeModified(
  vtkMRMLNode* vtkNotUsed(node))
{
    // test
}

//---------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionDisplayableManager::CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& closestDistance2)
{
  return this->Internal->SliceIntersectionInteractionWidget->CanProcessInteractionEvent(eventData, closestDistance2);
}

//---------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionDisplayableManager::ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData)
{
  return this->Internal->SliceIntersectionInteractionWidget->ProcessInteractionEvent(eventData);
}

//---------------------------------------------------------------------------
void vtkMRMLSliceIntersectionDisplayableManager::SetActionsEnabled(int actions)
{
  this->Internal->SliceIntersectionInteractionWidget->SetActionsEnabled(actions);
}

//---------------------------------------------------------------------------
int vtkMRMLSliceIntersectionDisplayableManager::GetActionsEnabled()
{
  return this->Internal->SliceIntersectionInteractionWidget->GetActionsEnabled();
}

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionWidget* vtkMRMLSliceIntersectionDisplayableManager::GetSliceIntersectionInteractionWidget()
{
  return this->Internal->SliceIntersectionInteractionWidget;
}
