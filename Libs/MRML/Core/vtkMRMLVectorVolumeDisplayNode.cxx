/*=auto=========================================================================

Portions (c) Copyright 2005 Brigham and Women\"s Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Program:   3D Slicer
Module:    $RCSfile: vtkMRMLVectorVolumeDisplayNode.cxx,v $
Date:      $Date: 2006/03/17 15:10:10 $
Version:   $Revision: 1.2 $

=========================================================================auto=*/

// MRML includes
#include "vtkMRMLVectorVolumeDisplayNode.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkImageAppendComponents.h>
#include <vtkAlgorithmOutput.h>
#include <vtkImageCast.h>
#include <vtkImageData.h>
#include <vtkImageExtractComponents.h>
#include <vtkImageRGBToHSI.h>
#include <vtkImageShiftScale.h>
#include <vtkImageStencil.h>
#include <vtkImageThreshold.h>
#include <vtkVersion.h>

// STD includes
#include <sstream>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLVectorVolumeDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLVectorVolumeDisplayNode::vtkMRMLVectorVolumeDisplayNode()
{
  this->ScalarMode = this->ScalarModeRGB;
  this->GlyphMode = this->GlyphModeLines;

  this->ShiftScale = vtkImageShiftScale::New();
  this->RGBToHSI = vtkImageRGBToHSI::New();
  this->ExtractIntensity = vtkImageExtractComponents::New();
  this->ExtractComponents = vtkImageExtractComponents::New();

  this->ShiftScale->SetOutputScalarTypeToUnsignedChar();
  this->ShiftScale->SetClampOverflow(1);

  this->ExtractIntensity->SetComponents(2);

  this->Threshold->SetInputConnection(this->ExtractIntensity->GetOutputPort());

  this->AppendComponents->RemoveAllInputs();
  this->AppendComponents->AddInputConnection(0, this->ShiftScale->GetOutputPort());
  this->AppendComponents->AddInputConnection(0, this->MultiplyAlpha->GetOutputPort());

  this->MultiplyAlpha->RemoveAllInputs();
  this->MultiplyAlpha->SetInputConnection(0, this->Threshold->GetOutputPort());

  this->ScalarComponent = 0;

  // This will force updating pipeline when an image is set
  this->PipelineType = -1;
}

//----------------------------------------------------------------------------
vtkMRMLVectorVolumeDisplayNode::~vtkMRMLVectorVolumeDisplayNode()
{
  this->ShiftScale->Delete();
  this->RGBToHSI->Delete();
  this->ExtractIntensity->Delete();
  this->ExtractComponents->Delete();
}

