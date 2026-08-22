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
#include "vtkProjectVectorsToPlane.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkSmartPointer.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkProjectVectorsToPlane);

//----------------------------------------------------------------------------
vtkProjectVectorsToPlane::vtkProjectVectorsToPlane()
{
  this->PlaneNormal[0] = 0.0;
  this->PlaneNormal[1] = 0.0;
  this->PlaneNormal[2] = 1.0;
}

//----------------------------------------------------------------------------
vtkProjectVectorsToPlane::~vtkProjectVectorsToPlane()
{
  this->SetVectorArrayName(nullptr);
}

//----------------------------------------------------------------------------
void vtkProjectVectorsToPlane::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "PlaneNormal: " << this->PlaneNormal[0] << " " << this->PlaneNormal[1] << " " << this->PlaneNormal[2] << "\n";
  os << indent << "VectorArrayName: " << (this->VectorArrayName ? this->VectorArrayName : "(active vectors)") << "\n";
}

//----------------------------------------------------------------------------
int vtkProjectVectorsToPlane::RequestData(vtkInformation* vtkNotUsed(request), vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkPointSet* input = vtkPointSet::GetData(inputVector[0]);
  vtkPointSet* output = vtkPointSet::GetData(outputVector);
  if (!input || !output)
  {
    return 0;
  }

  // The points and all the other arrays are unchanged, only the projected vector array is
  // replaced by a deep copy.
  output->ShallowCopy(input);

  vtkPointData* outputPointData = output->GetPointData();
  vtkDataArray* inputVectors = nullptr;
  if (this->VectorArrayName && this->VectorArrayName[0] != '\0')
  {
    inputVectors = outputPointData->GetArray(this->VectorArrayName);
  }
  else
  {
    inputVectors = outputPointData->GetVectors();
  }
  if (!inputVectors || inputVectors->GetNumberOfComponents() != 3)
  {
    // Nothing to project
    return 1;
  }

  double normal[3] = { this->PlaneNormal[0], this->PlaneNormal[1], this->PlaneNormal[2] };
  if (vtkMath::Normalize(normal) == 0.0)
  {
    vtkWarningMacro("vtkProjectVectorsToPlane: plane normal is a zero vector, vectors are not projected");
    return 1;
  }

  vtkSmartPointer<vtkDataArray> projectedVectors = vtkSmartPointer<vtkDataArray>::Take(inputVectors->NewInstance());
  projectedVectors->SetName(inputVectors->GetName());
  projectedVectors->SetNumberOfComponents(3);
  vtkIdType numberOfTuples = inputVectors->GetNumberOfTuples();
  projectedVectors->SetNumberOfTuples(numberOfTuples);
  for (vtkIdType i = 0; i < numberOfTuples; ++i)
  {
    double vector[3] = { 0.0, 0.0, 0.0 };
    inputVectors->GetTuple(i, vector);
    double outOfPlaneComponent = vtkMath::Dot(vector, normal);
    for (int component = 0; component < 3; ++component)
    {
      vector[component] -= outOfPlaneComponent * normal[component];
    }
    projectedVectors->SetTuple(i, vector);
  }

  bool wasActiveVectors = (outputPointData->GetVectors() == inputVectors);
  outputPointData->RemoveArray(projectedVectors->GetName());
  if (wasActiveVectors)
  {
    outputPointData->SetVectors(projectedVectors);
  }
  else
  {
    outputPointData->AddArray(projectedVectors);
  }
  return 1;
}
