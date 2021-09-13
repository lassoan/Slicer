/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMRMLSliceIntersectionInteractionWidget.cxx

  Copyright (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkMRMLSliceIntersectionInteractionWidget.h"
#include "vtkMRMLSliceIntersectionInteractionRepresentation.h"
#include "vtkMRMLSliceIntersectionInteractionRepresentation.h"

#include "vtkMRMLCrosshairDisplayableManager.h"
#include "vtkMRMLCrosshairNode.h"
#include "vtkMRMLApplicationLogic.h"
#include "vtkMRMLInteractionEventData.h"
#include "vtkMRMLInteractionNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSliceCompositeNode.h"
#include "vtkMRMLSliceLogic.h"

// VTK includes
#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkEvent.h>
#include <vtkLine.h>
#include <vtkPlane.h>
#include <vtkPointPlacer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>
#include <vtkObjectFactory.h>

// MRML includes
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLLinearTransformNode.h>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLSliceIntersectionInteractionWidget);

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionWidget::vtkMRMLSliceIntersectionInteractionWidget()
{
    this->StartEventPosition[0] = 0.0;
    this->StartEventPosition[1] = 0.0;

    this->PreviousRotationAngleRad = 0.0;

    this->PreviousEventPosition[0] = 0;
    this->PreviousEventPosition[1] = 0;

    this->StartRotationCenter[0] = 0.0;
    this->StartRotationCenter[1] = 0.0;

    this->StartRotationCenter_RAS[0] = 0.0;
    this->StartRotationCenter_RAS[1] = 0.0;
    this->StartRotationCenter_RAS[2] = 0.0;
    this->StartRotationCenter_RAS[3] = 1.0; // to allow easy homogeneous transformations

    this->TouchRotationThreshold = 10.0;
    this->TouchTranslationThreshold = 25.0;

    this->TotalTouchRotation = 0.0;
    this->TouchRotateEnabled = false;
    this->TotalTouchTranslation = 0.0;
    this->TouchTranslationEnabled = false;

    this->LastIntersectingSliceNodeIndex = -1; // Last intersecting slice node where interaction occurred

    this->SliceLogicsModifiedCommand->SetClientData(this);
    this->SliceLogicsModifiedCommand->SetCallback(vtkMRMLSliceIntersectionInteractionWidget::SliceLogicsModifiedCallback);

    this->ActionsEnabled = ActionAll;
    this->UpdateInteractionEventMapping();
}

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionWidget::~vtkMRMLSliceIntersectionInteractionWidget()
{
    this->SetMRMLApplicationLogic(nullptr);
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::UpdateInteractionEventMapping()
{
  this->EventTranslators.clear();

  // Update active component
  this->SetEventTranslation(WidgetStateIdle, vtkCommand::MouseMoveEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnWidget, vtkCommand::MouseMoveEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateIdle, vtkCommand::Move3DEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnWidget, vtkCommand::Move3DEvent, vtkEvent::NoModifier, WidgetEventMouseMove);

  if (this->GetActionEnabled(ActionRotateSliceIntersection))
    {
    this->SetEventTranslationClickAndDrag(WidgetStateOnRotationHandle, vtkCommand::LeftButtonPressEvent, vtkEvent::NoModifier,
      WidgetStateRotate, WidgetEventRotateStart, WidgetEventRotateEnd);
    }
  if (this->GetActionEnabled(ActionTranslateSliceIntersection))
    {
    this->SetEventTranslationClickAndDrag(WidgetStateOnTranslationHandle, vtkCommand::LeftButtonPressEvent, vtkEvent::NoModifier,
      WidgetStateTranslate, WidgetEventTranslateStart, WidgetEventTranslateEnd);
    }
  if (this->GetActionEnabled(ActionTranslate))
    {
    this->SetEventTranslationClickAndDrag(WidgetStateOnSliceTranslationHandle, vtkCommand::LeftButtonPressEvent, vtkEvent::NoModifier,
      WidgetStateTranslateSlice, WidgetEventTranslateSliceStart, WidgetEventTranslateSliceEnd);
    }

  // Update active interaction handle component
  this->SetEventTranslation(WidgetStateOnTranslationHandle, vtkCommand::MouseMoveEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnTranslationHandle, vtkCommand::Move3DEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnRotationHandle, vtkCommand::MouseMoveEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnRotationHandle, vtkCommand::Move3DEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnSliceTranslationHandle, vtkCommand::MouseMoveEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnSliceTranslationHandle, vtkCommand::Move3DEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnIntersectionLine, vtkCommand::MouseMoveEvent, vtkEvent::NoModifier, WidgetEventMouseMove);
  this->SetEventTranslation(WidgetStateOnIntersectionLine, vtkCommand::Move3DEvent, vtkEvent::NoModifier, WidgetEventMouseMove);

  // Context menu
  this->SetEventTranslation(WidgetStateIdle, vtkMRMLInteractionEventData::RightButtonClickEvent, vtkEvent::NoModifier, WidgetEventMenu);
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::CreateDefaultRepresentation()
{
  if (this->WidgetRep)
    {
    // already created
    return;
    }
  vtkMRMLApplicationLogic* mrmlAppLogic = this->GetMRMLApplicationLogic();
  if (!mrmlAppLogic)
    {
    vtkWarningMacro("vtkMRMLSliceIntersectionInteractionWidget::CreateDefaultRepresentation failed: application logic invalid");
    return;
    }
  vtkNew<vtkMRMLSliceIntersectionInteractionRepresentation> newRep;
  newRep->SetMRMLApplicationLogic(mrmlAppLogic);
  newRep->SetSliceNode(this->SliceNode);
  this->WidgetRep = newRep;
}

//-----------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2)
{
  unsigned long widgetEvent = this->TranslateInteractionEventToWidgetEvent(eventData);
  if (widgetEvent == WidgetEventNone)
    {
    return false;
    }
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->GetRepresentation());
  if (!rep)
    {
    return false;
    }
  int eventid = eventData->GetType();

  // Currently interacting
  if (this->WidgetState == WidgetStateTranslate
    || this->WidgetState == WidgetStateRotate
    || this->WidgetState == WidgetStateTranslateSlice)
    {
    distance2 = 0.0;
    return true;
    }

  // Interaction
  int foundComponentType = InteractionNone;
  int foundComponentIndex = -1;
  int intersectingSliceNodeIndex = -1;
  double closestDistance2 = 0.0;
  double handleOpacity = 0.0;
  rep->CanInteract(eventData, foundComponentType, foundComponentIndex, intersectingSliceNodeIndex, closestDistance2, handleOpacity);

  // Update handle visibility and opacity
  rep->SetPipelinesHandlesVisibility(true);
  rep->SetPipelinesHandlesOpacity(handleOpacity);

  // Enable updates of display pipelines in scene by modifying all associated slice nodes
  vtkMRMLScene* scene = this->SliceNode->GetScene();
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;
  for (int i = 0; i < nnodes; i++)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[i]);
    sliceNode->Modified();
    }

  // Verify interaction
  if (foundComponentType == InteractionNone)
    {
    return false;
    }

  // Store closest squared distance
  distance2 = closestDistance2;

  // Store last intersecting slice node index
  this->LastIntersectingSliceNodeIndex = intersectingSliceNodeIndex;

  return true;
}

