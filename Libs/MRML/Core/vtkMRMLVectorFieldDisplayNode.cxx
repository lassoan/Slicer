/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLVectorFieldDisplayNode.cxx,v $

=========================================================================auto=*/
// MRML includes
#include "vtkMRMLVectorFieldDisplayNode.h"
#include "vtkMRMLImageFieldSampler.h"
#include "vtkMRMLModelNode.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLVectorFieldModePipeline.h"
#include "vtkMRMLVectorFieldSampler.h"
#include "vtkMRMLVolumeNode.h"

// VTK includes
#include <vtkAlgorithmOutput.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>

// STD includes
#include <algorithm>
#include <cstring>
#include <sstream>

//----------------------------------------------------------------------------
namespace
{
/// Node reference role of the node that bounds the region in which the field is sampled.
const char RegionReferenceRole[] = "region";
} // namespace

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLVectorFieldDisplayNode);

//-----------------------------------------------------------------------------
vtkMRMLVectorFieldDisplayNode::vtkMRMLVectorFieldDisplayNode()
  : GlyphType(vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow)
  , OrientationArrayName(nullptr)
  , ScaleArrayName(nullptr)
  , VectorScaleMode(vtkMRMLVectorFieldDisplayNode::VectorScaleModeByMagnitude)
  , ScaleFactor(10.0)
  , MaskingMode(vtkMRMLVectorFieldDisplayNode::MaskingModeAllPoints)
  , MaskingNthPoint(10)
  , MaskingPointsNumber(1000)
  , VisualizationMode(vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph)
  , GridScalePercent(100.0)
  , GridLineDiameterMm(1.0)
  , SeedSpacingMm(0.0)
  , MaximumPropagationMm(100.0)
  , StreamlineTubeDiameterMm(0.5)
  , SamplingSpacingMm(0.0)
  , SliceProjectionEnabled(true)
  // Glyph geometry defaults reproduce the geometry of the VTK glyph sources with their own
  // default parameters (vtkArrowSource: tip length 0.35, shaft radius 0.3 * tip radius).
  , GlyphResolution(16)
  , GlyphTipLengthPercent(35.0)
  , GlyphShaftDiameterPercent(30.0)
  , GlyphResolution2D(12)
  , GlyphTipLengthPercent2D(30.0)
  , SliceSlabThicknessMm(1.0)
  , SliceGlyphScalePercent(100.0)
  , ThresholdEnabled(false)
{
  this->ThresholdRange[0] = 0.0;
  this->ThresholdRange[1] = -1.0; // invalid range by default (same convention as vtkMRMLModelDisplayNode)
  // Glyphs are shown in slice views by default (the base class default is off, because for
  // most display nodes the slice view representation is only an intersection contour).
  this->Visibility2D = 1;
  this->TypeDisplayName = vtkMRMLTr("vtkMRMLVectorFieldDisplayNode", "Vector Field Display");
}

