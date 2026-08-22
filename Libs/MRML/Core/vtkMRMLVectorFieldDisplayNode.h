/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLVectorFieldDisplayNode.h,v $

=========================================================================auto=*/

#ifndef __vtkMRMLVectorFieldDisplayNode_h
#define __vtkMRMLVectorFieldDisplayNode_h

// MRML includes
#include "vtkMRMLDisplayNode.h"

// VTK includes
#include <vtkSmartPointer.h>

// STD includes
#include <string>
#include <vector>

class vtkAlgorithmOutput;
class vtkDataArray;
class vtkDataSet;
class vtkMatrix4x4;
class vtkMRMLVectorFieldModePipeline;
class vtkMRMLVectorFieldSampler;
class vtkMRMLVolumeNode;

/// \brief MRML node to represent the display properties of a vector field.
///
/// vtkMRMLVectorFieldDisplayNode stores the settings needed to display a vector field as
/// oriented and/or scaled glyphs (arrow, cone, box, cylinder, line, sphere), independently
/// of where the field is stored. It can be added to any displayable node that can provide
/// a vector field:
/// - a model node: the vectors are a 3-component point data array of its mesh,
/// - a vector volume node: the vectors are the components of its voxels.
///
/// The source is hidden behind GetFieldConnection() and CreateSliceFieldSampler(), which
/// return VTK pipeline objects, so the field is only sampled when the renderer pulls the
/// data and not when a node changes.
///
/// \sa vtkMRMLVectorFieldSampler
class VTK_MRML_EXPORT vtkMRMLVectorFieldDisplayNode : public vtkMRMLDisplayNode
{
public:
  static vtkMRMLVectorFieldDisplayNode* New();
  vtkTypeMacro(vtkMRMLVectorFieldDisplayNode, vtkMRMLDisplayNode);
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

  /// How the vector field is drawn.
  enum VisualizationModeType
  {
    VisualizationModeGlyph = 0,  ///< a glyph at each sampled point, oriented and scaled by the vector
    VisualizationModeGrid,       ///< a lattice of lines, deformed by the vectors
    VisualizationModeContour,    ///< isosurfaces (isolines in slice views) of the vector magnitude
    VisualizationModeStreamline, ///< curves that follow the direction of the field
    VisualizationMode_Last       // placeholder after the last valid value, this must be the last in the list of modes
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
  const char* GetNodeTagName() override { return "VectorFieldDisplay"; }

  /// Read node attributes from XML file
  void ReadXMLAttributes(const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLVectorFieldDisplayNode);

  /// Alternative method to propagate events generated in the referenced nodes
  void ProcessMRMLEvents(vtkObject* caller, unsigned long event, void* callData) override;

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

  ///@{
  /// How the vector field is drawn.
  /// Default is VisualizationModeGlyph.
  vtkGetMacro(VisualizationMode, int);
  vtkSetMacro(VisualizationMode, int);
  const char* GetVisualizationModeAsString();
  void SetVisualizationModeFromString(const char* modeString);
  static const char* GetVisualizationModeAsString(int id);
  static int GetVisualizationModeFromString(const char* modeString);
  ///@}

  ///@{
  /// Grid mode: length of the deformation applied to the grid, in percent of the vectors.
  /// 100 means that the grid is deformed by the vectors themselves.
  /// Default is 100.
  vtkGetMacro(GridScalePercent, double);
  vtkSetMacro(GridScalePercent, double);
  ///@}

  ///@{
  /// Grid mode: diameter of the grid lines, in mm. 0 draws them as thin lines.
  /// Default is 1.
  vtkGetMacro(GridLineDiameterMm, double);
  vtkSetClampMacro(GridLineDiameterMm, double, 0.0, VTK_DOUBLE_MAX);
  ///@}

