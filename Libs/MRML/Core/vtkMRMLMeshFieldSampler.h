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

#ifndef __vtkMRMLMeshFieldSampler_h
#define __vtkMRMLMeshFieldSampler_h

#include "vtkMRMLVectorFieldSampler.h"

class vtkFindCellStrategy;
class vtkPointSet;

/// \brief Samples the vector field that a mesh carries, at chosen positions.
///
/// A mesh holds its vectors at its own points, but where its cells enclose a volume the
/// field they describe is defined everywhere inside them, and can be interpolated at any
/// position in between. That is what lets a deformed grid, an isosurface of the magnitude
/// or a streamline be drawn from a mesh, and what lets an evenly spaced set of glyphs be
/// drawn in the plane of a slice view rather than only at the points the mesh happens to
/// have there.
///
/// Positions that fall outside the cells have no field at them: they are given a zero
/// vector, which draws nothing.
///
/// \sa vtkMRMLVectorFieldDisplayNode, vtkMRMLVectorFieldSampler, vtkMRMLImageFieldSampler
class VTK_MRML_EXPORT vtkMRMLMeshFieldSampler : public vtkMRMLVectorFieldSampler
{
public:
  static vtkMRMLMeshFieldSampler* New();
  vtkTypeMacro(vtkMRMLMeshFieldSampler, vtkMRMLVectorFieldSampler);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /// Name of the point data array of the mesh that holds the vectors. If it is not set, the
  /// active vectors of the mesh are used.
  vtkSetStringMacro(MeshVectorArrayName);
  vtkGetStringMacro(MeshVectorArrayName);
  ///@}

  /// A mesh with volumetric cells can be interpolated anywhere inside them, including in
  /// the plane of a slice view.
  bool CanSampleOnSlice() override { return true; }

protected:
  vtkMRMLMeshFieldSampler();
  ~vtkMRMLMeshFieldSampler() override;
  vtkMRMLMeshFieldSampler(const vtkMRMLMeshFieldSampler&) = delete;
  void operator=(const vtkMRMLMeshFieldSampler&) = delete;

  int FillInputPortInformation(int port, vtkInformation* info) override;
  int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;

  vtkMTimeType GetSampledObjectMTime() override;

  /// A spacing that gives a readable number of samples over the mesh: a fiftieth of the
  /// diagonal of its bounds. Probing an unstructured grid is far more expensive than
  /// evaluating a transform, so the default keeps the count modest.
  double GetDefaultSamplingSpacingMm() override;

  /// The bounds of the mesh.
  bool GetFieldBounds(double bounds_RAS[6]) override;

  /// A lattice over the bounds of the mesh, at the sampling spacing.
  bool GetDefaultSamplePositions(vtkPoints* samplePositions_RAS, int latticeSize[3]) override;

  /// The locator that finds the cell a position falls in. It is built once for a mesh and
  /// kept, because building it for every update would cost more than the probing does.
  vtkFindCellStrategy* GetFindCellStrategy(vtkPointSet* mesh);

  char* MeshVectorArrayName{ nullptr };

  vtkSmartPointer<vtkFindCellStrategy> FindCellStrategy;
  vtkWeakPointer<vtkPointSet> FindCellStrategyMesh;
  vtkMTimeType FindCellStrategyMeshMTime{ 0 };
};

#endif
