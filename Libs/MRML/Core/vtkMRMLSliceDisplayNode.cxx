/*=auto=========================================================================

Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/
// MRML includes
#include "vtkMRMLSliceDisplayNode.h"

// VTK includes

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLSliceDisplayNode);

//-----------------------------------------------------------------------------
vtkMRMLSliceDisplayNode::vtkMRMLSliceDisplayNode()
{
}

//-----------------------------------------------------------------------------
vtkMRMLSliceDisplayNode::~vtkMRMLSliceDisplayNode()
{
}

//----------------------------------------------------------------------------
void vtkMRMLSliceDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintBooleanMacro(SliceIntersectionInteractive);
  vtkMRMLPrintBooleanMacro(SliceIntersectionTranslationEnabled);
  vtkMRMLPrintBooleanMacro(SliceIntersectionRotationEnabled);
  vtkMRMLPrintEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLSliceDisplayNode::WriteXML(ostream& of, int nIndent)
{
  // Write all attributes not equal to their defaults
  this->Superclass::WriteXML(of, nIndent);

  vtkMRMLWriteXMLBeginMacro(of);
  vtkMRMLWriteXMLBooleanMacro(sliceIntersectionInteractive, SliceIntersectionInteractive);
  vtkMRMLWriteXMLBooleanMacro(sliceIntersectionTranslationEnabled, SliceIntersectionTranslationEnabled);
  vtkMRMLWriteXMLBooleanMacro(sliceIntersectionRotationEnabled, SliceIntersectionRotationEnabled);
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLSliceDisplayNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();
  this->Superclass::ReadXMLAttributes(atts);

  vtkMRMLReadXMLBeginMacro(atts);
  vtkMRMLReadXMLBooleanMacro(sliceIntersectionInteractive, SliceIntersectionInteractive);
  vtkMRMLReadXMLBooleanMacro(sliceIntersectionTranslationEnabled, SliceIntersectionTranslationEnabled);
  vtkMRMLReadXMLBooleanMacro(sliceIntersectionRotationEnabled, SliceIntersectionRotationEnabled);
  vtkMRMLReadXMLEndMacro();

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLSliceDisplayNode::CopyContent(vtkMRMLNode* anode, bool deepCopy/*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::CopyContent(anode, deepCopy);

  vtkMRMLSliceDisplayNode* node = vtkMRMLSliceDisplayNode::SafeDownCast(anode);
  if (!node)
    {
    return;
    }

  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyBooleanMacro(SliceIntersectionInteractive);
  vtkMRMLCopyBooleanMacro(SliceIntersectionTranslationEnabled);
  vtkMRMLCopyBooleanMacro(SliceIntersectionRotationEnabled);
  vtkMRMLCopyEndMacro();
}

//---------------------------------------------------------------------------
void vtkMRMLSliceDisplayNode::SetSliceIntersectionInteractiveModeEnabled(
  SliceIntersectionInteractiveMode mode, bool enabled)
{
  switch (mode)
    {
    case vtkMRMLSliceDisplayNode::ModeTranslation:
      this->SetSliceIntersectionTranslationEnabled(enabled);
      break;
    case vtkMRMLSliceDisplayNode::ModeRotation:
      this->SetSliceIntersectionRotationEnabled(enabled);
      break;
    default:
      vtkErrorMacro("Unknown mode");
      break;
    }
}

//---------------------------------------------------------------------------
bool vtkMRMLSliceDisplayNode::GetSliceIntersectionInteractiveModeEnabled(
  SliceIntersectionInteractiveMode mode)
{
  switch (mode)
    {
    case vtkMRMLSliceDisplayNode::ModeTranslation:
      return this->GetSliceIntersectionTranslationEnabled();
    case vtkMRMLSliceDisplayNode::ModeRotation:
      return this->GetSliceIntersectionRotationEnabled();
    default:
      vtkErrorMacro("Unknown mode");
    }
  return false;
}