  ///@{
  /// Contour mode: magnitude values (in mm) that isosurfaces are drawn at.
  unsigned int GetNumberOfContourLevels();
  void SetContourLevelsMm(double* levels, int size);
  double* GetContourLevelsMm() VTK_SIZEHINT(GetNumberOfContourLevels());
  void GetContourLevelsMm(std::vector<double>& levels);
  std::string GetContourLevelsMmAsString();
  void SetContourLevelsMmAsString(const std::string& levelsString);
  void SetContourLevelsMmFromString(const char* levelsString);
  ///@}

  ///@{
  /// Streamline mode: distance between the points that streamlines are started from, in mm.
  /// 0 uses the sampling spacing.
  /// Default is 0.
  vtkGetMacro(SeedSpacingMm, double);
  vtkSetClampMacro(SeedSpacingMm, double, 0.0, VTK_DOUBLE_MAX);
  ///@}

  ///@{
  /// Streamline mode: maximum length of a streamline, in mm.
  /// Default is 100.
  vtkGetMacro(MaximumPropagationMm, double);
  vtkSetClampMacro(MaximumPropagationMm, double, 0.0, VTK_DOUBLE_MAX);
  ///@}

  ///@{
  /// Streamline mode: diameter of the streamline tubes, in mm. 0 draws them as thin lines.
  /// Default is 0.5.
  vtkGetMacro(StreamlineTubeDiameterMm, double);
  vtkSetClampMacro(StreamlineTubeDiameterMm, double, 0.0, VTK_DOUBLE_MAX);
  ///@}

  ///@{
  /// Distance between the sampled points, in mm. Only used by sources that can be sampled
  /// at arbitrary positions (vector volumes, transforms); the point data of a mesh is
  /// always taken at the points of the mesh.
  /// 0 means that the source decides (for example the voxel size of a vector volume).
  /// Default is 0.
  vtkGetMacro(SamplingSpacingMm, double);
  vtkSetClampMacro(SamplingSpacingMm, double, 0.0, VTK_DOUBLE_MAX);
  ///@}

  ///@{
  /// Project the vectors onto the slice plane in slice views, so that the length of a
  /// glyph shows the in-plane component of the vector instead of a foreshortened 3D
  /// vector. Coloring still uses the magnitude of the original vector.
  /// Default is true.
  vtkGetMacro(SliceProjectionEnabled, bool);
  vtkSetMacro(SliceProjectionEnabled, bool);
  vtkBooleanMacro(SliceProjectionEnabled, bool);
  ///@}

  ///@{
  /// Number of subdivisions used to build the glyph geometry (for example the number of
  /// sides of an arrow's shaft and tip). Higher values give smoother glyphs at the cost of
  /// more geometry.
  /// Default is 16.
  vtkGetMacro(GlyphResolution, int);
  vtkSetClampMacro(GlyphResolution, int, 3, VTK_INT_MAX);
  ///@}

  ///@{
  /// Length of an arrow glyph's tip, in percent of the total glyph length.
  /// Only used by glyph types that have a tip (arrow, and the 2D arrow in slice views).
  /// Default is 35.
  vtkGetMacro(GlyphTipLengthPercent, double);
  vtkSetClampMacro(GlyphTipLengthPercent, double, 0.0, 100.0);
  ///@}

  ///@{
  /// Diameter of an arrow glyph's shaft, in percent of the diameter of its tip.
  /// Default is 30.
  vtkGetMacro(GlyphShaftDiameterPercent, double);
  vtkSetClampMacro(GlyphShaftDiameterPercent, double, 0.0, 100.0);
  ///@}

  ///@{
  /// Number of subdivisions used to build the flat glyph geometry that is drawn in slice
  /// views (for example the number of sides of a circle glyph).
  /// Default is 12.
  vtkGetMacro(GlyphResolution2D, int);
  vtkSetClampMacro(GlyphResolution2D, int, 3, VTK_INT_MAX);
  ///@}

  ///@{
  /// Length of the tip of an arrow glyph drawn in slice views, in percent of the total
  /// glyph length.
  /// Default is 30.
  vtkGetMacro(GlyphTipLengthPercent2D, double);
  vtkSetClampMacro(GlyphTipLengthPercent2D, double, 0.0, 100.0);
  ///@}

