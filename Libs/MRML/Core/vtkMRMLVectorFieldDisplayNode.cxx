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
#include "vtkMRMLMeshFieldSampler.h"
#include "vtkMRMLMarkupsPlaneNode.h"
#include "vtkMRMLMarkupsROINode.h"
#include "vtkMRMLModelNode.h"
#include "vtkMRMLMarkupsCurveNode.h"
#include "vtkMRMLMarkupsNode.h"
#include "vtkMRMLTransformNode.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLVectorFieldModePipeline.h"
#include "vtkMRMLVectorFieldSampler.h"
#include "vtkMRMLVolumeNode.h"

// VTK includes
#include <vtkAlgorithmOutput.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkImageData.h>
#include <vtkIntArray.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkCellTypes.h>
#include <vtkPointSet.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPoints.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

//----------------------------------------------------------------------------
namespace
{
/// Node reference role of the node that bounds the region in which the field is sampled.
const char RegionReferenceRole[] = "region";
/// Node reference role of the markups node whose control points are sampled.
const char SamplePointsReferenceRole[] = "samplePoints";
/// Node reference role of the node whose points the streamlines are started from.
const char SeedPointsReferenceRole[] = "seedPoints";
} // namespace

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLVectorFieldDisplayNode);

//-----------------------------------------------------------------------------
vtkMRMLVectorFieldDisplayNode::vtkMRMLVectorFieldDisplayNode()
  : GlyphType(vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow)
  , OrientationArrayName(nullptr)
  , ScaleArrayName(nullptr)
  , FieldArrayName(nullptr)
  , VectorScaleMode(vtkMRMLVectorFieldDisplayNode::VectorScaleModeByMagnitude)
  , ScaleFactor(10.0)
  , MaskingMode(vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds)
  , MaskingPointsNumber(5000)
  , VisualizationMode(vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph)
  , ScaleDirectional(true)
  , GlyphDiameterMm(5.0)
  , GlyphDiameterAbsolute(false)
  , GlyphDiameterPercent(20.0)
  , GridSpacingMm(3.0)
  , GridResolutionMm(1.0)
  , ContourResolutionMm(1.0)
  , GridShowNonWarped(false)
  , ContourOpacity(0.8)
  , GridScalePercent(100.0)
  , GridLineDiameterMm(0.5)
  , SeedSpacingMm(3.0)
  , MaximumPropagationMm(100.0)
  , StreamlineBidirectional(true)
  , StreamlineInitialIntegrationStepMm(0.1)
  , StreamlineTubeDiameterMm(0.5)
  , SamplingSpacingMm(5.0)
  , SliceProjectionEnabled(true)
  // Glyph geometry defaults reproduce the geometry of the VTK glyph sources with their own
  // default parameters (vtkArrowSource: tip length 0.35, shaft radius 0.3 * tip radius).
  , GlyphResolution(16)
  , GlyphTipLengthPercent(35.0)
  , GlyphShaftDiameterPercent(30.0)
  , GlyphResolution2D(12)
  , GlyphTipLengthPercent2D(30.0)
  , ThresholdEnabled(false)
{
  this->ThresholdRange[0] = 0.0;
  this->ThresholdRange[1] = -1.0; // invalid range by default (same convention as vtkMRMLModelDisplayNode)
  // A vector field says most when the length of the vectors is visible in their color too
  this->ScalarVisibility = 1;
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
  // Freed directly rather than through SetFieldArrayName(): that setter makes the other
  // arrays follow the field, calls SetActiveScalar() and recomputes the scale factor from
  // the displayable node, and invokes events on the way out. None of that is safe on a node
  // whose subclass parts have already been destroyed.
  delete[] this->FieldArrayName;
  this->FieldArrayName = nullptr;
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintEnumMacro(GlyphType);
  vtkMRMLPrintStringMacro(OrientationArrayName);
  vtkMRMLPrintStringMacro(ScaleArrayName);
  vtkMRMLPrintStringMacro(FieldArrayName);
  vtkMRMLPrintEnumMacro(VectorScaleMode);
  vtkMRMLPrintFloatMacro(ScaleFactor);
  vtkMRMLPrintEnumMacro(MaskingMode);
  vtkMRMLPrintIntMacro(MaskingPointsNumber);
  vtkMRMLPrintEnumMacro(VisualizationMode);
  vtkMRMLPrintBooleanMacro(ScaleDirectional);
  vtkMRMLPrintFloatMacro(GlyphDiameterMm);
  vtkMRMLPrintBooleanMacro(GlyphDiameterAbsolute);
  vtkMRMLPrintFloatMacro(GlyphDiameterPercent);
  vtkMRMLPrintFloatMacro(GridSpacingMm);
  vtkMRMLPrintFloatMacro(GridResolutionMm);
  vtkMRMLPrintFloatMacro(ContourResolutionMm);
  vtkMRMLPrintBooleanMacro(GridShowNonWarped);
  vtkMRMLPrintFloatMacro(ContourOpacity);
  vtkMRMLPrintFloatMacro(GridScalePercent);
  vtkMRMLPrintFloatMacro(GridLineDiameterMm);
  vtkMRMLPrintStdStringMacro(ContourLevelsMmAsString);
  vtkMRMLPrintFloatMacro(SeedSpacingMm);
  vtkMRMLPrintFloatMacro(MaximumPropagationMm);
  vtkMRMLPrintBooleanMacro(StreamlineBidirectional);
  vtkMRMLPrintFloatMacro(StreamlineInitialIntegrationStepMm);
  vtkMRMLPrintFloatMacro(StreamlineTubeDiameterMm);
  vtkMRMLPrintFloatMacro(SamplingSpacingMm);
  vtkMRMLPrintBooleanMacro(SliceProjectionEnabled);
  vtkMRMLPrintIntMacro(GlyphResolution);
  vtkMRMLPrintFloatMacro(GlyphTipLengthPercent);
  vtkMRMLPrintFloatMacro(GlyphShaftDiameterPercent);
  vtkMRMLPrintIntMacro(GlyphResolution2D);
  vtkMRMLPrintFloatMacro(GlyphTipLengthPercent2D);
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
  vtkMRMLWriteXMLStringMacro(fieldArrayName, FieldArrayName);
  vtkMRMLWriteXMLEnumMacro(vectorScaleMode, VectorScaleMode);
  vtkMRMLWriteXMLFloatMacro(scaleFactor, ScaleFactor);
  vtkMRMLWriteXMLEnumMacro(maskingMode, MaskingMode);
  vtkMRMLWriteXMLIntMacro(maskingPointsNumber, MaskingPointsNumber);
  vtkMRMLWriteXMLEnumMacro(visualizationMode, VisualizationMode);
  vtkMRMLWriteXMLBooleanMacro(scaleDirectional, ScaleDirectional);
  vtkMRMLWriteXMLFloatMacro(glyphDiameterMm, GlyphDiameterMm);
  vtkMRMLWriteXMLBooleanMacro(glyphDiameterAbsolute, GlyphDiameterAbsolute);
  vtkMRMLWriteXMLFloatMacro(glyphDiameterPercent, GlyphDiameterPercent);
  vtkMRMLWriteXMLFloatMacro(gridSpacingMm, GridSpacingMm);
  vtkMRMLWriteXMLFloatMacro(gridResolutionMm, GridResolutionMm);
  vtkMRMLWriteXMLFloatMacro(contourResolutionMm, ContourResolutionMm);
  vtkMRMLWriteXMLBooleanMacro(gridShowNonWarped, GridShowNonWarped);
  vtkMRMLWriteXMLFloatMacro(contourOpacity, ContourOpacity);
  vtkMRMLWriteXMLFloatMacro(gridScalePercent, GridScalePercent);
  vtkMRMLWriteXMLFloatMacro(gridLineDiameterMm, GridLineDiameterMm);
  vtkMRMLWriteXMLStdStringMacro(contourLevelsMm, ContourLevelsMmAsString);
  vtkMRMLWriteXMLFloatMacro(seedSpacingMm, SeedSpacingMm);
  vtkMRMLWriteXMLFloatMacro(maximumPropagationMm, MaximumPropagationMm);
  vtkMRMLWriteXMLBooleanMacro(streamlineBidirectional, StreamlineBidirectional);
  vtkMRMLWriteXMLFloatMacro(streamlineInitialIntegrationStepMm, StreamlineInitialIntegrationStepMm);
  vtkMRMLWriteXMLFloatMacro(streamlineTubeDiameterMm, StreamlineTubeDiameterMm);
  vtkMRMLWriteXMLFloatMacro(samplingSpacingMm, SamplingSpacingMm);
  vtkMRMLWriteXMLBooleanMacro(sliceProjectionEnabled, SliceProjectionEnabled);
  vtkMRMLWriteXMLIntMacro(glyphResolution, GlyphResolution);
  vtkMRMLWriteXMLFloatMacro(glyphTipLengthPercent, GlyphTipLengthPercent);
  vtkMRMLWriteXMLFloatMacro(glyphShaftDiameterPercent, GlyphShaftDiameterPercent);
  vtkMRMLWriteXMLIntMacro(glyphResolution2D, GlyphResolution2D);
  vtkMRMLWriteXMLFloatMacro(glyphTipLengthPercent2D, GlyphTipLengthPercent2D);
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
  vtkMRMLReadXMLStringMacro(fieldArrayName, FieldArrayName);
  vtkMRMLReadXMLEnumMacro(vectorScaleMode, VectorScaleMode);
  vtkMRMLReadXMLFloatMacro(scaleFactor, ScaleFactor);
  vtkMRMLReadXMLEnumMacro(maskingMode, MaskingMode);
  vtkMRMLReadXMLIntMacro(maskingPointsNumber, MaskingPointsNumber);
  vtkMRMLReadXMLEnumMacro(visualizationMode, VisualizationMode);
  vtkMRMLReadXMLBooleanMacro(scaleDirectional, ScaleDirectional);
  vtkMRMLReadXMLFloatMacro(glyphDiameterMm, GlyphDiameterMm);
  vtkMRMLReadXMLBooleanMacro(glyphDiameterAbsolute, GlyphDiameterAbsolute);
  vtkMRMLReadXMLFloatMacro(glyphDiameterPercent, GlyphDiameterPercent);
  vtkMRMLReadXMLFloatMacro(gridSpacingMm, GridSpacingMm);
  vtkMRMLReadXMLFloatMacro(gridResolutionMm, GridResolutionMm);
  vtkMRMLReadXMLFloatMacro(contourResolutionMm, ContourResolutionMm);
  vtkMRMLReadXMLBooleanMacro(gridShowNonWarped, GridShowNonWarped);
  vtkMRMLReadXMLFloatMacro(contourOpacity, ContourOpacity);
  vtkMRMLReadXMLFloatMacro(gridScalePercent, GridScalePercent);
  vtkMRMLReadXMLFloatMacro(gridLineDiameterMm, GridLineDiameterMm);
  vtkMRMLReadXMLStdStringMacro(contourLevelsMm, ContourLevelsMmAsString);
  vtkMRMLReadXMLFloatMacro(seedSpacingMm, SeedSpacingMm);
  vtkMRMLReadXMLFloatMacro(maximumPropagationMm, MaximumPropagationMm);
  vtkMRMLReadXMLBooleanMacro(streamlineBidirectional, StreamlineBidirectional);
  vtkMRMLReadXMLFloatMacro(streamlineInitialIntegrationStepMm, StreamlineInitialIntegrationStepMm);
  vtkMRMLReadXMLFloatMacro(streamlineTubeDiameterMm, StreamlineTubeDiameterMm);
  vtkMRMLReadXMLFloatMacro(samplingSpacingMm, SamplingSpacingMm);
  vtkMRMLReadXMLBooleanMacro(sliceProjectionEnabled, SliceProjectionEnabled);
  vtkMRMLReadXMLIntMacro(glyphResolution, GlyphResolution);
  vtkMRMLReadXMLFloatMacro(glyphTipLengthPercent, GlyphTipLengthPercent);
  vtkMRMLReadXMLFloatMacro(glyphShaftDiameterPercent, GlyphShaftDiameterPercent);
  vtkMRMLReadXMLIntMacro(glyphResolution2D, GlyphResolution2D);
  vtkMRMLReadXMLFloatMacro(glyphTipLengthPercent2D, GlyphTipLengthPercent2D);
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
  vtkMRMLCopyStringMacro(FieldArrayName);
  vtkMRMLCopyEnumMacro(VectorScaleMode);
  vtkMRMLCopyFloatMacro(ScaleFactor);
  vtkMRMLCopyEnumMacro(MaskingMode);
  vtkMRMLCopyIntMacro(MaskingPointsNumber);
  vtkMRMLCopyEnumMacro(VisualizationMode);
  vtkMRMLCopyBooleanMacro(ScaleDirectional);
  vtkMRMLCopyFloatMacro(GlyphDiameterMm);
  vtkMRMLCopyBooleanMacro(GlyphDiameterAbsolute);
  vtkMRMLCopyFloatMacro(GlyphDiameterPercent);
  vtkMRMLCopyFloatMacro(GridSpacingMm);
  vtkMRMLCopyFloatMacro(GridResolutionMm);
  vtkMRMLCopyFloatMacro(ContourResolutionMm);
  vtkMRMLCopyBooleanMacro(GridShowNonWarped);
  vtkMRMLCopyFloatMacro(ContourOpacity);
  vtkMRMLCopyFloatMacro(GridScalePercent);
  vtkMRMLCopyFloatMacro(GridLineDiameterMm);
  vtkMRMLCopyStdStringMacro(ContourLevelsMmAsString);
  vtkMRMLCopyFloatMacro(SeedSpacingMm);
  vtkMRMLCopyFloatMacro(MaximumPropagationMm);
  vtkMRMLCopyBooleanMacro(StreamlineBidirectional);
  vtkMRMLCopyFloatMacro(StreamlineInitialIntegrationStepMm);
  vtkMRMLCopyFloatMacro(StreamlineTubeDiameterMm);
  vtkMRMLCopyFloatMacro(SamplingSpacingMm);
  vtkMRMLCopyBooleanMacro(SliceProjectionEnabled);
  vtkMRMLCopyIntMacro(GlyphResolution);
  vtkMRMLCopyFloatMacro(GlyphTipLengthPercent);
  vtkMRMLCopyFloatMacro(GlyphShaftDiameterPercent);
  vtkMRMLCopyIntMacro(GlyphResolution2D);
  vtkMRMLCopyFloatMacro(GlyphTipLengthPercent2D);
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
    case MaskingModeUniformBounds: return "uniformBounds";
    case MaskingModeUniformSurface: return "uniformSurface";
    case MaskingModeUniformVolume: return "uniformVolume";
    case MaskingModeFixedSpacing: return "fixedSpacing";
    case MaskingModeNodePoints: return "nodePoints";
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
  vtkNew<vtkIntArray> events;
  vtkMRMLVectorFieldDisplayNode::AddSamplingEvents(events);
  this->SetAndObserveNodeReferenceID(RegionReferenceRole, node ? node->GetID() : nullptr, events);
}

