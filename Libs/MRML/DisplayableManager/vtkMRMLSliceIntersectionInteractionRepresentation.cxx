/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMRMLSliceIntersectionInteractionRepresentation.cxx

  Copyright (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkMRMLSliceIntersectionInteractionRepresentation.h"


#include <deque>

#include "vtkMRMLApplicationLogic.h"
#include "vtkMRMLDisplayableNode.h"
#include "vtkMRMLInteractionNode.h"
#include "vtkMRMLModelDisplayNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSliceLogic.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLSliceCompositeNode.h"

#include "vtkActor2D.h"
#include "vtkAssemblyPath.h"
#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkCommand.h"
#include "vtkCoordinate.h"
#include "vtkCursor2D.h"
#include "vtkGlyph2D.h"
#include "vtkInteractorObserver.h"
#include "vtkLeaderActor2D.h"
#include "vtkLine.h"
#include "vtkLineSource.h"
#include "vtkMath.h"
#include "vtkMatrix3x3.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPoints.h"
#include "vtkPolyDataAlgorithm.h"
#include "vtkPolyDataMapper2D.h"
#include "vtkProperty2D.h"
#include "vtkPlane.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkSphereSource.h"
#include "vtkTransform.h"
#include "vtkTextMapper.h"
#include "vtkTextProperty.h"
#include "vtkWindow.h"

// MRML includes
#include <vtkMRMLInteractionEventData.h>

static const double ROTATION_HANDLE_RADIUS = 10.0;
static const double TRANSLATION_HANDLE_RADIUS = 10.0;
static const double SLICETRANSLATION_HANDLE_RADIUS = 7.0;
static const double FOV_HANDLES_MARGIN = 0.03; // 3% margin
static const double LINE_POINTS_FILTERING_THRESHOLD = 15.0;
static const bool HANDLES_ALWAYS_VISIBLE = false;
static const double INTERACTION_SIZE_PIXELS = 30.0;
static const double   OPACITY_RANGE = 2000.0;

vtkStandardNewMacro(vtkMRMLSliceIntersectionInteractionRepresentation);
vtkCxxSetObjectMacro(vtkMRMLSliceIntersectionInteractionRepresentation, MRMLApplicationLogic, vtkMRMLApplicationLogic);

class SliceIntersectionInteractionDisplayPipeline
{
  public:

    //----------------------------------------------------------------------
    SliceIntersectionInteractionDisplayPipeline()
    {
      // Intersection line
      this->IntersectionLine = vtkSmartPointer<vtkLineSource>::New();
      this->IntersectionLineMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->IntersectionLineProperty = vtkSmartPointer<vtkProperty2D>::New();
      this->IntersectionLineActor = vtkSmartPointer<vtkActor2D>::New();
      this->IntersectionLineActor->SetVisibility(false); // invisible until slice node is set
      this->IntersectionLineMapper->SetInputConnection(this->IntersectionLine->GetOutputPort());
      this->IntersectionLineActor->SetMapper(this->IntersectionLineMapper);
      this->IntersectionLineActor->SetProperty(this->IntersectionLineProperty);

      // Center sphere
      this->TranslationHandle = vtkSmartPointer<vtkSphereSource>::New();
      this->TranslationHandle->SetRadius(TRANSLATION_HANDLE_RADIUS);
      this->TranslationHandleMapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->TranslationHandleProperty = vtkSmartPointer<vtkProperty2D>::New();
      this->TranslationHandleActor = vtkSmartPointer<vtkActor2D>::New();
      this->TranslationHandleActor->SetVisibility(false); // invisible until slice node is set
      this->TranslationHandleMapper->SetInputConnection(this->TranslationHandle->GetOutputPort());
      this->TranslationHandleActor->SetMapper(this->TranslationHandleMapper);
      this->TranslationHandleActor->SetProperty(this->TranslationHandleProperty);

      // Rotation sphere 1
      this->RotationHandle1 = vtkSmartPointer<vtkSphereSource>::New();
      this->RotationHandle1->SetRadius(ROTATION_HANDLE_RADIUS);
      this->RotationHandle1Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->RotationHandle1Property = vtkSmartPointer<vtkProperty2D>::New();
      this->RotationHandle1Actor = vtkSmartPointer<vtkActor2D>::New();
      this->RotationHandle1Actor->SetVisibility(false); // invisible until slice node is set
      this->RotationHandle1Mapper->SetInputConnection(this->RotationHandle1->GetOutputPort());
      this->RotationHandle1Actor->SetMapper(this->RotationHandle1Mapper);
      this->RotationHandle1Actor->SetProperty(this->RotationHandle1Property);

      // Rotation sphere 2
      this->RotationHandle2 = vtkSmartPointer<vtkSphereSource>::New();
      this->RotationHandle2->SetRadius(ROTATION_HANDLE_RADIUS);
      this->RotationHandle2Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->RotationHandle2Property = vtkSmartPointer<vtkProperty2D>::New();
      this->RotationHandle2Actor = vtkSmartPointer<vtkActor2D>::New();
      this->RotationHandle2Actor->SetVisibility(false); // invisible until slice node is set
      this->RotationHandle2Mapper->SetInputConnection(this->RotationHandle2->GetOutputPort());
      this->RotationHandle2Actor->SetMapper(this->RotationHandle2Mapper);
      this->RotationHandle2Actor->SetProperty(this->RotationHandle2Property);

      // Translation sphere 1
      this->SliceOffsetHandle1 = vtkSmartPointer<vtkSphereSource>::New();
      this->SliceOffsetHandle1->SetRadius(SLICETRANSLATION_HANDLE_RADIUS);
      this->SliceOffsetHandle1Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->SliceOffsetHandle1Property = vtkSmartPointer<vtkProperty2D>::New();
      this->SliceOffsetHandle1Actor = vtkSmartPointer<vtkActor2D>::New();
      this->SliceOffsetHandle1Actor->SetVisibility(false); // invisible until slice node is set
      this->SliceOffsetHandle1Mapper->SetInputConnection(this->SliceOffsetHandle1->GetOutputPort());
      this->SliceOffsetHandle1Actor->SetMapper(this->SliceOffsetHandle1Mapper);
      this->SliceOffsetHandle1Actor->SetProperty(this->SliceOffsetHandle1Property);

      // Translation sphere 2
      this->SliceOffsetHandle2 = vtkSmartPointer<vtkSphereSource>::New();
      this->SliceOffsetHandle2->SetRadius(SLICETRANSLATION_HANDLE_RADIUS);
      this->SliceOffsetHandle2Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
      this->SliceOffsetHandle2Property = vtkSmartPointer<vtkProperty2D>::New();
      this->SliceOffsetHandle2Actor = vtkSmartPointer<vtkActor2D>::New();
      this->SliceOffsetHandle2Actor->SetVisibility(false); // invisible until slice node is set
      this->SliceOffsetHandle2Mapper->SetInputConnection(this->SliceOffsetHandle2->GetOutputPort());
      this->SliceOffsetHandle2Actor->SetMapper(this->SliceOffsetHandle2Mapper);
      this->SliceOffsetHandle2Actor->SetProperty(this->SliceOffsetHandle2Property);

      // Handle points
      this->IntersectionLinePoints = vtkSmartPointer<vtkPolyData>::New();
      this->RotationHandlePoints = vtkSmartPointer<vtkPolyData>::New();
      this->TranslationHandlePoints = vtkSmartPointer<vtkPolyData>::New();
      this->SliceOffsetHandlePoints = vtkSmartPointer<vtkPolyData>::New();
    }

    //----------------------------------------------------------------------
    virtual ~SliceIntersectionInteractionDisplayPipeline()
    {
      this->SetAndObserveSliceLogic(nullptr, nullptr);
    }

    //----------------------------------------------------------------------
    void SetAndObserveSliceLogic(vtkMRMLSliceLogic* sliceLogic, vtkCallbackCommand* callback)
    {

      if (sliceLogic != this->SliceLogic || callback != this->Callback)
        {
        if (this->SliceLogic && this->Callback)
          {
          this->SliceLogic->RemoveObserver(this->Callback);
          }
        if (sliceLogic)
          {
          sliceLogic->AddObserver(vtkCommand::ModifiedEvent, callback);
          sliceLogic->AddObserver(vtkMRMLSliceLogic::CompositeModifiedEvent, callback);
          }
        this->SliceLogic = sliceLogic;
        }
      this->Callback = callback;
    }

    //----------------------------------------------------------------------
    void GetActors2D(vtkPropCollection* pc)
    {
      pc->AddItem(this->IntersectionLineActor);
      pc->AddItem(this->TranslationHandleActor);
      pc->AddItem(this->RotationHandle1Actor);
      pc->AddItem(this->RotationHandle2Actor);
      pc->AddItem(this->SliceOffsetHandle1Actor);
      pc->AddItem(this->SliceOffsetHandle2Actor);
    }

    //----------------------------------------------------------------------
    void AddActors(vtkRenderer* renderer)
    {
      if (!renderer)
        {
        return;
        }
      renderer->AddViewProp(this->IntersectionLineActor);
      renderer->AddViewProp(this->TranslationHandleActor);
      renderer->AddViewProp(this->RotationHandle1Actor);
      renderer->AddViewProp(this->RotationHandle2Actor);
      renderer->AddViewProp(this->SliceOffsetHandle1Actor);
      renderer->AddViewProp(this->SliceOffsetHandle2Actor);
    }

    //----------------------------------------------------------------------
    void ReleaseGraphicsResources(vtkWindow* win)
    {
      this->IntersectionLineActor->ReleaseGraphicsResources(win);
      this->TranslationHandleActor->ReleaseGraphicsResources(win);
      this->RotationHandle1Actor->ReleaseGraphicsResources(win);
      this->RotationHandle2Actor->ReleaseGraphicsResources(win);
      this->SliceOffsetHandle1Actor->ReleaseGraphicsResources(win);
      this->SliceOffsetHandle2Actor->ReleaseGraphicsResources(win);
    }

    //----------------------------------------------------------------------
    int RenderOverlay(vtkViewport* viewport)
    {
      int count = 0;
      if (this->TranslationHandleActor->GetVisibility())
        {
        count += this->TranslationHandleActor->RenderOverlay(viewport);
        }
      return count;
    }

    //----------------------------------------------------------------------
    void RemoveActors(vtkRenderer* renderer)
    {
      if (!renderer)
        {
        return;
        }
      renderer->RemoveViewProp(this->IntersectionLineActor);
      renderer->RemoveViewProp(this->TranslationHandleActor);
      renderer->RemoveViewProp(this->RotationHandle1Actor);
      renderer->RemoveViewProp(this->RotationHandle2Actor);
      renderer->RemoveViewProp(this->SliceOffsetHandle1Actor);
      renderer->RemoveViewProp(this->SliceOffsetHandle2Actor);
    }

    //----------------------------------------------------------------------
    void SetVisibility(bool visibility)
      {
      this->IntersectionLineActor->SetVisibility(visibility);
      if (HANDLES_ALWAYS_VISIBLE)
        {
        this->TranslationHandleActor->SetVisibility(visibility);
        this->RotationHandle1Actor->SetVisibility(visibility);
        this->RotationHandle2Actor->SetVisibility(visibility);
        this->SliceOffsetHandle1Actor->SetVisibility(visibility);
        this->SliceOffsetHandle2Actor->SetVisibility(visibility);
        }
      }

    //----------------------------------------------------------------------
    void SetHandlesVisibility(bool visibility)
      {
      this->TranslationHandleActor->SetVisibility(visibility);
      this->RotationHandle1Actor->SetVisibility(visibility);
      this->RotationHandle2Actor->SetVisibility(visibility);
      this->SliceOffsetHandle1Actor->SetVisibility(visibility);
      this->SliceOffsetHandle2Actor->SetVisibility(visibility);
      }

    //----------------------------------------------------------------------
    void SetHandlesOpacity(double opacity)
      {
      this->TranslationHandleProperty->SetOpacity(opacity);
      this->RotationHandle1Property->SetOpacity(opacity);
      this->RotationHandle2Property->SetOpacity(opacity);
      this->SliceOffsetHandle1Property->SetOpacity(opacity);
      this->SliceOffsetHandle2Property->SetOpacity(opacity);
      }

    //----------------------------------------------------------------------
    bool GetVisibility()
    {
      return this->IntersectionLineActor->GetVisibility();
    }

    vtkSmartPointer<vtkLineSource> IntersectionLine;
    vtkSmartPointer<vtkPolyDataMapper2D> IntersectionLineMapper;
    vtkSmartPointer<vtkProperty2D> IntersectionLineProperty;
    vtkSmartPointer<vtkActor2D> IntersectionLineActor;

    vtkSmartPointer<vtkSphereSource> TranslationHandle;
    vtkSmartPointer<vtkPolyDataMapper2D> TranslationHandleMapper;
    vtkSmartPointer<vtkProperty2D> TranslationHandleProperty;
    vtkSmartPointer<vtkActor2D> TranslationHandleActor;

    vtkSmartPointer<vtkSphereSource> RotationHandle1;
    vtkSmartPointer<vtkPolyDataMapper2D> RotationHandle1Mapper;
    vtkSmartPointer<vtkProperty2D> RotationHandle1Property;
    vtkSmartPointer<vtkActor2D> RotationHandle1Actor;

    vtkSmartPointer<vtkSphereSource> RotationHandle2;
    vtkSmartPointer<vtkPolyDataMapper2D> RotationHandle2Mapper;
    vtkSmartPointer<vtkProperty2D> RotationHandle2Property;
    vtkSmartPointer<vtkActor2D> RotationHandle2Actor;

    vtkSmartPointer<vtkSphereSource> SliceOffsetHandle1;
    vtkSmartPointer<vtkPolyDataMapper2D> SliceOffsetHandle1Mapper;
    vtkSmartPointer<vtkProperty2D> SliceOffsetHandle1Property;
    vtkSmartPointer<vtkActor2D> SliceOffsetHandle1Actor;

    vtkSmartPointer<vtkSphereSource> SliceOffsetHandle2;
    vtkSmartPointer<vtkPolyDataMapper2D> SliceOffsetHandle2Mapper;
    vtkSmartPointer<vtkProperty2D> SliceOffsetHandle2Property;
    vtkSmartPointer<vtkActor2D> SliceOffsetHandle2Actor;

    vtkWeakPointer<vtkMRMLSliceLogic> SliceLogic;
    vtkWeakPointer<vtkCallbackCommand> Callback;

    vtkSmartPointer<vtkPolyData> IntersectionLinePoints;
    vtkSmartPointer<vtkPolyData> RotationHandlePoints;
    vtkSmartPointer<vtkPolyData> TranslationHandlePoints;
    vtkSmartPointer<vtkPolyData> SliceOffsetHandlePoints;
  };

class vtkMRMLSliceIntersectionInteractionRepresentation::vtkInternal
{
  public:
    vtkInternal(vtkMRMLSliceIntersectionInteractionRepresentation* external);
    ~vtkInternal();

    vtkMRMLSliceIntersectionInteractionRepresentation* External;

    vtkSmartPointer<vtkMRMLSliceNode> SliceNode;

    std::deque<SliceIntersectionInteractionDisplayPipeline*> SliceIntersectionInteractionDisplayPipelines;
    vtkNew<vtkCallbackCommand> SliceNodeModifiedCommand;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::vtkInternal
::vtkInternal(vtkMRMLSliceIntersectionInteractionRepresentation* external)
{
  this->External = external;
}

//---------------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::vtkInternal::~vtkInternal() = default;

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::vtkMRMLSliceIntersectionInteractionRepresentation()
{
  this->MRMLApplicationLogic = nullptr;

  this->Internal = new vtkInternal(this);
  this->Internal->SliceNodeModifiedCommand->SetClientData(this);
  this->Internal->SliceNodeModifiedCommand->SetCallback(vtkMRMLSliceIntersectionInteractionRepresentation::SliceNodeModifiedCallback);

  this->SliceIntersectionPoint[0] = 0.0;
  this->SliceIntersectionPoint[1] = 0.0;
  this->SliceIntersectionPoint[2] = 0.0;
  this->SliceIntersectionPoint[3] = 1.0; // to allow easy homogeneous transformations

  // Set interaction size. Determines the maximum distance for interaction.
  this->InteractionSize = INTERACTION_SIZE_PIXELS;
}

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::~vtkMRMLSliceIntersectionInteractionRepresentation()
{
  this->SetSliceNode(nullptr);
  this->SetMRMLApplicationLogic(nullptr);
  delete this->Internal;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::GetActors2D(vtkPropCollection * pc)
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->GetActors2D(pc);
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::ReleaseGraphicsResources(vtkWindow * win)
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->ReleaseGraphicsResources(win);
    }
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::RenderOverlay(vtkViewport * viewport)
{
  int count = 0;

  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    count += (*sliceIntersectionIt)->RenderOverlay(viewport);
    }
  return count;
}


//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::PrintSelf(ostream & os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);
}