  ///@{
  /// Thickness (mm) of the slab around the slice plane in which mesh points are
  /// shown as glyphs in slice views. Points farther from the slice plane than half
  /// of this thickness are not displayed.
  /// Default is 1.0.
  vtkGetMacro(SliceSlabThicknessMm, double);
  vtkSetMacro(SliceSlabThicknessMm, double);
  ///@}

  ///@{
  /// Scale factor of the glyphs in slice views, in percent of the glyph size used
  /// in 3D views (ScaleFactor).
  /// Default is 100.
  vtkGetMacro(SliceGlyphScalePercent, double);
  vtkSetMacro(SliceGlyphScalePercent, double);
  ///@}

  ///@{
  /// Only show glyphs at points where the active scalar value is within
  /// ThresholdRange. Glyphs at all other points are hidden.
  /// Default is off.
  vtkGetMacro(ThresholdEnabled, bool);
  vtkSetMacro(ThresholdEnabled, bool);
  vtkBooleanMacro(ThresholdEnabled, bool);
  ///@}

  ///@{
  /// Range of active scalar values for which glyphs are shown.
  /// Only used if ThresholdEnabled is true.
  void SetThresholdRange(double min, double max);
  void SetThresholdRange(double range[2]);
  void GetThresholdRange(double range[2]);
  double* GetThresholdRange() VTK_SIZEHINT(2);
  double GetThresholdMin();
  double GetThresholdMax();
  ///@}

  //--------------------------------------------------------------------------
  /// Vector field source
  ///
  /// The field can be stored in any kind of displayable node (a model's point data,
  /// a vector volume, a transform, ...). These methods hide that difference from the
  /// displayable managers and from the GUI. None of them samples the field: they only
  /// return pipeline objects and metadata, so that the field is sampled by VTK when the
  /// renderer pulls the data.
  //--------------------------------------------------------------------------

  ///@{
  /// Node that bounds the region in which the field is sampled. Any displayable node can be
  /// used (a region of interest, a plane, a model, a volume), and a slice node.
  /// If not set then the source decides (for example the extent of a vector volume).
  /// Only used by sources that can be sampled at arbitrary positions; the point data of a
  /// mesh is always taken at the points of the mesh.
  vtkMRMLNode* GetRegionNode();
  void SetAndObserveRegionNode(vtkMRMLNode* node);
  ///@}

  /// Get the region that the field is sampled in, from the region node.
  /// regionToRAS defines the origin and the axis directions (scaled by the sampling
  /// spacing), regionSize the number of samples along each axis.
  /// Returns false if no valid region node is set.
  virtual bool GetSamplingRegion(vtkMatrix4x4* regionToRAS, int regionSize[3]);

  /// Connection that produces the point set the glyphs are placed at, for 3D views.
  /// Returns nullptr if the displayable node cannot provide a vector field.
  virtual vtkAlgorithmOutput* GetFieldConnection();

  /// Connection that produces the geometry of the non-glyph visualization modes (grid,
  /// contour, streamline), for 3D views. Returns nullptr in glyph mode, or if the source
  /// cannot provide the cells that these modes need.
  /// Slice views build the same pipeline on their own sampler, with a
  /// vtkMRMLVectorFieldModePipeline of their own.
  /// fieldConnection can be used to insert filters between the field source and the mode
  /// geometry (the displayable managers use it to apply a non-linear parent transform);
  /// GetFieldConnection() is used when it is nullptr.
  virtual vtkAlgorithmOutput* GetOutputPolyDataConnection(vtkAlgorithmOutput* fieldConnection = nullptr);

  /// True if the current visualization mode draws poly data instead of glyphs.
  bool IsPolyDataVisualizationMode();

  /// Create a sampler that resamples the field in the plane of a slice view. Each slice
  /// view owns its own sampler, because the same display node can be shown in several
  /// slice views at different positions.
  /// Returns nullptr if the field cannot be resampled in an arbitrary plane (for example
  /// the point data of a mesh); the slice view then selects the points that are within a
  /// slab around the slice plane instead.
  virtual vtkSmartPointer<vtkMRMLVectorFieldSampler> CreateSliceFieldSampler();

