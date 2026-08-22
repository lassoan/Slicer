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
#include "vtkMRMLImageFieldSampler.h"

// VTK includes
#include <vtkDoubleArray.h>
#include <vtkImageData.h>
#include <vtkImageInterpolator.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

// STD includes
#include <algorithm>
#include <vector>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLImageFieldSampler);

//----------------------------------------------------------------------------
vtkMRMLImageFieldSampler::vtkMRMLImageFieldSampler()
{
  this->IJKToRAS = vtkSmartPointer<vtkMatrix4x4>::New();
  this->SetNumberOfInputPorts(1);
}

//----------------------------------------------------------------------------
vtkMRMLImageFieldSampler::~vtkMRMLImageFieldSampler() = default;

//----------------------------------------------------------------------------
void vtkMRMLImageFieldSampler::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "VectorsInRAS: " << (this->VectorsInRAS ? "true" : "false") << "\n";
  os << indent << "IJKToRAS:\n";
  this->IJKToRAS->PrintSelf(os, indent.GetNextIndent());
}

//----------------------------------------------------------------------------
int vtkMRMLImageFieldSampler::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;
}

//----------------------------------------------------------------------------
void vtkMRMLImageFieldSampler::SetIJKToRAS(vtkMatrix4x4* ijkToRAS)
{
  if (!ijkToRAS)
  {
    return;
  }
  for (int row = 0; row < 4; ++row)
  {
    for (int column = 0; column < 4; ++column)
    {
      if (this->IJKToRAS->GetElement(row, column) != ijkToRAS->GetElement(row, column))
      {
        this->IJKToRAS->DeepCopy(ijkToRAS);
        this->Modified();
        return;
      }
    }
  }
}

