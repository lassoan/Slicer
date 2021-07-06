/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMRMLSliceIntersectionInteractionRepresentation.cxx

  Copyright (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// VTK includes
#include "vtkCallbackCommand.h"
#include "vtkCamera.h"
#include "vtkCellPicker.h"
#include "vtkLine.h"
#include "vtkFloatArray.h"
#include "vtkGlyph3DMapper.h"

#include "vtkMath.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPointSetToLabelHierarchy.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"

#include "vtkMRMLSliceIntersectionInteractionRepresentation.h"
#include "vtkSphereSource.h"
#include "vtkStringArray.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkTransform.h"
#include "vtkTransformPolyDataFilter.h"

// MRML includes
#include <vtkMRMLFolderDisplayNode.h>
#include <vtkMRMLInteractionEventData.h>
#include <vtkMRMLLinearTransformNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLViewNode.h>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLSliceIntersectionInteractionRepresentation);

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::vtkMRMLSliceIntersectionInteractionRepresentation()
{
}

//----------------------------------------------------------------------
vtkMRMLSliceIntersectionInteractionRepresentation::~vtkMRMLSliceIntersectionInteractionRepresentation() = default;

//-----------------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::PrintSelf(ostream & os, vtkIndent indent)
{
    Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------
vtkMRMLTransformDisplayNode* vtkMRMLSliceIntersectionInteractionRepresentation::GetDisplayNode()
{
    return this->DisplayNode;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetDisplayNode(vtkMRMLTransformDisplayNode * displayNode)
{
    this->DisplayNode = displayNode;
}

//----------------------------------------------------------------------
vtkMRMLTransformNode* vtkMRMLSliceIntersectionInteractionRepresentation::GetTransformNode()
{
    if (!this->GetDisplayNode())
    {
        return nullptr;
    }
    return vtkMRMLTransformNode::SafeDownCast(this->GetDisplayNode()->GetDisplayableNode());
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetActiveComponentType()
{
    return this->DisplayNode->GetActiveInteractionType();
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetActiveComponentType(int type)
{
    this->DisplayNode->SetActiveInteractionType(type);
}

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetActiveComponentIndex()
{
    return this->DisplayNode->GetActiveInteractionIndex();
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetActiveComponentIndex(int index)
{
    this->DisplayNode->SetActiveInteractionIndex(index);
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionRepresentation::IsDisplayable()
{
    if (!this->GetDisplayNode())
    {
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::UpdateInteractionPipeline()
{
    vtkMRMLAbstractViewNode* viewNode = vtkMRMLAbstractViewNode::SafeDownCast(this->ViewNode);
    vtkMRMLLinearTransformNode* linearTransform = vtkMRMLLinearTransformNode::SafeDownCast(this->GetTransformNode());
    if (!viewNode || !this->GetTransformNode())
    {
        this->Pipeline->Actor->SetVisibility(false);
        return;
    }

    // Final visibility handled by superclass in vtkMRMLInteractionWidgetRepresentation
    Superclass::UpdateInteractionPipeline();

    this->Pipeline->HandleToWorldTransform->Identity();

    vtkMRMLDisplayableNode* displayableNode = this->DisplayNode->GetDisplayableNode();

    double centerOfTransformation[3] = { 0.0, 0.0, 0.0 };
    linearTransform->GetCenterOfTransformation(centerOfTransformation);
    this->Pipeline->HandleToWorldTransform->Translate(centerOfTransformation);

    vtkNew<vtkMatrix4x4> nodeToWorld;
    vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(this->GetTransformNode(), nullptr, nodeToWorld);
    this->Pipeline->HandleToWorldTransform->Concatenate(nodeToWorld);
}

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetInteractionScale()
{
    return this->GetDisplayNode()->GetInteractionScale();
}

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetInteractionSize()
{
    return this->GetDisplayNode()->GetInteractionSize();
}

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionRepresentation::GetInteractionSizeAbsolute()
{
    return this->GetDisplayNode()->GetInteractionSizeAbsolute();
}