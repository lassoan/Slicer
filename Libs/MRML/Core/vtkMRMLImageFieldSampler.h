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

#ifndef __vtkMRMLImageFieldSampler_h
#define __vtkMRMLImageFieldSampler_h

// MRML includes
#include "vtkMRMLVectorFieldSampler.h"

class vtkImageData;

/// \brief Samples the vector field stored in the voxels of an image.
///
/// The image must have at least 3 scalar components; the first three are interpreted as
/// the components of a vector. Sampling positions are computed in the RAS coordinate
/// system and the image is interpolated at those positions, so the glyph lattice is
/// independent of the voxel lattice - this is what makes an evenly spaced set of glyphs
/// possible in a slice view with any orientation.
///
/// \sa vtkMRMLVectorFieldDisplayNode, vtkMRMLVectorFieldSampler
class VTK_MRML_EXPORT vtkMRMLImageFieldSampler : public vtkMRMLVectorFieldSampler
{
public:
  static vtkMRMLImageFieldSampler* New();
  vtkTypeMacro(vtkMRMLImageFieldSampler, vtkMRMLVectorFieldSampler);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /// Geometry of the sampled image: the matrix that maps its IJK voxel coordinates to RAS.
  virtual void SetIJKToRAS(vtkMatrix4x4* ijkToRAS);
  vtkMatrix4x4* GetIJKToRAS();
  ///@}

  ///@{
  /// True if the voxel values are the RAS components of the vector
  /// (vtkMRMLVolumeNode::VoxelVectorTypeSpatial). If false, they are interpreted as IJK
  /// components and rotated into RAS using IJKToRAS.
  /// Default is true.
  vtkSetMacro(VectorsInRAS, bool);
  vtkGetMacro(VectorsInRAS, bool);
  vtkBooleanMacro(VectorsInRAS, bool);
  ///@}

  /// The image can be interpolated anywhere, including in the plane of a slice view.
  bool CanSampleOnSlice() override { return true; }

protected:
  vtkMRMLImageFieldSampler();
  ~vtkMRMLImageFieldSampler() override;
  vtkMRMLImageFieldSampler(const vtkMRMLImageFieldSampler&) = delete;
  void operator=(const vtkMRMLImageFieldSampler&) = delete;

  int FillInputPortInformation(int port, vtkInformation* info) override;
  int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;

  vtkMTimeType GetSampledObjectMTime() override;

  /// Compute the positions that the image is sampled at, in RAS. Uses, in this order of
  /// preference: the explicitly set sample positions, a lattice in the slice plane, a
  /// lattice in the sampling region, or a lattice over the whole image.
  /// latticeSize is filled with the number of positions along each axis of the lattice, or
  /// with zeros if the positions do not form a lattice.
  void GetSamplePositions(vtkImageData* image, vtkPoints* samplePositions_RAS, int latticeSize[3]);

  vtkSmartPointer<vtkMatrix4x4> IJKToRAS;
  bool VectorsInRAS{ true };
};

#endif