//----------------------------------------------------------------------------
void vtkMRMLVectorVolumeDisplayNode::SetInputToImageDataPipeline(vtkAlgorithmOutput *imageDataConnection)
{
  // Determine what the new pipeline shoud be
  int newPipelineType = PipelineSingleComponent;
  int numberOfScalarComponents = 0;
  if (this->ScalarMode == ScalarModeRGB)
    {
    vtkAlgorithm* producer = imageDataConnection ? imageDataConnection->GetProducer() : nullptr;
    vtkImageData* inputImageData = vtkImageData::SafeDownCast(producer ? producer->GetOutputDataObject(0) : nullptr);
    if (inputImageData)
      {
      numberOfScalarComponents = inputImageData->GetNumberOfScalarComponents();
      if (numberOfScalarComponents == 3)
        {
        newPipelineType = PipelineRGB;
        }
      else if (numberOfScalarComponents > 3)
        {
        newPipelineType = PipelineRGBA;
        }
      }
    }
  bool pipelineChanged = (this->PipelineType != newPipelineType);
  this->PipelineType = newPipelineType;

  if (this->PipelineType == PipelineRGB)
    {
    // RGB and input has 3 components (PipelineRGB):
    //
    //                                   ShiftScale
    //          /                                                          \
    // imageData                                                           AppendComponents
    //          \                                                          /
    //           RGBToHSI -> ExtractIntensity -> Threshold -> MultiplyAlpha

    this->ShiftScale->SetInputConnection(imageDataConnection);
    this->RGBToHSI->SetInputConnection(imageDataConnection);
    if (pipelineChanged)
      {
      this->ExtractIntensity->SetInputConnection(this->RGBToHSI->GetOutputPort());
      this->Threshold->SetInputConnection(this->ExtractComponents->GetOutputPort());
      }
    }
  else if (this->PipelineType == PipelineRGBA)
    {
    // RGB and input has more than 3 components (PipelineRGBA):
    //
    //                                                       ShiftScale
    //                               /                                                          \
    // imageData -> ExtractComponents                                                           AppendComponents
    //                               \                                                          /
    //                                RGBToHSI -> ExtractIntensity -> Threshold -> MultiplyAlpha

    this->ExtractComponents->SetInputConnection(imageDataConnection);
    if (pipelineChanged)
      {
      this->ShiftScale->SetInputConnection(this->ExtractComponents->GetOutputPort());
      this->RGBToHSI->SetInputConnection(this->ExtractComponents->GetOutputPort());
      this->ExtractIntensity->SetInputConnection(this->RGBToHSI->GetOutputPort());
      this->Threshold->SetInputConnection(this->ExtractComponents->GetOutputPort());
      }
    }
  else
    {
    // All other cases (PipelineSingleComponent)
    //
    //                                        ShiftScale
    //                               /                          \
    // imageData -> ExtractComponents                            AppendComponents
    //                               \                          /
    //                                Threshold -> MultiplyAlpha

    this->ExtractComponents->SetInputConnection(imageDataConnection);
    if (pipelineChanged)
      {
      this->ShiftScale->SetInputConnection(this->ExtractComponents->GetOutputPort());
      this->Threshold->SetInputConnection(this->ExtractComponents->GetOutputPort());
      }
    }
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorVolumeDisplayNode::GetInputImageDataConnection()
{
  vtkImageAlgorithm* algorithm = this->ExtractComponents;
  if (this->PipelineType == PipelineRGB)
    {
    algorithm = this->ShiftScale;
    }
  return algorithm->GetNumberOfInputConnections(0) ?
    algorithm->GetInputConnection(0,0) : nullptr;
}

//---------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorVolumeDisplayNode::GetScalarImageDataConnection()
{
  return this->GetInputImageDataConnection();
}

//----------------------------------------------------------------------------
void vtkMRMLVectorVolumeDisplayNode::UpdateImageDataPipeline()
{
  Superclass::UpdateImageDataPipeline();

  double halfWindow = (this->GetWindow() / 2.);
  double min = this->GetLevel() - halfWindow;
  this->ShiftScale->SetShift ( -min );
  this->ShiftScale->SetScale ( 255. / (this->GetWindow()) );
  if (this->PipelineType == PipelineSingleComponent)
    {
    this->ExtractComponents->SetComponents(this->ScalarComponent);
    }
  else
    {
    this->ExtractComponents->SetComponents(this->ScalarComponent,
      this->ScalarComponent+1, this->ScalarComponent+2);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLVectorVolumeDisplayNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  vtkMRMLWriteXMLBeginMacro(of);
  vtkMRMLWriteXMLEnumMacro(scalarMode, ScalarMode);
  vtkMRMLWriteXMLIntMacro(scalarComponent, ScalarComponent);
  vtkMRMLWriteXMLEnumMacro(glyphMode, GlyphMode);
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLVectorVolumeDisplayNode::ReadXMLAttributes(const char** atts)
{
  MRMLNodeModifyBlocker blocker(this);

  Superclass::ReadXMLAttributes(atts);

  vtkMRMLReadXMLBeginMacro(atts);
  vtkMRMLReadXMLEnumMacro(scalarMode, ScalarMode);
  vtkMRMLReadXMLIntMacro(scalarComponent, ScalarComponent);
  vtkMRMLReadXMLEnumMacro(glyphMode, GlyphMode);
  vtkMRMLReadXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLVectorVolumeDisplayNode::CopyContent(vtkMRMLNode* anode, bool deepCopy/*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  vtkMRMLVectorVolumeDisplayNode *node = vtkMRMLVectorVolumeDisplayNode::SafeDownCast(anode);
  if (node)
    {
    vtkMRMLCopyBeginMacro(anode);
    vtkMRMLCopyEnumMacro(ScalarMode);
    vtkMRMLCopyIntMacro(ScalarComponent);
    vtkMRMLCopyEnumMacro(GlyphMode);
    vtkMRMLCopyEndMacro();
    }
  // copy also updates imaging pipeline, therefore we need to do it after copying values
  Superclass::CopyContent(anode, deepCopy);
}

//----------------------------------------------------------------------------
void vtkMRMLVectorVolumeDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintEnumMacro(ScalarMode);
  vtkMRMLPrintIntMacro(ScalarComponent);
  vtkMRMLPrintEnumMacro(GlyphMode);
  vtkMRMLPrintEndMacro();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorVolumeDisplayNode::ProcessMRMLEvents ( vtkObject *caller,
                                           unsigned long event,
                                           void *callData )
{
  Superclass::ProcessMRMLEvents(caller, event, callData);
}

//-------------------------------------------------------
const char* vtkMRMLVectorVolumeDisplayNode::GetScalarModeAsString(int mode)
{
  switch (mode)
  {
  case ScalarModeRGB: return "RGB";
  case ScalarModeSingleComponent: return "single";
  default:
    // invalid id
    return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLVectorVolumeDisplayNode::GetScalarModeFromString(const char* name)
{
  if (name == nullptr)
    {
    // invalid name
    return -1;
    }
  for (int i = 0; i < vtkMRMLVectorVolumeDisplayNode::ScalarMode_Last; i++)
    {
    if (strcmp(name, vtkMRMLVectorVolumeDisplayNode::GetScalarModeAsString(i)) == 0)
      {
      // found a matching name
      return i;
      }
    }
  // name not found
  return -1;
}

//-------------------------------------------------------
const char* vtkMRMLVectorVolumeDisplayNode::GetGlyphModeAsString(int mode)
{
  switch (mode)
  {
  case vtkMRMLVectorVolumeDisplayNode::GlyphModeLines: return "rgb";
  case vtkMRMLVectorVolumeDisplayNode::GlyphModeTubes: return "single";
  default:
    // invalid id
    return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLVectorVolumeDisplayNode::GetGlyphModeFromString(const char* name)
{
  if (name == nullptr)
    {
    // invalid name
    return -1;
    }
  for (int i = 0; i < vtkMRMLVectorVolumeDisplayNode::GlyphMode_Last; i++)
    {
    if (strcmp(name, vtkMRMLVectorVolumeDisplayNode::GetGlyphModeAsString(i)) == 0)
      {
      // found a matching name
      return i;
      }
    }
  // name not found
  return -1;
}
