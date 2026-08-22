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

#ifndef __vtkMRMLVectorFieldGridLines_h
#define __vtkMRMLVectorFieldGridLines_h

// MRML includes
#include "vtkMRML.h"

// VTK includes
#include <vtkPolyDataAlgorithm.h>

/// \brief Builds the lines of a grid from a lattice of sampled points.
///
/// The input is the output of a vtkMRMLVectorFieldSampler: points ordered as a lattice,
/// with the size of that lattice in the field data array named GetLatticeSizeArrayName().
///
/// A line is drawn every Subdivision-th row of the lattice, but through *every* point along
/// that row. Sampling the field more finely than the grid spacing therefore shows how the
/// grid curves between its lines, which is what makes a non-linear deformation visible.
///
/// \sa vtkMRMLVectorFieldSampler, vtkMRMLVectorFieldModePipeline
class VTK_MRML_EXPORT vtkMRMLVectorFieldGridLines : public vtkPolyDataAlgorithm
{
public:
  static vtkMRMLVectorFieldGridLines* New();
  vtkTypeMacro(vtkMRMLVectorFieldGridLines, vtkPolyDataAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /// Number of lattice steps between two grid lines. 1 draws a line through every row of
  /// sampled points.
  /// Default is 1.
  vtkSetClampMacro(Subdivision, int, 1, VTK_INT_MAX);
  vtkGetMacro(Subdivision, int);
  ///@}

protected:
  vtkMRMLVectorFieldGridLines();
  ~vtkMRMLVectorFieldGridLines() override;
  vtkMRMLVectorFieldGridLines(const vtkMRMLVectorFieldGridLines&) = delete;
  void operator=(const vtkMRMLVectorFieldGridLines&) = delete;

  int FillInputPortInformation(int port, vtkInformation* info) override;
  int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;

  int Subdivision{ 1 };
};

#endif
