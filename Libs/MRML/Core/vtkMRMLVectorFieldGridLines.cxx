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

// MRML includes
#include "vtkMRMLVectorFieldGridLines.h"
#include "vtkMRMLVectorFieldSampler.h"

// VTK includes
#include <vtkCellArray.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPolyData.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLVectorFieldGridLines);

//----------------------------------------------------------------------------
vtkMRMLVectorFieldGridLines::vtkMRMLVectorFieldGridLines() = default;

//----------------------------------------------------------------------------
vtkMRMLVectorFieldGridLines::~vtkMRMLVectorFieldGridLines() = default;

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldGridLines::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Subdivision: " << this->Subdivision << "\n";
}

//----------------------------------------------------------------------------
int vtkMRMLVectorFieldGridLines::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}

//----------------------------------------------------------------------------
int vtkMRMLVectorFieldGridLines::RequestData(vtkInformation* vtkNotUsed(request), vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkPointSet* input = vtkPointSet::GetData(inputVector[0]);
  vtkPolyData* output = vtkPolyData::GetData(outputVector);
  if (!input || !output)
  {
    return 0;
  }
  output->Initialize();

  int latticeSize[3] = { 0, 0, 0 };
  vtkIntArray* latticeSizeArray = vtkIntArray::SafeDownCast(input->GetFieldData()->GetArray(vtkMRMLVectorFieldSampler::GetLatticeSizeArrayName()));
  if (latticeSizeArray && latticeSizeArray->GetNumberOfValues() >= 3)
  {
    for (int axis = 0; axis < 3; ++axis)
    {
      latticeSize[axis] = latticeSizeArray->GetValue(axis);
    }
  }
  if (latticeSize[0] < 1 || latticeSize[1] < 1 || latticeSize[2] < 1)
  {
    // The samples are not arranged in a lattice, there is no grid to draw
    return 1;
  }

  output->SetPoints(input->GetPoints());
  output->GetPointData()->PassData(input->GetPointData());

  auto pointId = [&latticeSize](int i, int j, int k)
  { return static_cast<vtkIdType>(i) + static_cast<vtkIdType>(j) * latticeSize[0] + static_cast<vtkIdType>(k) * latticeSize[0] * latticeSize[1]; };

  vtkNew<vtkCellArray> lines;
  int index[3] = { 0, 0, 0 };
  // One polyline along each axis, for every Subdivision-th row of the two other axes
  for (int lineAxis = 0; lineAxis < 3; ++lineAxis)
  {
    if (latticeSize[lineAxis] < 2)
    {
      // A single sample along this axis is a point, not a line
      continue;
    }
    int firstOtherAxis = (lineAxis + 1) % 3;
    int secondOtherAxis = (lineAxis + 2) % 3;
    for (int second = 0; second < latticeSize[secondOtherAxis]; second += this->Subdivision)
    {
      for (int first = 0; first < latticeSize[firstOtherAxis]; first += this->Subdivision)
      {
        index[firstOtherAxis] = first;
        index[secondOtherAxis] = second;
        lines->InsertNextCell(latticeSize[lineAxis]);
        for (int along = 0; along < latticeSize[lineAxis]; ++along)
        {
          index[lineAxis] = along;
          lines->InsertCellPoint(pointId(index[0], index[1], index[2]));
        }
      }
    }
  }
  output->SetLines(lines);
  return 1;
}
