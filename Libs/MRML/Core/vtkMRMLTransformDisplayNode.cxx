/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Andras Lasso and Franklin King at
  PerkLab, Queen's University and was supported through the Applied Cancer
  Research Unit program of Cancer Care Ontario with funds provided by the
  Ontario Ministry of Health and Long-Term Care.

==============================================================================*/

// MRML includes
#include "vtkMRMLTransformDisplayNode.h"
#include "vtkMRMLColorNode.h"
#include "vtkMRMLProceduralColorNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLTransformFieldSampler.h"
#include "vtkMRMLTransformInteractionDisplayNode.h"
#include "vtkMRMLTransformNode.h"

// VTK includes
#include <vtkAlgorithmOutput.h>
#include <vtkColorTransferFunction.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <sstream>

namespace
{
const char* DEFAULT_COLOR_TABLE_NAME = "Displacement to color";
const char CONTOUR_LEVEL_SEPARATOR = ' ';
} // namespace

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLTransformDisplayNode);

//----------------------------------------------------------------------------
vtkMRMLTransformDisplayNode::vtkMRMLTransformDisplayNode()
  : GlyphSpacingMm(10.0)
  , GridResolutionMm(5.0)
  , ContourResolutionMm(5.0)
{
  this->SetVisualizationMode(vtkMRMLTransformDisplayNode::VIS_MODE_GLYPH);
  this->SetScalarVisibility(1);
  this->SetActiveScalarName(vtkMRMLVectorFieldSampler::GetMagnitudeArrayName());
  this->SetOrientationArrayName(vtkMRMLVectorFieldSampler::GetVectorArrayName());
  this->SetScaleArrayName(vtkMRMLVectorFieldSampler::GetVectorArrayName());

  this->Visibility = 0;
  this->Visibility2D = 0;
  this->Visibility3D = 1;

  // Arrows keep their thickness however long they are, which is what makes a field of
  // displacement vectors readable.
  this->SetScaleDirectional(true);
  this->SetGlyphDiameterMm(5.0);
  this->SetScaleFactor(1.0); // 100%
  this->SetGlyphTipLengthPercent(30.0);
  this->SetGlyphShaftDiameterPercent(40.0);
  this->SetGlyphResolution(6);
  this->SetGlyphResolution2D(12);
  this->SetGlyphTipLengthPercent2D(30.0);

  this->SetThresholdEnabled(true);
  this->SetThresholdRange(0.01, 100.0);

  this->SetGridSpacingMm(15.0);
  this->SetGridScalePercent(100.0);
  this->SetGridLineDiameterMm(1.0);
  this->SetGridShowNonWarped(false);

  this->SetContourOpacity(0.8);
  std::vector<double> contourLevels;
  for (int i = 2; i <= 18; i += 2)
  {
    contourLevels.push_back(i);
  }
  this->SetContourLevelsMm(contourLevels.data(), static_cast<int>(contourLevels.size()));

  this->TypeDisplayName = vtkMRMLTr("vtkMRMLTransformDisplayNode", "Transform Display");
}

//----------------------------------------------------------------------------
vtkMRMLTransformDisplayNode::~vtkMRMLTransformDisplayNode() = default;

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  vtkMRMLPrintBeginMacro(os, indent);
  vtkMRMLPrintFloatMacro(GlyphSpacingMm);
  vtkMRMLPrintFloatMacro(GridResolutionMm);
  vtkMRMLPrintFloatMacro(ContourResolutionMm);
  vtkMRMLPrintEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);

  // The attribute names are the ones that transform display used before it was built on the
  // shared vector field display node, so that scenes stay readable by both.
  of << " VisualizationMode=\"" << ConvertVisualizationModeToString(this->GetVisualizationMode()) << "\"";
  of << " GlyphSpacingMm=\"" << this->GlyphSpacingMm << "\"";
  of << " GlyphScalePercent=\"" << this->GetGlyphScalePercent() << "\"";
  of << " GlyphDisplayRangeMaxMm=\"" << this->GetGlyphDisplayRangeMaxMm() << "\"";
  of << " GlyphDisplayRangeMinMm=\"" << this->GetGlyphDisplayRangeMinMm() << "\"";
  of << " GlyphType=\"" << ConvertGlyphTypeToString(this->GetGlyphType()) << "\"";
  of << " GridResolutionMm=\"" << this->GridResolutionMm << "\"";
  of << " ContourResolutionMm=\"" << this->ContourResolutionMm << "\"";
}

