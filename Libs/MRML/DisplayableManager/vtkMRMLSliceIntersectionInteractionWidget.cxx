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
#include "vtkMRMLTransformNode.h"
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
void vtkMRMLSliceIntersectionInteractionWidget::ApplyTransform(vtkTransform * transform)
{
    if (!this->GetTransformNode())
    {
        return;
    }

    MRMLNodeModifyBlocker transformBlocker(this->GetTransformNode());

    vtkNew<vtkMatrix4x4> transformFromParent;
    this->GetTransformNode()->GetMatrixTransformToParent(transformFromParent);
    vtkNew<vtkTransform> t;
    t->Concatenate(transform);
    t->Concatenate(transformFromParent);
    this->GetTransformNode()->SetMatrixTransformToParent(t->GetMatrix());
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionWidget::CreateDefaultRepresentation(vtkMRMLTransformDisplayNode * displayNode,
    vtkMRMLAbstractViewNode * viewNode, vtkRenderer * renderer)
{
    vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation> rep = vtkSmartPointer<vtkMRMLSliceIntersectionInteractionRepresentation>::New();
    this->SetRenderer(renderer);
    this->SetRepresentation(rep);
    rep->SetViewNode(viewNode);
    rep->SetDisplayNode(displayNode);
    rep->UpdateFromMRML(nullptr, 0); // full update
}

//----------------------------------------------------------------------
vtkMRMLTransformDisplayNode* vtkMRMLSliceIntersectionInteractionWidget::GetDisplayNode()
{
    vtkMRMLSliceIntersectionInteractionRepresentation* widgetRep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->GetRepresentation());
    if (!widgetRep)
    {
        return nullptr;
    }
    return widgetRep->GetDisplayNode();
}

//----------------------------------------------------------------------
vtkMRMLTransformNode* vtkMRMLSliceIntersectionInteractionWidget::GetTransformNode()
{
    vtkMRMLSliceIntersectionInteractionRepresentation* widgetRep = vtkMRMLSliceIntersectionInteractionRepresentation::SafeDownCast(this->GetRepresentation());
    if (!widgetRep)
    {
        return nullptr;
    }
    return widgetRep->GetTransformNode();
}