//----------------------------------------------------------------------------
vtkMatrix4x4* vtkMRMLImageFieldSampler::GetIJKToRAS()
{
  return this->IJKToRAS;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkMRMLImageFieldSampler::GetSampledObjectMTime()
{
  vtkImageData* image = vtkImageData::SafeDownCast(this->GetInput());
  return image ? image->GetMTime() : 0;
}

//----------------------------------------------------------------------------
double vtkMRMLImageFieldSampler::GetDefaultSamplingSpacingMm()
{
  // Follow the resolution of the image
  double voxelSizeMm[3] = { 0.0, 0.0, 0.0 };
  for (int axis = 0; axis < 3; ++axis)
  {
    double axisVector[3] = { this->IJKToRAS->GetElement(0, axis), this->IJKToRAS->GetElement(1, axis), this->IJKToRAS->GetElement(2, axis) };
    voxelSizeMm[axis] = vtkMath::Norm(axisVector);
  }
  return std::max(std::max(voxelSizeMm[0], voxelSizeMm[1]), voxelSizeMm[2]);
}

//----------------------------------------------------------------------------
bool vtkMRMLImageFieldSampler::GetDefaultSamplePositions(vtkPoints* samplePositions_RAS, int latticeSize[3])
{
  vtkImageData* image = vtkImageData::SafeDownCast(this->GetInput());
  if (!image)
  {
    return false;
  }
  // The whole image: step over the voxels so that the samples are SamplingSpacingMm apart.
  int extent[6] = { 0, -1, 0, -1, 0, -1 };
  image->GetExtent(extent);
  vtkNew<vtkMatrix4x4> latticeToRAS;
  latticeToRAS->DeepCopy(this->IJKToRAS);
  for (int axis = 0; axis < 3; ++axis)
  {
    int numberOfVoxels = extent[2 * axis + 1] - extent[2 * axis] + 1;
    double axisVector[3] = { this->IJKToRAS->GetElement(0, axis), this->IJKToRAS->GetElement(1, axis), this->IJKToRAS->GetElement(2, axis) };
    double voxelSizeMm = vtkMath::Norm(axisVector);
    int stride = 1;
    if (this->SamplingSpacingMm > 0.0 && voxelSizeMm > 0.0)
    {
      stride = std::max(1, static_cast<int>(this->SamplingSpacingMm / voxelSizeMm + 0.5));
    }
    latticeSize[axis] = std::max(1, numberOfVoxels / stride);
    for (int row = 0; row < 3; ++row)
    {
      latticeToRAS->SetElement(row, axis, this->IJKToRAS->GetElement(row, axis) * stride);
    }
  }
  // Start at the first voxel of the extent
  double firstVoxel_IJK[4] = { static_cast<double>(extent[0]), static_cast<double>(extent[2]), static_cast<double>(extent[4]), 1.0 };
  double firstVoxel_RAS[4] = { 0.0, 0.0, 0.0, 1.0 };
  this->IJKToRAS->MultiplyPoint(firstVoxel_IJK, firstVoxel_RAS);
  for (int row = 0; row < 3; ++row)
  {
    latticeToRAS->SetElement(row, 3, firstVoxel_RAS[row]);
  }
  vtkMRMLVectorFieldSampler::GetLatticePositions(latticeToRAS, latticeSize, samplePositions_RAS);
  return true;
}

//----------------------------------------------------------------------------
int vtkMRMLImageFieldSampler::RequestData(vtkInformation* vtkNotUsed(request), vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  vtkImageData* image = vtkImageData::GetData(inputVector[0]);
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
  if (!image || image->GetNumberOfScalarComponents() < 3 || numberOfSamples == 0)
  {
    output->SetPoints(nullptr);
    return 1;
  }

  vtkNew<vtkMatrix4x4> rasToIJK;
  vtkMatrix4x4::Invert(this->IJKToRAS, rasToIJK);

  double imageOrigin[3] = { 0.0, 0.0, 0.0 };
  double imageSpacing[3] = { 1.0, 1.0, 1.0 };
  image->GetOrigin(imageOrigin);
  image->GetSpacing(imageSpacing);

  vtkNew<vtkImageInterpolator> interpolator;
  interpolator->SetInterpolationModeToLinear();
  // Sampling positions can fall outside the image (a slice view is usually larger than the
  // volume); those samples must be zero, not the value of the closest voxel.
  interpolator->SetOutValue(0.0);
  interpolator->Initialize(image);
  interpolator->Update();

  // Every sampling position is kept, including the ones that fall outside the image, so
  // that the samples stay a regular lattice: the cells that connect them are what makes
  // contours, streamlines and deformed grids possible. Vectors outside the image are zero,
  // and a zero vector produces no visible glyph.
  vtkNew<vtkDoubleArray> vectors_RAS;
  vectors_RAS->SetNumberOfComponents(3);
  vectors_RAS->SetNumberOfTuples(numberOfSamples);

  int imageExtent[6] = { 0, -1, 0, -1, 0, -1 };
  image->GetExtent(imageExtent);

  int numberOfComponents = image->GetNumberOfScalarComponents();
  std::vector<double> voxelValue(numberOfComponents, 0.0);
  for (vtkIdType sampleIndex = 0; sampleIndex < numberOfSamples; ++sampleIndex)
  {
    double point_RAS[4] = { 0.0, 0.0, 0.0, 1.0 };
    samplePositions_RAS->GetPoint(sampleIndex, point_RAS);
    double point_IJK[4] = { 0.0, 0.0, 0.0, 1.0 };
    rasToIJK->MultiplyPoint(point_RAS, point_IJK);
    // vtkImageInterpolator works in the data coordinate system of the image
    double point_Data[3] = { imageOrigin[0] + point_IJK[0] * imageSpacing[0], //
                             imageOrigin[1] + point_IJK[1] * imageSpacing[1],
                             imageOrigin[2] + point_IJK[2] * imageSpacing[2] };
    bool insideImage = (point_IJK[0] >= imageExtent[0] && point_IJK[0] <= imageExtent[1]  //
                        && point_IJK[1] >= imageExtent[2] && point_IJK[1] <= imageExtent[3]
                        && point_IJK[2] >= imageExtent[4] && point_IJK[2] <= imageExtent[5]);
    if (insideImage)
    {
      interpolator->Interpolate(point_Data, voxelValue.data());
    }
    else
    {
      std::fill(voxelValue.begin(), voxelValue.end(), 0.0);
    }

    double vector_RAS[3] = { voxelValue[0], voxelValue[1], voxelValue[2] };
    if (!this->VectorsInRAS)
    {
      // The components are along the IJK axes, rotate them into RAS
      double vector_IJK[4] = { vector_RAS[0], vector_RAS[1], vector_RAS[2], 0.0 };
      double rotated_RAS[4] = { 0.0, 0.0, 0.0, 0.0 };
      this->IJKToRAS->MultiplyPoint(vector_IJK, rotated_RAS);
      vector_RAS[0] = rotated_RAS[0];
      vector_RAS[1] = rotated_RAS[1];
      vector_RAS[2] = rotated_RAS[2];
    }
    vectors_RAS->SetTuple(sampleIndex, vector_RAS);
  }

  output->SetPoints(samplePositions_RAS);
  vtkMRMLVectorFieldSampler::SetOutputVectors(output, vectors_RAS);
  if (this->GenerateCells)
  {
    vtkMRMLVectorFieldSampler::GenerateLatticeCells(output, latticeSize);
  }
  return 1;
}
