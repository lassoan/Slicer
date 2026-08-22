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
#include "vtkMRMLTransformFieldSampler.h"
#include "vtkMRMLTransformNode.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkGeneralTransform.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLTransformFieldSampler);

//----------------------------------------------------------------------------
vtkMRMLTransformFieldSampler::vtkMRMLTransformFieldSampler() = default;

//----------------------------------------------------------------------------
vtkMRMLTransformFieldSampler::~vtkMRMLTransformFieldSampler() = default;

//----------------------------------------------------------------------------
void vtkMRMLTransformFieldSampler::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "TransformNode: " << (this->TransformNode ? this->TransformNode->GetID() : "(none)") << "\n";
  os << indent << "TransformToWorld: " << (this->TransformToWorld ? "true" : "false") << "\n";
}

//----------------------------------------------------------------------------
void vtkMRMLTransformFieldSampler::SetTransformNode(vtkMRMLTransformNode* transformNode)
{
  if (this->TransformNode == transformNode)
  {
    return;
  }
  this->TransformNode = transformNode;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLTransformNode* vtkMRMLTransformFieldSampler::GetTransformNode()
{
  return this->TransformNode;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkMRMLTransformFieldSampler::GetSampledObjectMTime()
{
  if (!this->TransformNode)
  {
    return 0;
  }
  // Includes the transforms that the transform node is under, because the displacement
  // that is drawn is the one that the node applies in world coordinates.
  return this->TransformNode->GetTransformToWorldMTime();
}

//----------------------------------------------------------------------------
int vtkMRMLTransformFieldSampler::RequestData(vtkInformation* vtkNotUsed(request),
                                              vtkInformationVector** vtkNotUsed(inputVector),
                                              vtkInformationVector* outputVector)
{
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
  if (!this->TransformNode || numberOfSamples == 0)
  {
    output->SetPoints(nullptr);
    return 1;
  }

  vtkNew<vtkGeneralTransform> transform;
  if (this->TransformToWorld)
  {
    this->TransformNode->GetTransformToWorld(transform);
  }
  else
  {
    this->TransformNode->GetTransformFromWorld(transform);
  }

  vtkNew<vtkDoubleArray> vectors_RAS;
  vectors_RAS->SetNumberOfComponents(3);
  vectors_RAS->SetNumberOfTuples(numberOfSamples);
  for (vtkIdType sampleIndex = 0; sampleIndex < numberOfSamples; ++sampleIndex)
  {
    double point_RAS[3] = { 0.0, 0.0, 0.0 };
    samplePositions_RAS->GetPoint(sampleIndex, point_RAS);
    double transformedPoint_RAS[3] = { 0.0, 0.0, 0.0 };
    transform->TransformPoint(point_RAS, transformedPoint_RAS);
    double displacement_RAS[3] = { transformedPoint_RAS[0] - point_RAS[0], //
                                   transformedPoint_RAS[1] - point_RAS[1],
                                   transformedPoint_RAS[2] - point_RAS[2] };
    vectors_RAS->SetTuple(sampleIndex, displacement_RAS);
  }

  output->SetPoints(samplePositions_RAS);
  vtkMRMLVectorFieldSampler::SetOutputVectors(output, vectors_RAS);
  if (this->GenerateCells)
  {
    vtkMRMLVectorFieldSampler::GenerateLatticeCells(output, latticeSize);
  }
  return 1;
}