//-----------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData)
{
  if (!this->SliceLogic)
    {
    return false;
    }

  unsigned long widgetEvent = this->TranslateInteractionEventToWidgetEvent(eventData);

  bool processedEvent = true;

  switch (widgetEvent)
    {
    case WidgetEventMouseMove:
      // click-and-dragging the mouse cursor
      processedEvent = this->ProcessMouseMove(eventData);
      break;
    case  WidgetEventTouchGestureStart:
      this->ProcessTouchGestureStart(eventData);
      break;
    case WidgetEventTouchGestureEnd:
      this->ProcessTouchGestureEnd(eventData);
      break;
    case WidgetEventTouchRotateSliceIntersection:
      this->ProcessTouchRotate(eventData);
      break;
    case WidgetEventTouchTranslateSlice:
      this->ProcessTouchTranslate(eventData);
      break;
    case WidgetEventTranslateStart:
      this->SetWidgetState(WidgetStateTranslate);
      this->SliceLogic->GetMRMLScene()->SaveStateForUndo();
      processedEvent = this->ProcessStartMouseDrag(eventData);
      break;
    case WidgetEventTranslateEnd:
      processedEvent = this->ProcessEndMouseDrag(eventData);
      break;
    case WidgetEventRotateStart:
      this->SliceLogic->GetMRMLScene()->SaveStateForUndo();
      processedEvent = this->ProcessRotateStart(eventData);
      break;
    case WidgetEventRotateEnd:
      processedEvent = this->ProcessEndMouseDrag(eventData);
      break;
    case WidgetEventTranslateSliceStart:
      this->SetWidgetState(WidgetStateTranslateSlice);
      this->SliceLogic->GetMRMLScene()->SaveStateForUndo();
      processedEvent = this->ProcessStartMouseDrag(eventData);
      break;
    case WidgetEventTranslateSliceEnd:
      processedEvent = this->ProcessEndMouseDrag(eventData);
      break;
    case WidgetEventSetCrosshairPosition:
      // Event is handled in CanProcessInteractionEvent
      break;
    default:
      processedEvent = false;
    }

  // Enable updates of display pipelines in scene by modifying all associated slice nodes
  vtkMRMLScene* scene = this->SliceNode->GetScene();
  std::vector<vtkMRMLNode*> sliceNodes;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;
  for (int i = 0; i < nnodes; i++)
    {
    vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[i]);
    sliceNode->Modified();
    }

  return processedEvent;
}