//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SliceNodeModifiedCallback(
  vtkObject * caller, unsigned long vtkNotUsed(eid), void* clientData, void* vtkNotUsed(callData))
{
  vtkMRMLSliceIntersectionInteractionRepresentation* self = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast((vtkObject*)clientData);

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(caller);
  if (sliceNode)
    {
    self->SliceNodeModified(sliceNode);
    return;
    }

  vtkMRMLSliceLogic* sliceLogic = vtkMRMLSliceLogic::SafeDownCast(caller);
  if (sliceLogic)
    {
    self->UpdateSliceIntersectionDisplay(self->GetDisplayPipelineFromSliceLogic(sliceLogic));
    return;
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SliceNodeModified(vtkMRMLSliceNode * sliceNode)
{
  if (!sliceNode)
    {
    return;
    }
  if (sliceNode == this->Internal->SliceNode)
    {
    // update all slice intersection
    for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
      sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
      sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
      {
      this->UpdateSliceIntersectionDisplay(*sliceIntersectionIt);
      }
    }
}

//----------------------------------------------------------------------
SliceIntersectionInteractionDisplayPipeline* vtkMRMLSliceIntersectionInteractionRepresentation::GetDisplayPipelineFromSliceLogic(vtkMRMLSliceLogic * sliceLogic)
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)->SliceLogic)
      {
      continue;
      }
    if (sliceLogic == (*sliceIntersectionIt)->SliceLogic)
      {
      // found it
      return *sliceIntersectionIt;
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::UpdateSliceIntersectionDisplay(SliceIntersectionInteractionDisplayPipeline * pipeline)
{
  if (!pipeline || !this->Internal->SliceNode || pipeline->SliceLogic == nullptr)
    {
    return;
    }
  vtkMRMLSliceNode* intersectingSliceNode = pipeline->SliceLogic->GetSliceNode();
  if (!pipeline->SliceLogic || !this->GetVisibility()
    || !intersectingSliceNode
    || this->Internal->SliceNode->GetViewGroup() != intersectingSliceNode->GetViewGroup()
    || !intersectingSliceNode->IsMappedInLayout())
    {
    pipeline->SetVisibility(false);
    pipeline->SetHandlesVisibility(false);
    return;
    }

  vtkMRMLModelDisplayNode* displayNode = nullptr;
  vtkMRMLSliceCompositeNode* compositeNode = nullptr;
  vtkMRMLSliceLogic* sliceLogic = nullptr;
  vtkMRMLApplicationLogic* mrmlAppLogic = this->GetMRMLApplicationLogic();
  if (mrmlAppLogic)
    {
    sliceLogic = mrmlAppLogic->GetSliceLogic(intersectingSliceNode);
    }
  if (sliceLogic)
    {
    displayNode = sliceLogic->GetSliceModelDisplayNode();
    compositeNode = sliceLogic->GetSliceCompositeNode();
    }
  if (displayNode && compositeNode)
    {
    //bool sliceIntersectionVisible = displayNode->GetSliceIntersectionVisibility();
    bool sliceInteractionHandlesVisible = compositeNode->GetSliceIntersectionHandlesVisibility();
    if (!sliceInteractionHandlesVisible)
      {
      pipeline->SetVisibility(false);
      pipeline->SetHandlesVisibility(false);
      return;
      }
    pipeline->IntersectionLineProperty->SetLineWidth(displayNode->GetLineWidth()+1.0);
    }

  // Set color of handles
  pipeline->IntersectionLineProperty->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->TranslationHandleProperty->SetColor(255, 255, 255); // white color
  pipeline->RotationHandle1Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->RotationHandle2Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->SliceOffsetHandle1Property->SetColor(intersectingSliceNode->GetLayoutColor());
  pipeline->SliceOffsetHandle2Property->SetColor(intersectingSliceNode->GetLayoutColor());

  // Get slice node transforms
  vtkMatrix4x4* xyToRAS = this->Internal->SliceNode->GetXYToRAS();
  vtkNew<vtkMatrix4x4> rasToXY;
  vtkMatrix4x4::Invert(xyToRAS, rasToXY);

  // Get slice intersection point XY
  this->GetSliceIntersectionPoint(); // Get slice intersection point RAS
  double sliceIntersectionPoint_XY[4] = { this->SliceIntersectionPoint[0], this->SliceIntersectionPoint[1], this->SliceIntersectionPoint[2], 1 };
  rasToXY->MultiplyPoint(sliceIntersectionPoint_XY, sliceIntersectionPoint_XY); // Get slice intersection point XY
  double sliceIntersectionPoint_XY_2D[2] = { sliceIntersectionPoint_XY[0], sliceIntersectionPoint_XY[1] };

  // Get intersection line tips
  vtkMatrix4x4* intersectingXYToRAS = intersectingSliceNode->GetXYToRAS();
  vtkNew<vtkMatrix4x4> intersectingXYToXY;
  vtkMatrix4x4::Multiply4x4(rasToXY, intersectingXYToRAS, intersectingXYToXY);
  double intersectionLineTip1[3] = { 0.0, 0.0, 0.0};
  double intersectionLineTip2[3] = { 0.0, 0.0, 0.0};
  int intersectionFound = this->GetLineTipsFromIntersectingSliceNode(intersectingSliceNode, intersectingXYToXY,
      intersectionLineTip1, intersectionLineTip2);
  double intersectionLineTip1_2D[2] = { intersectionLineTip1[0], intersectionLineTip1[1] };
  double intersectionLineTip2_2D[2] = { intersectionLineTip2[0], intersectionLineTip2[1] };

  // Pipelines not visible if no intersection is found
  if (!intersectionFound)
    {
    pipeline->SetVisibility(false);
    pipeline->SetHandlesVisibility(false);
    return;
    }

  // Get current slice view bounds
  vtkMRMLSliceNode* currentSliceNode = this->GetSliceNode();
  double sliceViewBounds[4] = {};
  this->GetSliceViewBoundariesXY(currentSliceNode, sliceViewBounds);

  // Add margin to slice view bounds
  sliceViewBounds[0] = sliceViewBounds[0] + (sliceViewBounds[1] - sliceViewBounds[0]) * FOV_HANDLES_MARGIN;
  sliceViewBounds[1] = sliceViewBounds[1] - (sliceViewBounds[1] - sliceViewBounds[0]) * FOV_HANDLES_MARGIN;
  sliceViewBounds[2] = sliceViewBounds[2] + (sliceViewBounds[3] - sliceViewBounds[2]) * FOV_HANDLES_MARGIN;
  sliceViewBounds[3] = sliceViewBounds[3] - (sliceViewBounds[3] - sliceViewBounds[2]) * FOV_HANDLES_MARGIN;

  // Adjust intersection points to FOV margins
  bool intersectionFoundLineTip1 = false;
  double intersectionPointLineTip1[2] = { 0.0,0.0 };
  bool intersectionFoundLineTip2 = false;
  double intersectionPointLineTip2[2] = { 0.0,0.0 };
  if ((sliceIntersectionPoint_XY[0] > sliceViewBounds[0]) || // If intersection point is within FOV
    (sliceIntersectionPoint_XY[0] < sliceViewBounds[1]) ||
    (sliceIntersectionPoint_XY[1] > sliceViewBounds[2]) ||
    (sliceIntersectionPoint_XY[1] < sliceViewBounds[3]))
    {
    if ((intersectionLineTip1[0] < sliceViewBounds[0]) || // If line tip 1 is outside the FOV
    (intersectionLineTip1[0] > sliceViewBounds[1]) ||
    (intersectionLineTip1[1] < sliceViewBounds[2]) ||
    (intersectionLineTip1[1] > sliceViewBounds[3]))
      {
      intersectionFoundLineTip1 = this->GetIntersectionWithSliceViewBoundaries(intersectionLineTip1_2D, sliceIntersectionPoint_XY_2D,
                                                                                  sliceViewBounds, intersectionLineTip1_2D);
      }
    if ((intersectionLineTip2[0] < sliceViewBounds[0]) || // If line tip 2 is outside the FOV
      (intersectionLineTip2[0] > sliceViewBounds[1]) ||
      (intersectionLineTip2[1] < sliceViewBounds[2]) ||
      (intersectionLineTip2[1] > sliceViewBounds[3]))
      {
      intersectionFoundLineTip2 = this->GetIntersectionWithSliceViewBoundaries(intersectionLineTip2_2D, sliceIntersectionPoint_XY_2D,
                                                                                  sliceViewBounds, intersectionLineTip2_2D);
      }
    }

  // Define intersection lines
  pipeline->IntersectionLine->SetPoint1(intersectionLineTip1);
  pipeline->IntersectionLine->SetPoint2(intersectionLineTip2);

  // Set translation handle position
  vtkNew<vtkPoints> translationHandlePoints;
  double translationHandlePosition[3] = { sliceIntersectionPoint_XY[0], sliceIntersectionPoint_XY[1], 0.0 };
  pipeline->TranslationHandle->SetCenter(translationHandlePosition[0], translationHandlePosition[1], translationHandlePosition[2]);
  translationHandlePoints->InsertNextPoint(translationHandlePosition);
  pipeline->TranslationHandlePoints->SetPoints(translationHandlePoints);

  // Set position of rotation handles
  vtkNew<vtkPoints> rotationHandlePoints;
  double rotationHandle1Position[3] = { intersectionLineTip1_2D[0], intersectionLineTip1_2D[1], 0.0};
  double rotationHandle2Position[3] = { intersectionLineTip2_2D[0], intersectionLineTip2_2D[1], 0.0 };
  rotationHandlePoints->InsertNextPoint(rotationHandle1Position);
  rotationHandlePoints->InsertNextPoint(rotationHandle2Position);
  pipeline->RotationHandle1->SetCenter(rotationHandle1Position[0], rotationHandle1Position[1], rotationHandle1Position[2]);
  pipeline->RotationHandle2->SetCenter(rotationHandle2Position[0], rotationHandle2Position[1], rotationHandle2Position[2]);
  pipeline->RotationHandlePoints->SetPoints(rotationHandlePoints);

  // Set position of slice translation handles
  vtkNew<vtkPoints> sliceOffsetHandlePoints;
  double sliceOffsetHandle1Position[3] = { (rotationHandle1Position[0] + sliceIntersectionPoint_XY[0]) / 2 ,
                                           (rotationHandle1Position[1] + sliceIntersectionPoint_XY[1]) / 2 , 0.0 };
  double sliceOffsetHandle2Position[3] = { (rotationHandle2Position[0] + sliceIntersectionPoint_XY[0]) / 2 ,
                                           (rotationHandle2Position[1] + sliceIntersectionPoint_XY[1]) / 2 , 0.0 };
  sliceOffsetHandlePoints->InsertNextPoint(sliceOffsetHandle1Position);
  sliceOffsetHandlePoints->InsertNextPoint(sliceOffsetHandle2Position);
  pipeline->SliceOffsetHandle1->SetCenter(sliceOffsetHandle1Position[0], sliceOffsetHandle1Position[1], sliceOffsetHandle1Position[2]);
  pipeline->SliceOffsetHandle2->SetCenter(sliceOffsetHandle2Position[0], sliceOffsetHandle2Position[1], sliceOffsetHandle2Position[2]);
  pipeline->SliceOffsetHandlePoints->SetPoints(sliceOffsetHandlePoints);

  // Store rotation and translation handle positions
  vtkNew<vtkPoints> handlePoints;
  handlePoints->InsertNextPoint(translationHandlePosition);
  handlePoints->InsertNextPoint(rotationHandle1Position);
  handlePoints->InsertNextPoint(rotationHandle2Position);
  handlePoints->InsertNextPoint(sliceOffsetHandle1Position);
  handlePoints->InsertNextPoint(sliceOffsetHandle2Position);

  // Line resolution
  pipeline->IntersectionLine->SetResolution(50);
  pipeline->IntersectionLine->Update();

  // Define points along intersection line for interaction
  vtkPoints* linePointsDefault;
  linePointsDefault = pipeline->IntersectionLine->GetOutput()->GetPoints();

  // Filter out those line points near interaction handles
  // If this is not done, when clicking over the handle, interaction with line (right below) could occur instead.
  vtkNew<vtkPoints> linePointsFiltered;
  vtkIdType numLinePoints = linePointsDefault->GetNumberOfPoints();
  vtkIdType numHandlePoints = handlePoints->GetNumberOfPoints();
  double linePoint[3] = { 0.0, 0.0, 0.0 };
  double handlePoint[3] = { 0.0, 0.0, 0.0 };
  bool closeToHandle;
  for (int i = 0; i < numLinePoints; i++)
    {
    linePointsDefault->GetPoint(i, linePoint);
    closeToHandle = false;
    for (int j = 0; j < numHandlePoints; j++)
      {
      handlePoints->GetPoint(j, handlePoint);
      double linePointToHandlePoint[3] = { handlePoint[0] - linePoint[0], handlePoint[1] - linePoint[1] , handlePoint[2] - linePoint[2] };
      double distanceLinePointToHandlePoint = vtkMath::Norm(linePointToHandlePoint);
      if (distanceLinePointToHandlePoint < LINE_POINTS_FILTERING_THRESHOLD)
        {
        closeToHandle = true;
        break;
        }
      }
    if (!closeToHandle)
      {
      linePointsFiltered->InsertNextPoint(linePoint);
      }
    }
  vtkIdType numPoints = linePointsFiltered->GetNumberOfPoints();
  pipeline->IntersectionLinePoints->SetPoints(linePointsFiltered);

  // Visibility
  pipeline->SetVisibility(true);
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceViewBoundariesXY(vtkMRMLSliceNode* sliceNode, double* sliceViewBounds)
{
  // Get FOV of current slice node in mm
  char* sliceNodeName = sliceNode->GetName();
  double sliceFOVMm[3] = { 0.0,0.0,0.0 };
  sliceNode->GetFieldOfView(sliceFOVMm);

  // Get XYToRAS and RASToXY transform matrices
  vtkMatrix4x4* currentXYToRAS = sliceNode->GetXYToRAS();
  vtkNew<vtkMatrix4x4> currentRASToXY;
  vtkMatrix4x4::Invert(currentXYToRAS, currentRASToXY);

  // Get slice view axes in RAS
  double sliceOrigin[4] = { 0.0,0.0,0.0,1.0 };
  double slicePointAxisX[4] = { 100.0,0.0,0.0,1.0 };
  double slicePointAxisY[4] = { 0.0,100.0,0.0,1.0 };
  currentXYToRAS->MultiplyPoint(sliceOrigin, sliceOrigin);
  currentXYToRAS->MultiplyPoint(slicePointAxisX, slicePointAxisX);
  currentXYToRAS->MultiplyPoint(slicePointAxisY, slicePointAxisY);
  double sliceAxisX[3] = { slicePointAxisX[0] - sliceOrigin[0],
                           slicePointAxisX[1] - sliceOrigin[1],
                           slicePointAxisX[2] - sliceOrigin[2] };
  double sliceAxisY[3] = { slicePointAxisY[0] - sliceOrigin[0],
                           slicePointAxisY[1] - sliceOrigin[1],
                           slicePointAxisY[2] - sliceOrigin[2] };
  vtkMath::Normalize(sliceAxisX);
  vtkMath::Normalize(sliceAxisY);

  // Calculate corners of FOV in RAS coordinate system
  double bottomLeftCornerRAS[4] = { 0.0,0.0,0.0,1.0 };
  double topLeftCornerRAS[4] = { 0.0,0.0,0.0,1.0 };
  double bottomRightCornerRAS[4] = { 0.0,0.0,0.0,1.0 };
  double topRightCornerRAS[4] = { 0.0,0.0,0.0,1.0 };

  // Get slice view corners RAS
  bottomLeftCornerRAS[0] = sliceOrigin[0];
  bottomLeftCornerRAS[1] = sliceOrigin[1];
  bottomLeftCornerRAS[2] = sliceOrigin[2];
  topLeftCornerRAS[0] = sliceOrigin[0] + sliceAxisY[0] * sliceFOVMm[1];
  topLeftCornerRAS[1] = sliceOrigin[1] + sliceAxisY[1] * sliceFOVMm[1];
  topLeftCornerRAS[2] = sliceOrigin[2] + sliceAxisY[2] * sliceFOVMm[1];
  bottomRightCornerRAS[0] = sliceOrigin[0] + sliceAxisX[0] * sliceFOVMm[0];
  bottomRightCornerRAS[1] = sliceOrigin[1] + sliceAxisX[1] * sliceFOVMm[0];
  bottomRightCornerRAS[2] = sliceOrigin[2] + sliceAxisX[2] * sliceFOVMm[0];
  topRightCornerRAS[0] = sliceOrigin[0] + sliceAxisY[0] * sliceFOVMm[1] + sliceAxisX[0] * sliceFOVMm[0];
  topRightCornerRAS[1] = sliceOrigin[1] + sliceAxisY[1] * sliceFOVMm[1] + sliceAxisX[1] * sliceFOVMm[0];
  topRightCornerRAS[2] = sliceOrigin[2] + sliceAxisY[2] * sliceFOVMm[1] + sliceAxisX[2] * sliceFOVMm[0];

  // Calculate corners of FOV in XY coordinate system
  double bottomLeftCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  double topLeftCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  double bottomRightCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  double topRightCornerXY[4] = { 0.0,0.0,0.0,1.0 };
  currentRASToXY->MultiplyPoint(bottomLeftCornerRAS, bottomLeftCornerXY);
  currentRASToXY->MultiplyPoint(topLeftCornerRAS, topLeftCornerXY);
  currentRASToXY->MultiplyPoint(bottomRightCornerRAS, bottomRightCornerXY);
  currentRASToXY->MultiplyPoint(topRightCornerRAS, topRightCornerXY);

  // Get slice view range XY
  sliceViewBounds[0] = bottomLeftCornerXY[0]; // Min value of X
  sliceViewBounds[1] = topRightCornerXY[0]; // Max value of X
  sliceViewBounds[2] = bottomLeftCornerXY[1]; //  Min value of Y
  sliceViewBounds[3] = topRightCornerXY[1]; //  Max value of Y
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionRepresentation::GetIntersectionWithSliceViewBoundaries(double* pointA, double* pointB,
      double* sliceViewBounds, double* intersectionPoint)
{
    // Get line equation -> y = slope * x + intercept
    double xA = pointA[0];
    double yA = pointA[1];
    double xB = pointB[0];
    double yB = pointB[1];
    double dx, dy, slope, intercept;
    dx = xB - xA;
    dy = yB - yA;
    slope = dy / dx;
    intercept = yA - slope * xA;

    // Get line bounding box
    double lineBounds[4] = { std::min(xA, xB), std::max(xA, xB), std::min(yA, yB), std::max(yA, yB) };

    // Slice view bounds
    double xMin = sliceViewBounds[0];
    double xMax = sliceViewBounds[1];
    double yMin = sliceViewBounds[2];
    double yMax = sliceViewBounds[3];

    // Get intersection point using line equation
    double x0, y0;
    if ((sliceViewBounds[0] > lineBounds[0]) && (sliceViewBounds[0] < lineBounds[1]))
      {
      y0 = slope * sliceViewBounds[0] + intercept;
      if ((y0 > sliceViewBounds[2]) && (y0 < sliceViewBounds[3]))
        {
        intersectionPoint[0] = sliceViewBounds[0];
        intersectionPoint[1] = y0;
        return true;
        }
      }
    if ((sliceViewBounds[1] > lineBounds[0]) && (sliceViewBounds[1] < lineBounds[1]))
      {
      y0 = slope * sliceViewBounds[1] + intercept;
      if ((y0 > sliceViewBounds[2]) && (y0 < sliceViewBounds[3]))
        {
        intersectionPoint[0] = sliceViewBounds[1];
        intersectionPoint[1] = y0;
        return true;
        }
      }
    if ((sliceViewBounds[2] > lineBounds[2]) && (sliceViewBounds[2] < lineBounds[3]))
      {
      if (std::isfinite(slope)) // check if slope is finite
        {
        x0 = (sliceViewBounds[2] - intercept)/slope;
        if ((x0 > sliceViewBounds[0]) && (x0 < sliceViewBounds[1]))
          {
          intersectionPoint[0] = x0;
          intersectionPoint[1] = sliceViewBounds[2];
          return true;
          }
        }
      else // infinite slope = vertical line
        {
          intersectionPoint[0] = lineBounds[0]; // or lineBounds[1] (if the line is vertical, then both points A and B have the same value of X)
          intersectionPoint[1] = sliceViewBounds[2];
          return true;
        }
      }
    if ((sliceViewBounds[3] > lineBounds[2]) && (sliceViewBounds[3] < lineBounds[3]))
      {
      if (std::isfinite(slope)) // check if slope is finite
        {
        x0 = (sliceViewBounds[3] - intercept)/slope;
        if ((x0 > sliceViewBounds[0]) && (x0 < sliceViewBounds[1]))
          {
          intersectionPoint[0] = x0;
          intersectionPoint[1] = sliceViewBounds[3];
          return true;
          }
        }
      else // infinite slope = vertical line
        {
          intersectionPoint[0] = lineBounds[0]; // or lineBounds[1] (if the line is vertical, then both points A and B have the same value of X)
          intersectionPoint[1] = sliceViewBounds[3];
          return true;
        }
      }
    return false;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetSliceNode(vtkMRMLSliceNode * sliceNode)
{
  if (sliceNode == this->Internal->SliceNode)
    {
    // no change
    return;
    }
  if (this->Internal->SliceNode)
    {
    this->Internal->SliceNode->RemoveObserver(this->Internal->SliceNodeModifiedCommand);
    }
  if (sliceNode)
    {
    sliceNode->AddObserver(vtkCommand::ModifiedEvent, this->Internal->SliceNodeModifiedCommand.GetPointer());
    }
  this->Internal->SliceNode = sliceNode;
  this->UpdateIntersectingSliceNodes();
}

//----------------------------------------------------------------------
vtkMRMLSliceNode* vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceNode()
{
  return this->Internal->SliceNode;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::AddIntersectingSliceLogic(vtkMRMLSliceLogic * sliceLogic)
{
  if (!sliceLogic)
    {
    return;
    }
  if (sliceLogic->GetSliceNode() == this->Internal->SliceNode)
    {
    // it is the slice itself, not an intersecting slice
    return;
    }
  if (this->GetDisplayPipelineFromSliceLogic(sliceLogic))
    {
    // slice node already added
    return;
    }

  SliceIntersectionInteractionDisplayPipeline* pipeline = new SliceIntersectionInteractionDisplayPipeline;
  pipeline->SetAndObserveSliceLogic(sliceLogic, this->Internal->SliceNodeModifiedCommand);
  pipeline->AddActors(this->Renderer);
  this->Internal->SliceIntersectionInteractionDisplayPipelines.push_back(pipeline);
  this->UpdateSliceIntersectionDisplay(pipeline);
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::RemoveIntersectingSliceNode(vtkMRMLSliceNode * sliceNode)
{
  if (!sliceNode)
    {
    return;
    }
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)->SliceLogic)
      {
      continue;
      }
    if (sliceNode == (*sliceIntersectionIt)->SliceLogic->GetSliceNode())
      {
      // found it
      (*sliceIntersectionIt)->RemoveActors(this->Renderer);
      delete (*sliceIntersectionIt);
      this->Internal->SliceIntersectionInteractionDisplayPipelines.erase(sliceIntersectionIt);
      break;
      }
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::UpdateIntersectingSliceNodes()
{
  this->RemoveAllIntersectingSliceNodes();
  if (!this->GetSliceNode() || !this->MRMLApplicationLogic)
    {
    return;
    }
  vtkCollection* sliceLogics = this->MRMLApplicationLogic->GetSliceLogics();
  if (!sliceLogics)
    {
    return;
    }
  vtkMRMLSliceLogic* sliceLogic;
  vtkCollectionSimpleIterator it;
  for (sliceLogics->InitTraversal(it);
    (sliceLogic = vtkMRMLSliceLogic::SafeDownCast(sliceLogics->GetNextItemAsObject(it)));)
    {
    this->AddIntersectingSliceLogic(sliceLogic);
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::RemoveAllIntersectingSliceNodes()
{
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->RemoveActors(this->Renderer);
    delete (*sliceIntersectionIt);
    }
  this->Internal->SliceIntersectionInteractionDisplayPipelines.clear();
}

//----------------------------------------------------------------------
double* vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceIntersectionPoint()
{
  // Slice node where interaction takes place
  vtkMRMLSliceNode* intersectingSliceNode = this->Internal->SliceNode;

  // Get slice nodes from scene
  vtkMRMLScene* scene = intersectingSliceNode->GetScene();
  if (!scene)
    {
    return this->SliceIntersectionPoint;
    }
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;

  // Red slice
  vtkMRMLSliceNode* redSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[0]);
  vtkMatrix4x4* redSliceToRASTransformMatrix = redSliceNode->GetSliceToRAS();
  double redSlicePlaneOrigin[3] = { redSliceToRASTransformMatrix->GetElement(0, 3), redSliceToRASTransformMatrix->GetElement(1,3),
                                    redSliceToRASTransformMatrix->GetElement(2,3) };
  double redSlicePlaneNormal[3] = { redSliceToRASTransformMatrix->GetElement(0, 2), redSliceToRASTransformMatrix->GetElement(1,2),
                                    redSliceToRASTransformMatrix->GetElement(2,2) };

  // Green slice
  vtkMRMLSliceNode* greenSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[1]);
  vtkMatrix4x4* greenSliceToRASTransformMatrix = greenSliceNode->GetSliceToRAS();
  double greenSlicePlaneOrigin[3] = { greenSliceToRASTransformMatrix->GetElement(0, 3), greenSliceToRASTransformMatrix->GetElement(1,3),
                                      greenSliceToRASTransformMatrix->GetElement(2,3) };
  double greenSlicePlaneNormal[3] = { greenSliceToRASTransformMatrix->GetElement(0, 2), greenSliceToRASTransformMatrix->GetElement(1,2),
                                      greenSliceToRASTransformMatrix->GetElement(2,2) };

  // Yellow slice
  vtkMRMLSliceNode* yellowSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[2]);
  vtkMatrix4x4* yellowSliceToRASTransformMatrix = yellowSliceNode->GetSliceToRAS();
  double yellowSlicePlaneOrigin[3] = { yellowSliceToRASTransformMatrix->GetElement(0, 3), yellowSliceToRASTransformMatrix->GetElement(1,3),
                                       yellowSliceToRASTransformMatrix->GetElement(2,3) };
  double yellowSlicePlaneNormal[3] = { yellowSliceToRASTransformMatrix->GetElement(0, 2), yellowSliceToRASTransformMatrix->GetElement(1,2),
                                       yellowSliceToRASTransformMatrix->GetElement(2,2) };

  // Build matrix with normals (n1,n2,n3) and compute determinant det(n1,n2,n3)
  vtkNew<vtkMatrix3x3> slicePlaneNormalsMatrix;
  slicePlaneNormalsMatrix->SetElement(0, 0, redSlicePlaneNormal[0]);
  slicePlaneNormalsMatrix->SetElement(0, 1, redSlicePlaneNormal[1]);
  slicePlaneNormalsMatrix->SetElement(0, 2, redSlicePlaneNormal[2]);
  slicePlaneNormalsMatrix->SetElement(1, 0, greenSlicePlaneNormal[0]);
  slicePlaneNormalsMatrix->SetElement(1, 1, greenSlicePlaneNormal[1]);
  slicePlaneNormalsMatrix->SetElement(1, 2, greenSlicePlaneNormal[2]);
  slicePlaneNormalsMatrix->SetElement(2, 0, yellowSlicePlaneNormal[0]);
  slicePlaneNormalsMatrix->SetElement(2, 1, yellowSlicePlaneNormal[1]);
  slicePlaneNormalsMatrix->SetElement(2, 2, yellowSlicePlaneNormal[2]);
  double determinant = slicePlaneNormalsMatrix->Determinant();

  // 3-plane intersection point: P = (( point_on1 • n1 )( n2 × n3 ) + ( point_on2 • n2 )( n3 × n1 ) + ( point_on3 • n3 )( n1 × n2 )) / det(n1,n2,n3)
  double d1 = vtkMath::Dot(redSlicePlaneOrigin, redSlicePlaneNormal);
  double cross1[3];
  vtkMath::Cross(greenSlicePlaneNormal, yellowSlicePlaneNormal, cross1);
  vtkMath::MultiplyScalar(cross1, d1);
  double d2 = vtkMath::Dot(greenSlicePlaneOrigin, greenSlicePlaneNormal);
  double cross2[3];
  vtkMath::Cross(yellowSlicePlaneNormal, redSlicePlaneNormal, cross2);
  vtkMath::MultiplyScalar(cross2, d2);
  double d3 = vtkMath::Dot(yellowSlicePlaneOrigin, yellowSlicePlaneNormal);
  double cross3[3];
  vtkMath::Cross(redSlicePlaneNormal, greenSlicePlaneNormal, cross3);
  vtkMath::MultiplyScalar(cross3, d3);
  double crossSum12[3];
  vtkMath::Add(cross1, cross2, crossSum12);
  double sliceIntersectionPoint[3];
  vtkMath::Add(crossSum12, cross3, sliceIntersectionPoint);
  vtkMath::MultiplyScalar(sliceIntersectionPoint, 1 / determinant);

  this->SliceIntersectionPoint[0] = sliceIntersectionPoint[0];
  this->SliceIntersectionPoint[1] = sliceIntersectionPoint[1];
  this->SliceIntersectionPoint[2] = sliceIntersectionPoint[2];

  return this->SliceIntersectionPoint;
}

//---------------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::IntersectWithFinitePlane(double n[3], double o[3],
  double pOrigin[3], double px[3], double py[3],
  double x0[3], double x1[3])
{
  // Since we are dealing with convex shapes, if there is an intersection a
  // single line is produced as output. So all this is necessary is to
  // intersect the four bounding lines of the finite line and find the two
  // intersection points.
  int numInts = 0;
  double t, * x = x0;
  double xr0[3], xr1[3];

  // First line
  xr0[0] = pOrigin[0];
  xr0[1] = pOrigin[1];
  xr0[2] = pOrigin[2];
  xr1[0] = px[0];
  xr1[1] = px[1];
  xr1[2] = px[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    x = x1;
    }

  // Second line
  xr1[0] = py[0];
  xr1[1] = py[1];
  xr1[2] = py[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    x = x1;
    }
  if (numInts == 2)
    {
    return 1;
    }

  // Third line
  xr0[0] = -pOrigin[0] + px[0] + py[0];
  xr0[1] = -pOrigin[1] + px[1] + py[1];
  xr0[2] = -pOrigin[2] + px[2] + py[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    x = x1;
    }
  if (numInts == 2)
    {
    return 1;
    }

  // Fourth and last line
  xr1[0] = px[0];
  xr1[1] = px[1];
  xr1[2] = px[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
    {
    numInts++;
    }
  if (numInts == 2)
    {
    return 1;
    }

  // No intersection has occurred, or a single degenerate point
  return 0;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::TransformIntersectingSlices(vtkMatrix4x4 * rotatedSliceToSliceTransformMatrix)
{
  std::deque<int> wasModified;
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)
      || !(*sliceIntersectionIt)->GetVisibility())
      {
      continue;
      }
    vtkMRMLSliceNode* sliceNode = (*sliceIntersectionIt)->SliceLogic->GetSliceNode();
    wasModified.push_back(sliceNode->StartModify());

    vtkNew<vtkMatrix4x4> rotatedSliceToRAS;
    vtkMatrix4x4::Multiply4x4(rotatedSliceToSliceTransformMatrix, sliceNode->GetSliceToRAS(),
      rotatedSliceToRAS);

    sliceNode->GetSliceToRAS()->DeepCopy(rotatedSliceToRAS);
    }

  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)
      || !(*sliceIntersectionIt)->GetVisibility())
      {
      continue;
      }
    vtkMRMLSliceNode* sliceNode = (*sliceIntersectionIt)->SliceLogic->GetSliceNode();
    sliceNode->UpdateMatrices();
    sliceNode->EndModify(wasModified.front());
    wasModified.pop_front();
    }
}

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetMaximumHandlePickingDistance2()
{
  double maximumHandlePickingDistance = this->InteractionSize / 2.0 + this->PickingTolerance * this->ScreenScaleFactor;
  return maximumHandlePickingDistance * maximumHandlePickingDistance;
}