#define READ_FROM_ATT(varName)           \
  if (!strcmp(xmlReadAttName, #varName)) \
  {                                      \
    std::stringstream ss;                \
    ss << xmlReadAttValue;               \
    ss >> this->varName;                 \
    continue;                            \
  }

#define READ_FROM_ATT_INTO(attributeName, setter)  \
  if (!strcmp(xmlReadAttName, #attributeName))     \
  {                                                \
    double attributeValue = 0.0;                   \
    std::stringstream ss;                          \
    ss << xmlReadAttValue;                         \
    ss >> attributeValue;                          \
    this->setter(attributeValue);                  \
    continue;                                      \
  }

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::ReadXMLAttributes(const char** atts)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::ReadXMLAttributes(atts);

  vtkMRMLReadXMLBeginMacro(atts);

  if (!strcmp(xmlReadAttName, "VisualizationMode"))
  {
    int mode = ConvertVisualizationModeFromString(xmlReadAttValue);
    if (mode >= 0)
    {
      this->SetVisualizationMode(mode);
    }
    continue;
  }
  if (!strcmp(xmlReadAttName, "GlyphType"))
  {
    int glyphType = ConvertGlyphTypeFromString(xmlReadAttValue);
    if (glyphType >= 0)
    {
      this->SetGlyphType(glyphType);
    }
    continue;
  }
  READ_FROM_ATT(GlyphSpacingMm);
  READ_FROM_ATT(GridResolutionMm);
  READ_FROM_ATT(ContourResolutionMm);
  READ_FROM_ATT_INTO(GlyphScalePercent, SetGlyphScalePercent);
  READ_FROM_ATT_INTO(GlyphDisplayRangeMaxMm, SetGlyphDisplayRangeMaxMm);
  READ_FROM_ATT_INTO(GlyphDisplayRangeMinMm, SetGlyphDisplayRangeMinMm);
  // Properties that moved to the shared node but kept their old attribute name
  READ_FROM_ATT_INTO(GlyphTipLengthPercent, SetGlyphTipLengthPercent);
  READ_FROM_ATT_INTO(GlyphDiameterMm, SetGlyphDiameterMm);
  READ_FROM_ATT_INTO(GlyphShaftDiameterPercent, SetGlyphShaftDiameterPercent);
  READ_FROM_ATT_INTO(GlyphResolution, SetGlyphResolution);
  READ_FROM_ATT_INTO(GlyphResolution2D, SetGlyphResolution2D);
  READ_FROM_ATT_INTO(GlyphTipLengthPercent2D, SetGlyphTipLengthPercent2D);
  READ_FROM_ATT_INTO(GridScalePercent, SetGridScalePercent);
  READ_FROM_ATT_INTO(GridSpacingMm, SetGridSpacingMm);
  READ_FROM_ATT_INTO(GridLineDiameterMm, SetGridLineDiameterMm);
  READ_FROM_ATT_INTO(GridShowNonWarped, SetGridShowNonWarped);
  READ_FROM_ATT_INTO(ContourOpacity, SetContourOpacity);
  if (!strcmp(xmlReadAttName, "ContourLevelsMm"))
  {
    this->SetContourLevelsMmFromString(xmlReadAttValue);
    continue;
  }
  // Editor properties of scenes that were saved before they moved into their own node
  READ_FROM_ATT_INTO(EditorVisibility, SetEditorVisibility);
  READ_FROM_ATT_INTO(EditorVisibility3D, SetEditorVisibility3D);
  READ_FROM_ATT_INTO(EditorSliceIntersectionVisibility, SetEditorSliceIntersectionVisibility);
  READ_FROM_ATT_INTO(EditorTranslationEnabled, SetEditorTranslationEnabled);
  READ_FROM_ATT_INTO(EditorRotationEnabled, SetEditorRotationEnabled);
  READ_FROM_ATT_INTO(EditorScalingEnabled, SetEditorScalingEnabled);
  READ_FROM_ATT_INTO(EditorTranslationSliceAnywhereEnabled, SetEditorTranslationSliceAnywhereEnabled);
  READ_FROM_ATT_INTO(EditorTranslationSliceAnywhereSensitivity, SetEditorTranslationSliceAnywhereSensitivity);
  READ_FROM_ATT_INTO(InteractionSizeAbsolute, SetInteractionSizeAbsolute);
  READ_FROM_ATT_INTO(InteractionSizeMm, SetInteractionSizeMm);
  READ_FROM_ATT_INTO(InteractionScalePercent, SetInteractionScalePercent);
  READ_FROM_ATT_INTO(InteractionHandleOpacity, SetInteractionHandleOpacity);

  vtkMRMLReadXMLEndMacro();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::CopyContent(vtkMRMLNode* anode, bool deepCopy /*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  Superclass::CopyContent(anode, deepCopy);
  vtkMRMLTransformDisplayNode* node = vtkMRMLTransformDisplayNode::SafeDownCast(anode);
  if (!node)
  {
    return;
  }

  vtkMRMLCopyBeginMacro(anode);
  vtkMRMLCopyFloatMacro(GlyphSpacingMm);
  vtkMRMLCopyFloatMacro(GridResolutionMm);
  vtkMRMLCopyFloatMacro(ContourResolutionMm);
  vtkMRMLCopyEndMacro();
}

//----------------------------------------------------------------------------
// Vector field source
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
bool vtkMRMLTransformDisplayNode::CanSampleAtArbitraryPositions()
{
  return vtkMRMLTransformNode::SafeDownCast(this->GetDisplayableNode()) != nullptr;
}

//----------------------------------------------------------------------------
double vtkMRMLTransformDisplayNode::GetEffectiveSamplingSpacingMm()
{
  switch (this->GetVisualizationMode())
  {
    case vtkMRMLTransformDisplayNode::VIS_MODE_GRID: return this->GridResolutionMm;
    case vtkMRMLTransformDisplayNode::VIS_MODE_CONTOUR: return this->ContourResolutionMm;
    case vtkMRMLTransformDisplayNode::VIS_MODE_GLYPH:
    default: return this->GlyphSpacingMm;
  }
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetEffectiveSamplingSpacingMm(double spacingMm)
{
  switch (this->GetVisualizationMode())
  {
    case vtkMRMLTransformDisplayNode::VIS_MODE_GRID: this->SetGridResolutionMm(spacingMm); break;
    case vtkMRMLTransformDisplayNode::VIS_MODE_CONTOUR: this->SetContourResolutionMm(spacingMm); break;
    case vtkMRMLTransformDisplayNode::VIS_MODE_GLYPH:
    default: this->SetGlyphSpacingMm(spacingMm); break;
  }
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::UpdateScalarRange()
{
  // Transform display uses the colors exactly as the color map defines them: the map goes
  // from a displacement in mm to a color, so its range is the scalar range.
  vtkMRMLColorNode* colorNode = this->GetColorNode();
  if (colorNode && colorNode->GetLookupTable())
  {
    double* range = colorNode->GetLookupTable()->GetRange();
    if (range && range[1] >= range[0])
    {
      this->SetScalarRange(range[0], range[1]);
      return;
    }
  }
  this->Superclass::UpdateScalarRange();
}

//----------------------------------------------------------------------------
vtkMRMLVectorFieldSampler* vtkMRMLTransformDisplayNode::UpdateTransformFieldSampler(vtkMRMLVectorFieldSampler* sampler)
{
  vtkMRMLTransformFieldSampler* transformSampler = vtkMRMLTransformFieldSampler::SafeDownCast(sampler);
  if (!transformSampler)
  {
    vtkNew<vtkMRMLTransformFieldSampler> newSampler;
    this->FieldSampler = newSampler.GetPointer();
    transformSampler = newSampler.GetPointer();
  }
  transformSampler->SetTransformNode(vtkMRMLTransformNode::SafeDownCast(this->GetDisplayableNode()));
  this->UpdateSamplerRegion(transformSampler);
  return transformSampler;
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLTransformDisplayNode::GetFieldConnection()
{
  if (!this->CanSampleAtArbitraryPositions())
  {
    return nullptr;
  }
  // A transform is defined everywhere, so there is nothing to draw until the user says
  // which region to draw it in.
  if (!this->GetRegionNode() && !this->GetSamplePointsNode())
  {
    return nullptr;
  }
  vtkMRMLVectorFieldSampler* sampler = this->UpdateTransformFieldSampler(this->FieldSampler);
  return sampler ? sampler->GetOutputPort() : nullptr;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkMRMLVectorFieldSampler> vtkMRMLTransformDisplayNode::CreateSliceFieldSampler()
{
  if (!this->CanSampleAtArbitraryPositions())
  {
    return nullptr;
  }
  vtkSmartPointer<vtkMRMLTransformFieldSampler> sliceSampler = vtkSmartPointer<vtkMRMLTransformFieldSampler>::New();
  sliceSampler->SetTransformNode(vtkMRMLTransformNode::SafeDownCast(this->GetDisplayableNode()));
  this->UpdateSamplerRegion(sliceSampler);
  return sliceSampler;
}

//----------------------------------------------------------------------------
// Display options
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
vtkMRMLNode* vtkMRMLTransformDisplayNode::GetRegionNode()
{
  return this->Superclass::GetRegionNode();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetAndObserveRegionNode(vtkMRMLNode* node)
{
  this->Superclass::SetAndObserveRegionNode(node);
}

//----------------------------------------------------------------------------
vtkMRMLNode* vtkMRMLTransformDisplayNode::GetGlyphPointsNode()
{
  return this->GetSamplePointsNode();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetAndObserveGlyphPointsNode(vtkMRMLNode* node)
{
  this->SetAndObserveSamplePointsNode(node);
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetGlyphScalePercent(double scalePercent)
{
  this->SetScaleFactor(scalePercent * 0.01);
}

//----------------------------------------------------------------------------
double vtkMRMLTransformDisplayNode::GetGlyphScalePercent()
{
  return this->GetScaleFactor() * 100.0;
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetGlyphDisplayRangeMinMm(double minMm)
{
  double range[2] = { 0.0, 0.0 };
  this->GetThresholdRange(range);
  this->SetThresholdRange(minMm, range[1]);
  this->SetThresholdEnabled(true);
}

//----------------------------------------------------------------------------
double vtkMRMLTransformDisplayNode::GetGlyphDisplayRangeMinMm()
{
  return this->GetThresholdMin();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetGlyphDisplayRangeMaxMm(double maxMm)
{
  double range[2] = { 0.0, 0.0 };
  this->GetThresholdRange(range);
  this->SetThresholdRange(range[0], maxMm);
  this->SetThresholdEnabled(true);
}

//----------------------------------------------------------------------------
double vtkMRMLTransformDisplayNode::GetGlyphDisplayRangeMaxMm()
{
  return this->GetThresholdMax();
}

//----------------------------------------------------------------------------
const char* vtkMRMLTransformDisplayNode::ConvertVisualizationModeToString(int modeIndex)
{
  switch (modeIndex)
  {
    case VIS_MODE_GLYPH: return "GLYPH";
    case VIS_MODE_GRID: return "GRID";
    case VIS_MODE_CONTOUR: return "CONTOUR";
    default: return "";
  }
}

//----------------------------------------------------------------------------
int vtkMRMLTransformDisplayNode::ConvertVisualizationModeFromString(const char* modeString)
{
  if (!modeString)
  {
    return -1;
  }
  for (int modeIndex = 0; modeIndex < VIS_MODE_LAST; ++modeIndex)
  {
    if (strcmp(modeString, ConvertVisualizationModeToString(modeIndex)) == 0)
    {
      return modeIndex;
    }
  }
  return -1;
}

//----------------------------------------------------------------------------
const char* vtkMRMLTransformDisplayNode::ConvertGlyphTypeToString(int typeIndex)
{
  // The shared node knows more glyph types than transform display offered; the ones it
  // offered keep their old names so that saved scenes keep working.
  switch (typeIndex)
  {
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow: return "ARROW";
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeCone: return "CONE";
    case vtkMRMLVectorFieldDisplayNode::GlyphTypeSphere: return "SPHERE";
    default: return vtkMRMLVectorFieldDisplayNode::GetGlyphTypeAsString(typeIndex);
  }
}

//----------------------------------------------------------------------------
int vtkMRMLTransformDisplayNode::ConvertGlyphTypeFromString(const char* typeString)
{
  if (!typeString)
  {
    return -1;
  }
  if (strcmp(typeString, "ARROW") == 0)
  {
    return vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow;
  }
  if (strcmp(typeString, "CONE") == 0)
  {
    return vtkMRMLVectorFieldDisplayNode::GlyphTypeCone;
  }
  if (strcmp(typeString, "SPHERE") == 0)
  {
    return vtkMRMLVectorFieldDisplayNode::GlyphTypeSphere;
  }
  return vtkMRMLVectorFieldDisplayNode::GetGlyphTypeFromString(typeString);
}

//----------------------------------------------------------------------------
// Contour levels
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
unsigned int vtkMRMLTransformDisplayNode::GetNumberOfContourLevels()
{
  return this->Superclass::GetNumberOfContourLevels();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetContourLevelsMm(double* levels, int size)
{
  this->Superclass::SetContourLevelsMm(levels, size);
}

//----------------------------------------------------------------------------
double* vtkMRMLTransformDisplayNode::GetContourLevelsMm()
{
  return this->Superclass::GetContourLevelsMm();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::GetContourLevelsMm(std::vector<double>& levels)
{
  this->Superclass::GetContourLevelsMm(levels);
}

//----------------------------------------------------------------------------
std::string vtkMRMLTransformDisplayNode::GetContourLevelsMmAsString()
{
  return this->Superclass::GetContourLevelsMmAsString();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetContourLevelsMmFromString(const char* str)
{
  this->Superclass::SetContourLevelsMmFromString(str);
}

//----------------------------------------------------------------------------
std::vector<double> vtkMRMLTransformDisplayNode::ConvertContourLevelsFromString(const char* str)
{
  return StringToDoubleVector(str);
}

//----------------------------------------------------------------------------
std::string vtkMRMLTransformDisplayNode::ConvertContourLevelsToString(const std::vector<double>& levels)
{
  return DoubleVectorToString(levels.data(), static_cast<int>(levels.size()));
}

//----------------------------------------------------------------------------
bool vtkMRMLTransformDisplayNode::IsContourLevelEqual(const std::vector<double>& levels1, const std::vector<double>& levels2)
{
  if (levels1.size() != levels2.size())
  {
    return false;
  }
  const double COMPARISON_TOLERANCE = 0.01;
  for (unsigned int i = 0; i < levels1.size(); i++)
  {
    if (fabs(levels1[i] - levels2[i]) > COMPARISON_TOLERANCE)
    {
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
std::vector<double> vtkMRMLTransformDisplayNode::StringToDoubleVector(const char* sourceStr)
{
  std::vector<double> values;
  if (!sourceStr)
  {
    return values;
  }
  std::stringstream ss(sourceStr);
  std::string itemString;
  while (std::getline(ss, itemString, CONTOUR_LEVEL_SEPARATOR))
  {
    if (itemString.empty())
    {
      continue;
    }
    std::stringstream itemStream(itemString);
    double value = 0.0;
    itemStream >> value;
    if (!itemStream.fail())
    {
      values.push_back(value);
    }
  }
  return values;
}

//----------------------------------------------------------------------------
std::string vtkMRMLTransformDisplayNode::DoubleVectorToString(const double* values, int numberOfValues)
{
  std::stringstream ss;
  for (int i = 0; i < numberOfValues; i++)
  {
    if (i > 0)
    {
      ss << CONTOUR_LEVEL_SEPARATOR;
    }
    ss << values[i];
  }
  return ss.str();
}

//----------------------------------------------------------------------------
// Interaction parameters, forwarded to the interaction display node
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
vtkMRMLTransformInteractionDisplayNode* vtkMRMLTransformDisplayNode::GetInteractionDisplayNode(bool createIfMissing /*=false*/)
{
  vtkMRMLDisplayableNode* displayableNode = this->GetDisplayableNode();
  if (!displayableNode)
  {
    return nullptr;
  }
  int numberOfDisplayNodes = displayableNode->GetNumberOfDisplayNodes();
  for (int i = 0; i < numberOfDisplayNodes; ++i)
  {
    vtkMRMLTransformInteractionDisplayNode* interactionDisplayNode =
      vtkMRMLTransformInteractionDisplayNode::SafeDownCast(displayableNode->GetNthDisplayNode(i));
    if (interactionDisplayNode)
    {
      return interactionDisplayNode;
    }
  }
  if (!createIfMissing || !this->GetScene())
  {
    return nullptr;
  }
  vtkMRMLTransformInteractionDisplayNode* interactionDisplayNode =
    vtkMRMLTransformInteractionDisplayNode::SafeDownCast(this->GetScene()->AddNewNodeByClass("vtkMRMLTransformInteractionDisplayNode"));
  if (interactionDisplayNode)
  {
    displayableNode->AddAndObserveDisplayNodeID(interactionDisplayNode->GetID());
  }
  return interactionDisplayNode;
}

/// Forwards a property to the interaction display node. The node is only created when a
/// property is set, so that a transform that is never edited does not get one.
#define INTERACTION_PROPERTY_FORWARDER(type, name, defaultValue)                                       \
  type vtkMRMLTransformDisplayNode::Get##name()                                                        \
  {                                                                                                    \
    vtkMRMLTransformInteractionDisplayNode* interactionDisplayNode = this->GetInteractionDisplayNode(); \
    return interactionDisplayNode ? interactionDisplayNode->Get##name() : defaultValue;                 \
  }                                                                                                    \
  void vtkMRMLTransformDisplayNode::Set##name(type value)                                              \
  {                                                                                                    \
    vtkMRMLTransformInteractionDisplayNode* interactionDisplayNode = this->GetInteractionDisplayNode(true); \
    if (interactionDisplayNode)                                                                        \
    {                                                                                                  \
      interactionDisplayNode->Set##name(value);                                                        \
    }                                                                                                  \
    /* Observers of this node expect to hear about the change, wherever it is stored now */            \
    this->Modified();                                                                                  \
  }

INTERACTION_PROPERTY_FORWARDER(bool, EditorVisibility, false);
INTERACTION_PROPERTY_FORWARDER(bool, EditorVisibility3D, false);
INTERACTION_PROPERTY_FORWARDER(bool, EditorSliceIntersectionVisibility, false);
INTERACTION_PROPERTY_FORWARDER(bool, EditorTranslationEnabled, true);
INTERACTION_PROPERTY_FORWARDER(bool, EditorTranslationSliceEnabled, true);
INTERACTION_PROPERTY_FORWARDER(bool, EditorTranslationSliceAnywhereEnabled, false);
INTERACTION_PROPERTY_FORWARDER(double, EditorTranslationSliceAnywhereSensitivity, 0.2);
INTERACTION_PROPERTY_FORWARDER(bool, EditorRotationEnabled, true);
INTERACTION_PROPERTY_FORWARDER(bool, EditorRotationSliceEnabled, true);
INTERACTION_PROPERTY_FORWARDER(bool, EditorScalingEnabled, true);
INTERACTION_PROPERTY_FORWARDER(bool, EditorScalingSliceEnabled, true);
INTERACTION_PROPERTY_FORWARDER(double, InteractionSizeMm, 5.0);
INTERACTION_PROPERTY_FORWARDER(double, InteractionScalePercent, 15.0);
INTERACTION_PROPERTY_FORWARDER(bool, InteractionSizeAbsolute, false);
INTERACTION_PROPERTY_FORWARDER(double, InteractionHandleOpacity, 1.0);
INTERACTION_PROPERTY_FORWARDER(int, ActiveInteractionType, -1);
INTERACTION_PROPERTY_FORWARDER(int, ActiveInteractionIndex, -1);

/// Forwards a handle visibility vector to the interaction display node.
#define INTERACTION_VISIBILITY_FORWARDER(name)                                                              \
  void vtkMRMLTransformDisplayNode::Get##name(bool visibility[4])                                           \
  {                                                                                                         \
    vtkMRMLTransformInteractionDisplayNode* interactionDisplayNode = this->GetInteractionDisplayNode();      \
    if (interactionDisplayNode)                                                                             \
    {                                                                                                       \
      interactionDisplayNode->Get##name(visibility);                                                        \
      return;                                                                                               \
    }                                                                                                       \
    for (int i = 0; i < 4; ++i)                                                                             \
    {                                                                                                       \
      visibility[i] = true;                                                                                 \
    }                                                                                                       \
  }                                                                                                         \
  void vtkMRMLTransformDisplayNode::Set##name(bool visibility[4])                                           \
  {                                                                                                         \
    vtkMRMLTransformInteractionDisplayNode* interactionDisplayNode = this->GetInteractionDisplayNode(true);  \
    if (interactionDisplayNode)                                                                             \
    {                                                                                                       \
      interactionDisplayNode->Set##name(visibility);                                                        \
    }                                                                                                       \
    this->Modified();                                                                                       \
  }

INTERACTION_VISIBILITY_FORWARDER(RotationHandleComponentVisibility3D);
INTERACTION_VISIBILITY_FORWARDER(ScaleHandleComponentVisibility3D);
INTERACTION_VISIBILITY_FORWARDER(TranslationHandleComponentVisibility3D);
INTERACTION_VISIBILITY_FORWARDER(RotationHandleComponentVisibilitySlice);
INTERACTION_VISIBILITY_FORWARDER(ScaleHandleComponentVisibilitySlice);
INTERACTION_VISIBILITY_FORWARDER(TranslationHandleComponentVisibilitySlice);

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::UpdateEditorBounds()
{
  vtkMRMLTransformInteractionDisplayNode* interactionDisplayNode = this->GetInteractionDisplayNode();
  if (interactionDisplayNode)
  {
    interactionDisplayNode->UpdateEditorBounds();
  }
  this->InvokeEvent(vtkMRMLTransformDisplayNode::TransformUpdateEditorBoundsEvent);
}

//----------------------------------------------------------------------------
// Color map
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetDefaultColors()
{
  if (!this->GetScene())
  {
    vtkErrorMacro("vtkMRMLTransformDisplayNode::SetDefaultColors failed: scene is not set");
    return;
  }

  // Create and set a new color table node
  vtkNew<vtkMRMLProceduralColorNode> colorNode;
  colorNode->SetName(this->GetScene()->GenerateUniqueName(DEFAULT_COLOR_TABLE_NAME).c_str());
  colorNode->SetAttribute("Category", "Transform display");
  // The color node is a procedural color node, which is saved using a storage node.
  // Hidden nodes are not saved if they use a storage node, therefore
  // the color node must be visible.
  colorNode->SetHideFromEditors(false);

  vtkColorTransferFunction* colorMap = colorNode->GetColorTransferFunction();
  // Map: mm -> RGB
  colorMap->AddRGBPoint(1.0, 0.2, 0.2, 0.2);
  colorMap->AddRGBPoint(2.0, 0.0, 1.0, 0.0);
  colorMap->AddRGBPoint(5.0, 1.0, 1.0, 0.0);
  colorMap->AddRGBPoint(10.0, 1.0, 0.0, 0.0);

  this->GetScene()->AddNode(colorNode.GetPointer());
  this->SetAndObserveColorNodeID(colorNode->GetID());
}

//----------------------------------------------------------------------------
vtkColorTransferFunction* vtkMRMLTransformDisplayNode::GetColorMap()
{
  vtkMRMLProceduralColorNode* colorNode = vtkMRMLProceduralColorNode::SafeDownCast(GetColorNode());
  if (colorNode == nullptr                                //
      || colorNode->GetColorTransferFunction() == nullptr //
      || colorNode->GetColorTransferFunction()->GetSize() == 0)
  {
    // We don't have a color node or it is not the right type
    this->SetDefaultColors();
    colorNode = vtkMRMLProceduralColorNode::SafeDownCast(this->GetColorNode());
    if (colorNode == nullptr)
    {
      vtkErrorMacro("vtkMRMLTransformDisplayNode::GetColorMap failed: could not create default color node");
      return nullptr;
    }
  }
  vtkColorTransferFunction* colorMap = colorNode->GetColorTransferFunction();
  return colorMap;
}

//----------------------------------------------------------------------------
void vtkMRMLTransformDisplayNode::SetColorMap(vtkColorTransferFunction* newColorMap)
{
  int oldModified = this->StartModify();
  vtkMRMLProceduralColorNode* colorNode = vtkMRMLProceduralColorNode::SafeDownCast(this->GetColorNode());
  if (colorNode == nullptr)
  {
    // We don't have a color node or it is not the right type
    this->SetDefaultColors();
    colorNode = vtkMRMLProceduralColorNode::SafeDownCast(this->GetColorNode());
  }
  if (colorNode != nullptr)
  {
    if (colorNode->GetColorTransferFunction() == nullptr)
    {
      vtkNew<vtkColorTransferFunction> ctf;
      colorNode->SetAndObserveColorTransferFunction(ctf.GetPointer());
    }
    if (!vtkMRMLProceduralColorNode::IsColorMapEqual(colorNode->GetColorTransferFunction(), newColorMap))
    {
      colorNode->GetColorTransferFunction()->DeepCopy(newColorMap);
    }
  }
  else
  {
    vtkErrorMacro("vtkMRMLTransformDisplayNode::SetColorMap failed: could not create default color node");
  }
  this->EndModify(oldModified);
}
