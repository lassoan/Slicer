/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLVectorVolumeDisplayNode.h,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.3 $

=========================================================================auto=*/

#ifndef __vtkMRMLVectorVolumeDisplayNode_h
#define __vtkMRMLVectorVolumeDisplayNode_h

#include "vtkMRMLGlyphableVolumeDisplayNode.h"

class vtkAlgorithmOutput;
class vtkImageData;
class vtkImageShiftScale;
class vtkImageExtractComponents;
class vtkImageRGBToHSI;

/// \brief MRML node for representing a volume (image stack).
///
/// Volume nodes describe data sets that can be thought of as stacks of 2D
/// images that form a 3D volume.  Volume nodes describe where the images
/// are stored on disk, how to render the data (window and level), and how
/// to read the files.  This information is extracted from the image
/// headers (if they exist) at the time the MRML file is generated.
/// Consequently, MRML files isolate MRML browsers from understanding how
/// to read the myriad of file formats for medical data.
class VTK_MRML_EXPORT vtkMRMLVectorVolumeDisplayNode : public vtkMRMLGlyphableVolumeDisplayNode
{
  public:
  static vtkMRMLVectorVolumeDisplayNode *New();
  vtkTypeMacro(vtkMRMLVectorVolumeDisplayNode,vtkMRMLGlyphableVolumeDisplayNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  vtkMRMLNode* CreateNodeInstance() override;

  ///
  /// Set node attributes
  void ReadXMLAttributes( const char** atts) override;

  ///
  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLVectorVolumeDisplayNode);

  ///
  /// Get node XML tag name (like Volume, Model)
  const char* GetNodeTagName() override {return "VectorVolumeDisplay";}

  //--------------------------------------------------------------------------
  /// Display Information
  //--------------------------------------------------------------------------

  enum
    {
    ScalarModeRGB = 0, // RGB if number of components is 3 or 4, single component otherwise
    ScalarModeSingleComponent = 1,
    ScalarMode_Last
    };
  vtkGetMacro(ScalarMode, int);
  vtkSetMacro(ScalarMode, int);

  void SetScalarModeToRGB() { this->SetScalarMode(this->ScalarModeRGB); }
  void SetScalarModeToSingleComponent() { this->SetScalarMode(this->ScalarModeSingleComponent); }

  static const char* GetScalarModeAsString(int mode);
  static int GetScalarModeFromString(const char* name);


  /// Index of displayed scalar component
  vtkGetMacro(ScalarComponent, int);
  vtkSetMacro(ScalarComponent, int);

  enum
    {
    GlyphModeLines = 1,
    GlyphModeTubes = 2,
    GlyphMode_Last
    };
  vtkGetMacro(GlyphMode, int);
  vtkSetMacro(GlyphMode, int);

  void SetGlyphModeToLines() { this->SetGlyphMode(this->GlyphModeLines); };
  void SetGlyphModeToTubes() { this->SetGlyphMode(this->GlyphModeTubes); };

  static const char* GetGlyphModeAsString(int mode);
  static int GetGlyphModeFromString(const char* name);

  //virtual vtkPolyData* ExecuteGlyphPipeLineAndGetPolyData( vtkImageData* ) {return nullptr;};

  void SetDefaultColorMap() override {}

  ///
  /// alternative method to propagate events generated in Display nodes
  void ProcessMRMLEvents ( vtkObject * /*caller*/,
                                   unsigned long /*event*/,
                                   void * /*callData*/ ) override;

  /// Get the input of the pipeline
  vtkAlgorithmOutput* GetInputImageDataConnection() override;

  void UpdateImageDataPipeline() override;

  ///
  /// get associated slice glyph display node
  /// TODO: return empty list for now, later add glyphs
  std::vector< vtkMRMLGlyphableVolumeSliceDisplayNode*>
    GetSliceGlyphDisplayNodes( vtkMRMLVolumeNode* vtkNotUsed(node) ) override
    {
    return std::vector< vtkMRMLGlyphableVolumeSliceDisplayNode*>();
    }

  ///
  /// Access to this class's internal filter elements
  vtkGetObjectMacro (ShiftScale, vtkImageShiftScale);
  vtkGetObjectMacro (RGBToHSI, vtkImageRGBToHSI);
  vtkGetObjectMacro (ExtractIntensity, vtkImageExtractComponents);
  vtkGetObjectMacro (AppendComponents, vtkImageAppendComponents);
  vtkGetObjectMacro (Threshold, vtkImageThreshold);

protected:
  vtkMRMLVectorVolumeDisplayNode();
  ~vtkMRMLVectorVolumeDisplayNode() override;
  vtkMRMLVectorVolumeDisplayNode(const vtkMRMLVectorVolumeDisplayNode&);
  void operator=(const vtkMRMLVectorVolumeDisplayNode&);

  /// Set the input of the pipeline
  void SetInputToImageDataPipeline(vtkAlgorithmOutput *imageDataConnection) override;
  vtkAlgorithmOutput* GetScalarImageDataConnection() override;

  int ScalarMode;
  int GlyphMode;
  int ScalarComponent;

  enum
  {
    PipelineRGB,
    PipelineRGBA,
    PipelineSingleComponent
  };
  int PipelineType;

  vtkImageShiftScale *ShiftScale;
  vtkImageRGBToHSI *RGBToHSI;
  vtkImageExtractComponents *ExtractIntensity;
  vtkImageExtractComponents* ExtractComponents;
};

#endif