//-----------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::CanInteract(vtkMRMLInteractionEventData* interactionEventData,
  int& foundComponentType, int& foundComponentIndex, int& intersectingSliceNodeIndex, double& closestDistance2, double& handleOpacity)
{
  foundComponentType = InteractionNone;
  vtkMRMLAbstractViewNode* viewNode = this->GetViewNode();
  /*
  if (!viewNode || !this->GetVisibility() || !interactionEventData)
    {
    return;
    }
  */
  closestDistance2 = VTK_DOUBLE_MAX; // in display coordinate system
  foundComponentIndex = -1;
  intersectingSliceNodeIndex = -1;
  handleOpacity = 0.0;

  double maxPickingDistanceFromControlPoint2 = this->GetMaximumHandlePickingDistance2();
  double extendedPickingDistanceFromControlPoint2 = maxPickingDistanceFromControlPoint2 + OPACITY_RANGE;
  double displayPosition3[3] = { 0.0, 0.0, 0.0 };
  // Display position is valid in case of desktop interactions. Otherwise it is a 3D only context such as
  // virtual reality, and then we expect a valid world position in the absence of display position.
  if (interactionEventData->IsDisplayPositionValid())
    {
    const int* displayPosition = interactionEventData->GetDisplayPosition();
    displayPosition3[0] = static_cast<double>(displayPosition[0]);
    displayPosition3[1] = static_cast<double>(displayPosition[1]);
    }
  else if (!interactionEventData->IsWorldPositionValid())
    {
    return;
    }

  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    if (!(*sliceIntersectionIt)
      || !(*sliceIntersectionIt)->GetVisibility())
      {
      continue;
      }
    vtkMRMLSliceNode* sliceNode = this->GetSliceNode();
    if (sliceNode)
      {
      double handleDisplayPos[4] = { 0.0, 0.0, 0.0, 1.0 };

      vtkNew<vtkMatrix4x4> rasToxyMatrix;
      vtkMatrix4x4::Invert(sliceNode->GetXYToRAS(), rasToxyMatrix);

      bool handlePicked = false;
      double opacity = 0.0;
      HandleInfoList handleInfoList = this->GetHandleInfoList((*sliceIntersectionIt));
      for (HandleInfo handleInfo : handleInfoList)
        {
        double* handleWorldPos = handleInfo.PositionWorld;
        rasToxyMatrix->MultiplyPoint(handleWorldPos, handleDisplayPos);
        handleDisplayPos[2] = displayPosition3[2]; // Handles are always projected
        double dist2 = vtkMath::Distance2BetweenPoints(handleDisplayPos, displayPosition3);
        if (dist2 < extendedPickingDistanceFromControlPoint2)
          {
          if (dist2 < maxPickingDistanceFromControlPoint2)
            {
            handleOpacity = 1.0;
            if (dist2 < closestDistance2)
              {
              closestDistance2 = dist2;
              foundComponentType = handleInfo.ComponentType;
              foundComponentIndex = handleInfo.Index;
              intersectingSliceNodeIndex = handleInfo.IntersectingSliceNodeIndex;
              handlePicked = true;
              }
            }
          else
            {
            opacity = (dist2 - extendedPickingDistanceFromControlPoint2) / ( - OPACITY_RANGE);
            if (opacity > handleOpacity)
              {
              handleOpacity = opacity;
              }
            }
          }
        }
      }
    else
      {
      bool handlePicked = false;
      HandleInfoList handleInfoList = this->GetHandleInfoList((*sliceIntersectionIt));
      for (HandleInfo handleInfo : handleInfoList)
        {
        double* handleWorldPos = handleInfo.PositionWorld;
        double handleDisplayPos[3] = { 0 };

        if (interactionEventData->IsDisplayPositionValid())
          {
          double pixelTolerance = this->InteractionSize / 2.0 / this->GetViewScaleFactorAtPosition(handleWorldPos)
            + this->PickingTolerance * this->ScreenScaleFactor;
          this->Renderer->SetWorldPoint(handleWorldPos);
          this->Renderer->WorldToDisplay();
          this->Renderer->GetDisplayPoint(handleDisplayPos);
          handleDisplayPos[2] = 0.0;
          double dist2 = vtkMath::Distance2BetweenPoints(handleDisplayPos, displayPosition3);
          if (dist2 < pixelTolerance * pixelTolerance && dist2 < closestDistance2)
            {
            closestDistance2 = dist2;
            foundComponentType = handleInfo.ComponentType;
            foundComponentIndex = handleInfo.Index;
            intersectingSliceNodeIndex = handleInfo.IntersectingSliceNodeIndex;
            handlePicked = true;
            }
          }
        else
          {
          const double* worldPosition = interactionEventData->GetWorldPosition();
          double worldTolerance = this->InteractionSize / 2.0 +
            this->PickingTolerance / interactionEventData->GetWorldToPhysicalScale();
          double dist2 = vtkMath::Distance2BetweenPoints(handleWorldPos, worldPosition);
          if (dist2 < worldTolerance * worldTolerance && dist2 < closestDistance2)
            {
            closestDistance2 = dist2;
            foundComponentType = handleInfo.ComponentType;
            foundComponentIndex = handleInfo.Index;
            intersectingSliceNodeIndex = handleInfo.IntersectingSliceNodeIndex;
            }
          }
        }
      }
    }
  }

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::HandleInfoList
vtkMRMLSliceIntersectionInteractionRepresentation::GetHandleInfoList(SliceIntersectionInteractionDisplayPipeline* pipeline)
{
  vtkMRMLSliceNode* currentSliceNode = this->GetSliceNode(); // Get slice node
  vtkMRMLSliceNode* intersectingSliceNode = pipeline->SliceLogic->GetSliceNode(); // Get intersecting slice node
  int intersectingSliceNodeIndex = this->GetSliceNodeIndex(intersectingSliceNode);
  vtkMatrix4x4* currentXYToRAS = currentSliceNode->GetXYToRAS(); // Get XY to RAS transform matrix
  HandleInfoList handleInfoList;
  for (int i = 0; i < pipeline->RotationHandlePoints->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->RotationHandlePoints->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionRotationHandle, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  for (int i = 0; i < pipeline->TranslationHandlePoints->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->TranslationHandlePoints->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionTranslationHandle, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  for (int i = 0; i < pipeline->SliceOffsetHandlePoints->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->SliceOffsetHandlePoints->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionSliceOffsetHandle, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  for (int i = 0; i < pipeline->IntersectionLinePoints->GetNumberOfPoints(); ++i)
    {
    double handlePositionLocal[3] = { 0 };
    double handlePositionLocal_h[4] = { 0.0,0.0,0.0,1.0 };
    double handlePositionWorld[3] = { 0 };
    double handlePositionWorld_h[4] = { 0.0,0.0,0.0,1.0 };
    pipeline->IntersectionLinePoints->GetPoint(i, handlePositionLocal);
    handlePositionLocal_h[0] = handlePositionLocal[0];
    handlePositionLocal_h[1] = handlePositionLocal[1];
    handlePositionLocal_h[2] = handlePositionLocal[2];
    currentXYToRAS->MultiplyPoint(handlePositionLocal_h, handlePositionWorld_h);
    handlePositionWorld[0] = handlePositionWorld_h[0];
    handlePositionWorld[1] = handlePositionWorld_h[1];
    handlePositionWorld[2] = handlePositionWorld_h[2];
    HandleInfo info(i, InteractionIntersectionLine, intersectingSliceNodeIndex, handlePositionWorld, handlePositionLocal);
    handleInfoList.push_back(info);
    }

  return handleInfoList;
}

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetViewScaleFactorAtPosition(double positionWorld[3])
{
  double viewScaleFactorMmPerPixel = 1.0;
  if (!this->Renderer || !this->Renderer->GetActiveCamera())
    {
    return viewScaleFactorMmPerPixel;
    }

  vtkCamera* cam = this->Renderer->GetActiveCamera();
  if (cam->GetParallelProjection())
    {
    // Viewport: xmin, ymin, xmax, ymax; range: 0.0-1.0; origin is bottom left
    // Determine the available renderer size in pixels
    double minX = 0;
    double minY = 0;
    this->Renderer->NormalizedDisplayToDisplay(minX, minY);
    double maxX = 1;
    double maxY = 1;
    this->Renderer->NormalizedDisplayToDisplay(maxX, maxY);
    int rendererSizeInPixels[2] = { static_cast<int>(maxX - minX), static_cast<int>(maxY - minY) };
    // Parallel scale: height of the viewport in world-coordinate distances.
    // Larger numbers produce smaller images.
    viewScaleFactorMmPerPixel = (cam->GetParallelScale() * 2.0) / double(rendererSizeInPixels[1]);
    }
  else
    {
    double cameraFP[4] = { positionWorld[0], positionWorld[1], positionWorld[2], 1.0 };

    double cameraViewUp[3] = { 0 };
    cam->GetViewUp(cameraViewUp);
    vtkMath::Normalize(cameraViewUp);

    // Get distance in pixels between two points at unit distance above and below the focal point
    this->Renderer->SetWorldPoint(cameraFP[0] + cameraViewUp[0], cameraFP[1] + cameraViewUp[1], cameraFP[2] + cameraViewUp[2], cameraFP[3]);
    this->Renderer->WorldToDisplay();
    double topCenter[3] = { 0 };
    this->Renderer->GetDisplayPoint(topCenter);
    topCenter[2] = 0.0;
    this->Renderer->SetWorldPoint(cameraFP[0] - cameraViewUp[0], cameraFP[1] - cameraViewUp[1], cameraFP[2] - cameraViewUp[2], cameraFP[3]);
    this->Renderer->WorldToDisplay();
    double bottomCenter[3] = { 0 };
    this->Renderer->GetDisplayPoint(bottomCenter);
    bottomCenter[2] = 0.0;
    double distInPixels = sqrt(vtkMath::Distance2BetweenPoints(topCenter, bottomCenter));

    // if render window is not initialized yet then distInPixels == 0.0,
    // in that case just leave the default viewScaleFactorMmPerPixel
    if (distInPixels > 1e-3)
      {
      // 2.0 = 2x length of viewUp vector in mm (because viewUp is unit vector)
      viewScaleFactorMmPerPixel = 2.0 / distInPixels;
      }
    }
  return viewScaleFactorMmPerPixel;
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceNodeIndex(vtkMRMLSliceNode* sliceNode)
{
  // Get red, green and yellow slice nodes
  vtkMRMLScene* scene = sliceNode->GetScene();
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;
  vtkMRMLSliceNode* redSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[0]);
  vtkMRMLSliceNode* greenSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[1]);
  vtkMRMLSliceNode* yellowSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[2]);

  // Slice node index
  int sliceNodeIndex = -1;

  if (sliceNode == redSliceNode)
    {
    sliceNodeIndex = 0;
    }
  else if (sliceNode == greenSliceNode)
    {
    sliceNodeIndex = 1;
    }
  else if (sliceNode == yellowSliceNode)
    {
    sliceNodeIndex = 2;
    }

  return sliceNodeIndex;
}

//----------------------------------------------------------------------
vtkMRMLSliceNode* vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceNodeFromIndex(vtkMRMLScene* scene, int sliceNodeIndex)
{
  // Get red, green and yellow slice nodes
  vtkMRMLScene* currentScene = vtkMRMLScene::SafeDownCast(scene);
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = currentScene ? currentScene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;

  // Slice node
  if (sliceNodeIndex == 0)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[0]); // red slice node
    return sliceNode;
    }
  else if (sliceNodeIndex == 1)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[1]); // green slice node
    return sliceNode;
    }
  else if (sliceNodeIndex == 2)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[2]); // yellow slice node
    return sliceNode;
    }
  else
    {
    vtkMRMLSliceNode* sliceNode = nullptr;
    return sliceNode;
    }
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetLineTipsFromIntersectingSliceNode(vtkMRMLSliceNode* intersectingSliceNode,
    vtkMatrix4x4* intersectingXYToXY, double intersectionLineTip1[3], double intersectionLineTip2[3])
{
  // Define current slice plane
  double slicePlaneNormal[3] = { 0.,0.,1. };
  double slicePlaneOrigin[3] = { 0., 0., 0. };

  // Define slice size dimensions
  int* intersectingSliceSizeDimensions = intersectingSliceNode->GetDimensions();
  double intersectingPlaneOrigin[4] = { 0, 0, 0, 1 };
  double intersectingPlaneX[4] = { double(intersectingSliceSizeDimensions[0]), 0., 0., 1. };
  double intersectingPlaneY[4] = { 0., double(intersectingSliceSizeDimensions[1]), 0., 1. };
  intersectingXYToXY->MultiplyPoint(intersectingPlaneOrigin, intersectingPlaneOrigin);
  intersectingXYToXY->MultiplyPoint(intersectingPlaneX, intersectingPlaneX);
  intersectingXYToXY->MultiplyPoint(intersectingPlaneY, intersectingPlaneY);

  // Compute intersection
  int intersectionFound = this->IntersectWithFinitePlane(slicePlaneNormal, slicePlaneOrigin,
    intersectingPlaneOrigin, intersectingPlaneX, intersectingPlaneY, intersectionLineTip1, intersectionLineTip2);

  return intersectionFound;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetPipelinesHandlesVisibility(bool visible)
{
  // Force visible handles in the "handles always visible" mode is activated
  if (HANDLES_ALWAYS_VISIBLE)
    {
      visible = true;
    }

  // Update handles visibility in all display pipelines
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->SetHandlesVisibility(visible);
    }
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetPipelinesHandlesOpacity(double opacity)
  {
  // Force visible handles in the "handles always visible" mode is activated
  if (HANDLES_ALWAYS_VISIBLE)
    {
    opacity = 1.0;
    }

  // Update handles visibility in all display pipelines
  for (std::deque<SliceIntersectionInteractionDisplayPipeline*>::iterator
    sliceIntersectionIt = this->Internal->SliceIntersectionInteractionDisplayPipelines.begin();
    sliceIntersectionIt != this->Internal->SliceIntersectionInteractionDisplayPipelines.end(); ++sliceIntersectionIt)
    {
    (*sliceIntersectionIt)->SetHandlesOpacity(opacity);
    }
  }
