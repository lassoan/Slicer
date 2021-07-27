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
#include "vtkMRMLSliceIntersectionRepresentation2D.h"

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
vtkMRMLSliceIntersectionInteractionWidget::vtkMRMLSliceIntersectionInteractionWidget() = default;

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionWidget::~vtkMRMLSliceIntersectionInteractionWidget() = default;

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::PrintSelf(ostream & os, vtkIndent indent)
  {
    this->Superclass::PrintSelf(os, indent);
  }

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionWidget::GetActiveComponentType()
  {
    vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation> rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
    return rep->GetActiveComponentType();
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::SetActiveComponentType(int type)
  {
    vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation> rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
    rep->SetActiveComponentType(type);
  }

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionWidget::GetActiveComponentIndex()
  {
    vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation> rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
    return rep->GetActiveComponentIndex();
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::SetActiveComponentIndex(int index)
  {
    vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation> rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
    rep->SetActiveComponentIndex(index);
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::ApplyTransform(vtkTransform* transform)
  {
  // Slice node where interaction takes place
  vtkMRMLSliceNode* sliceNode = this->GetSliceNode();

  // Iterate along slice nodes in scene to update sliceToRAS transforms
  vtkMRMLScene* scene = sliceNode->GetScene();
  if (!scene)
    {
    return;
    }
  std::vector<vtkMRMLNode*> sliceNodes;
  vtkNew<vtkMatrix4x4> transformedSliceToRAS;
  int nnodes = scene ? scene->GetNodesByClass("vtkMRMLSliceNode", sliceNodes) : 0;
  for (int i = 0; i < nnodes; i++)
    {
    vtkMRMLSliceNode* currentSliceNode = vtkMRMLSliceNode::SafeDownCast(sliceNodes[i]);
    if (currentSliceNode == sliceNode)
      {
        continue;
      }
    // Update SliceToRAS transform in slice nodes within the view group
    if (currentSliceNode->GetViewGroup() == sliceNode->GetViewGroup())
      {
      // Update SliceToRAS transform
      vtkMatrix4x4::Multiply4x4(transform->GetMatrix(), currentSliceNode->GetSliceToRAS(), transformedSliceToRAS);
      currentSliceNode->GetSliceToRAS()->DeepCopy(transformedSliceToRAS);
      currentSliceNode->UpdateMatrices();
      }
    }

  // Set handles transform in representation to update handlesToWorldTransform
  vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation> rep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->WidgetRep);
  rep->UpdateInteractionPipeline();
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::CreateDefaultRepresentation(vtkMRMLSliceNode * sliceNode,
  vtkMRMLAbstractViewNode* viewNode, vtkRenderer * renderer)
  {
    vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation> rep = vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation>::New();
    this->SetRenderer(renderer);
    this->SetRepresentation(rep);
    rep->SetViewNode(viewNode);
    rep->SetSliceNode(sliceNode);
    rep->UpdateFromMRML(nullptr, 0); // full update
  }

//----------------------------------------------------------------------
vtkMRMLSliceNode* vtkMRMLSliceIntersectionInteractionWidget::GetSliceNode()
  {
    vtkMRMLSliceIntersectionInteractionRepresentation* widgetRep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->GetRepresentation());
    if (!widgetRep)
    {
        return nullptr;
    }
    return widgetRep->GetSliceNode();
  }

//----------------------------------------------------------------------
vtkMatrix4x4* vtkMRMLSliceIntersectionInteractionWidget::GetHandlesInteractionTransformMatrix ()
  {
  vtkMRMLSliceIntersectionInteractionRepresentation* widgetRep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->GetRepresentation());
  if (!widgetRep)
    {
    return nullptr;
    }
  return widgetRep->GetHandlesInteractionTransformMatrix ();
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::UpdateHandlesInteractionTransformMatrix (vtkMatrix4x4* transform)
  {
  vtkMRMLSliceIntersectionInteractionRepresentation* widgetRep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->GetRepresentation());
  if (!widgetRep)
    {
    return;
    }
  widgetRep->UpdateHandlesInteractionTransformMatrix (transform);
  }
