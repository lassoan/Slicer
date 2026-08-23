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

#ifndef __vtkMRMLTransformDisplayNode_h
#define __vtkMRMLTransformDisplayNode_h

#include "vtkMRMLVectorFieldDisplayNode.h"

// vtkAddon includes
#include <vtkAddonSetGet.h>

class vtkColorTransferFunction;
class vtkPointSet;
class vtkMatrix4x4;
class vtkMRMLProceduralColorNode;
class vtkMRMLTransformInteractionDisplayNode;
class vtkMRMLTransformNode;
class vtkMRMLVolumeNode;

/// \brief MRML node to represent display properties for transforms visualization in the slice and 3D viewers.
///
/// A transform is a vector field: the displacement it applies at each point. This node is
/// therefore a vtkMRMLVectorFieldDisplayNode whose source is the transform, and everything
/// it shows - glyphs, a deformed grid, contours of the displacement magnitude - is drawn by
/// the shared vector field display pipeline.
///
/// The properties that this class adds on top of the shared ones are the ones that are
/// specific to a transform: a separate sampling spacing per visualization mode, and the
/// color map that maps a displacement in mm to a color. Its older accessors are kept as
/// forwarders to the shared properties, so that existing code and saved scenes keep working;
/// see the note on each of them for the property it now sets.
///
/// The interactive editing handles are not part of this node any more: they are stored in a
/// vtkMRMLTransformInteractionDisplayNode of the same transform node, so that a transform
/// has one display node per concern. The editor accessors below forward to it.
///
/// \sa vtkMRMLVectorFieldDisplayNode, vtkMRMLTransformFieldSampler, vtkMRMLTransformInteractionDisplayNode
class VTK_MRML_EXPORT vtkMRMLTransformDisplayNode : public vtkMRMLVectorFieldDisplayNode
{
public:
  static vtkMRMLTransformDisplayNode* New();
  vtkTypeMacro(vtkMRMLTransformDisplayNode, vtkMRMLVectorFieldDisplayNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Visualization modes, kept for backward compatibility. The values are the same as the
  /// corresponding vtkMRMLVectorFieldDisplayNode::VisualizationModeType values.
  enum VisualizationModes
  {
    VIS_MODE_GLYPH = vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph,
    VIS_MODE_GRID = vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid,
    VIS_MODE_CONTOUR = vtkMRMLVectorFieldDisplayNode::VisualizationModeContour,
    VIS_MODE_LAST // this should be the last mode
  };

  /// Glyph types that transform display offers, kept for backward compatibility. The values
  /// are the corresponding vtkMRMLVectorFieldDisplayNode::GlyphTypeType values, so that code
  /// using these names keeps working now that GetGlyphType() returns the shared values.
  /// Note that the shared node knows more glyph types in between, so GLYPH_TYPE_SPHERE is no
  /// longer 2.
  enum GlyphTypes
  {
    GLYPH_TYPE_ARROW = vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow,
    GLYPH_TYPE_CONE = vtkMRMLVectorFieldDisplayNode::GlyphTypeCone,
    GLYPH_TYPE_SPHERE = vtkMRMLVectorFieldDisplayNode::GlyphTypeSphere,
    GLYPH_TYPE_LAST = vtkMRMLVectorFieldDisplayNode::GlyphType_Last // this should be the last glyph type
  };

  //--------------------------------------------------------------------------
  /// MRMLNode methods
  //--------------------------------------------------------------------------

  vtkMRMLNode* CreateNodeInstance() override;

  ///
  /// Read node attributes from XML (MRML) file
  void ReadXMLAttributes(const char** atts) override;

  ///
  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLTransformDisplayNode);

  ///
  /// Get node XML tag name (like Volume, UnstructuredGrid)
  const char* GetNodeTagName() override { return "TransformDisplayNode"; }

  //--------------------------------------------------------------------------
  /// Vector field source
  //--------------------------------------------------------------------------

  /// The displacement field of the transform, sampled in the region that the region node
  /// defines. Returns nullptr if no region node is set: a transform is defined everywhere,
  /// so without a region there is nothing to draw.
  vtkAlgorithmOutput* GetFieldConnection() override;

  /// A sampler that evaluates the transform in the plane of one slice view.
  vtkSmartPointer<vtkMRMLVectorFieldSampler> CreateSliceFieldSampler() override;

  /// A transform can be evaluated at any position.
  bool CanSampleAtArbitraryPositions() override;

  /// Transform display offers glyphs, a deformed grid and contours of the displacement
  /// magnitude, the modes it has always had. Streamlines of a displacement field are not
  /// meaningful: the field is a displacement, not a velocity.
  bool IsVisualizationModeSupported(int visualizationMode) override;

  /// Transform display keeps a separate sampling spacing for each visualization mode, so
  /// that a grid can be sampled more finely than the glyphs are spaced.
  double GetEffectiveSamplingSpacingMm() override;
  void SetEffectiveSamplingSpacingMm(double spacingMm) override;

  /// The color map of a transform maps a displacement in mm to a color, so its range is the
  /// scalar range: the colors are used exactly as the map defines them.
  void UpdateScalarRange() override;

