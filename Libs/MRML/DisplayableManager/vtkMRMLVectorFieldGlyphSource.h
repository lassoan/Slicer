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

==============================================================================*/

#ifndef __vtkMRMLVectorFieldGlyphSource_h
#define __vtkMRMLVectorFieldGlyphSource_h

// MRML includes
#include "vtkMRMLVectorFieldDisplayNode.h"

// VTK includes
#include <vtkArrowSource.h>
#include <vtkConeSource.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkGlyphSource2D.h>
#include <vtkLineSource.h>
#include <vtkNew.h>
#include <vtkPolyDataAlgorithm.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

/// Glyph geometry shared by the 3D and the slice vector field displayable managers.
/// Not part of the public API: this header is only included by those two managers.
namespace vtkMRMLVectorFieldGlyphSource
{

/// Build the 3D glyph geometry described by a display node. The output geometry's
/// "long axis" points along +X, which is the direction that both vtkGlyph3D and
/// vtkGlyph3DMapper (in DIRECTION orientation mode) align with the orientation vector.
inline vtkSmartPointer<vtkPolyDataAlgorithm> Create3D(vtkMRMLVectorFieldDisplayNode* displayNode)
{
  int glyphType = displayNode ? displayNode->GetGlyphType() : vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow;
  int resolution = displayNode ? displayNode->GetGlyphResolution() : 16;
  switch (glyphType)
  {
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeCone:
    {
      vtkNew<vtkConeSource> coneSource;
      coneSource->SetResolution(resolution);
      if (displayNode && displayNode->GetScaleDirectional())
      {
        coneSource->SetHeight(1.0);
        coneSource->SetRadius(displayNode->GetGlyphDiameterMm() * 0.5);
      }
      return coneSource.GetPointer();
    }
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeBox:
    {
      vtkNew<vtkCubeSource> cubeSource;
      return cubeSource.GetPointer();
    }
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeCylinder:
    {
      // vtkCylinderSource generates a cylinder with its axis along Y by default.
      // Rotate it so that its axis is along X, to be consistent with the other glyph sources.
      vtkNew<vtkCylinderSource> cylinderSource;
      cylinderSource->SetResolution(resolution);
      vtkNew<vtkTransform> cylinderTransform;
      cylinderTransform->RotateZ(90.0);
      vtkNew<vtkTransformPolyDataFilter> cylinderTransformFilter;
      cylinderTransformFilter->SetTransform(cylinderTransform);
      cylinderTransformFilter->SetInputConnection(cylinderSource->GetOutputPort());
      return cylinderTransformFilter.GetPointer();
    }
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeLine:
    {
      vtkNew<vtkLineSource> lineSource;
      return lineSource.GetPointer();
    }
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeSphere:
    {
      vtkNew<vtkSphereSource> sphereSource;
      sphereSource->SetThetaResolution(resolution);
      sphereSource->SetPhiResolution(resolution);
      return sphereSource.GetPointer();
    }
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow:
    default:
    {
      vtkNew<vtkArrowSource> arrowSource;
      arrowSource->SetTipResolution(resolution);
      arrowSource->SetShaftResolution(resolution);
      if (displayNode)
      {
        arrowSource->SetTipLength(displayNode->GetGlyphTipLengthPercent() * 0.01);
        if (displayNode->GetScaleDirectional())
        {
          // The glyph is only stretched along its axis, so its thickness is the thickness of
          // the source geometry and can be given in mm.
          arrowSource->SetTipRadius(displayNode->GetGlyphDiameterMm() * 0.5);
        }
        arrowSource->SetShaftRadius(arrowSource->GetTipRadius() * 0.01 * displayNode->GetGlyphShaftDiameterPercent());
      }
      return arrowSource.GetPointer();
    }
  }
}

/// Build the flat glyph geometry drawn in slice views. Slice views use 2D glyphs (as the
/// transform display does) rather than the projection of the 3D geometry, because outlines
/// stay crisp at any zoom level and do not hide the image underneath.
/// The geometry is generated in the XY plane with its long axis along +X.
/// sourceTransform, if given, is filled with the transform that has to be applied to the
/// glyph geometry (it moves the origin of the directional glyphs to their base).
inline vtkSmartPointer<vtkPolyDataAlgorithm> Create2D(vtkMRMLVectorFieldDisplayNode* displayNode, vtkTransform* sourceTransform = nullptr)
{
  int glyphType = displayNode ? displayNode->GetGlyphType() : vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow;
  vtkNew<vtkGlyphSource2D> glyph2DSource;
  glyph2DSource->SetScale(1.0);
  glyph2DSource->SetFilled(0);
  if (displayNode)
  {
    glyph2DSource->SetResolution(displayNode->GetGlyphResolution2D());
    glyph2DSource->SetTipLength(displayNode->GetGlyphTipLengthPercent2D() * 0.01);
  }
  bool directional = false;
  switch (glyphType)
  {
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeCone:
      glyph2DSource->SetGlyphTypeToEdgeArrow();
      directional = true;
      break;
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeBox:
      glyph2DSource->SetGlyphTypeToSquare();
      break;
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeCylinder:
      glyph2DSource->SetGlyphTypeToDash();
      break;
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeLine:
      glyph2DSource->SetGlyphTypeToDash();
      break;
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeSphere:
      glyph2DSource->SetGlyphTypeToCircle();
      break;
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow:
    default:
      glyph2DSource->SetGlyphTypeToArrow();
      directional = true;
      break;
  }
  if (sourceTransform && directional)
  {
    // vtkGlyphSource2D centers the glyph on the point; directional glyphs are more readable
    // when they start at the point and grow along the vector.
    sourceTransform->Translate(0.5, 0.0, 0.0);
  }
  return glyph2DSource.GetPointer();
}

} // namespace vtkMRMLVectorFieldGlyphSource

#endif
