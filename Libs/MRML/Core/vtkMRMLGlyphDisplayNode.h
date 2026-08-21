/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLGlyphDisplayNode.h,v $

=========================================================================auto=*/

#ifndef __vtkMRMLGlyphDisplayNode_h
#define __vtkMRMLGlyphDisplayNode_h

// MRML includes
#include "vtkMRMLDisplayNode.h"

class vtkDataArray;
class vtkDataSet;

/// \brief MRML node to represent glyph display properties for point data arrays of a model node.
///
/// vtkMRMLGlyphDisplayNode stores the settings needed to display oriented and/or
/// scaled glyphs (arrow, cone, box, cylinder, line, sphere) at the points of the
/// mesh of the associated model node, using vector/scalar point data arrays of
/// that model node to control glyph orientation and scale.
class VTK_MRML_EXPORT vtkMRMLGlyphDisplayNode : public vtkMRMLDisplayNode
{
public:
  static vtkMRMLGlyphDisplayNode* New();
  vtkTypeMacro(vtkMRMLGlyphDisplayNode, vtkMRMLDisplayNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Shape of the glyph geometry used to represent each point.
  enum GlyphTypeType
  {
    GlyphTypeArrow = 0,
    GlyphTypeCone,
    GlyphTypeBox,
    GlyphTypeCylinder,
    GlyphTypeLine,
    GlyphTypeSphere,
    GlyphType_Last // placeholder after the last valid value, this must be the last in the list of modes
  };

  /// Determines how a 3-component scale array is used to scale the glyph.
  enum GlyphVectorScaleModeType
  {
    VectorScaleModeByMagnitude = 0, ///< scale uniformly by the magnitude of the vector
    VectorScaleModeByComponents,    ///< scale each axis of the glyph independently by the vector components
    VectorScaleMode_Last            // placeholder after the last valid value, this must be the last in the list of modes
  };

  /// Determines which mesh points get a glyph.
  enum GlyphMaskingModeType
  {
    MaskingModeAllPoints = 0,      ///< show a glyph at every point of the mesh
    MaskingModeEveryNthPoint,      ///< show a glyph at every Nth point of the mesh
    MaskingModeUniformBounds,      ///< randomly sample points, uniformly distributed within the bounding box of the mesh
    MaskingModeUniformSurface,     ///< randomly sample points, uniformly distributed on the surface of the mesh
    MaskingModeUniformVolume,      ///< randomly sample points, uniformly distributed within the volume of the mesh
    MaskingMode_Last                // placeholder after the last valid value, this must be the last in the list of modes
  };

  vtkMRMLNode* CreateNodeInstance() override;

  /// Get node XML tag name (like Volume, Model)
  const char* GetNodeTagName() override { return "GlyphDisplay"; }

  /// Read node attributes from XML file
  void ReadXMLAttributes(const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLGlyphDisplayNode);

  ///@{
  /// Shape of the glyph geometry used to represent each point.
  /// Default is GlyphTypeArrow.
  vtkGetMacro(GlyphType, int);
  vtkSetMacro(GlyphType, int);
  const char* GetGlyphTypeAsString();
  void SetGlyphTypeFromString(const char* modeString);
  static const char* GetGlyphTypeAsString(int id);
  static int GetGlyphTypeFromString(const char* modeString);
  ///@}

  ///@{
  /// Name of the model node's point data array used to orient each glyph.
  /// The array must have 3 components. If empty (nullptr), glyphs are not oriented.
  vtkGetStringMacro(OrientationArrayName);
  vtkSetStringMacro(OrientationArrayName);
  ///@}

  ///@{
  /// Name of the model node's point data array used to scale each glyph.
  /// If empty (nullptr), glyphs are only scaled by ScaleFactor.
  vtkGetStringMacro(ScaleArrayName);
  vtkSetStringMacro(ScaleArrayName);
  ///@}

  ///@{
  /// Determines how a 3-component scale array is used to scale the glyph.
  /// Only used if the scale array has 3 components.
  /// Default is VectorScaleModeByMagnitude.
  vtkGetMacro(VectorScaleMode, int);
  vtkSetMacro(VectorScaleMode, int);
  const char* GetVectorScaleModeAsString();
  void SetVectorScaleModeFromString(const char* modeString);
  static const char* GetVectorScaleModeAsString(int id);
  static int GetVectorScaleModeFromString(const char* modeString);
  ///@}

  ///@{
  /// Factor used to scale the glyph geometry.
  /// Default is 10.
  vtkGetMacro(ScaleFactor, double);
  vtkSetMacro(ScaleFactor, double);
  ///@}

  ///@{
  /// Determines which mesh points get a glyph.
  /// Default is MaskingModeAllPoints.
  vtkGetMacro(MaskingMode, int);
  vtkSetMacro(MaskingMode, int);
  const char* GetMaskingModeAsString();
  void SetMaskingModeFromString(const char* modeString);
  static const char* GetMaskingModeAsString(int id);
  static int GetMaskingModeFromString(const char* modeString);
  ///@}

  ///@{
  /// Show a glyph at every Nth point of the mesh.
  /// Only used if MaskingMode is MaskingModeEveryNthPoint.
  /// Default is 10.
  vtkGetMacro(MaskingNthPoint, int);
  vtkSetClampMacro(MaskingNthPoint, int, 1, VTK_INT_MAX);
  ///@}

  ///@{
  /// Maximum number of glyphs to show.
  /// Only used if MaskingMode is MaskingModeUniformBounds, MaskingModeUniformSurface,
  /// or MaskingModeUniformVolume.
  /// Default is 1000.
  vtkGetMacro(MaskingPointsNumber, int);
  vtkSetClampMacro(MaskingPointsNumber, int, 1, VTK_INT_MAX);
  ///@}

  /// Get data set containing the scalar/vector arrays that can be used for glyph
  /// orientation and scaling. For glyph display nodes this is the associated model
  /// node's mesh.
  vtkDataSet* GetScalarDataSet() override;

  /// Return the orientation array from the associated model node's mesh, looked up by
  /// OrientationArrayName. Returns nullptr if not found or not set.
  virtual vtkDataArray* GetOrientationArray();

  /// Return the scale array from the associated model node's mesh, looked up by
  /// ScaleArrayName. Returns nullptr if not found or not set.
  virtual vtkDataArray* GetScaleArray();

protected:
  vtkMRMLGlyphDisplayNode();
  ~vtkMRMLGlyphDisplayNode() override;
  vtkMRMLGlyphDisplayNode(const vtkMRMLGlyphDisplayNode&);
  void operator=(const vtkMRMLGlyphDisplayNode&);

  int GlyphType;
  char* OrientationArrayName;
  char* ScaleArrayName;
  int VectorScaleMode;
  double ScaleFactor;
  int MaskingMode;
  int MaskingNthPoint;
  int MaskingPointsNumber;
};

#endif