  /// Keeps the scalar range in step with the color map, which is what defines it.
  void ProcessMRMLEvents(vtkObject* caller, unsigned long event, void* callData) override;

  //--------------------------------------------------------------------------
  /// Display options
  //--------------------------------------------------------------------------

  /// A node that defines the region of interest where the transform should be
  /// displayed. Same as vtkMRMLVectorFieldDisplayNode::GetRegionNode().
  vtkMRMLNode* GetRegionNode();
  void SetAndObserveRegionNode(vtkMRMLNode* node);

  /// A node that defines glyph starting point positions.
  /// If not set then glyphs positions are arranged evenly in the full region.
  /// Same as vtkMRMLVectorFieldDisplayNode::GetSamplePointsNode().
  vtkMRMLNode* GetGlyphPointsNode();
  void SetAndObserveGlyphPointsNode(vtkMRMLNode* node);

  /// Convert visualization mode index to a string for serialization.
  /// Returns an empty string if the index is unknown.
  static const char* ConvertVisualizationModeToString(int modeIndex);
  /// Convert visualization mode string to an index that can be set in VisualizationMode.
  /// Returns -1 if the string is unknown.
  static int ConvertVisualizationModeFromString(const char* modeString);

  ///@{
  /// Distance between the glyphs, in mm. Sets the sampling spacing that is used in glyph
  /// visualization mode.
  vtkSetMacro(GlyphSpacingMm, double);
  vtkGetMacro(GlyphSpacingMm, double);
  ///@}

  ///@{
  /// Size of the glyphs, in percent. Forwards to ScaleFactor (100% is a scale factor of 1).
  void SetGlyphScalePercent(double scalePercent);
  double GetGlyphScalePercent();
  ///@}

  ///@{
  /// Only displacements within this range are shown as glyphs. Forwards to the shared
  /// threshold on the displacement magnitude.
  void SetGlyphDisplayRangeMaxMm(double maxMm);
  double GetGlyphDisplayRangeMaxMm();
  void SetGlyphDisplayRangeMinMm(double minMm);
  double GetGlyphDisplayRangeMinMm();
  ///@}

  /// Convert glyph type index to a string for serialization.
  /// Returns an empty string if the index is unknown.
  static const char* ConvertGlyphTypeToString(int typeIndex);
  /// Convert glyph type string to an index that can be set in GlyphType.
  /// Returns -1 if the string is unknown.
  static int ConvertGlyphTypeFromString(const char* typeString);

  ///@{
  /// Distance between the sampled points in grid mode, in mm. The grid lines themselves are
  /// GridSpacingMm apart; sampling more finely shows how the grid curves in between.
  vtkSetMacro(GridResolutionMm, double);
  vtkGetMacro(GridResolutionMm, double);
  ///@}

  ///@{
  /// Distance between the sampled points in contour mode, in mm.
  vtkSetMacro(ContourResolutionMm, double);
  vtkGetMacro(ContourResolutionMm, double);
  ///@}

  ///@{
  /// Contour levels, in mm. Forwards to the shared contour levels.
  unsigned int GetNumberOfContourLevels();
  void SetContourLevelsMm(double* levels, int size);
  double* GetContourLevelsMm();
  void GetContourLevelsMm(std::vector<double>& levels);
  std::string GetContourLevelsMmAsString();
  void SetContourLevelsMmFromString(const char* str);
  static std::vector<double> ConvertContourLevelsFromString(const char* str);
  static std::string ConvertContourLevelsToString(const std::vector<double>& levels);
  static bool IsContourLevelEqual(const std::vector<double>& levels1, const std::vector<double>& levels2);
  ///@}

  //--------------------------------------------------------------------------
  /// Interaction parameters
  ///
  /// These live in the transform node's vtkMRMLTransformInteractionDisplayNode; the
  /// accessors below forward to it and create it if it does not exist yet.
  //--------------------------------------------------------------------------

  /// Interaction display node of the same transform node, or nullptr if there is none and
  /// createIfMissing is false.
  vtkMRMLTransformInteractionDisplayNode* GetInteractionDisplayNode(bool createIfMissing = false);

