/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#ifndef __vtkMRMLVectorFieldSampler_h
#define __vtkMRMLVectorFieldSampler_h

// MRML includes
#include "vtkMRML.h"

// VTK includes
#include <vtkUnstructuredGridAlgorithm.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

class vtkDataArray;
class vtkMatrix4x4;
class vtkPointSet;
class vtkPoints;
class vtkUnsignedCharArray;
class vtkUnstructuredGrid;

/// \brief Produces the point set that a vector field visualization is built from.
///
/// Subclasses sample whatever stores the field (a transform, an image, ...) and all
/// produce the same output: points in the RAS coordinate system, a 3-component vector
/// array named GetVectorArrayName(), and a magnitude scalar array named
/// GetMagnitudeArrayName().
///
/// Samplers are demand-driven: setting the sampling region, the slice plane, or the
/// spacing only marks the filter modified, and the field is sampled in RequestData()
/// when the renderer pulls the data. GetMTime() folds in the modified time of the
/// sampled object (see GetSampledObjectMTime), so a modified transform or image is
/// re-sampled on the next render without anyone having to observe it.
///
/// \sa vtkMRMLVectorFieldDisplayNode
class VTK_MRML_EXPORT vtkMRMLVectorFieldSampler : public vtkUnstructuredGridAlgorithm
{
public:
  vtkTypeMacro(vtkMRMLVectorFieldSampler, vtkUnstructuredGridAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Name of the 3-component point data array that stores the sampled vectors, in RAS.
  static const char* GetVectorArrayName();

  /// Name of the point data array that stores the magnitude of the sampled vectors.
  static const char* GetMagnitudeArrayName();

  /// Name of the field data array that stores the size of the lattice that the samples are
  /// arranged in. Filters downstream (the grid lines) need it to know how the points are
  /// connected; it is absent when the samples do not form a lattice.
  static const char* GetLatticeSizeArrayName();

  /// Name of the point data array that says whether the field had a value at each sample:
  /// 1 where it did, 0 where the sample fell outside the field. A field that is defined
  /// everywhere it is asked about (a transform) does not write it, and a missing array means
  /// that every sample is valid. Grid lines and lattice cells stop at the invalid samples,
  /// so that the geometry ends where the field does instead of continuing through the empty
  /// space inside the bounding box.
  static const char* GetValidSampleArrayName();

  ///@{
  /// Region that is sampled, in the RAS coordinate system. regionToRAS defines the origin
  /// and the axis directions, size defines the number of sampling steps along each axis.
  /// Only marks the filter modified; sampling happens on the next pipeline update.
  virtual void SetSamplingRegion(vtkMatrix4x4* regionToRAS, const int size[3]);
  vtkMatrix4x4* GetRegionToRAS();
  vtkGetVector3Macro(RegionSize, int);
  ///@}

  ///@{
  /// Distance between sampled points, in mm. 0 means the source decides (for example the
  /// voxel size of the sampled image).
  vtkSetClampMacro(SamplingSpacingMm, double, 0.0, VTK_DOUBLE_MAX);
  vtkGetMacro(SamplingSpacingMm, double);
  ///@}

  ///@{
  /// Thickness of the slice that is sampled, in mm. Only the given sample positions that are
  /// within this thickness of the slice plane are used. Ignored when the samples are placed
  /// on a lattice, which is in the plane anyway. Default is 1.
  vtkSetClampMacro(SliceThicknessMm, double, 0.0, VTK_DOUBLE_MAX);
  vtkGetMacro(SliceThicknessMm, double);
  ///@}

  ///@{
  /// Number of sampling steps that have to stay together, so that the lattice holds a whole
  /// number of groups along each axis and is centered in the sampled region. Grid
  /// visualization sets it to the number of steps between two grid lines, which is what
  /// makes the drawn grid consist of complete cells. Default is 1.
  vtkSetClampMacro(LatticeGroupSize, int, 1, VTK_INT_MAX);
  vtkGetMacro(LatticeGroupSize, int);
  ///@}

  ///@{
  /// Restrict sampling to the plane of a slice view. Ignored by samplers that return false
  /// from CanSampleOnSlice(). sliceXYToRAS maps slice view coordinates to RAS, and
  /// fieldOfViewSizeMm is the size of the visible slice region.
  virtual void SetSlicePlane(vtkMatrix4x4* sliceXYToRAS, const double fieldOfViewSizeMm[2]);
  vtkMatrix4x4* GetSliceXYToRAS();
  vtkGetVector2Macro(FieldOfViewSizeMm, double);
  ///@}

  /// True if this sampler can sample the field in an arbitrary plane. Sources that only
  /// have values at fixed locations (such as the points of a mesh) return false, and the
  /// slice view then selects the points that are near the slice plane instead.
  virtual bool CanSampleOnSlice() { return false; }

  ///@{
  /// Connect the sampled points into cells, so that the field can be contoured, traced, or
  /// drawn as a deformed grid. Glyphs only need the points, so this is off by default: the
  /// cells of a large lattice cost time and memory.
  vtkSetMacro(GenerateCells, bool);
  vtkGetMacro(GenerateCells, bool);
  vtkBooleanMacro(GenerateCells, bool);
  ///@}

  /// Number of sampled points along each axis of the lattice, as produced by the last
  /// update. All zero if the samples are not arranged in a lattice.
  vtkGetVector3Macro(OutputLatticeSize, int);

  /// Sampling positions to use instead of a regular lattice, in RAS. Optional.
  virtual void SetSamplePositions(vtkPoints* samplePositions_RAS);
  vtkPoints* GetSamplePositions();

  /// Modified time, including the modified time of the object that is sampled.
  vtkMTimeType GetMTime() override;

protected:
  vtkMRMLVectorFieldSampler();
  ~vtkMRMLVectorFieldSampler() override;
  vtkMRMLVectorFieldSampler(const vtkMRMLVectorFieldSampler&) = delete;
  void operator=(const vtkMRMLVectorFieldSampler&) = delete;

  /// Modified time of the sampled object (transform, image, ...). Subclasses reimplement
  /// this so that the sampler re-executes when the field itself changes.
  virtual vtkMTimeType GetSampledObjectMTime() { return 0; }

  /// Compute the positions that the field is sampled at, in RAS. Uses, in this order of
  /// preference: the explicitly set sample positions, a lattice in the slice plane, a
  /// lattice in the sampling region, or whatever GetDefaultSamplePositions() provides.
  /// latticeSize is filled with the number of positions along each axis of the lattice, or
  /// with zeros if the positions do not form a lattice.
  void GetSamplePositions(vtkPoints* samplePositions_RAS, int latticeSize[3]);

  /// Sampling spacing to use when SamplingSpacingMm is 0. Subclasses reimplement this to
  /// follow the resolution of what they sample.
  virtual double GetDefaultSamplingSpacingMm() { return 1.0; }

  /// Bounds of the region where the field has values, in RAS. Used to keep a slice view from
  /// sampling the whole of its field of view when the field only occupies a small part of it.
  /// Returns false for a field that has no bounds of its own, such as a transform, which is
  /// defined everywhere.
  virtual bool GetFieldBounds(double vtkNotUsed(bounds_RAS)[6]) { return false; }

  /// Positions to sample when neither sample positions, a slice plane, nor a region is set.
  /// Sources that have a natural extent of their own (an image) reimplement this; sources
  /// that do not (a transform is defined everywhere) show nothing until a region is given.
  virtual bool GetDefaultSamplePositions(vtkPoints* vtkNotUsed(samplePositions_RAS), int vtkNotUsed(latticeSize)[3]) { return false; }

  /// Fill samplePositions_RAS with the points of a lattice: latticeToRAS maps the lattice
  /// indices to RAS, latticeSize is the number of points along each of its axes.
  static void GetLatticePositions(vtkMatrix4x4* latticeToRAS, const int latticeSize[3], vtkPoints* samplePositions_RAS);

  /// Add the vector array and the magnitude array to the output point set.
  /// vectors must have 3 components and as many tuples as outputPointSet has points.
  static void SetOutputVectors(vtkPointSet* outputPointSet, vtkDataArray* vectors_RAS);

  /// Connect the points of a lattice of the given size with voxel cells, so that the output
  /// can be contoured and interpolated. The points must be ordered with the first axis
  /// varying fastest.
  /// validSamples, when given, says which points the field had a value at; a cell is only
  /// created where all of its corners did, so that the cells cover the field and not the
  /// whole bounding box around it.
  static void GenerateLatticeCells(vtkUnstructuredGrid* outputGrid, const int latticeSize[3], vtkUnsignedCharArray* validSamples = nullptr);

  /// Set by subclasses at the end of RequestData. Also stores the size in the field data of
  /// the output, so that downstream filters can use it.
  void SetOutputLatticeSize(vtkUnstructuredGrid* output, const int latticeSize[3]);
  vtkSetVector3Macro(OutputLatticeSize, int);

  int OutputLatticeSize[3];
  bool GenerateCells{ false };

  vtkSmartPointer<vtkMatrix4x4> RegionToRAS;
  int RegionSize[3];

  vtkSmartPointer<vtkMatrix4x4> SliceXYToRAS;
  double FieldOfViewSizeMm[2];

  double SamplingSpacingMm{ 0.0 };
  int LatticeGroupSize{ 1 };
  double SliceThicknessMm{ 1.0 };

  vtkSmartPointer<vtkPoints> SamplePositions;
};

#endif
