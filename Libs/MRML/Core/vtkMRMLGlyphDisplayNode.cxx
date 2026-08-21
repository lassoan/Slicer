/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLGlyphDisplayNode.cxx,v $

=========================================================================auto=*/
// MRML includes
#include "vtkMRMLGlyphDisplayNode.h"
#include "vtkMRMLModelNode.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>

// STD includes
#include <cstring>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLGlyphDisplayNode);

//-----------------------------------------------------------------------------
vtkMRMLGlyphDisplayNode::vtkMRMLGlyphDisplayNode()
  : GlyphType(vtkMRMLGlyphDisplayNode::GlyphTypeArrow)
  , OrientationArrayName(nullptr)
  , ScaleArrayName(nullptr)
  , VectorScaleMode(vtkMRMLGlyphDisplayNode::VectorScaleModeByMagnitude)
  , ScaleFactor(10.0)
  , MaskingMode(vtkMRMLGlyphDisplayNode::MaskingModeAllPoints)
  , MaskingNthPoint(10)
  , MaskingPointsNumber(1000)
{
  this->TypeDisplayName = vtkMRMLTr("vtkMRMLGlyphDisplayNode", "Glyph Display");
}

//-----------------------------------------------------------------------------
vtkMRMLGlyphDisplayNode::~vtkMRMLGlyphDisplayNode()
{
  this->SetOrientationArrayName(nullptr);
  this->SetScaleArrayName(nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLGlyphDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintEnumMacro(GlyphType);
  vtkMRMLPrintStringMacro(OrientationArrayName);
  vtkMRMLPrintStringMacro(ScaleArrayName);
  vtkMRMLPrintEnumMacro(VectorScaleMode);
  vtkMRMLPrintFloatMacro(ScaleFactor);
  vtkMRMLPrintEnumMacro(MaskingMode);
  vtkMRMLPrintIntMacro(MaskingNthPoint);
  vtkMRMLPrintIntMacro(MaskingPointsNumber);
  vtkMRMLPrintEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLGlyphDisplayNode::WriteXML(ostream& of, int nIndent)
{
  // Write all attributes not equal to their defaults
  this->Superclass::WriteXML(of, nIndent);

  vtkMRMLWriteXMLBeginMacro(of);
  vtkMRMLWriteXMLEnumMacro(glyphType, GlyphType);
  vtkMRMLWriteXMLStringMacro(orientationArrayName, OrientationArrayName);
  vtkMRMLWriteXMLStringMacro(scaleArrayName, ScaleArrayName);
  vtkMRMLWriteXMLEnumMacro(vectorScaleMode, VectorScaleMode);
  vtkMRMLWriteXMLFloatMacro(scaleFactor, ScaleFactor);
  vtkMRMLWriteXMLEnumMacro(maskingMode, MaskingMode);
  vtkMRMLWriteXMLIntMacro(maskingNthPoint, MaskingNthPoint);
  vtkMRMLWriteXMLIntMacro(maskingPointsNumber, MaskingPointsNumber);
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLGlyphDisplayNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();
  this->Superclass::ReadXMLAttributes(atts);

  vtkMRMLReadXMLBeginMacro(atts);
  vtkMRMLReadXMLEnumMacro(glyphType, GlyphType);
  vtkMRMLReadXMLStringMacro(orientationArrayName, OrientationArrayName);
  vtkMRMLReadXMLStringMacro(scaleArrayName, ScaleArrayName);
  vtkMRMLReadXMLEnumMacro(vectorScaleMode, VectorScaleMode);
  vtkMRMLReadXMLFloatMacro(scaleFactor, ScaleFactor);
  vtkMRMLReadXMLEnumMacro(maskingMode, MaskingMode);
  vtkMRMLReadXMLIntMacro(maskingNthPoint, MaskingNthPoint);
  vtkMRMLReadXMLIntMacro(maskingPointsNumber, MaskingPointsNumber);
  vtkMRMLReadXMLEndMacro();

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLGlyphDisplayNode::CopyContent(vtkMRMLNode* anode, bool deepCopy /*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::CopyContent(anode, deepCopy);

  vtkMRMLGlyphDisplayNode* node = vtkMRMLGlyphDisplayNode::SafeDownCast(anode);
  if (!node)
  {
    return;
  }

  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyEnumMacro(GlyphType);
  vtkMRMLCopyStringMacro(OrientationArrayName);
  vtkMRMLCopyStringMacro(ScaleArrayName);
  vtkMRMLCopyEnumMacro(VectorScaleMode);
  vtkMRMLCopyFloatMacro(ScaleFactor);
  vtkMRMLCopyEnumMacro(MaskingMode);
  vtkMRMLCopyIntMacro(MaskingNthPoint);
  vtkMRMLCopyIntMacro(MaskingPointsNumber);
  vtkMRMLCopyEndMacro();
}

//-----------------------------------------------------------
const char* vtkMRMLGlyphDisplayNode::GetGlyphTypeAsString()
{
  return vtkMRMLGlyphDisplayNode::GetGlyphTypeAsString(this->GlyphType);
}

//-----------------------------------------------------------
void vtkMRMLGlyphDisplayNode::SetGlyphTypeFromString(const char* modeString)
{
  this->SetGlyphType(vtkMRMLGlyphDisplayNode::GetGlyphTypeFromString(modeString));
}

//-----------------------------------------------------------
const char* vtkMRMLGlyphDisplayNode::GetGlyphTypeAsString(int id)
{
  switch (id)
  {
    case GlyphTypeArrow: return "arrow";
    case GlyphTypeCone: return "cone";
    case GlyphTypeBox: return "box";
    case GlyphTypeCylinder: return "cylinder";
    case GlyphTypeLine: return "line";
    case GlyphTypeSphere: return "sphere";
    default: return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLGlyphDisplayNode::GetGlyphTypeFromString(const char* modeString)
{
  if (modeString == nullptr)
  {
    return -1;
  }
  for (int i = 0; i < vtkMRMLGlyphDisplayNode::GlyphType_Last; i++)
  {
    if (strcmp(modeString, vtkMRMLGlyphDisplayNode::GetGlyphTypeAsString(i)) == 0)
    {
      return i;
    }
  }
  return -1;
}

//-----------------------------------------------------------
const char* vtkMRMLGlyphDisplayNode::GetVectorScaleModeAsString()
{
  return vtkMRMLGlyphDisplayNode::GetVectorScaleModeAsString(this->VectorScaleMode);
}

//-----------------------------------------------------------
void vtkMRMLGlyphDisplayNode::SetVectorScaleModeFromString(const char* modeString)
{
  this->SetVectorScaleMode(vtkMRMLGlyphDisplayNode::GetVectorScaleModeFromString(modeString));
}

//-----------------------------------------------------------
const char* vtkMRMLGlyphDisplayNode::GetVectorScaleModeAsString(int id)
{
  switch (id)
  {
    case VectorScaleModeByMagnitude: return "byMagnitude";
    case VectorScaleModeByComponents: return "byComponents";
    default: return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLGlyphDisplayNode::GetVectorScaleModeFromString(const char* modeString)
{
  if (modeString == nullptr)
  {
    return -1;
  }
  for (int i = 0; i < vtkMRMLGlyphDisplayNode::VectorScaleMode_Last; i++)
  {
    if (strcmp(modeString, vtkMRMLGlyphDisplayNode::GetVectorScaleModeAsString(i)) == 0)
    {
      return i;
    }
  }
  return -1;
}

//-----------------------------------------------------------
const char* vtkMRMLGlyphDisplayNode::GetMaskingModeAsString()
{
  return vtkMRMLGlyphDisplayNode::GetMaskingModeAsString(this->MaskingMode);
}

//-----------------------------------------------------------
void vtkMRMLGlyphDisplayNode::SetMaskingModeFromString(const char* modeString)
{
  this->SetMaskingMode(vtkMRMLGlyphDisplayNode::GetMaskingModeFromString(modeString));
}

//-----------------------------------------------------------
const char* vtkMRMLGlyphDisplayNode::GetMaskingModeAsString(int id)
{
  switch (id)
  {
    case MaskingModeAllPoints: return "allPoints";
    case MaskingModeEveryNthPoint: return "everyNthPoint";
    case MaskingModeUniformBounds: return "uniformBounds";
    case MaskingModeUniformSurface: return "uniformSurface";
    case MaskingModeUniformVolume: return "uniformVolume";
    default: return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLGlyphDisplayNode::GetMaskingModeFromString(const char* modeString)
{
  if (modeString == nullptr)
  {
    return -1;
  }
  for (int i = 0; i < vtkMRMLGlyphDisplayNode::MaskingMode_Last; i++)
  {
    if (strcmp(modeString, vtkMRMLGlyphDisplayNode::GetMaskingModeAsString(i)) == 0)
    {
      return i;
    }
  }
  return -1;
}

//-----------------------------------------------------------
vtkDataSet* vtkMRMLGlyphDisplayNode::GetScalarDataSet()
{
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode());
  if (!modelNode)
  {
    return nullptr;
  }
  return modelNode->GetMesh();
}

//-----------------------------------------------------------
vtkDataArray* vtkMRMLGlyphDisplayNode::GetOrientationArray()
{
  if (this->OrientationArrayName == nullptr || strcmp(this->OrientationArrayName, "") == 0)
  {
    return nullptr;
  }
  vtkDataSet* mesh = this->GetScalarDataSet();
  if (!mesh)
  {
    return nullptr;
  }
  return mesh->GetPointData()->GetArray(this->OrientationArrayName);
}

//-----------------------------------------------------------
vtkDataArray* vtkMRMLGlyphDisplayNode::GetScaleArray()
{
  if (this->ScaleArrayName == nullptr || strcmp(this->ScaleArrayName, "") == 0)
  {
    return nullptr;
  }
  vtkDataSet* mesh = this->GetScalarDataSet();
  if (!mesh)
  {
    return nullptr;
  }
  return mesh->GetPointData()->GetArray(this->ScaleArrayName);
}
