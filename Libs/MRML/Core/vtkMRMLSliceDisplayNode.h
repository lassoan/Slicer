/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

#ifndef __vtkMRMLSliceDisplayNode_h
#define __vtkMRMLSliceDisplayNode_h

// MRML includes
#include "vtkMRMLModelDisplayNode.h"

/// \brief MRML node to store display properties of slice nodes.
///
/// This node controls appearance of slice intersections in slice views
/// and slices in 3D views.
class VTK_MRML_EXPORT vtkMRMLSliceDisplayNode : public vtkMRMLModelDisplayNode
{
public:
  static vtkMRMLSliceDisplayNode *New();
  vtkTypeMacro(vtkMRMLSliceDisplayNode,vtkMRMLModelDisplayNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///
  /// Read node attributes from XML file
  void ReadXMLAttributes(const char** atts) override;

  ///
  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLSliceDisplayNode);

  vtkMRMLNode* CreateNodeInstance() override;

  /// Get node XML tag name (like Volume, Model)
  const char* GetNodeTagName() override {return "SliceDisplay";}

  /// toggles visibility of intersections of other slices in the slice viewer
  bool GetSliceIntersectionVisibility() { return this->GetVisibility2D(); };
  void SetSliceIntersectionVisibility(bool visible) { this->SetVisibility2D(visible); };
  vtkBooleanMacro(SliceIntersectionVisibility, bool);

  /// toggles interaction with slice intersections
  vtkGetMacro(SliceIntersectionInteractive, bool);
  vtkSetMacro(SliceIntersectionInteractive, bool);
  vtkBooleanMacro(SliceIntersectionInteractive, bool);

  // Interaction handles
  enum SliceIntersectionInteractiveMode
  {
    ModeRotation,
    ModeTranslation
  };
  vtkGetMacro(SliceIntersectionTranslationEnabled, bool);
  vtkSetMacro(SliceIntersectionTranslationEnabled, bool);
  vtkBooleanMacro(SliceIntersectionTranslationEnabled, bool);
  vtkGetMacro(SliceIntersectionRotationEnabled, bool);
  vtkSetMacro(SliceIntersectionRotationEnabled, bool);
  vtkBooleanMacro(SliceIntersectionRotationEnabled, bool);
  void SetSliceIntersectionInteractiveModeEnabled(SliceIntersectionInteractiveMode mode, bool enabled);
  bool GetSliceIntersectionInteractiveModeEnabled(SliceIntersectionInteractiveMode mode);


protected:
  vtkMRMLSliceDisplayNode();
  ~vtkMRMLSliceDisplayNode() override;
  vtkMRMLSliceDisplayNode(const vtkMRMLSliceDisplayNode&);
  void operator=(const vtkMRMLSliceDisplayNode&);

  bool SliceIntersectionInteractive{ false };
  bool SliceIntersectionTranslationEnabled{ true };
  bool SliceIntersectionRotationEnabled{ true };
};

#endif