//-----------------------------------------------------------
vtkMRMLNode* vtkMRMLVectorFieldDisplayNode::GetSamplePointsNode()
{
  return this->GetNodeReference(SamplePointsReferenceRole);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetAndObserveSamplePointsNode(vtkMRMLNode* node)
{
  vtkNew<vtkIntArray> events;
  vtkMRMLVectorFieldDisplayNode::AddSamplingEvents(events);
  this->SetAndObserveNodeReferenceID(SamplePointsReferenceRole, node ? node->GetID() : nullptr, events);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::AddSamplingEvents(vtkIntArray* events)
{
  if (!events)
  {
    return;
  }
  // A markups node does not invoke ModifiedEvent when its control points change: dragging a
  // point fires PointModifiedEvent, placing or deleting one fires PointAddedEvent /
  // PointRemovedEvent, and a node moved by a transform fires only TransformModifiedEvent.
  // All of them change where the field has to be sampled or seeded from, so all are observed.
  events->InsertNextValue(vtkCommand::ModifiedEvent);
  events->InsertNextValue(vtkMRMLMarkupsNode::PointModifiedEvent);
  events->InsertNextValue(vtkMRMLMarkupsNode::PointAddedEvent);
  events->InsertNextValue(vtkMRMLMarkupsNode::PointRemovedEvent);
  events->InsertNextValue(vtkMRMLMarkupsNode::PointPositionDefinedEvent);
  events->InsertNextValue(vtkMRMLMarkupsNode::PointPositionUndefinedEvent);
  events->InsertNextValue(vtkMRMLTransformableNode::TransformModifiedEvent);
  // A model node used as a region or a seed source does not invoke ModifiedEvent when only
  // its mesh changes.
  events->InsertNextValue(vtkMRMLModelNode::MeshModifiedEvent);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::ProcessMRMLEvents(vtkObject* caller, unsigned long event, void* callData)
{
  this->Superclass::ProcessMRMLEvents(caller, event, callData);
  // Moving or resizing the region node, moving the sample points, or editing the streamline
  // seed points changes where the field is sampled or where streamlines start from
  if (caller != nullptr
      && (caller == this->GetRegionNode()      //
          || caller == this->GetSamplePointsNode() //
          || caller == this->GetSeedPointsNode()))
  {
    this->Modified();
  }
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::GetRegionBox(vtkMatrix4x4* boxToRAS, double bounds[6])
{
  vtkMRMLNode* regionNode = this->GetRegionNode();
  if (!boxToRAS || !bounds || !regionNode)
  {
    return false;
  }
  boxToRAS->Identity();
  // The box is the region itself, not the lattice that would be sampled in it: a lattice is
  // rounded up to a whole number of steps and so reaches past the region, which would leave
  // glyphs outside the region the user asked for.
  vtkMRMLMarkupsROINode* roiNode = vtkMRMLMarkupsROINode::SafeDownCast(regionNode);
  if (roiNode)
  {
    double sizeMm[3] = { 0.0, 0.0, 0.0 };
    roiNode->GetSize(sizeMm);
    boxToRAS->DeepCopy(roiNode->GetObjectToWorldMatrix());
    for (int axis = 0; axis < 3; ++axis)
    {
      bounds[2 * axis] = -0.5 * sizeMm[axis];
      bounds[2 * axis + 1] = 0.5 * sizeMm[axis];
    }
    return true;
  }

  vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(regionNode);
  if (planeNode)
  {
    double sizeMm[2] = { 0.0, 0.0 };
    planeNode->GetSize(sizeMm);
    planeNode->GetObjectToWorldMatrix(boxToRAS);
    for (int axis = 0; axis < 2; ++axis)
    {
      bounds[2 * axis] = -0.5 * sizeMm[axis];
      bounds[2 * axis + 1] = 0.5 * sizeMm[axis];
    }
    // A plane has no thickness of its own, so it is given one sampling step of it; without
    // that, no point would be exactly on it and nothing would be drawn.
    double thicknessMm = this->GetSamplingSpacingForFieldMm();
    if (thicknessMm <= 0.0)
    {
      thicknessMm = std::max(std::max(sizeMm[0], sizeMm[1]) / 20.0, 1e-6);
    }
    bounds[4] = -0.5 * thicknessMm;
    bounds[5] = 0.5 * thicknessMm;
    return true;
  }

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(regionNode);
  if (sliceNode)
  {
    double fieldOfView[3] = { 0.0, 0.0, 0.0 };
    sliceNode->GetFieldOfView(fieldOfView);
    boxToRAS->DeepCopy(sliceNode->GetSliceToRAS());
    // The slice axes carry no scale of their own, so the field of view is the box directly
    for (int axis = 0; axis < 3; ++axis)
    {
      bounds[2 * axis] = -0.5 * fieldOfView[axis];
      bounds[2 * axis + 1] = 0.5 * fieldOfView[axis];
    }
    return (fieldOfView[0] > 0.0 && fieldOfView[1] > 0.0);
  }

  // A model or a volume bounds the region by its axis-aligned bounding box in RAS
  vtkMRMLDisplayableNode* displayableRegionNode = vtkMRMLDisplayableNode::SafeDownCast(regionNode);
  if (!displayableRegionNode)
  {
    return false;
  }
  displayableRegionNode->GetRASBounds(bounds);
  return (bounds[0] <= bounds[1] && bounds[2] <= bounds[3] && bounds[4] <= bounds[5]);
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
  double spacingMm = this->GetSamplingSpacingForFieldMm();

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
    // Scale the in-plane axes by the sampling spacing and move the origin to the corner. The
    // lattice holds a whole number of groups, so that grid visualization draws complete grid
    // cells, and it is centered in the field of view.
    int groupSize = std::max(1, this->GetGridSubdivision());
    double origin_RAS[3] = { regionToRAS->GetElement(0, 3), regionToRAS->GetElement(1, 3), regionToRAS->GetElement(2, 3) };
    for (int axis = 0; axis < 2; ++axis)
    {
      int numberOfSteps = std::max(groupSize, static_cast<int>(fieldOfView[axis] / (spacingMm * groupSize)) * groupSize);
      double axisVector[3] = { regionToRAS->GetElement(0, axis), regionToRAS->GetElement(1, axis), regionToRAS->GetElement(2, axis) };
      vtkMath::Normalize(axisVector);
      for (int row = 0; row < 3; ++row)
      {
        origin_RAS[row] -= 0.5 * numberOfSteps * spacingMm * axisVector[row];
        regionToRAS->SetElement(row, axis, axisVector[row] * spacingMm);
      }
      regionSize[axis] = numberOfSteps + 1;
    }
    for (int row = 0; row < 3; ++row)
    {
      regionToRAS->SetElement(row, 3, origin_RAS[row]);
    }
    regionSize[2] = 1;
    return true;
  }

  vtkMRMLMarkupsROINode* roiNode = vtkMRMLMarkupsROINode::SafeDownCast(regionNode);
  if (roiNode)
  {
    // An oriented box: sample along its own axes, not along the RAS axes.
    double roiSizeMm[3] = { 0.0, 0.0, 0.0 };
    roiNode->GetSize(roiSizeMm);
    return this->GetOrientedSamplingRegion(roiNode->GetObjectToWorldMatrix(), roiSizeMm, regionToRAS, regionSize);
  }

  vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(regionNode);
  if (planeNode)
  {
    // An oriented rectangle: a flat region in the plane.
    double planeSizeMm[2] = { 0.0, 0.0 };
    planeNode->GetSize(planeSizeMm);
    double regionSizeMm[3] = { planeSizeMm[0], planeSizeMm[1], 0.0 };
    vtkNew<vtkMatrix4x4> planeToWorld;
    planeNode->GetObjectToWorldMatrix(planeToWorld);
    return this->GetOrientedSamplingRegion(planeToWorld, regionSizeMm, regionToRAS, regionSize);
  }

  // Any other displayable node (a model, a volume) bounds the sampled region by its
  // axis-aligned bounding box in RAS.
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
bool vtkMRMLVectorFieldDisplayNode::GetOrientedSamplingRegion(vtkMatrix4x4* objectToWorld, const double sizeMm[3], vtkMatrix4x4* regionToRAS, int regionSize[3])
{
  if (!objectToWorld)
  {
    return false;
  }
  double spacingMm = this->GetSamplingSpacingForFieldMm();
  if (spacingMm <= 0.0)
  {
    // Aim for a reasonable number of samples along the largest axis of the region
    double largestSizeMm = std::max(std::max(sizeMm[0], sizeMm[1]), sizeMm[2]);
    spacingMm = (largestSizeMm > 0.0 ? largestSizeMm / 20.0 : 1.0);
  }

  // The lattice holds a whole number of groups along each axis, so that grid visualization
  // draws complete grid cells, and it is centered in the region: the leftover of the region
  // that the lattice does not span is split between its two sides.
  int groupSize = std::max(1, this->GetGridSubdivision());
  int numberOfSteps[3] = { 0, 0, 0 };
  double corner_Object[4] = { 0.0, 0.0, 0.0, 1.0 };
  for (int axis = 0; axis < 3; ++axis)
  {
    // Rounding, because a region size that is a whole number of steps may be computed as a
    // hair less than that
    numberOfSteps[axis] = std::max(groupSize, static_cast<int>(sizeMm[axis] / (spacingMm * groupSize) + 0.5) * groupSize);
    corner_Object[axis] = -0.5 * numberOfSteps[axis] * spacingMm;
    regionSize[axis] = numberOfSteps[axis] + 1;
  }
  double corner_RAS[4] = { 0.0, 0.0, 0.0, 1.0 };
  objectToWorld->MultiplyPoint(corner_Object, corner_RAS);

  regionToRAS->DeepCopy(objectToWorld);
  for (int axis = 0; axis < 3; ++axis)
  {
    double axisVector[3] = { objectToWorld->GetElement(0, axis), objectToWorld->GetElement(1, axis), objectToWorld->GetElement(2, axis) };
    vtkMath::Normalize(axisVector);
    for (int row = 0; row < 3; ++row)
    {
      regionToRAS->SetElement(row, axis, axisVector[row] * spacingMm);
    }
  }
  for (int row = 0; row < 3; ++row)
  {
    regionToRAS->SetElement(row, 3, corner_RAS[row]);
  }
  return true;
}

//-----------------------------------------------------------
double vtkMRMLVectorFieldDisplayNode::GetEffectiveSamplingSpacingMm()
{
  // Each mode keeps its own resolution: a grid and a set of isosurfaces need a finer
  // sampling to look smooth than a field of glyphs needs to stay readable.
  switch (this->VisualizationMode)
  {
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid: return this->GridResolutionMm;
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeContour: return this->ContourResolutionMm;
    default: return this->SamplingSpacingMm;
  }
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetEffectiveSamplingSpacingMm(double spacingMm)
{
  switch (this->VisualizationMode)
  {
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid: this->SetGridResolutionMm(spacingMm); break;
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeContour: this->SetContourResolutionMm(spacingMm); break;
    default: this->SetSamplingSpacingMm(spacingMm); break;
  }
}

//-----------------------------------------------------------
vtkMRMLNode* vtkMRMLVectorFieldDisplayNode::GetSeedPointsNode()
{
  return this->GetNodeReference(SeedPointsReferenceRole);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetAndObserveSeedPointsNode(vtkMRMLNode* node)
{
  vtkNew<vtkIntArray> events;
  vtkMRMLVectorFieldDisplayNode::AddSamplingEvents(events);
  this->SetAndObserveNodeReferenceID(SeedPointsReferenceRole, node ? node->GetID() : nullptr, events);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::GetStreamlineSeedPositions(vtkPoints* seedPositions_RAS)
{
  if (!seedPositions_RAS)
  {
    return false;
  }
  seedPositions_RAS->Initialize();
  vtkMRMLNode* seedNode = this->GetSeedPointsNode();
  if (!seedNode)
  {
    return false;
  }

  // A curve is followed along its length, not only at the points that define it
  vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(seedNode);
  if (curveNode)
  {
    vtkPoints* curvePoints = curveNode->GetCurvePointsWorld();
    if (!curvePoints || curvePoints->GetNumberOfPoints() == 0)
    {
      return false;
    }
    // A curve can have thousands of points along it, which is more streamlines than anyone
    // wants; they are taken SeedSpacingMm apart.
    double lengthMm = curveNode->GetCurveLengthWorld();
    double spacingMm = this->GetSeedSpacingMm();
    if (spacingMm <= 0.0)
    {
      // Ten streamlines along the curve is a readable starting point
      spacingMm = (lengthMm > 0.0 ? lengthMm / 10.0 : 0.0);
    }
    int numberOfSeeds = 1;
    if (spacingMm > 0.0 && lengthMm > 0.0)
    {
      numberOfSeeds = std::max(1, static_cast<int>(lengthMm / spacingMm) + 1);
    }
    numberOfSeeds = std::min(numberOfSeeds, static_cast<int>(curvePoints->GetNumberOfPoints()));
    for (int seedIndex = 0; seedIndex < numberOfSeeds; ++seedIndex)
    {
      vtkIdType curvePointIndex = (numberOfSeeds > 1 ? //
                                     (curvePoints->GetNumberOfPoints() - 1) * seedIndex / (numberOfSeeds - 1)
                                                     : 0);
      seedPositions_RAS->InsertNextPoint(curvePoints->GetPoint(curvePointIndex));
    }
    return (seedPositions_RAS->GetNumberOfPoints() > 0);
  }

  // A plane is covered by a lattice over its extent, so that the streamlines start as a sheet
  vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(seedNode);
  if (planeNode)
  {
    double planeSizeMm[2] = { 0.0, 0.0 };
    planeNode->GetSize(planeSizeMm);
    vtkNew<vtkMatrix4x4> planeToWorld;
    planeNode->GetObjectToWorldMatrix(planeToWorld);
    double spacingMm = this->GetSeedSpacingMm();
    if (spacingMm <= 0.0)
    {
      // A ten by ten sheet of streamlines over the plane, whatever size it is
      spacingMm = std::max(std::max(planeSizeMm[0], planeSizeMm[1]) / 10.0, 1e-6);
    }
    int numberOfSteps[2] = { std::max(1, static_cast<int>(planeSizeMm[0] / spacingMm)), //
                             std::max(1, static_cast<int>(planeSizeMm[1] / spacingMm)) };
    for (int stepY = 0; stepY <= numberOfSteps[1]; ++stepY)
    {
      for (int stepX = 0; stepX <= numberOfSteps[0]; ++stepX)
      {
        double position_Plane[4] = { -0.5 * planeSizeMm[0] + stepX * planeSizeMm[0] / numberOfSteps[0],
                                     -0.5 * planeSizeMm[1] + stepY * planeSizeMm[1] / numberOfSteps[1],
                                     0.0,
                                     1.0 };
        double position_World[4] = { 0.0, 0.0, 0.0, 1.0 };
        planeToWorld->MultiplyPoint(position_Plane, position_World);
        seedPositions_RAS->InsertNextPoint(position_World);
      }
    }
    return (seedPositions_RAS->GetNumberOfPoints() > 0);
  }

  // A slice view: a lattice over what that slice shows, so the streamlines start from the
  // plane the user is looking at
  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(seedNode);
  if (sliceNode)
  {
    double fieldOfView[3] = { 0.0, 0.0, 0.0 };
    sliceNode->GetFieldOfView(fieldOfView);
    if (fieldOfView[0] <= 0.0 || fieldOfView[1] <= 0.0)
    {
      return false;
    }
    double spacingMm = this->GetSeedSpacingMm();
    if (spacingMm <= 0.0)
    {
      spacingMm = std::max(std::max(fieldOfView[0], fieldOfView[1]) / 10.0, 1e-6);
    }
    vtkMatrix4x4* sliceToRAS = sliceNode->GetSliceToRAS();
    int numberOfSteps[2] = { std::max(1, static_cast<int>(fieldOfView[0] / spacingMm)), //
                             std::max(1, static_cast<int>(fieldOfView[1] / spacingMm)) };
    for (int stepY = 0; stepY <= numberOfSteps[1]; ++stepY)
    {
      for (int stepX = 0; stepX <= numberOfSteps[0]; ++stepX)
      {
        double position_Slice[4] = { -0.5 * fieldOfView[0] + stepX * fieldOfView[0] / numberOfSteps[0],
                                     -0.5 * fieldOfView[1] + stepY * fieldOfView[1] / numberOfSteps[1],
                                     0.0,
                                     1.0 };
        double position_RAS[4] = { 0.0, 0.0, 0.0, 1.0 };
        sliceToRAS->MultiplyPoint(position_Slice, position_RAS);
        seedPositions_RAS->InsertNextPoint(position_RAS);
      }
    }
    return (seedPositions_RAS->GetNumberOfPoints() > 0);
  }

  // A point list or a model: the points it has
  vtkNew<vtkPoints> nodePositions_RAS;
  if (!vtkMRMLVectorFieldDisplayNode::GetNodePointPositions(seedNode, nodePositions_RAS))
  {
    return false;
  }
  seedPositions_RAS->DeepCopy(nodePositions_RAS);
  return (seedPositions_RAS->GetNumberOfPoints() > 0);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::GetSamplePositions(vtkPoints* samplePositions_RAS)
{
  return vtkMRMLVectorFieldDisplayNode::GetNodePointPositions(this->GetSamplePointsNode(), samplePositions_RAS);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::GetNodePointPositions(vtkMRMLNode* pointsNode, vtkPoints* positions_RAS)
{
  vtkPoints* samplePositions_RAS = positions_RAS;
  vtkMRMLNode* samplePointsNode = pointsNode;
  if (!samplePositions_RAS)
  {
    return false;
  }
  samplePositions_RAS->Initialize();
  if (!samplePointsNode)
  {
    return false;
  }

  // A markups node: its control points
  vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(samplePointsNode);
  if (markupsNode)
  {
    int numberOfControlPoints = markupsNode->GetNumberOfControlPoints();
    samplePositions_RAS->SetNumberOfPoints(numberOfControlPoints);
    for (int controlPointIndex = 0; controlPointIndex < numberOfControlPoints; ++controlPointIndex)
    {
      double position_World[3] = { 0.0, 0.0, 0.0 };
      markupsNode->GetNthControlPointPositionWorld(controlPointIndex, position_World);
      samplePositions_RAS->SetPoint(controlPointIndex, position_World);
    }
    return (numberOfControlPoints > 0);
  }

  // A model or any other node with a mesh: the points of that mesh, in world coordinates
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(samplePointsNode);
  if (modelNode)
  {
    vtkPointSet* mesh = modelNode->GetMesh();
    if (!mesh || !mesh->GetPoints())
    {
      return false;
    }
    vtkNew<vtkGeneralTransform> meshToWorld;
    meshToWorld->Identity();
    if (modelNode->GetParentTransformNode())
    {
      modelNode->GetParentTransformNode()->GetTransformToWorld(meshToWorld);
    }
    vtkPoints* meshPoints = mesh->GetPoints();
    vtkIdType numberOfPoints = meshPoints->GetNumberOfPoints();
    samplePositions_RAS->SetNumberOfPoints(numberOfPoints);
    for (vtkIdType pointIndex = 0; pointIndex < numberOfPoints; ++pointIndex)
    {
      double position_Mesh[3] = { 0.0, 0.0, 0.0 };
      double position_World[3] = { 0.0, 0.0, 0.0 };
      meshPoints->GetPoint(pointIndex, position_Mesh);
      meshToWorld->TransformPoint(position_Mesh, position_World);
      samplePositions_RAS->SetPoint(pointIndex, position_World);
    }
    return (numberOfPoints > 0);
  }

  // A volume: the center of each of its voxels. This is as many glyphs as there are voxels,
  // so it is only practical for a small volume.
  vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(samplePointsNode);
  if (volumeNode)
  {
    vtkImageData* image = volumeNode->GetImageData();
    if (!image)
    {
      return false;
    }
    vtkNew<vtkMatrix4x4> ijkToRAS;
    volumeNode->GetIJKToRASMatrix(ijkToRAS);
    vtkNew<vtkGeneralTransform> nodeToWorld;
    nodeToWorld->Identity();
    if (volumeNode->GetParentTransformNode())
    {
      volumeNode->GetParentTransformNode()->GetTransformToWorld(nodeToWorld);
    }
    int extent[6] = { 0, -1, 0, -1, 0, -1 };
    image->GetExtent(extent);
    for (int k = extent[4]; k <= extent[5]; ++k)
    {
      for (int j = extent[2]; j <= extent[3]; ++j)
      {
        for (int i = extent[0]; i <= extent[1]; ++i)
        {
          double position_IJK[4] = { static_cast<double>(i), static_cast<double>(j), static_cast<double>(k), 1.0 };
          double position_Node[4] = { 0.0, 0.0, 0.0, 1.0 };
          ijkToRAS->MultiplyPoint(position_IJK, position_Node);
          double position_World[3] = { 0.0, 0.0, 0.0 };
          nodeToWorld->TransformPoint(position_Node, position_World);
          samplePositions_RAS->InsertNextPoint(position_World);
        }
      }
    }
    return (samplePositions_RAS->GetNumberOfPoints() > 0);
  }

  return false;
}

//-----------------------------------------------------------
double vtkMRMLVectorFieldDisplayNode::GetGlyphSourceRadius()
{
  // The source geometry is one unit long, so a relative thickness is that fraction of it and
  // grows with the glyph, while an absolute one is a length in mm and only stays that
  // thickness because such a glyph is stretched along its axis alone.
  if (this->GlyphDiameterAbsolute)
  {
    return 0.5 * this->GlyphDiameterMm;
  }
  return 0.5 * 0.01 * this->GlyphDiameterPercent;
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::IsScaleDirectionalUsed()
{
  // A thickness in mm is only a thickness in mm if it is not scaled with the glyph
  return (this->GlyphDiameterAbsolute && this->ScaleDirectional                                                  //
          && (this->GlyphType == vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow    //
              || this->GlyphType == vtkMRMLVectorFieldDisplayNode::GlyphTypeCone  //
              || this->GlyphType == vtkMRMLVectorFieldDisplayNode::GlyphTypeCylinder));
}

//-----------------------------------------------------------
double vtkMRMLVectorFieldDisplayNode::GetEffectiveScaleFactor()
{
  if (this->VisualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid)
  {
    return this->GridScalePercent * 0.01;
  }
  return this->ScaleFactor;
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetEffectiveScaleFactor(double scaleFactor)
{
  if (this->VisualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid)
  {
    this->SetGridScalePercent(scaleFactor * 100.0);
    return;
  }
  this->SetScaleFactor(scaleFactor);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::IsScaleFactorUsed()
{
  return (this->VisualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph || //
          this->VisualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::IsThresholdUsed()
{
  // Contours are isosurfaces of the magnitude, so the levels already say which magnitudes
  // are drawn and a threshold on the same quantity would only be confusing.
  return (this->VisualizationMode != vtkMRMLVectorFieldDisplayNode::VisualizationModeContour);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::SetFieldArrayName(const char* arrayName)
{
  std::string previousName = (this->FieldArrayName ? this->FieldArrayName : "");
  std::string newName = (arrayName ? arrayName : "");
  if (previousName == newName)
  {
    return;
  }
  MRMLNodeModifyBlocker blocker(this);

  // The arrays that were following this one keep following it. An array that was set to
  // something else stays where the user put it.
  auto follows = [&previousName](const char* name)
  {
    std::string current = (name ? name : "");
    return (current.empty() || current == previousName);
  };
  bool orientationFollows = follows(this->OrientationArrayName);
  bool scaleFollows = follows(this->ScaleArrayName);
  bool colorFollows = follows(this->GetActiveScalarName());

  vtkSetStringBodyMacro(FieldArrayName, arrayName);

  if (orientationFollows)
  {
    this->SetOrientationArrayName(arrayName);
  }
  if (scaleFollows)
  {
    this->SetScaleArrayName(arrayName);
  }
  if (colorFollows)
  {
    // Through SetActiveScalar, not the string setter, because that is what tells the node
    // that the array the colors are read from has changed
    this->SetActiveScalar(arrayName, this->GetActiveAttributeLocation());
  }

  // A field that has just been chosen should be visible at a sensible size straight away
  double defaultScaleFactor = this->ComputeDefaultScaleFactor();
  if (defaultScaleFactor > 0.0)
  {
    this->SetScaleFactor(defaultScaleFactor);
  }
}

//-----------------------------------------------------------
double vtkMRMLVectorFieldDisplayNode::ComputeDefaultScaleFactor()
{
  vtkDataSet* dataSet = this->GetScalarDataSet();
  vtkDataArray* vectorArray = this->GetOrientationArray();
  if (!dataSet || !vectorArray || vectorArray->GetNumberOfComponents() < 2)
  {
    return 0.0;
  }
  double magnitudeRange[2] = { 0.0, -1.0 };
  vectorArray->GetRange(magnitudeRange, -1); // -1: L2 norm over all components
  if (magnitudeRange[1] <= 0.0)
  {
    return 0.0;
  }
  double bounds[6] = { 0.0, -1.0, 0.0, -1.0, 0.0, -1.0 };
  dataSet->GetBounds(bounds);
  double diagonal = sqrt((bounds[1] - bounds[0]) * (bounds[1] - bounds[0]) //
                  + (bounds[3] - bounds[2]) * (bounds[3] - bounds[2])
                  + (bounds[5] - bounds[4]) * (bounds[5] - bounds[4]));
  if (diagonal <= 0.0)
  {
    return 0.0;
  }
  // The longest vector reaches about a twentieth of the way across what is shown
  return 0.05 * diagonal / magnitudeRange[1];
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetEffectiveScaleArrayName()
{
  if (this->ScaleArrayName && this->ScaleArrayName[0] != '\0')
  {
    return this->ScaleArrayName;
  }
  return this->OrientationArrayName;
}

//-----------------------------------------------------------
vtkDataArray* vtkMRMLVectorFieldDisplayNode::GetEffectiveScaleArray()
{
  if (this->ScaleArrayName && this->ScaleArrayName[0] != '\0')
  {
    return this->GetScaleArray();
  }
  return this->GetOrientationArray();
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::GetEffectiveContourLevelsMm(std::vector<double>& levels)
{
  this->GetContourLevelsMm(levels);
  if (!levels.empty())
  {
    return;
  }
  // A transform is a displacement in mm and has levels that mean something on their own, but
  // the magnitudes of an arbitrary field are not known in advance, so the levels are spread
  // over the range that its colors are mapped across.
  double scalarRange[2] = { 0.0, -1.0 };
  this->GetScalarRange(scalarRange);
  if (scalarRange[1] <= scalarRange[0])
  {
    return;
  }
  const double levelPercentages[] = { 10.0, 25.0, 50.0, 75.0, 90.0 };
  for (double percentage : levelPercentages)
  {
    levels.push_back(scalarRange[0] + (scalarRange[1] - scalarRange[0]) * percentage / 100.0);
  }
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::IsFieldSampled()
{
  vtkAlgorithmOutput* fieldConnection = this->GetFieldConnection();
  vtkAlgorithm* producer = (fieldConnection ? fieldConnection->GetProducer() : nullptr);
  return (vtkMRMLVectorFieldSampler::SafeDownCast(producer) != nullptr);
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetRenderedOrientationArrayName()
{
  if (this->IsFieldSampled())
  {
    return vtkMRMLVectorFieldSampler::GetVectorArrayName();
  }
  return this->OrientationArrayName;
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetRenderedScaleArrayName()
{
  if (this->IsFieldSampled())
  {
    // A sampled field carries the vectors and their magnitude, and nothing else, so the
    // glyphs are scaled by the vectors themselves
    return vtkMRMLVectorFieldSampler::GetVectorArrayName();
  }
  return this->GetEffectiveScaleArrayName();
}

//-----------------------------------------------------------
const char* vtkMRMLVectorFieldDisplayNode::GetRenderedActiveScalarName()
{
  if (this->IsFieldSampled())
  {
    return vtkMRMLVectorFieldSampler::GetMagnitudeArrayName();
  }
  return this->GetActiveScalarName();
}

//-----------------------------------------------------------
vtkDataSet* vtkMRMLVectorFieldDisplayNode::GetSampledFieldDataSet()
{
  vtkAlgorithmOutput* fieldConnection = this->GetFieldConnection();
  vtkAlgorithm* producer = (fieldConnection ? fieldConnection->GetProducer() : nullptr);
  if (!producer)
  {
    return nullptr;
  }
  // Without executing the pipeline: what a render has already produced, or nothing
  return vtkDataSet::SafeDownCast(producer->GetOutputDataObject(fieldConnection->GetIndex()));
}

//-----------------------------------------------------------
vtkDataArray* vtkMRMLVectorFieldDisplayNode::GetRenderedActiveScalarArray()
{
  const char* arrayName = this->GetRenderedActiveScalarName();
  if (!this->IsFieldSampled())
  {
    return this->GetActiveScalarArray();
  }
  vtkDataSet* sampledField = this->GetSampledFieldDataSet();
  vtkPointData* pointData = (sampledField ? sampledField->GetPointData() : nullptr);
  return (pointData && arrayName ? pointData->GetArray(arrayName) : nullptr);
}

//-----------------------------------------------------------
vtkDataArray* vtkMRMLVectorFieldDisplayNode::GetRenderedScaleArray()
{
  const char* arrayName = this->GetRenderedScaleArrayName();
  if (!this->IsFieldSampled())
  {
    return this->GetEffectiveScaleArray();
  }
  vtkDataSet* sampledField = this->GetSampledFieldDataSet();
  vtkPointData* pointData = (sampledField ? sampledField->GetPointData() : nullptr);
  return (pointData && arrayName ? pointData->GetArray(arrayName) : nullptr);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::IsMaskingModeSupported(int maskingMode)
{
  // A mesh has points of its own to place glyphs at, and a field that can be interpolated
  // anywhere can also place them on a lattice or at the points of another node. A mesh whose
  // cells enclose a volume is both.
  bool hasOwnPoints = (vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode()) != nullptr);
  if (maskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeFixedSpacing || //
      maskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeNodePoints)
  {
    return this->CanSampleAtArbitraryPositions();
  }
  return (hasOwnPoints && maskingMode >= 0 && maskingMode < vtkMRMLVectorFieldDisplayNode::MaskingModeFixedSpacing);
}

//-----------------------------------------------------------
int vtkMRMLVectorFieldDisplayNode::GetEffectiveMaskingMode()
{
  if (this->IsMaskingModeSupported(this->MaskingMode))
  {
    return this->MaskingMode;
  }
  // A mesh shows its own points by default, even when it could also be sampled on a lattice
  if (vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode()))
  {
    return vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds;
  }
  return vtkMRMLVectorFieldDisplayNode::MaskingModeFixedSpacing;
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::IsVisualizationModeSupported(int visualizationMode)
{
  if (visualizationMode < 0 || visualizationMode >= vtkMRMLVectorFieldDisplayNode::VisualizationMode_Last)
  {
    return false;
  }
  if (visualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph)
  {
    return true;
  }
  // A deformed grid, an isosurface of the magnitude and a streamline are all built from a
  // field sampled through a volume, which the point data of a mesh cannot give: its vectors
  // exist at its own points and nowhere else.
  return this->CanSampleAtArbitraryPositions();
}

//-----------------------------------------------------------
int vtkMRMLVectorFieldDisplayNode::GetGridSubdivision()
{
  if (this->VisualizationMode != vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid || this->GridSpacingMm <= 0.0)
  {
    return 1;
  }
  double requestedSpacingMm = this->GetEffectiveSamplingSpacingMm();
  if (requestedSpacingMm <= 0.0)
  {
    // No sampling spacing requested: sample at the grid lines themselves
    return 1;
  }
  // Round up, so that the lattice is never coarser than what was asked for
  return std::max(1, static_cast<int>(std::ceil(this->GridSpacingMm / requestedSpacingMm - 1e-6)));
}

//-----------------------------------------------------------
double vtkMRMLVectorFieldDisplayNode::GetSamplingSpacingForFieldMm()
{
  if (this->VisualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid && this->GridSpacingMm > 0.0)
  {
    return this->GridSpacingMm / this->GetGridSubdivision();
  }
  return this->GetEffectiveSamplingSpacingMm();
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::UpdateSamplerRegion(vtkMRMLVectorFieldSampler* sampler)
{
  if (!sampler)
  {
    return;
  }
  sampler->SetSamplingSpacingMm(this->GetSamplingSpacingForFieldMm());
  sampler->SetLatticeGroupSize(this->GetGridSubdivision());

  // Glyphs at the points of another node, instead of on a lattice
  vtkNew<vtkPoints> samplePositions_RAS;
  if (this->GetEffectiveMaskingMode() == vtkMRMLVectorFieldDisplayNode::MaskingModeNodePoints //
      && this->GetSamplePositions(samplePositions_RAS))
  {
    sampler->SetSamplePositions(samplePositions_RAS);
  }
  else
  {
    sampler->SetSamplePositions(nullptr);
  }

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
vtkMRMLVectorFieldSampler* vtkMRMLVectorFieldDisplayNode::UpdateMeshFieldSampler(vtkMRMLModelNode* modelNode, vtkMRMLVectorFieldSampler* sampler)
{
  if (!modelNode)
  {
    return nullptr;
  }
  vtkMRMLMeshFieldSampler* meshSampler = vtkMRMLMeshFieldSampler::SafeDownCast(sampler);
  if (!meshSampler)
  {
    vtkNew<vtkMRMLMeshFieldSampler> newSampler;
    this->FieldSampler = newSampler.GetPointer();
    meshSampler = newSampler.GetPointer();
  }
  meshSampler->SetInputConnection(modelNode->GetMeshConnection());
  // The array the user chose to read the field from is the one that is interpolated
  meshSampler->SetMeshVectorArrayName(this->GetOrientationArrayName());
  this->UpdateSamplerRegion(meshSampler);
  return meshSampler;
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
  // Model point data. A mesh whose cells enclose a volume describes a field that is defined
  // between its points as well, so it is sampled the way the other sources are when
  // something other than its own points is being drawn; otherwise the mesh is already a
  // pipeline output and can be glyphed as it is.
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode());
  if (modelNode)
  {
    if (this->HasVolumetricMesh() && (this->NeedsLatticeSampling() || this->NeedsSamplingOffMeshPoints()))
    {
      vtkMRMLVectorFieldSampler* sampler = this->UpdateMeshFieldSampler(modelNode, this->FieldSampler);
      if (sampler)
      {
        return sampler->GetOutputPort();
      }
    }
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
bool vtkMRMLVectorFieldDisplayNode::HasVolumetricMesh()
{
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode());
  vtkUnstructuredGrid* grid = (modelNode ? vtkUnstructuredGrid::SafeDownCast(modelNode->GetMesh()) : nullptr);
  if (!grid || grid->GetNumberOfCells() == 0)
  {
    return false;
  }
  vtkNew<vtkCellTypes> cellTypes;
  grid->GetCellTypes(cellTypes);
  for (vtkIdType typeIndex = 0; typeIndex < cellTypes->GetNumberOfTypes(); ++typeIndex)
  {
    if (vtkCellTypes::GetDimension(cellTypes->GetCellType(typeIndex)) == 3)
    {
      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::NeedsSamplingOffMeshPoints()
{
  if (!vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode()))
  {
    // A field that is not carried by a mesh has no points of its own to draw on
    return true;
  }
  // The uniform masking modes choose a spatially even subset of the points that are already
  // there, so they are applied to the points of the mesh. Sampling a lattice over the
  // bounding box instead would put most of the glyphs outside the mesh, where the field has
  // no value: for a thin shell in a wide box only a few percent of a lattice is inside it,
  // which is why the glyphs then appear only in patches instead of evenly.
  int maskingMode = this->GetEffectiveMaskingMode();
  return (maskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeFixedSpacing //
          || maskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeNodePoints);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::NeedsLatticeSampling()
{
  if (!vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode()))
  {
    return true;
  }
  // A deformed grid is a lattice by definition, so it has to be sampled on one. Isosurfaces
  // and streamlines are not: the cells of the mesh already say how its points connect, and
  // both filters work on them directly. Using them is not only cheaper than resampling, it
  // is the only thing that works for a mesh that is thin - a shell about as thick as one of
  // its own cells has almost no lattice point strictly inside it, so a lattice yields no
  // isosurface at any resolution, while the cells of the mesh give an exact one.
  return (this->VisualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid);
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::CanSampleAtArbitraryPositions()
{
  // A vector volume can be interpolated anywhere, and so can a mesh whose cells enclose a
  // volume. A surface mesh only has values at its own points.
  vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(this->GetDisplayableNode());
  if (volumeNode && volumeNode->GetImageData() && volumeNode->GetImageData()->GetNumberOfScalarComponents() >= 3)
  {
    return true;
  }
  return this->HasVolumetricMesh();
}

//-----------------------------------------------------------
vtkSmartPointer<vtkMRMLVectorFieldSampler> vtkMRMLVectorFieldDisplayNode::CreateSliceFieldSampler()
{
  return this->UpdateSliceFieldSampler(nullptr);
}

//-----------------------------------------------------------
vtkSmartPointer<vtkMRMLVectorFieldSampler> vtkMRMLVectorFieldDisplayNode::UpdateSliceFieldSampler(vtkMRMLVectorFieldSampler* sampler)
{
  bool glyphsOnMeshPoints = (this->VisualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph //
                             && !this->NeedsSamplingOffMeshPoints());
  if (!this->CanSampleAtArbitraryPositions() || glyphsOnMeshPoints)
  {
    // Slice views select the points that are near the slice plane instead. That is also what
    // happens when the glyphs belong on the points of the mesh, so that a slice view draws
    // the same field from the same places as the 3D view does.
    return nullptr;
  }
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode());
  if (modelNode)
  {
    // An existing sampler is reconfigured rather than replaced, so that the cell locator it
    // has built for the mesh is not thrown away on every update.
    vtkSmartPointer<vtkMRMLVectorFieldSampler> meshSliceSampler = vtkMRMLMeshFieldSampler::SafeDownCast(sampler);
    if (!meshSliceSampler)
    {
      meshSliceSampler = vtkSmartPointer<vtkMRMLMeshFieldSampler>::New();
    }
    this->UpdateMeshFieldSampler(modelNode, meshSliceSampler);
    return meshSliceSampler;
  }
  vtkMRMLVolumeNode* volumeNode = vtkMRMLVolumeNode::SafeDownCast(this->GetDisplayableNode());
  if (!volumeNode)
  {
    return nullptr;
  }
  vtkSmartPointer<vtkMRMLVectorFieldSampler> sliceSampler = vtkMRMLImageFieldSampler::SafeDownCast(sampler);
  if (!sliceSampler)
  {
    sliceSampler = vtkSmartPointer<vtkMRMLImageFieldSampler>::New();
  }
  this->UpdateVolumeFieldSampler(volumeNode, sliceSampler);
  return sliceSampler;
}

//-----------------------------------------------------------
bool vtkMRMLVectorFieldDisplayNode::HasFixedFieldArrays()
{
  // A sampled image gives the same two arrays every time, so there is nothing to choose
  // between; the point data of a mesh can hold anything, including several vector arrays,
  // and that is exactly where the user has to say which one is the field.
  return (vtkMRMLVolumeNode::SafeDownCast(this->GetDisplayableNode()) != nullptr);
}

//-----------------------------------------------------------
void vtkMRMLVectorFieldDisplayNode::GetFieldArrayInfo(std::vector<std::string>& arrayNames, std::vector<int>& numberOfComponents)
{
  arrayNames.clear();
  numberOfComponents.clear();

  // A sampled field always produces the same two arrays. They are reported without
  // executing the sampler, so that showing the GUI does not sample the field. A mesh is
  // read through a sampler too where its cells enclose a volume, but the arrays to choose
  // between are still the ones the mesh carries.
  vtkAlgorithmOutput* fieldConnection = this->GetFieldConnection();
  vtkAlgorithm* producer = fieldConnection ? fieldConnection->GetProducer() : nullptr;
  bool readsFromMesh = (vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode()) != nullptr);
  if (!readsFromMesh && vtkMRMLVectorFieldSampler::SafeDownCast(producer))
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
  // The arrays of a mesh are the mesh's own, and are there whether or not the field has been
  // sampled anywhere: a volumetric mesh is read through a sampler, but what the arrays hold
  // and what range they span is a question about the mesh.
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(this->GetDisplayableNode());
  if (modelNode)
  {
    return modelNode->GetMesh();
  }
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