  /// Get the name and the number of components of each array that can be used for glyph
  /// orientation, scaling, and coloring. Does not update the pipeline, so it is safe to
  /// call from the GUI.
  virtual void GetFieldArrayInfo(std::vector<std::string>& arrayNames, std::vector<int>& numberOfComponents);

  /// Get data set containing the scalar/vector arrays that can be used for glyph
  /// orientation and scaling. This is the output of GetFieldConnection() as it was last
  /// computed; it does not trigger an update, therefore it returns nullptr if the field
  /// has not been sampled yet.
  vtkDataSet* GetScalarDataSet() override;

  /// Return the active scalar array (used for coloring and thresholding the glyphs)
  /// from the associated model node's mesh. Returns nullptr if not found or not set.
  vtkDataArray* GetActiveScalarArray() override;

  /// Called when the active scalar array changed. Glyphs are colored by the array
  /// directly (there is no assign-attribute filter to update), but the automatically
  /// computed scalar range has to be recomputed for the newly selected array.
  void UpdateAssignedAttribute() override;

  /// Compute the scalar range according to ScalarRangeFlag.
  /// Glyphs are colored by the magnitude of multi-component arrays, therefore the
  /// automatically computed range is the range of the magnitude for those arrays.
  void UpdateScalarRange() override;

  /// Return the orientation array from the associated model node's mesh, looked up by
  /// OrientationArrayName. Returns nullptr if not found or not set.
  virtual vtkDataArray* GetOrientationArray();

  /// Return the scale array from the associated model node's mesh, looked up by
  /// ScaleArrayName. Returns nullptr if not found or not set.
  virtual vtkDataArray* GetScaleArray();

protected:
  vtkMRMLVectorFieldDisplayNode();
  ~vtkMRMLVectorFieldDisplayNode() override;
  vtkMRMLVectorFieldDisplayNode(const vtkMRMLVectorFieldDisplayNode&);
  void operator=(const vtkMRMLVectorFieldDisplayNode&);

  int GlyphType;
  char* OrientationArrayName;
  char* ScaleArrayName;
  int VectorScaleMode;
  double ScaleFactor;
  int MaskingMode;
  int MaskingNthPoint;
  int MaskingPointsNumber;
  /// Samples a vector volume. Created when the display node is used with a volume;
  /// model point data does not need a sampler.
  vtkSmartPointer<vtkMRMLVectorFieldSampler> FieldSampler;

  /// Create (if needed) and configure the sampler that reads the field from a vector volume.
  virtual vtkMRMLVectorFieldSampler* UpdateVolumeFieldSampler(vtkMRMLVolumeNode* volumeNode, vtkMRMLVectorFieldSampler* sampler);

  /// Push the region and the sampling spacing into a sampler.
  void UpdateSamplerRegion(vtkMRMLVectorFieldSampler* sampler);

  /// Set the array names to the arrays that a sampler produces, if they are not set yet.
  /// Sampled fields always provide the same two arrays, so the user does not have to pick.
  void SetDefaultSampledFieldArrayNames();

  /// Filters of the non-glyph visualization modes for the 3D views. Slice views own theirs,
  /// because each of them shows a different plane of the field.
  vtkSmartPointer<vtkMRMLVectorFieldModePipeline> ModePipeline;

  int VisualizationMode;
  double GridScalePercent;
  double GridLineDiameterMm;
  std::vector<double> ContourLevelsMm;
  double SeedSpacingMm;
  double MaximumPropagationMm;
  double StreamlineTubeDiameterMm;

  double SamplingSpacingMm;
  bool SliceProjectionEnabled;
  int GlyphResolution;
  double GlyphTipLengthPercent;
  double GlyphShaftDiameterPercent;
  int GlyphResolution2D;
  double GlyphTipLengthPercent2D;
  double SliceSlabThicknessMm;
  double SliceGlyphScalePercent;
  bool ThresholdEnabled;
  double ThresholdRange[2];
};

#endif
