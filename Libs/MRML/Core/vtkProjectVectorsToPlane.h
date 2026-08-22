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

#ifndef __vtkProjectVectorsToPlane_h
#define __vtkProjectVectorsToPlane_h

// MRML includes
#include "vtkMRML.h"

// VTK includes
#include <vtkPointSetAlgorithm.h>

/// \brief Projects a point data vector array onto a plane.
///
/// Each vector v is replaced by v - (v.n)n, where n is the plane normal. Only the
/// direction of the plane matters, not its position.
///
/// This is what makes a vector field readable in a slice view: the component that points
/// out of the slice plane would be drawn foreshortened and would be indistinguishable from
/// a shorter in-plane vector.
///
/// The scalar arrays are passed through unchanged, so glyphs can still be colored by the
/// magnitude of the original, unprojected vector.
class VTK_MRML_EXPORT vtkProjectVectorsToPlane : public vtkPointSetAlgorithm
{
public:
  static vtkProjectVectorsToPlane* New();
  vtkTypeMacro(vtkProjectVectorsToPlane, vtkPointSetAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /// Normal of the plane that the vectors are projected onto.
  /// Does not need to be normalized. Default is (0, 0, 1).
  vtkSetVector3Macro(PlaneNormal, double);
  vtkGetVector3Macro(PlaneNormal, double);
  ///@}

  ///@{
  /// Name of the point data array to project. If not set (the default), the active
  /// vectors array is projected.
  vtkSetStringMacro(VectorArrayName);
  vtkGetStringMacro(VectorArrayName);
  ///@}

  ///@{
  /// Name of the array that the projected vectors are written to. If not set (the default),
  /// the input array is replaced.
  /// Writing to a separate array keeps the original vectors available, which matters when
  /// they are also what the glyphs are colored by: the color should show the true magnitude
  /// of the vector, not the part of it that happens to lie in the plane.
  vtkSetStringMacro(OutputVectorArrayName);
  vtkGetStringMacro(OutputVectorArrayName);
  ///@}

protected:
  vtkProjectVectorsToPlane();
  ~vtkProjectVectorsToPlane() override;
  vtkProjectVectorsToPlane(const vtkProjectVectorsToPlane&) = delete;
  void operator=(const vtkProjectVectorsToPlane&) = delete;

  int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;

  double PlaneNormal[3];
  char* VectorArrayName{ nullptr };
  char* OutputVectorArrayName{ nullptr };
};

#endif
