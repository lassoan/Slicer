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
#include "vtkMRMLMeshFieldSampler.h"

// VTK includes
#include <vtkCellLocatorStrategy.h>
#include <vtkDoubleArray.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkProbeFilter.h>
#include <vtkStaticCellLocator.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>

// STD includes
#include <algorithm>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLMeshFieldSampler);

//----------------------------------------------------------------------------
vtkMRMLMeshFieldSampler::vtkMRMLMeshFieldSampler()
{
  // The mesh that carries the field is the input
  this->SetNumberOfInputPorts(1);
}

//----------------------------------------------------------------------------
vtkMRMLMeshFieldSampler::~vtkMRMLMeshFieldSampler()
{
  this->SetMeshVectorArrayName(nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLMeshFieldSampler::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "MeshVectorArrayName: " << (this->MeshVectorArrayName ? this->MeshVectorArrayName : "(none)") << "\n";
}

//----------------------------------------------------------------------------
int vtkMRMLMeshFieldSampler::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkMRMLMeshFieldSampler::GetSampledObjectMTime()
{
  vtkPointSet* mesh = vtkPointSet::SafeDownCast(this->GetInputDataObject(0, 0));
  return (mesh ? mesh->GetMTime() : 0);
}

//----------------------------------------------------------------------------
double vtkMRMLMeshFieldSampler::GetDefaultSamplingSpacingMm()
{
  vtkPointSet* mesh = vtkPointSet::SafeDownCast(this->GetInputDataObject(0, 0));
  if (!mesh || mesh->GetNumberOfPoints() == 0)
  {
    return 1.0;
  }
  double bounds[6] = { 0.0, -1.0, 0.0, -1.0, 0.0, -1.0 };
  mesh->GetBounds(bounds);
  double diagonal = sqrt((bounds[1] - bounds[0]) * (bounds[1] - bounds[0]) //
                         + (bounds[3] - bounds[2]) * (bounds[3] - bounds[2])
                         + (bounds[5] - bounds[4]) * (bounds[5] - bounds[4]));
  // Probing a mesh costs far more per sample than evaluating a transform, so the default is
  // a lattice that is readable rather than fine
  return (diagonal > 0.0 ? diagonal / 50.0 : 1.0);
}

//----------------------------------------------------------------------------
bool vtkMRMLMeshFieldSampler::GetDefaultSamplePositions(vtkPoints* samplePositions_RAS, int latticeSize[3])
{
  vtkPointSet* mesh = vtkPointSet::SafeDownCast(this->GetInputDataObject(0, 0));
  if (!samplePositions_RAS || !mesh || mesh->GetNumberOfPoints() == 0)
  {
    return false;
  }
  double bounds[6] = { 0.0, -1.0, 0.0, -1.0, 0.0, -1.0 };
  mesh->GetBounds(bounds);
  double spacingMm = this->SamplingSpacingMm;
  if (spacingMm <= 0.0)
  {
    spacingMm = this->GetDefaultSamplingSpacingMm();
  }
  if (spacingMm <= 0.0)
  {
    return false;
  }

  vtkNew<vtkMatrix4x4> latticeToRAS;
  latticeToRAS->Identity();
  for (int axis = 0; axis < 3; ++axis)
  {
    double sizeMm = bounds[2 * axis + 1] - bounds[2 * axis];
    int numberOfSteps = std::max(0, static_cast<int>(sizeMm / spacingMm));
    latticeSize[axis] = numberOfSteps + 1;
    latticeToRAS->SetElement(axis, axis, spacingMm);
    // The leftover of the last step is split between the two sides, so the lattice is
    // centered on the mesh
    latticeToRAS->SetElement(axis, 3, bounds[2 * axis] + 0.5 * (sizeMm - numberOfSteps * spacingMm));
  }
  vtkMRMLVectorFieldSampler::GetLatticePositions(latticeToRAS, latticeSize, samplePositions_RAS);
  return true;
}

//----------------------------------------------------------------------------
vtkFindCellStrategy* vtkMRMLMeshFieldSampler::GetFindCellStrategy(vtkPointSet* mesh)
{
  if (!mesh)
  {
    return nullptr;
  }
  // Building the locator is the expensive part of probing a mesh, so it is kept until the
  // mesh it was built for changes
  if (this->FindCellStrategy && this->FindCellStrategyMesh == mesh && this->FindCellStrategyMeshMTime == mesh->GetMTime())
  {
    return this->FindCellStrategy;
  }
  vtkNew<vtkCellLocatorStrategy> strategy;
  vtkNew<vtkStaticCellLocator> locator;
  strategy->SetCellLocator(locator);
  strategy->Initialize(mesh);
  this->FindCellStrategy = strategy;
  this->FindCellStrategyMesh = mesh;
  this->FindCellStrategyMeshMTime = mesh->GetMTime();
  return this->FindCellStrategy;
}

//----------------------------------------------------------------------------
int vtkMRMLMeshFieldSampler::RequestData(vtkInformation* vtkNotUsed(request), vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkPointSet* mesh = vtkPointSet::GetData(inputVector[0]);
  vtkUnstructuredGrid* output = vtkUnstructuredGrid::GetData(outputVector);
  if (!output)
  {
    return 0;
  }
  vtkNew<vtkPoints> samplePositions_RAS;
  int latticeSize[3] = { 0, 0, 0 };
  this->GetSamplePositions(samplePositions_RAS, latticeSize);
  this->SetOutputLatticeSize(output, latticeSize);
  vtkIdType numberOfSamples = samplePositions_RAS->GetNumberOfPoints();
  if (!mesh || mesh->GetNumberOfPoints() == 0 || numberOfSamples == 0)
  {
    output->SetPoints(nullptr);
    return 1;
  }

  vtkNew<vtkPolyData> probePoints;
  probePoints->SetPoints(samplePositions_RAS);

  vtkNew<vtkProbeFilter> probe;
  probe->SetInputData(probePoints);
  probe->SetSourceData(mesh);
  vtkFindCellStrategy* strategy = this->GetFindCellStrategy(mesh);
  if (strategy)
  {
    probe->SetFindCellStrategy(strategy);
  }
  probe->Update();

  vtkPointData* probedPointData = probe->GetOutput()->GetPointData();
  vtkDataArray* probedVectors = nullptr;
  if (this->MeshVectorArrayName && this->MeshVectorArrayName[0] != '\0')
  {
    probedVectors = probedPointData->GetArray(this->MeshVectorArrayName);
  }
  if (!probedVectors)
  {
    probedVectors = probedPointData->GetVectors();
  }
  if (!probedVectors || probedVectors->GetNumberOfComponents() < 3)
  {
    output->SetPoints(nullptr);
    return 1;
  }

  vtkNew<vtkDoubleArray> vectors_RAS;
  vectors_RAS->SetNumberOfComponents(3);
  vectors_RAS->SetNumberOfTuples(numberOfSamples);
  vtkUnsignedCharArray* validMask =
    vtkUnsignedCharArray::SafeDownCast(probedPointData->GetArray(probe->GetValidPointMaskArrayName()));
  for (vtkIdType sampleIndex = 0; sampleIndex < numberOfSamples; ++sampleIndex)
  {
    // A position that falls outside the cells has no field at it, and a zero vector draws
    // nothing there
    if (validMask && validMask->GetValue(sampleIndex) == 0)
    {
      vectors_RAS->SetTuple3(sampleIndex, 0.0, 0.0, 0.0);
      continue;
    }
    double vector[3] = { 0.0, 0.0, 0.0 };
    probedVectors->GetTuple(sampleIndex, vector);
    vectors_RAS->SetTuple(sampleIndex, vector);
  }

  output->SetPoints(samplePositions_RAS);
  vtkMRMLVectorFieldSampler::SetOutputVectors(output, vectors_RAS);
  if (this->GenerateCells)
  {
    vtkMRMLVectorFieldSampler::GenerateLatticeCells(output, latticeSize);
  }
  return 1;
}