//-----------------------------------------------------------------------------
vtkMRMLVectorFieldDisplayNode::~vtkMRMLVectorFieldDisplayNode()
{
  this->SetOrientationArrayName(nullptr);
  this->SetScaleArrayName(nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
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
  vtkMRMLPrintEnumMacro(VisualizationMode);
  vtkMRMLPrintFloatMacro(GridScalePercent);
  vtkMRMLPrintFloatMacro(GridLineDiameterMm);
  vtkMRMLPrintStdStringMacro(ContourLevelsMmAsString);
  vtkMRMLPrintFloatMacro(SeedSpacingMm);
  vtkMRMLPrintFloatMacro(MaximumPropagationMm);
  vtkMRMLPrintFloatMacro(StreamlineTubeDiameterMm);
  vtkMRMLPrintFloatMacro(SamplingSpacingMm);
  vtkMRMLPrintBooleanMacro(SliceProjectionEnabled);
  vtkMRMLPrintIntMacro(GlyphResolution);
  vtkMRMLPrintFloatMacro(GlyphTipLengthPercent);
  vtkMRMLPrintFloatMacro(GlyphShaftDiameterPercent);
  vtkMRMLPrintIntMacro(GlyphResolution2D);
  vtkMRMLPrintFloatMacro(GlyphTipLengthPercent2D);
  vtkMRMLPrintFloatMacro(SliceSlabThicknessMm);
  vtkMRMLPrintFloatMacro(SliceGlyphScalePercent);
  vtkMRMLPrintBooleanMacro(ThresholdEnabled);
  vtkMRMLPrintVectorMacro(ThresholdRange, double, 2);
  vtkMRMLPrintEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::WriteXML(ostream& of, int nIndent)
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
  vtkMRMLWriteXMLEnumMacro(visualizationMode, VisualizationMode);
  vtkMRMLWriteXMLFloatMacro(gridScalePercent, GridScalePercent);
  vtkMRMLWriteXMLFloatMacro(gridLineDiameterMm, GridLineDiameterMm);
  vtkMRMLWriteXMLStdStringMacro(contourLevelsMm, ContourLevelsMmAsString);
  vtkMRMLWriteXMLFloatMacro(seedSpacingMm, SeedSpacingMm);
  vtkMRMLWriteXMLFloatMacro(maximumPropagationMm, MaximumPropagationMm);
  vtkMRMLWriteXMLFloatMacro(streamlineTubeDiameterMm, StreamlineTubeDiameterMm);
  vtkMRMLWriteXMLFloatMacro(samplingSpacingMm, SamplingSpacingMm);
  vtkMRMLWriteXMLBooleanMacro(sliceProjectionEnabled, SliceProjectionEnabled);
  vtkMRMLWriteXMLIntMacro(glyphResolution, GlyphResolution);
  vtkMRMLWriteXMLFloatMacro(glyphTipLengthPercent, GlyphTipLengthPercent);
  vtkMRMLWriteXMLFloatMacro(glyphShaftDiameterPercent, GlyphShaftDiameterPercent);
  vtkMRMLWriteXMLIntMacro(glyphResolution2D, GlyphResolution2D);
  vtkMRMLWriteXMLFloatMacro(glyphTipLengthPercent2D, GlyphTipLengthPercent2D);
  vtkMRMLWriteXMLFloatMacro(sliceSlabThicknessMm, SliceSlabThicknessMm);
  vtkMRMLWriteXMLFloatMacro(sliceGlyphScalePercent, SliceGlyphScalePercent);
  vtkMRMLWriteXMLBooleanMacro(thresholdEnabled, ThresholdEnabled);
  vtkMRMLWriteXMLVectorMacro(thresholdRange, ThresholdRange, double, 2);
  vtkMRMLWriteXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::ReadXMLAttributes(const char** atts)
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
  vtkMRMLReadXMLEnumMacro(visualizationMode, VisualizationMode);
  vtkMRMLReadXMLFloatMacro(gridScalePercent, GridScalePercent);
  vtkMRMLReadXMLFloatMacro(gridLineDiameterMm, GridLineDiameterMm);
  vtkMRMLReadXMLStdStringMacro(contourLevelsMm, ContourLevelsMmAsString);
  vtkMRMLReadXMLFloatMacro(seedSpacingMm, SeedSpacingMm);
  vtkMRMLReadXMLFloatMacro(maximumPropagationMm, MaximumPropagationMm);
  vtkMRMLReadXMLFloatMacro(streamlineTubeDiameterMm, StreamlineTubeDiameterMm);
  vtkMRMLReadXMLFloatMacro(samplingSpacingMm, SamplingSpacingMm);
  vtkMRMLReadXMLBooleanMacro(sliceProjectionEnabled, SliceProjectionEnabled);
  vtkMRMLReadXMLIntMacro(glyphResolution, GlyphResolution);
  vtkMRMLReadXMLFloatMacro(glyphTipLengthPercent, GlyphTipLengthPercent);
  vtkMRMLReadXMLFloatMacro(glyphShaftDiameterPercent, GlyphShaftDiameterPercent);
  vtkMRMLReadXMLIntMacro(glyphResolution2D, GlyphResolution2D);
  vtkMRMLReadXMLFloatMacro(glyphTipLengthPercent2D, GlyphTipLengthPercent2D);
  vtkMRMLReadXMLFloatMacro(sliceSlabThicknessMm, SliceSlabThicknessMm);
  vtkMRMLReadXMLFloatMacro(sliceGlyphScalePercent, SliceGlyphScalePercent);
  vtkMRMLReadXMLBooleanMacro(thresholdEnabled, ThresholdEnabled);
  vtkMRMLReadXMLVectorMacro(thresholdRange, ThresholdRange, double, 2);
  vtkMRMLReadXMLEndMacro();

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::CopyContent(vtkMRMLNode* anode, bool deepCopy /*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::CopyContent(anode, deepCopy);

  vtkMRMLVectorFieldDisplayNode* node = vtkMRMLVectorFieldDisplayNode::SafeDownCast(anode);
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
  vtkMRMLCopyEnumMacro(VisualizationMode);
  vtkMRMLCopyFloatMacro(GridScalePercent);
  vtkMRMLCopyFloatMacro(GridLineDiameterMm);
  vtkMRMLCopyStdStringMacro(ContourLevelsMmAsString);
  vtkMRMLCopyFloatMacro(SeedSpacingMm);
  vtkMRMLCopyFloatMacro(MaximumPropagationMm);
  vtkMRMLCopyFloatMacro(StreamlineTubeDiameterMm);
  vtkMRMLCopyFloatMacro(SamplingSpacingMm);
  vtkMRMLCopyBooleanMacro(SliceProjectionEnabled);
  vtkMRMLCopyIntMacro(GlyphResolution);
  vtkMRMLCopyFloatMacro(GlyphTipLengthPercent);
  vtkMRMLCopyFloatMacro(GlyphShaftDiameterPercent);
  vtkMRMLCopyIntMacro(GlyphResolution2D);
  vtkMRMLCopyFloatMacro(GlyphTipLengthPercent2D);
  vtkMRMLCopyFloatMacro(SliceSlabThicknessMm);
  vtkMRMLCopyFloatMacro(SliceGlyphScalePercent);
  vtkMRMLCopyBooleanMacro(ThresholdEnabled);
  vtkMRMLCopyVectorMacro(ThresholdRange, double, 2);
  vtkMRMLCopyEndMacro();
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetGlyphTypeAsString()
{
  return vtkMRMLVectorFieldDisplayNode::GetGlyphTypeAsString(this->GlyphType);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetGlyphTypeFromString(const char* modeString)
{
  this->SetGlyphType(vtkMRMLVectorFieldDisplayNode::GetGlyphTypeFromString(modeString));
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetGlyphTypeAsString(int id)
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
int vtkMRMLVectorFieldDisplayNode::GetGlyphTypeFromString(const char* modeString)
{
  if (modeString == nullptr)
  {
    return -1;
  }
  for (int i = 0; i < vtkMRMLVectorFieldDisplayNode::GlyphType_Last; i++)
  {
    if (strcmp(modeString, vtkMRMLVectorFieldDisplayNode::GetGlyphTypeAsString(i)) == 0)
    {
      return i;
    }
  }
  return -1;
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetVectorScaleModeAsString()
{
  return vtkMRMLVectorFieldDisplayNode::GetVectorScaleModeAsString(this->VectorScaleMode);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetVectorScaleModeFromString(const char* modeString)
{
  this->SetVectorScaleMode(vtkMRMLVectorFieldDisplayNode::GetVectorScaleModeFromString(modeString));
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetVectorScaleModeAsString(int id)
{
  switch (id)
  {
    case VectorScaleModeByMagnitude: return "byMagnitude";
    case VectorScaleModeByComponents: return "byComponents";
    default: return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLVectorFieldDisplayNode::GetVectorScaleModeFromString(const char* modeString)
{
  if (modeString == nullptr)
  {
    return -1;
  }
  for (int i = 0; i < vtkMRMLVectorFieldDisplayNode::VectorScaleMode_Last; i++)
  {
    if (strcmp(modeString, vtkMRMLVectorFieldDisplayNode::GetVectorScaleModeAsString(i)) == 0)
    {
      return i;
    }
  }
  return -1;
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetMaskingModeAsString()
{
  return vtkMRMLVectorFieldDisplayNode::GetMaskingModeAsString(this->MaskingMode);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetMaskingModeFromString(const char* modeString)
{
  this->SetMaskingMode(vtkMRMLVectorFieldDisplayNode::GetMaskingModeFromString(modeString));
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetMaskingModeAsString(int id)
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
int vtkMRMLVectorFieldDisplayNode::GetMaskingModeFromString(const char* modeString)
{
  if (modeString == nullptr)
  {
    return -1;
  }
  for (int i = 0; i < vtkMRMLVectorFieldDisplayNode::MaskingMode_Last; i++)
  {
    if (strcmp(modeString, vtkMRMLVectorFieldDisplayNode::GetMaskingModeAsString(i)) == 0)
    {
      return i;
    }
  }
  return -1;
}

//-----------------------------------------------------------
vtkMRMLNode* vtkMRMLVectorFieldDisplayNode::GetRegionNode()
{
  return this->GetNodeReference(RegionReferenceRole);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetAndObserveRegionNode(vtkMRMLNode* node)
{
  this->SetAndObserveNodeReferenceID(RegionReferenceRole, node ? node->GetID() : nullptr);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::ProcessMRMLEvents(vtkObject* caller, unsigned long event, void* callData)
{
  this->Superclass::ProcessMRMLEvents(caller, event, callData);
  // Moving or resizing the region node changes where the field is sampled
  if (caller != nullptr && caller == this->GetRegionNode())
  {
    this->Modified();
  }
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::GetSamplingRegion(vtkMatrix4x4* regionToRAS, int regionSize[3])
{
  if (!regionToRAS || !regionSize)
  {
    return false;
  }
  vtkMRMLNode* regionNode = this->GetRegionNode();
  if (!regionNode)
  {
    return false;
  }
  double spacingMm = this->SamplingSpacingMm;

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(regionNode);
  if (sliceNode)
  {
    // A slice defines an oriented plane: sample a flat region in the plane of the slice.
    double fieldOfView[3] = { 0.0, 0.0, 0.0 };
    sliceNode->GetFieldOfView(fieldOfView);
    if (spacingMm <= 0.0 || fieldOfView[0] <= 0.0 || fieldOfView[1] <= 0.0)
    {
      return false;
    }
    regionToRAS->DeepCopy(sliceNode->GetSliceToRAS());
    // Scale the in-plane axes by the sampling spacing and move the origin to the corner
    double origin_RAS[3] = { regionToRAS->GetElement(0, 3), regionToRAS->GetElement(1, 3), regionToRAS->GetElement(2, 3) };
    for (int axis = 0; axis < 2; ++axis)
    {
      double axisVector[3] = { regionToRAS->GetElement(0, axis), regionToRAS->GetElement(1, axis), regionToRAS->GetElement(2, axis) };
      vtkMath::Normalize(axisVector);
      for (int row = 0; row < 3; ++row)
      {
        origin_RAS[row] -= 0.5 * fieldOfView[axis] * axisVector[row];
        regionToRAS->SetElement(row, axis, axisVector[row] * spacingMm);
      }
      regionSize[axis] = static_cast<int>(fieldOfView[axis] / spacingMm) + 1;
    }
    for (int row = 0; row < 3; ++row)
    {
      regionToRAS->SetElement(row, 3, origin_RAS[row]);
    }
    regionSize[2] = 1;
    return true;
  }

  // Any other displayable node (a region of interest, a plane, a model, a volume) bounds
  // the sampled region by its axis-aligned bounding box in RAS.
  vtkMRMLDisplayableNode* displayableRegionNode = vtkMRMLDisplayableNode::SafeDownCast(regionNode);
  if (!displayableRegionNode)
  {
    vtkWarningMacro("vtkMRMLVectorFieldDisplayNode: unsupported region node type " << regionNode->GetClassName());
    return false;
  }
  double bounds_RAS[6] = { 0.0, -1.0, 0.0, -1.0, 0.0, -1.0 };
  displayableRegionNode->GetRASBounds(bounds_RAS);
  if (bounds_RAS[1] < bounds_RAS[0] || bounds_RAS[3] < bounds_RAS[2] || bounds_RAS[5] < bounds_RAS[4])
  {
    return false;
  }
  if (spacingMm <= 0.0)
  {
    // Aim for a reasonable number of glyphs along the largest axis of the region
    double largestSizeMm = std::max(std::max(bounds_RAS[1] - bounds_RAS[0], bounds_RAS[3] - bounds_RAS[2]), bounds_RAS[5] - bounds_RAS[4]);
    spacingMm = (largestSizeMm > 0.0 ? largestSizeMm / 20.0 : 1.0);
  }
  regionToRAS->Identity();
  for (int axis = 0; axis < 3; ++axis)
  {
    regionToRAS->SetElement(axis, axis, spacingMm);
    regionToRAS->SetElement(axis, 3, bounds_RAS[2 * axis]);
    regionSize[axis] = static_cast<int>((bounds_RAS[2 * axis + 1] - bounds_RAS[2 * axis]) / spacingMm) + 1;
  }
  return true;
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::UpdateSamplerRegion(vtkMRMLVectorFieldSampler* sampler)
{
  if (!sampler)
  {
    return;
  }
  sampler->SetSamplingSpacingMm(this->SamplingSpacingMm);
  vtkNew<vtkMatrix4x4> regionToRAS;
  int regionSize[3] = { 0, 0, 0 };
  if (!this->GetSamplingRegion(regionToRAS, regionSize))
  {
    // No region node: the source samples the whole field that it has
    regionSize[0] = 0;
    regionSize[1] = 0;
    regionSize[2] = 0;
  }
  sampler->SetSamplingRegion(regionToRAS, regionSize);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetDefaultSampledFieldArrayNames()
{
  // A sampled field always provides the same two arrays, so there is nothing for the user
  // to pick: orient and scale the glyphs by the vector, color them by its magnitude.
  if (this->OrientationArrayName == nullptr || strcmp(this->OrientationArrayName, "") == 0)
  {
    this->SetOrientationArrayName(vtkMRMLVectorFieldSampler::GetVectorArrayName());
  }
  if (this->ScaleArrayName == nullptr || strcmp(this->ScaleArrayName, "") == 0)
  {
    this->SetScaleArrayName(vtkMRMLVectorFieldSampler::GetVectorArrayName());
  }
  if (this->ActiveScalarName == nullptr || strcmp(this->ActiveScalarName, "") == 0)
  {
    this->SetActiveScalarName(vtkMRMLVectorFieldSampler::GetMagnitudeArrayName());
  }
}

//-----------------------------------------------------------
vtkMRMLVectorFieldSampler* vtkMRMLVectorFieldDisplayNode::UpdateVolumeFieldSampler(vtkMRMLVolumeNode* volumeNode, vtkMRMLVectorFieldSampler* sampler)
{
  vtkMRMLImageFieldSampler* imageSampler = vtkMRMLImageFieldSampler::SafeDownCast(sampler);
  if (!imageSampler)
  {
    vtkNew<vtkMRMLImageFieldSampler> newSampler;
    this->FieldSampler = newSampler.GetPointer();
    imageSampler = newSampler.GetPointer();
    this->SetDefaultSampledFieldArrayNames();
  }
  imageSampler->SetInputConnection(volumeNode->GetImageDataConnection());
  vtkNew<vtkMatrix4x4> ijkToRAS;
  volumeNode->GetIJKToRASMatrix(ijkToRAS);
  imageSampler->SetIJKToRAS(ijkToRAS);
  // Spatial voxel vectors are stored with RAS components. For anything else the components
  // are assumed to be along the voxel axes, so that a plain 3-component image (a gradient
  // computed in IJK, for example) is still displayed with the right directions.
  imageSampler->SetVectorsInRAS(volumeNode->GetVoxelVectorType() == vtkMRMLVolumeNode::VoxelVectorTypeSpatial);
  this->UpdateSamplerRegion(imageSampler);
  return imageSampler;
}

//-----------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorFieldDisplayNode::GetFieldConnection()
{
  // Model point data: the mesh is already a pipeline output, it can be glyphed as it is.
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode());
  if (modelNode)
  {
    return modelNode->GetMeshConnection();
  }

  // Vector volume: the field is sampled from the voxels.
  vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(this->GetDisplayableNode());
  if (volumeNode && volumeNode->GetImageData() && volumeNode->GetImageData()->GetNumberOfScalarComponents() >= 3)
  {
    vtkMRMLVectorFieldSampler* sampler = this->UpdateVolumeFieldSampler(volumeNode, this->FieldSampler);
    return sampler ? sampler->GetOutputPort() : nullptr;
  }

  return nullptr;
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::IsPolyDataVisualizationMode()
{
  return this->VisualizationMode != vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph;
}

//-----------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorFieldDisplayNode::GetOutputPolyDataConnection(vtkAlgorithmOutput* fieldConnection /*=nullptr*/)
{
  if (!this->IsPolyDataVisualizationMode())
  {
    return nullptr;
  }
  // Grid, contour and streamline modes need the cells that connect the sampled points
  vtkMRMLVectorFieldSampler* sampler = vtkMRMLVectorFieldSampler::SafeDownCast(this->GetFieldConnection() ? this->GetFieldConnection()->GetProducer() : nullptr);
  if (!fieldConnection)
  {
    fieldConnection = this->GetFieldConnection();
  }
  if (!fieldConnection)
  {
    return nullptr;
  }
  if (sampler)
  {
    sampler->GenerateCellsOn();
  }
  if (!this->ModePipeline)
  {
    this->ModePipeline = vtkSmartPointer<vtkMRMLVectorFieldModePipeline>::New();
  }
  return this->ModePipeline->Update(this, fieldConnection);
}

//-----------------------------------------------------------
vtkSmartPointer<vtkMRMLVectorFieldSampler> vtkMRMLVectorFieldDisplayNode::CreateSliceFieldSampler()
{
  // A vector volume can be interpolated anywhere, including in the plane of a slice view.
  vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(this->GetDisplayableNode());
  if (!volumeNode || !volumeNode->GetImageData() || volumeNode->GetImageData()->GetNumberOfScalarComponents() < 3)
  {
    // A mesh only has values at its own points, there is nothing to interpolate in the
    // slice plane. Slice views select the points near the plane instead.
    return nullptr;
  }
  vtkSmartPointer<vtkMRMLImageFieldSampler> sliceSampler = vtkSmartPointer<vtkMRMLImageFieldSampler>::New();
  this->UpdateVolumeFieldSampler(volumeNode, sliceSampler);
  return sliceSampler;
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::GetFieldArrayInfo(std::vector<std::string>& arrayNames, std::vector<int>& numberOfComponents)
{
  arrayNames.clear();
  numberOfComponents.clear();

  // A sampled field always produces the same two arrays. They are reported without
  // executing the sampler, so that showing the GUI does not sample the field.
  vtkAlgorithmOutput* fieldConnection = this->GetFieldConnection();
  vtkAlgorithm* producer = fieldConnection ? fieldConnection->GetProducer() : nullptr;
  if (vtkMRMLVectorFieldSampler::SafeDownCast(producer))
  {
    arrayNames.emplace_back(vtkMRMLVectorFieldSampler::GetVectorArrayName());
    numberOfComponents.push_back(3);
    arrayNames.emplace_back(vtkMRMLVectorFieldSampler::GetMagnitudeArrayName());
    numberOfComponents.push_back(1);
    return;
  }

  vtkDataSet* fieldDataSet = this->GetScalarDataSet();
  if (!fieldDataSet)
  {
    return;
  }
  vtkPointData* pointData = fieldDataSet->GetPointData();
  if (!pointData)
  {
    return;
  }
  for (int arrayIndex = 0; arrayIndex < pointData->GetNumberOfArrays(); ++arrayIndex)
  {
    vtkDataArray* array = pointData->GetArray(arrayIndex);
    if (!array || !array->GetName())
    {
      continue;
    }
    arrayNames.emplace_back(array->GetName());
    numberOfComponents.push_back(array->GetNumberOfComponents());
  }
}

//-----------------------------------------------------------
vtkDataSet* vtkMRMLVectorFieldDisplayNode::GetScalarDataSet()
{
  vtkAlgorithmOutput* fieldConnection = this->GetFieldConnection();
  if (!fieldConnection)
  {
    return nullptr;
  }
  vtkAlgorithm* producer = fieldConnection->GetProducer();
  if (!producer)
  {
    return nullptr;
  }
  // GetOutputDataObject does not update the pipeline: sources that need sampling
  // (transforms, images) only return data that a render has already produced.
  return vtkDataSet::SafeDownCast(producer->GetOutputDataObject(fieldConnection->GetIndex()));
}

//-----------------------------------------------------------
vtkDataArray* vtkMRMLVectorFieldDisplayNode::GetOrientationArray()
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
vtkDataArray* vtkMRMLVectorFieldDisplayNode::GetScaleArray()
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

//-----------------------------------------------------------
vtkDataArray* vtkMRMLVectorFieldDisplayNode::GetActiveScalarArray()
{
  if (this->ActiveScalarName == nullptr || strcmp(this->ActiveScalarName, "") == 0)
  {
    return nullptr;
  }
  vtkDataSet* mesh = this->GetScalarDataSet();
  if (!mesh)
  {
    return nullptr;
  }
  // Glyphs are placed at mesh points, so only point data arrays can color them.
  return mesh->GetPointData()->GetArray(this->ActiveScalarName);
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetVisualizationModeAsString(int id)
{
  switch (id)
  {
    case VisualizationModeGlyph: return "glyph";
    case VisualizationModeGrid: return "grid";
    case VisualizationModeContour: return "contour";
    case VisualizationModeStreamline: return "streamline";
    default: return "";
  }
}

//-----------------------------------------------------------
int vtkMRMLVectorFieldDisplayNode::GetVisualizationModeFromString(const char* modeString)
{
  if (!modeString)
  {
    return -1;
  }
  for (int id = 0; id < VisualizationMode_Last; ++id)
  {
    if (strcmp(modeString, GetVisualizationModeAsString(id)) == 0)
    {
      return id;
    }
  }
  return -1;
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetVisualizationModeAsString()
{
  return vtkMRMLVectorFieldDisplayNode::GetVisualizationModeAsString(this->VisualizationMode);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetVisualizationModeFromString(const char* modeString)
{
  int id = vtkMRMLVectorFieldDisplayNode::GetVisualizationModeFromString(modeString);
  if (id < 0)
  {
    vtkWarningMacro("Invalid visualization mode: " << (modeString ? modeString : "(none)"));
    return;
  }
  this->SetVisualizationMode(id);
}

//-----------------------------------------------------------
unsigned int vtkMRMLVectorFieldDisplayNode::GetNumberOfContourLevels()
{
  return static_cast<unsigned int>(this->ContourLevelsMm.size());
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetContourLevelsMm(double* levels, int size)
{
  if (size < 0 || (size > 0 && !levels))
  {
    return;
  }
  std::vector<double> newLevels(levels, levels + size);
  if (newLevels == this->ContourLevelsMm)
  {
    return;
  }
  this->ContourLevelsMm = newLevels;
  this->Modified();
}

//-----------------------------------------------------------
double* vtkMRMLVectorFieldDisplayNode::GetContourLevelsMm()
{
  return this->ContourLevelsMm.empty() ? nullptr : this->ContourLevelsMm.data();
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::GetContourLevelsMm(std::vector<double>& levels)
{
  levels = this->ContourLevelsMm;
}

//-----------------------------------------------------------
std::string vtkMRMLVectorFieldDisplayNode::GetContourLevelsMmAsString()
{
  std::stringstream levelsStream;
  for (size_t levelIndex = 0; levelIndex < this->ContourLevelsMm.size(); ++levelIndex)
  {
    if (levelIndex > 0)
    {
      levelsStream << " ";
    }
    levelsStream << this->ContourLevelsMm[levelIndex];
  }
  return levelsStream.str();
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetContourLevelsMmFromString(const char* levelsString)
{
  this->SetContourLevelsMmAsString(levelsString ? std::string(levelsString) : std::string());
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetContourLevelsMmAsString(const std::string& levelsString)
{
  std::vector<double> levels;
  {
    std::stringstream levelsStream(levelsString);
    while (!levelsStream.eof())
    {
      double level = 0.0;
      levelsStream >> level;
      if (levelsStream.fail())
      {
        break;
      }
      levels.push_back(level);
    }
  }
  this->SetContourLevelsMm(levels.data(), static_cast<int>(levels.size()));
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::UpdateAssignedAttribute()
{
  this->Superclass::UpdateAssignedAttribute();
  this->UpdateScalarRange();
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::UpdateScalarRange()
{
  if (this->GetScalarRangeFlag() == vtkMRMLDisplayNode::UseDataScalarRange)
  {
    vtkDataArray* activeScalarArray = this->GetActiveScalarArray();
    if (activeScalarArray && activeScalarArray->GetNumberOfComponents() > 1)
    {
      // Multi-component arrays are displayed by magnitude (see the glyph display pipelines),
      // so the automatic range must be the range of the magnitude, not of the first component.
      double magnitudeRange[2] = { 0.0, -1.0 };
      activeScalarArray->GetRange(magnitudeRange, -1); // -1: L2 norm over all components
      this->SetScalarRange(magnitudeRange);
      return;
    }
  }
  this->Superclass::UpdateScalarRange();
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetThresholdRange(double min, double max)
{
  if (this->ThresholdRange[0] == min && this->ThresholdRange[1] == max)
  {
    return;
  }
  this->ThresholdRange[0] = min;
  this->ThresholdRange[1] = max;
  this->Modified();
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetThresholdRange(double range[2])
{
  this->SetThresholdRange(range[0], range[1]);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::GetThresholdRange(double range[2])
{
  range[0] = this->ThresholdRange[0];
  range[1] = this->ThresholdRange[1];
}

//-----------------------------------------------------------
double* vtkMRMLVectorFieldDisplayNode::GetThresholdRange()
{
  return this->ThresholdRange;
}

//-----------------------------------------------------------
double vtkMRMLVectorFieldDisplayNode::GetThresholdMin()
{
  return this->ThresholdRange[0];
}

//-----------------------------------------------------------
double vtkMRMLVectorFieldDisplayNode::GetThresholdMax()
{
  return this->ThresholdRange[1];
}
