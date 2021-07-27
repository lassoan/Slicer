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
#include "vtkMatrix3x3.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPointSetToLabelHierarchy.h"
#include "vtkPolyDataMapper.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"

#include "vtkMRMLSliceIntersectionInteractionRepresentation.h"
#include "vtkMRMLInteractionWidgetRepresentation.h"
#include "vtkSphereSource.h"
#include "vtkStringArray.h"
#include "vtkTextActor.h"
#include "vtkTextProperty.h"
#include "vtkTransform.h"
#include "vtkTransformPolyDataFilter.h"
#include "vtkMRMLSliceCompositeNode.h"

// MRML includes
#include <vtkMRMLFolderDisplayNode.h>
#include <vtkMRMLInteractionEventData.h>
#include <vtkMRMLLinearTransformNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLScene.h>

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
vtkMRMLSliceNode* vtkMRMLSliceIntersectionInteractionRepresentation::GetSliceNode()
  {
  return this->SliceNode;
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetSliceNode(vtkMRMLSliceNode* sliceNode)
  {
  this->SliceNode = sliceNode;
  }

//----------------------------------------------------------------------
vtkMatrix4x4* vtkMRMLSliceIntersectionInteractionRepresentation::GetHandlesInteractionTransformMatrix ()
  {
  return this->HandlesInteractionTransformMatrix ;
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::UpdateHandlesInteractionTransformMatrix (vtkMatrix4x4* transform)
  {
  // Set handles transform
  this->HandlesInteractionTransformMatrix  = transform;
  if (!this->HandlesInteractionTransformMatrix )
    {
      return;
    }
  }

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetActiveComponentType()
  {
    return this->SliceNode->GetActiveInteractionType();
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetActiveComponentType(int type)
  {
    this->SliceNode->SetActiveInteractionType(type);
  }

//----------------------------------------------------------------------
int vtkMRMLSliceIntersectionInteractionRepresentation::GetActiveComponentIndex()
  {
    return this->SliceNode->GetActiveInteractionIndex();
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::SetActiveComponentIndex(int index)
  {
    this->SliceNode->SetActiveInteractionIndex(index);
  }

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionRepresentation::IsDisplayable()
  {
    if (!this->GetSliceNode())
    {
        return false;
    }
    return true;
  }

//----------------------------------------------------------------------
void vtkMRMLSliceIntersectionInteractionRepresentation::UpdateInteractionPipeline()
  {
    // Update handles visibility in slice views
    vtkMRMLSliceCompositeNode* sliceCompositeNode = this->FindSliceCompositeNode();
    bool handlesVisibility = sliceCompositeNode->GetSliceIntersectionHandlesVisibility();
    bool intersectionVisibility = sliceCompositeNode->GetSliceIntersectionVisibility();
    if (handlesVisibility)
      {
        this->Pipeline->Actor->SetVisibility(true);
      }
    else
      {
        this->Pipeline->Actor->SetVisibility(false);
        return;
      }

    // Final visibility handled by superclass in vtkMRMLInteractionWidgetRepresentation
    Superclass::UpdateInteractionPipeline();

    // Slice node where interaction takes place
    vtkMRMLSliceNode* sliceNode = this->GetSliceNode();

    // Get slice nodes from scene
    vtkMRMLScene* scene = sliceNode->GetScene();
    if (!scene)
      {
      return;
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
    vtkMath::MultiplyScalar(sliceIntersectionPoint, 1/determinant);

    // In-plane rotation
    vtkMatrix4x4* sliceToRAS = sliceNode->GetSliceToRAS();
    vtkNew<vtkTransform> rotationTransform;
    rotationTransform->Identity();
    if (sliceNode == redSliceNode)
      {
      // Green line
      double greenIntersectionLineDirection[3];
      vtkMath::Cross(redSlicePlaneNormal, greenSlicePlaneNormal, greenIntersectionLineDirection);
      // Yellow line
      double yellowIntersectionLineDirection[3];
      vtkMath::Cross(redSlicePlaneNormal, yellowSlicePlaneNormal, yellowIntersectionLineDirection);
      // Calculate rotation matrix from intersection lines
      double rasPoint[4] = { 0,1,0};
      double slicePoint_h[4];
      sliceToRAS->MultiplyPoint(rasPoint, slicePoint_h);
      double slicePoint[3] = { slicePoint_h[0], slicePoint_h[1], slicePoint_h[2] };
      double angleRotationRad = vtkMath::AngleBetweenVectors(slicePoint, greenIntersectionLineDirection);
      double angleRotationDeg = vtkMath::DegreesFromRadians(angleRotationRad);
      rotationTransform->Identity();
      rotationTransform->RotateWXYZ(angleRotationDeg, redSlicePlaneNormal);
      }
    if (sliceNode == greenSliceNode)
      {
      // Red line
      double redIntersectionLineDirection[3];
      vtkMath::Cross(greenSlicePlaneNormal, redSlicePlaneNormal, redIntersectionLineDirection);
      // Yellow line
      double yellowIntersectionLineDirection[3];
      vtkMath::Cross(greenSlicePlaneNormal, yellowSlicePlaneNormal, yellowIntersectionLineDirection);
      // Calculate rotation matrix from intersection lines
      double rasPoint[4] = { 0,1,0 };
      double slicePoint_h[4];
      sliceToRAS->MultiplyPoint(rasPoint, slicePoint_h);
      double slicePoint[3] = { slicePoint_h[0], slicePoint_h[1], slicePoint_h[2] };
      double angleRotationRad = vtkMath::AngleBetweenVectors(slicePoint, redIntersectionLineDirection);
      double angleRotationDeg = vtkMath::DegreesFromRadians(angleRotationRad);
      rotationTransform->Identity();
      rotationTransform->RotateWXYZ(angleRotationDeg, greenSlicePlaneNormal);
      }
    if (sliceNode == yellowSliceNode)
      {
      // Red line
      double redIntersectionLineDirection[3];
      vtkMath::Cross(yellowSlicePlaneNormal, redSlicePlaneNormal, redIntersectionLineDirection);
      // Green line
      double greenIntersectionLineDirection[3];
      vtkMath::Cross(yellowSlicePlaneNormal, greenSlicePlaneNormal, greenIntersectionLineDirection);
      // Calculate rotation matrix from intersection lines
      double rasPoint[4] = { 0,1,0 };
      double slicePoint_h[4];
      sliceToRAS->MultiplyPoint(rasPoint, slicePoint_h);
      double slicePoint[3] = { slicePoint_h[0], slicePoint_h[1], slicePoint_h[2] };
      double angleRotationRad = vtkMath::AngleBetweenVectors(slicePoint, redIntersectionLineDirection);
      double angleRotationDeg = vtkMath::DegreesFromRadians(angleRotationRad);
      rotationTransform->Identity();
      rotationTransform->RotateWXYZ(angleRotationDeg, yellowSlicePlaneNormal);
      }

    // Update handle-to-world transform
    this->Pipeline->HandleToWorldTransform->Identity();
    this->Pipeline->HandleToWorldTransform->Translate(sliceIntersectionPoint);
    this->Pipeline->HandleToWorldTransform->Concatenate(rotationTransform->GetMatrix());
  }

//---------------------------------------------------------------------------
vtkMRMLSliceCompositeNode* vtkMRMLSliceIntersectionInteractionRepresentation::FindSliceCompositeNode()
  {
  if (!this->GetSliceNode()->GetScene() || !this->GetSliceNode())
    {
    return nullptr;
    }
  vtkMRMLNode* node;
  vtkCollectionSimpleIterator it;
  vtkCollection* scene = this->GetSliceNode()->GetScene()->GetNodes();
  for (scene->InitTraversal(it);
    (node = (vtkMRMLNode*)scene->GetNextItemAsObject(it));)
    {
    vtkMRMLSliceCompositeNode* sliceCompositeNode =
      vtkMRMLSliceCompositeNode::SafeDownCast(node);
    if (sliceCompositeNode)
      {
      const char* compositeLayoutName = sliceCompositeNode->GetLayoutName();
      const char* sliceLayoutName = this->GetSliceNode()->GetLayoutName();
      if (compositeLayoutName && !strcmp(compositeLayoutName, sliceLayoutName))
        {
        return sliceCompositeNode;
        }
      }
    }
  // no matching slice composite node is found
  return nullptr;
  }

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetInteractionScale()
  {
    return this->GetSliceNode()->GetInteractionScale();
  }

//----------------------------------------------------------------------
double vtkMRMLSliceIntersectionInteractionRepresentation::GetInteractionSize()
  {
    return this->GetSliceNode()->GetInteractionSize();
  }

//----------------------------------------------------------------------
bool vtkMRMLSliceIntersectionInteractionRepresentation::GetInteractionSizeAbsolute()
  {
    return this->GetSliceNode()->GetInteractionSizeAbsolute();
  }
