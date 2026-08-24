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
class vtkIntArray;
class vtkPoints;
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

  /// Determines where the glyphs are placed. The first modes select points of the mesh that
  /// carries the vectors; the last two apply to a field that can be evaluated anywhere (a
  /// transform, a vector volume).
  enum GlyphMaskingModeType
  {
    MaskingModeAllPoints = 0,      ///< show a glyph at every point of the mesh
    MaskingModeUniformBounds,      ///< randomly sample points, uniformly distributed within the bounding box of the mesh
    MaskingModeUniformSurface,     ///< randomly sample points, uniformly distributed on the surface of the mesh
    MaskingModeUniformVolume,      ///< randomly sample points, uniformly distributed within the volume of the mesh
    MaskingModeFixedSpacing,       ///< show a glyph on an evenly spaced lattice, at SamplingSpacingMm
    MaskingModeNodePoints,         ///< show a glyph at each point of the sample points node
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

  /// The events of a region or sample points node that change where the field is sampled.
  static void AddSamplingEvents(vtkIntArray* events);

  /// The points of the sample points node, in world coordinates: the control points of a
  /// markups node, the mesh points of a model, or the voxel centers of a volume.
  /// Returns false if there is no such node or it has no points.
  bool GetSamplePositions(vtkPoints* samplePositions_RAS);

  ///@{
  /// The array that the field is read from. It is what the orientation, scale and color
  /// arrays follow unless one of them is set to something else, so that choosing a field is
  /// one choice and not three. Empty by default, which means the source decides.
  vtkGetStringMacro(FieldArrayName);
  virtual void SetFieldArrayName(const char* arrayName);
  ///@}

  /// A scale factor that draws the field at a readable size: the longest vector spans about
  /// a twentieth of the sampled region. Returns 0 if it cannot be worked out, which is the
  /// case before anything has been sampled.
  virtual double ComputeDefaultScaleFactor();

  ///@{
  /// The array that scales the glyphs, and the array itself. It is the scale array when one
  /// is set, and the orientation array otherwise: a vector field is drawn with the length of
  /// its own vectors unless the user asks for something else.
  const char* GetEffectiveScaleArrayName();
  vtkDataArray* GetEffectiveScaleArray();
  ///@}

  ///@{
  /// How much the field is scaled by, whatever the visualization mode is: the length of the
  /// glyphs in glyph mode, the length of the deformation of the grid in grid mode. 1 draws
  /// the field at its own size. The modes that do not scale anything (contour, streamline)
  /// keep the glyph scale factor.
  virtual double GetEffectiveScaleFactor();
  virtual void SetEffectiveScaleFactor(double scaleFactor);
  /// True if the visualization mode has something to scale.
  virtual bool IsScaleFactorUsed();
  ///@}

  /// True if the visualization mode can hide parts of the field by their magnitude.
  virtual bool IsThresholdUsed();

  /// True if the glyphs are stretched along their own axis, keeping their thickness. Only
  /// a glyph that has an axis and a cross section can be, so this is false for a sphere or
  /// a box however ScaleDirectional is set.
  bool IsScaleDirectionalUsed();

  /// True if the source can place glyphs the way the mode asks for. A field that is sampled
  /// can only use a lattice or a list of points, and the point data of a mesh can only use
  /// the points of that mesh.
  virtual bool IsMaskingModeSupported(int maskingMode);

  /// The masking mode that is used, which is the mode that was set if the source supports
  /// it, and the source's natural one otherwise.
  int GetEffectiveMaskingMode();

  /// True if the source can be drawn the way the visualization mode asks for. Reimplemented
  /// by nodes whose source does not offer every mode (a transform has no streamlines).
  virtual bool IsVisualizationModeSupported(int visualizationMode);

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
  /// Scale the glyph along its own axis only, so that its thickness stays the same however
  /// long it is. This is what makes a field of arrows readable: without it a long arrow is
  /// also a thick arrow. The thickness is then GlyphDiameterMm.
  /// Default is false.
  vtkGetMacro(ScaleDirectional, bool);
  vtkSetMacro(ScaleDirectional, bool);
  vtkBooleanMacro(ScaleDirectional, bool);
  ///@}

  ///@{
  /// Whether the thickness of the glyphs is a length in mm or a percentage of the length of
  /// the glyph. An absolute thickness is the same for every glyph, however long it is; a
  /// relative one grows with the glyph, the way the tip of an arrow does.
  /// Default is true.
  vtkGetMacro(GlyphDiameterAbsolute, bool);
  vtkSetMacro(GlyphDiameterAbsolute, bool);
  vtkBooleanMacro(GlyphDiameterAbsolute, bool);
  ///@}

  ///@{
  /// Glyph geometry: the thickness of the glyphs, in percent of their length.
  /// Only used when GlyphDiameterAbsolute is false. Default is 20.
  vtkGetMacro(GlyphDiameterPercent, double);
  vtkSetMacro(GlyphDiameterPercent, double);
  ///@}

  /// The radius of the glyph source geometry, which is one unit long: the thickness in mm
  /// when it is absolute, and the fraction of the length when it is relative.
  double GetGlyphSourceRadius();

  ///@{
  /// Thickness of the glyphs, in mm. Only used when GlyphDiameterAbsolute is true.
  /// Default is 5.
  vtkGetMacro(GlyphDiameterMm, double);
  vtkSetClampMacro(GlyphDiameterMm, double, 0.0, VTK_DOUBLE_MAX);
  ///@}

  ///@{
  /// Grid mode: distance between the grid lines, in mm. The lines themselves follow the
  /// sampled points, so a sampling spacing finer than this shows how the grid curves
  /// between its lines. 0 draws a line through every sampled point.
  /// Default is 0.
  vtkGetMacro(GridSpacingMm, double);
  vtkSetClampMacro(GridSpacingMm, double, 0.0, VTK_DOUBLE_MAX);
  ///@}

  ///@{
  /// Grid mode: also draw the undeformed grid. Only used in slice views; in a 3D view it
  /// would make the visualization very cluttered.
  /// Default is false.
  vtkGetMacro(GridShowNonWarped, bool);
  vtkSetMacro(GridShowNonWarped, bool);
  vtkBooleanMacro(GridShowNonWarped, bool);
  ///@}

  ///@{
  /// Contour mode: opacity of the isosurfaces, between 0 and 1.
  /// Default is 0.8.
  vtkGetMacro(ContourOpacity, double);
  vtkSetClampMacro(ContourOpacity, double, 0.0, 1.0);
  ///@}

  ///@{
  /// Distance between the sampled points that is actually used, which can depend on the
  /// visualization mode. Reimplemented by nodes that keep a separate spacing per mode, so
  /// that the GUI reads and writes the spacing that applies to what is being shown.
  virtual double GetEffectiveSamplingSpacingMm();
  virtual void SetEffectiveSamplingSpacingMm(double spacingMm);
  ///@}

  /// Distance between the sampled points that the field is really sampled at, which is not
  /// always the requested one: in grid mode the lattice has to divide the grid line spacing
  /// evenly, so it is refined to the next such spacing that is no coarser than requested.
  /// Without that, the drawn grid spacing would be round(GridSpacingMm / requested) times
  /// the requested spacing, which jumps up and down as the requested spacing is changed.
  double GetSamplingSpacingForFieldMm();

  /// Number of sampling steps between two grid lines, which is what makes the grid lines
  /// land exactly GridSpacingMm apart at the spacing GetSamplingSpacingForFieldMm() returns.
  int GetGridSubdivision();

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

  ///@{
  /// Markups node whose control points are used as the positions that the field is sampled
  /// at, instead of a regular lattice. Glyphs are placed at those points, and streamlines
  /// are started from them.
  /// Only used by sources that can be sampled at arbitrary positions.
  vtkMRMLNode* GetSamplePointsNode();
  void SetAndObserveSamplePointsNode(vtkMRMLNode* node);
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

  /// True if the field can be sampled at any position, so that the sampling spacing, the
  /// region and the sample points apply to it. False for sources that only have values at
  /// fixed locations, such as the point data of a mesh.
  virtual bool CanSampleAtArbitraryPositions();

  /// Create a sampler that resamples the field in the plane of a slice view. Each slice
  /// view owns its own sampler, because the same display node can be shown in several
  /// slice views at different positions.
  /// Returns nullptr if the field cannot be resampled in an arbitrary plane (for example
  /// the point data of a mesh); the slice view then selects the points that are within a
  /// slab around the slice plane instead.
  virtual vtkSmartPointer<vtkMRMLVectorFieldSampler> CreateSliceFieldSampler();

  /// True if the field source always provides the same arrays, so there is nothing for the
  /// user to pick: a sampled field (a vector volume, a transform) always produces one vector
  /// array and its magnitude. False for a mesh, whose point data can hold any number of
  /// arrays.
  virtual bool HasFixedFieldArrays();

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
  char* FieldArrayName;
  int VectorScaleMode;
  double ScaleFactor;
  int MaskingMode;
  int MaskingPointsNumber;
  /// Samples a vector volume. Created when the display node is used with a volume;
  /// model point data does not need a sampler.
  vtkSmartPointer<vtkMRMLVectorFieldSampler> FieldSampler;

  /// Create (if needed) and configure the sampler that reads the field from a vector volume.
  virtual vtkMRMLVectorFieldSampler* UpdateVolumeFieldSampler(vtkMRMLVolumeNode* volumeNode, vtkMRMLVectorFieldSampler* sampler);

  /// Push the region, the sampling spacing and the sample positions into a sampler.
  void UpdateSamplerRegion(vtkMRMLVectorFieldSampler* sampler);

  /// Sampling lattice of a region that is defined by an oriented box (a markups region of
  /// interest or plane): objectToWorld holds its center and axis directions, sizeMm its
  /// extent along those axes.
  bool GetOrientedSamplingRegion(vtkMatrix4x4* objectToWorld, const double sizeMm[3], vtkMatrix4x4* regionToRAS, int regionSize[3]);

  /// Set the array names to the arrays that a sampler produces, if they are not set yet.
  /// Sampled fields always provide the same two arrays, so the user does not have to pick.
  void SetDefaultSampledFieldArrayNames();

  /// Filters of the non-glyph visualization modes for the 3D views. Slice views own theirs,
  /// because each of them shows a different plane of the field.
  vtkSmartPointer<vtkMRMLVectorFieldModePipeline> ModePipeline;

  int VisualizationMode;
  bool ScaleDirectional;
  double GlyphDiameterMm;
  bool GlyphDiameterAbsolute;
  double GlyphDiameterPercent;
  double GridSpacingMm;
  bool GridShowNonWarped;
  double ContourOpacity;
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
  bool ThresholdEnabled;
  double ThresholdRange[2];
};

#endif