  bool GetEditorVisibility();
  void SetEditorVisibility(bool enabled);
  void EditorVisibilityOn() { this->SetEditorVisibility(true); }
  void EditorVisibilityOff() { this->SetEditorVisibility(false); }
  bool GetEditorVisibility3D();
  void SetEditorVisibility3D(bool enabled);
  void EditorVisibility3DOn() { this->SetEditorVisibility3D(true); }
  void EditorVisibility3DOff() { this->SetEditorVisibility3D(false); }
  bool GetEditorSliceIntersectionVisibility();
  void SetEditorSliceIntersectionVisibility(bool enabled);
  void EditorSliceIntersectionVisibilityOn() { this->SetEditorSliceIntersectionVisibility(true); }
  void EditorSliceIntersectionVisibilityOff() { this->SetEditorSliceIntersectionVisibility(false); }
  bool GetEditorTranslationEnabled();
  void SetEditorTranslationEnabled(bool enabled);
  void EditorTranslationEnabledOn() { this->SetEditorTranslationEnabled(true); }
  void EditorTranslationEnabledOff() { this->SetEditorTranslationEnabled(false); }
  bool GetEditorTranslationSliceEnabled();
  void SetEditorTranslationSliceEnabled(bool enabled);
  void EditorTranslationSliceEnabledOn() { this->SetEditorTranslationSliceEnabled(true); }
  void EditorTranslationSliceEnabledOff() { this->SetEditorTranslationSliceEnabled(false); }
  bool GetEditorTranslationSliceAnywhereEnabled();
  void SetEditorTranslationSliceAnywhereEnabled(bool enabled);
  void EditorTranslationSliceAnywhereEnabledOn() { this->SetEditorTranslationSliceAnywhereEnabled(true); }
  void EditorTranslationSliceAnywhereEnabledOff() { this->SetEditorTranslationSliceAnywhereEnabled(false); }
  double GetEditorTranslationSliceAnywhereSensitivity();
  void SetEditorTranslationSliceAnywhereSensitivity(double sensitivity);
  bool GetEditorRotationEnabled();
  void SetEditorRotationEnabled(bool enabled);
  void EditorRotationEnabledOn() { this->SetEditorRotationEnabled(true); }
  void EditorRotationEnabledOff() { this->SetEditorRotationEnabled(false); }
  bool GetEditorRotationSliceEnabled();
  void SetEditorRotationSliceEnabled(bool enabled);
  void EditorRotationSliceEnabledOn() { this->SetEditorRotationSliceEnabled(true); }
  void EditorRotationSliceEnabledOff() { this->SetEditorRotationSliceEnabled(false); }
  bool GetEditorScalingEnabled();
  void SetEditorScalingEnabled(bool enabled);
  void EditorScalingEnabledOn() { this->SetEditorScalingEnabled(true); }
  void EditorScalingEnabledOff() { this->SetEditorScalingEnabled(false); }
  bool GetEditorScalingSliceEnabled();
  void SetEditorScalingSliceEnabled(bool enabled);
  void EditorScalingSliceEnabledOn() { this->SetEditorScalingSliceEnabled(true); }
  void EditorScalingSliceEnabledOff() { this->SetEditorScalingSliceEnabled(false); }

  double GetInteractionSizeMm();
  void SetInteractionSizeMm(double sizeMm);
  double GetInteractionScalePercent();
  void SetInteractionScalePercent(double scalePercent);
  bool GetInteractionSizeAbsolute();
  void SetInteractionSizeAbsolute(bool absolute);
  void InteractionSizeAbsoluteOn() { this->SetInteractionSizeAbsolute(true); }
  void InteractionSizeAbsoluteOff() { this->SetInteractionSizeAbsolute(false); }
  double GetInteractionHandleOpacity();
  void SetInteractionHandleOpacity(double opacity);
  int GetActiveInteractionType();
  void SetActiveInteractionType(int interactionType);
  int GetActiveInteractionIndex();
  void SetActiveInteractionIndex(int index);

  void GetRotationHandleComponentVisibility3D(bool visibility[4]);
  void SetRotationHandleComponentVisibility3D(bool visibility[4]);
  void GetScaleHandleComponentVisibility3D(bool visibility[4]);
  void SetScaleHandleComponentVisibility3D(bool visibility[4]);
  void GetTranslationHandleComponentVisibility3D(bool visibility[4]);
  void SetTranslationHandleComponentVisibility3D(bool visibility[4]);
  void GetRotationHandleComponentVisibilitySlice(bool visibility[4]);
  void SetRotationHandleComponentVisibilitySlice(bool visibility[4]);
  void GetScaleHandleComponentVisibilitySlice(bool visibility[4]);
  void SetScaleHandleComponentVisibilitySlice(bool visibility[4]);
  void GetTranslationHandleComponentVisibilitySlice(bool visibility[4]);
  void SetTranslationHandleComponentVisibilitySlice(bool visibility[4]);

  /// Ask the editor to recompute its bounds by invoking the
  /// TransformUpdateEditorBoundsEvent event.
  void UpdateEditorBounds();
  enum
  {
    TransformUpdateEditorBoundsEvent = 2750
  };

  /// Set the default color table
  /// Create and a procedural color node with default colors and use it for visualization.
  void SetDefaultColors();

  vtkColorTransferFunction* GetColorMap();
  void SetColorMap(vtkColorTransferFunction* newColorMap);

protected:
  static std::vector<double> StringToDoubleVector(const char* sourceStr);
  static std::string DoubleVectorToString(const double* values, int numberOfValues);

  /// Create (if needed) and configure the sampler that evaluates the transform.
  vtkMRMLVectorFieldSampler* UpdateTransformFieldSampler(vtkMRMLVectorFieldSampler* sampler);

  double GlyphSpacingMm;
  double GridResolutionMm;
  double ContourResolutionMm;

protected:
  vtkMRMLTransformDisplayNode();
  ~vtkMRMLTransformDisplayNode() override;
  vtkMRMLTransformDisplayNode(const vtkMRMLTransformDisplayNode&);
  void operator=(const vtkMRMLTransformDisplayNode&);
};

#endif