//-------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::Leave(vtkMRMLInteractionEventData* eventData)
{
    this->Superclass::Leave(eventData);
    this->activeComponentType = InteractionNone;
    this->activeComponentIndex = -1;
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessMouseMove(vtkMRMLInteractionEventData* eventData)
{
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
  if (!this->WidgetRep || !eventData)
    {
    return false;
    }

  // Get widget state
  int state = this->WidgetState;

  if (state == WidgetStateIdle
    || state == WidgetStateOnWidget
    || state == WidgetStateOnTranslationHandle
    || state == WidgetStateOnRotationHandle
    || state == WidgetStateOnSliceTranslationHandle
    || state == WidgetStateOnIntersectionLine)
    {
    // Update widget state according to distance to interaction handles
    int foundComponentType = InteractionNone;
    int foundComponentIndex = -1;
    int intersectingSliceNodeIndex = -1;
    double closestDistance2 = 0.0;
    double handleOpacity = 0.0;
    rep->CanInteract(eventData, foundComponentType, foundComponentIndex, intersectingSliceNodeIndex, closestDistance2, handleOpacity);
    if (foundComponentType == InteractionNone)
      {
      this->SetWidgetState(WidgetStateIdle);
      }
    else if (foundComponentType == InteractionTranslationHandle)
      {
      this->SetWidgetState(WidgetStateOnTranslationHandle);
      }
    else if (foundComponentType == InteractionRotationHandle)
      {
      this->SetWidgetState(WidgetStateOnRotationHandle);
      }
    else if (foundComponentType == InteractionSliceOffsetHandle)
      {
      this->SetWidgetState(WidgetStateOnSliceTranslationHandle);
      }
    else if (foundComponentType == InteractionIntersectionLine)
      {
      this->SetWidgetState(WidgetStateOnIntersectionLine);
      }
    else
      {
      this->SetWidgetState(WidgetStateOnWidget);
      }

    // Store last intersecting slice node index
    if (foundComponentType != InteractionNone)
      {
      this->LastIntersectingSliceNodeIndex = intersectingSliceNodeIndex;
      }

    this->activeComponentIndex = foundComponentIndex;
    this->activeComponentType = foundComponentType;
    }
  else
    {
    // Process the motion
    // Based on the displacement vector (computed in display coordinates) and
    // the cursor state (which corresponds to which part of the widget has been
    // selected), the widget points are modified.
    // First construct a local coordinate system based on the display coordinates
    // of the widget.
    double eventPos[2]
      {
        static_cast<double>(eventData->GetDisplayPosition()[0]),
        static_cast<double>(eventData->GetDisplayPosition()[1]),
      };

    if (state == WidgetStateTranslate)
      {
      const double* worldPos = eventData->GetWorldPosition();
      this->GetSliceNode()->JumpAllSlices(worldPos[0], worldPos[1], worldPos[2]);
      }
    else if (state == WidgetStateTranslateSlice)
      {
      this->ProcessTranslateSlice(eventData);
      }
    else if (state == WidgetStateRotate)
      {
      this->ProcessRotate(eventData);
      }
    }

  return true;
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessStartMouseDrag(vtkMRMLInteractionEventData* eventData)
{
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
  if (!rep)
    {
    return false;
    }

  const int* displayPos = eventData->GetDisplayPosition();

  this->StartEventPosition[0] = displayPos[0];
  this->StartEventPosition[1] = displayPos[1];

  this->PreviousEventPosition[0] = this->StartEventPosition[0];
  this->PreviousEventPosition[1] = this->StartEventPosition[1];

  this->ProcessMouseMove(eventData);
  return true;
}

//----------------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionWidget::GetSliceRotationAngleRad(double eventPos[2])
{
  return atan2(eventPos[1] - this->StartRotationCenter[1],
    eventPos[0] - this->StartRotationCenter[0]);
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessEndMouseDrag(vtkMRMLInteractionEventData* vtkNotUsed(eventData))
{
  if (!this->WidgetRep)
    {
    return false;
    }

  if ((this->WidgetState != vtkMRMLInteractionWidget::WidgetStateTranslate
    && this->WidgetState != vtkMRMLInteractionWidget::WidgetStateRotate
    && this->WidgetState != WidgetStateTranslateSlice
    ) || !this->WidgetRep)
    {
    return false;
    }

  int activeComponentType = this->activeComponentType;
  if (activeComponentType == InteractionTranslationHandle)
    {
    this->SetWidgetState(WidgetStateOnTranslationHandle);
    }
  else if (activeComponentType == InteractionRotationHandle)
    {
    this->SetWidgetState(WidgetStateOnRotationHandle);
    }
  else if (activeComponentType == InteractionSliceOffsetHandle)
    {
    this->SetWidgetState(WidgetStateOnSliceTranslationHandle);
    }
  else if (activeComponentType == InteractionIntersectionLine)
    {
    this->SetWidgetState(WidgetStateOnIntersectionLine);
    }
  else
    {
    this->SetWidgetState(WidgetStateOnWidget);
    }

  return true;
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessTouchGestureStart(vtkMRMLInteractionEventData* vtkNotUsed(eventData))
{
  this->SetWidgetState(WidgetStateTouchGesture);
  return true;
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessTouchGestureEnd(vtkMRMLInteractionEventData* vtkNotUsed(eventData))
{
  this->TotalTouchRotation = 0.0;
  this->TouchRotateEnabled = false;
  this->TotalTouchZoom = 0.0;
  this->TouchZoomEnabled = false;
  this->TotalTouchTranslation = 0.0;
  this->TouchTranslationEnabled = false;
  this->SetWidgetState(WidgetStateIdle);
  return true;
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessTouchRotate(vtkMRMLInteractionEventData* eventData)
{
  this->TotalTouchRotation += eventData->GetRotation() - eventData->GetLastRotation();
  if (this->TouchRotateEnabled || std::abs(this->TotalTouchRotation) >= this->TouchRotationThreshold)
    {
    this->Rotate(vtkMath::RadiansFromDegrees(eventData->GetRotation() - eventData->GetLastRotation()));
    this->TouchRotateEnabled = true;
    }
  return true;
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessTouchTranslate(vtkMRMLInteractionEventData* eventData)
{
  vtkMRMLSliceNode* sliceNode = this->SliceLogic->GetSliceNode();

  vtkMatrix4x4* xyToSlice = sliceNode->GetXYToSlice();
  const double* translate = eventData->GetTranslation();
  double translation[2] = {
    xyToSlice->GetElement(0, 0) * translate[0],
    xyToSlice->GetElement(1, 1) * translate[1]
    };


  this->TotalTouchTranslation += vtkMath::Norm2D(translate);

  if (this->TouchTranslationEnabled || this->TotalTouchTranslation >= this->TouchTranslationThreshold)
    {
    double sliceOrigin[3];
    sliceNode->GetXYZOrigin(sliceOrigin);
    sliceNode->SetSliceOrigin(sliceOrigin[0] - translation[0], sliceOrigin[1] - translation[1], 0);
    this->TouchTranslationEnabled = true;
    }

  return true;
}

//----------------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::PrintSelf(ostream& os, vtkIndent indent)
{
    Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::SetMRMLApplicationLogic(vtkMRMLApplicationLogic* appLogic)
{
  if (appLogic == this->ApplicationLogic)
    {
    return;
    }
  this->Superclass::SetMRMLApplicationLogic(appLogic);
  vtkCollection* sliceLogics = nullptr;
  this->SliceLogic = nullptr;
  if (appLogic)
    {
    sliceLogics = appLogic->GetSliceLogics();
    }
  if (sliceLogics != this->SliceLogics)
    {
    if (this->SliceLogics)
      {
      this->SliceLogics->RemoveObserver(this->SliceLogicsModifiedCommand);
      }
    if (sliceLogics)
      {
      sliceLogics->AddObserver(vtkCommand::ModifiedEvent, this->SliceLogicsModifiedCommand);
      }
    this->SliceLogics = sliceLogics;
    if (this->GetSliceNode())
      {
      this->SliceLogic = this->GetMRMLApplicationLogic()->GetSliceLogic(this->GetSliceNode());
      }
    }
}

//----------------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::SliceLogicsModifiedCallback(vtkObject* vtkNotUsed(caller),
  unsigned long vtkNotUsed(eid), void* clientData, void* vtkNotUsed(callData))
{
  vtkMRMLSliceIntersectionInteractionWidget* self = vtkMRMLSliceIntersectionInteractionWidget::SafeDownCast((vtkObject*)clientData);
  if (!self)
    {
    return;
    }
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(self->WidgetRep);
  if (rep)
    {
    rep->UpdateIntersectingSliceNodes();
    }
  self->SliceLogic = nullptr;
  if (self->GetMRMLApplicationLogic())
    {
    self->SliceLogic = self->GetMRMLApplicationLogic()->GetSliceLogic(self->GetSliceNode());
    }
}

//----------------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::SetSliceNode(vtkMRMLSliceNode* sliceNode)
{
  if (this->SliceNode == sliceNode)
    {
    // no change
    return;
    }
  this->SliceNode = sliceNode;

  // Update slice logic
  this->SliceLogic = nullptr;
  if (this->GetMRMLApplicationLogic())
    {
    this->SliceLogic = this->GetMRMLApplicationLogic()->GetSliceLogic(this->SliceNode);
    }

  // Update representation
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
  if (rep)
    {
    rep->SetSliceNode(sliceNode);
    }
}

//----------------------------------------------------------------------------------
vtkMRMLSliceNode* vtkMRMLSliceIntersectionInteractionWidget::GetSliceNode()
{
  return this->SliceNode;
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessRotateStart(vtkMRMLInteractionEventData* eventData)
{
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
  if (!rep)
    {
    return false;
    }

  this->SetWidgetState(WidgetStateRotate);

  // Save rotation center RAS
  double* sliceIntersectionPoint_RAS = rep->GetSliceIntersectionPoint(); // in RAS coordinate system
  this->StartRotationCenter_RAS[0] = sliceIntersectionPoint_RAS[0];
  this->StartRotationCenter_RAS[1] = sliceIntersectionPoint_RAS[1];
  this->StartRotationCenter_RAS[2] = sliceIntersectionPoint_RAS[2];

  // Save rotation center XY
  vtkMatrix4x4* xyToRAS = this->GetSliceNode()->GetXYToRAS();
  vtkNew<vtkMatrix4x4> rasToXY;
  vtkMatrix4x4::Invert(xyToRAS, rasToXY);
  double sliceIntersectionPoint_XY[4] = { 0, 0, 0, 1 };
  rasToXY->MultiplyPoint(sliceIntersectionPoint_RAS, sliceIntersectionPoint_XY);
  this->StartRotationCenter[0] = sliceIntersectionPoint_XY[0];
  this->StartRotationCenter[1] = sliceIntersectionPoint_XY[1];

  // Save initial rotation angle
  const int* displayPos = eventData->GetDisplayPosition();
  double displayPosDouble[2] = { static_cast<double>(displayPos[0]), static_cast<double>(displayPos[1]) };
  this->PreviousRotationAngleRad = this->GetSliceRotationAngleRad(displayPosDouble);

  return this->ProcessStartMouseDrag(eventData);
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessRotate(vtkMRMLInteractionEventData* eventData)
{
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
  if (!rep)
    {
    return false;
    }

  const int* displayPos = eventData->GetDisplayPosition();
  double displayPosDouble[2] = { static_cast<double>(displayPos[0]), static_cast<double>(displayPos[1]) };
  double sliceRotationAngleRad = this->GetSliceRotationAngleRad(displayPosDouble);

  vtkMatrix4x4* sliceToRAS = this->GetSliceNode()->GetSliceToRAS();
  vtkNew<vtkTransform> rotatedSliceToSliceTransform;

  rotatedSliceToSliceTransform->Translate(this->StartRotationCenter_RAS[0], this->StartRotationCenter_RAS[1], this->StartRotationCenter_RAS[2]);
  double rotationDirection = vtkMath::Determinant3x3(sliceToRAS->Element[0], sliceToRAS->Element[1], sliceToRAS->Element[2]) >= 0 ? 1.0 : -1.0;
  rotatedSliceToSliceTransform->RotateWXYZ(rotationDirection * vtkMath::DegreesFromRadians(sliceRotationAngleRad - this->PreviousRotationAngleRad),
    sliceToRAS->GetElement(0, 2), sliceToRAS->GetElement(1, 2), sliceToRAS->GetElement(2, 2));
  rotatedSliceToSliceTransform->Translate(-this->StartRotationCenter_RAS[0], -this->StartRotationCenter_RAS[1], -this->StartRotationCenter_RAS[2]);

  this->PreviousRotationAngleRad = sliceRotationAngleRad;
  this->PreviousEventPosition[0] = displayPos[0];
  this->PreviousEventPosition[1] = displayPos[1];

  rep->TransformIntersectingSlices(rotatedSliceToSliceTransform->GetMatrix());

  return true;
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::Rotate(double sliceRotationAngleRad)
{
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
  if (!rep)
    {
    return false;
    }
  vtkMatrix4x4* sliceToRAS = this->GetSliceNode()->GetSliceToRAS();
  vtkNew<vtkTransform> rotatedSliceToSliceTransform;

  // Save rotation center RAS
  double* sliceIntersectionPoint_RAS = rep->GetSliceIntersectionPoint(); // in RAS coordinate system
  this->StartRotationCenter_RAS[0] = sliceIntersectionPoint_RAS[0];
  this->StartRotationCenter_RAS[1] = sliceIntersectionPoint_RAS[1];
  this->StartRotationCenter_RAS[2] = sliceIntersectionPoint_RAS[2];

  // Save rotation center XY
  vtkMatrix4x4* xyToRAS = this->GetSliceNode()->GetXYToRAS();
  vtkNew<vtkMatrix4x4> rasToXY;
  vtkMatrix4x4::Invert(xyToRAS, rasToXY);
  double sliceIntersectionPoint_XY[4] = { 0, 0, 0, 1 };
  rasToXY->MultiplyPoint(sliceIntersectionPoint_RAS, sliceIntersectionPoint_XY);
  this->StartRotationCenter[0] = sliceIntersectionPoint_XY[0];
  this->StartRotationCenter[1] = sliceIntersectionPoint_XY[1];

  rotatedSliceToSliceTransform->Translate(this->StartRotationCenter_RAS[0], this->StartRotationCenter_RAS[1], this->StartRotationCenter_RAS[2]);
  double rotationDirection = vtkMath::Determinant3x3(sliceToRAS->Element[0], sliceToRAS->Element[1], sliceToRAS->Element[2]) >= 0 ? 1.0 : -1.0;
  rotatedSliceToSliceTransform->RotateWXYZ(rotationDirection * vtkMath::DegreesFromRadians(sliceRotationAngleRad),
    sliceToRAS->GetElement(0, 2), sliceToRAS->GetElement(1, 2), sliceToRAS->GetElement(2, 2));
  rotatedSliceToSliceTransform->Translate(-this->StartRotationCenter_RAS[0], -this->StartRotationCenter_RAS[1], -this->StartRotationCenter_RAS[2]);

  this->PreviousRotationAngleRad = sliceRotationAngleRad;

  rep->TransformIntersectingSlices(rotatedSliceToSliceTransform->GetMatrix());
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessTranslateSlice(vtkMRMLInteractionEventData* eventData)
{
  // Get current slice node and scene
  vtkMRMLSliceNode* sliceNode = this->SliceLogic->GetSliceNode();
  vtkMRMLScene* scene = sliceNode->GetScene();

  // Get event position
  const int* eventPosition = eventData->GetDisplayPosition();
  const double* worldPos = eventData->GetWorldPosition();

  // Get representation
  vtkMRMLSliceIntersectionInteractionRepresentation* rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->GetRepresentation());
  if (!rep)
    {
    return false;
    }

  // Get intersecting slice node
  int intersectingSliceNodeIndex = this->LastIntersectingSliceNodeIndex;
  vtkMRMLSliceNode* intersectingSliceNode = rep->GetSliceNodeFromIndex(scene, intersectingSliceNodeIndex);
  if (!intersectingSliceNode)
    {
    return false;
    }

  // Jump slice in intersecting slice node
  intersectingSliceNode->JumpSlice(worldPos[0], worldPos[1], worldPos[2]);

  // Store previous event position
  this->PreviousEventPosition[0] = eventPosition[0];
  this->PreviousEventPosition[1] = eventPosition[1];

  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessSetCrosshair(vtkMRMLInteractionEventData* eventData)
{
  vtkMRMLSliceNode* sliceNode = this->GetSliceNode();
  vtkMRMLCrosshairNode* crosshairNode = vtkMRMLCrosshairDisplayableManager::FindCrosshairNode(sliceNode->GetScene());
  if (!crosshairNode)
    {
    return false;
    }
  const double* worldPos = eventData->GetWorldPosition();
  crosshairNode->SetCrosshairRAS(const_cast<double*>(worldPos));
  if (crosshairNode->GetCrosshairBehavior() != vtkMRMLCrosshairNode::NoAction)
    {
    int viewJumpSliceMode = vtkMRMLSliceNode::OffsetJumpSlice;
    if (crosshairNode->GetCrosshairBehavior() == vtkMRMLCrosshairNode::CenteredJumpSlice)
      {
      viewJumpSliceMode = vtkMRMLSliceNode::CenteredJumpSlice;
      }
    sliceNode->JumpAllSlices(sliceNode->GetScene(),
      worldPos[0], worldPos[1], worldPos[2],
      viewJumpSliceMode, sliceNode->GetViewGroup(), sliceNode);
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::GetActionEnabled(int actionsMask)
{
  return (this->ActionsEnabled & actionsMask) == actionsMask;
}

//----------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::SetActionEnabled(int actionsMask, bool enable /*=true*/)
{
  if (enable)
    {
    this->ActionsEnabled |= actionsMask;
    }
  else
    {
    this->ActionsEnabled &= (~actionsMask);
    }
  this->UpdateInteractionEventMapping();
}

//----------------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionWidget::GetActionsEnabled()
{
  return this->ActionsEnabled;
}

//----------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::SetActionsEnabled(int actions)
{
  this->ActionsEnabled = actions;
  this->UpdateInteractionEventMapping();
}

//-------------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionWidget::ProcessWidgetMenu(vtkMRMLInteractionEventData* eventData)
{
  if (this->WidgetState != WidgetStateIdle)
    {
    return false;
    }
  if (!this->SliceNode)
    {
    return false;
    }
  vtkMRMLInteractionNode* interactionNode = this->SliceNode->GetInteractionNode();
  if (!interactionNode)
    {
    return false;
    }
  vtkNew<vtkMRMLInteractionEventData> pickEventData;
  pickEventData->SetType(vtkMRMLInteractionNode::ShowViewContextMenuEvent);
  pickEventData->SetViewNode(this->SliceNode);
  if (pickEventData->IsDisplayPositionValid())
    {
    pickEventData->SetDisplayPosition(eventData->GetDisplayPosition());
    }
  interactionNode->ShowViewContextMenu(pickEventData);
  return true;
}
