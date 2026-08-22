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
#include "vtkMRMLVectorFieldSampler.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

// STD includes
#include <algorithm>

namespace
{
const char VectorArrayName[] = "Vector";
const char MagnitudeArrayName[] = "Magnitude";
} // namespace

//----------------------------------------------------------------------------
vtkMRMLVectorFieldSampler::vtkMRMLVectorFieldSampler()
{
  this->RegionToRAS = vtkSmartPointer<vtkMatrix4x4>::New();
  this->RegionSize[0] = 0;
  this->RegionSize[1] = 0;
  this->RegionSize[2] = 0;
  this->SliceXYToRAS = vtkSmartPointer<vtkMatrix4x4>::New();
  this->FieldOfViewSizeMm[0] = 0.0;
  this->FieldOfViewSizeMm[1] = 0.0;
  this->OutputLatticeSize[0] = 0;
  this->OutputLatticeSize[1] = 0;
  this->OutputLatticeSize[2] = 0;
  this->SetNumberOfInputPorts(0);
}

//----------------------------------------------------------------------------
vtkMRMLVectorFieldSampler::~vtkMRMLVectorFieldSampler() = default;

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldSampler::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "RegionSize: " << this->RegionSize[0] << " " << this->RegionSize[1] << " " << this->RegionSize[2] << "\n";
  os << indent << "SamplingSpacingMm: " << this->SamplingSpacingMm << "\n";
  os << indent << "FieldOfViewSizeMm: " << this->FieldOfViewSizeMm[0] << " " << this->FieldOfViewSizeMm[1] << "\n";
  os << indent << "CanSampleOnSlice: " << (this->CanSampleOnSlice() ? "true" : "false") << "\n";
}

//----------------------------------------------------------------------------
const char* vtkMRMLVectorFieldSampler::GetVectorArrayName()
{
  return VectorArrayName;
}

//----------------------------------------------------------------------------
const char* vtkMRMLVectorFieldSampler::GetMagnitudeArrayName()
{
  return MagnitudeArrayName;
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldSampler::SetSamplingRegion(vtkMatrix4x4* regionToRAS, const int size[3])
{
  bool modified = false;
  if (regionToRAS)
  {
    for (int row = 0; row < 4 && !modified; ++row)
    {
      for (int col = 0; col < 4; ++col)
      {
        if (this->RegionToRAS->GetElement(row, col) != regionToRAS->GetElement(row, col))
        {
          modified = true;
          break;
        }
      }
    }
    if (modified)
    {
      this->RegionToRAS->DeepCopy(regionToRAS);
    }
  }
  if (size)
  {
    for (int i = 0; i < 3; ++i)
    {
      if (this->RegionSize[i] != size[i])
      {
        this->RegionSize[i] = size[i];
        modified = true;
      }
    }
  }
  if (modified)
  {
    this->Modified();
  }
}

//----------------------------------------------------------------------------
vtkMatrix4x4* vtkMRMLVectorFieldSampler::GetRegionToRAS()
{
  return this->RegionToRAS;
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldSampler::SetSlicePlane(vtkMatrix4x4* sliceXYToRAS, const double fieldOfViewSizeMm[2])
{
  bool modified = false;
  if (sliceXYToRAS)
  {
    for (int row = 0; row < 4 && !modified; ++row)
    {
      for (int col = 0; col < 4; ++col)
      {
        if (this->SliceXYToRAS->GetElement(row, col) != sliceXYToRAS->GetElement(row, col))
        {
          modified = true;
          break;
        }
      }
    }
    if (modified)
    {
      this->SliceXYToRAS->DeepCopy(sliceXYToRAS);
    }
  }
  if (fieldOfViewSizeMm)
  {
    for (int i = 0; i < 2; ++i)
    {
      if (this->FieldOfViewSizeMm[i] != fieldOfViewSizeMm[i])
      {
        this->FieldOfViewSizeMm[i] = fieldOfViewSizeMm[i];
        modified = true;
      }
    }
  }
  if (modified)
  {
    this->Modified();
  }
}

//----------------------------------------------------------------------------
vtkMatrix4x4* vtkMRMLVectorFieldSampler::GetSliceXYToRAS()
{
  return this->SliceXYToRAS;
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldSampler::SetSamplePositions(vtkPoints* samplePositions_RAS)
{
  if (this->SamplePositions == samplePositions_RAS)
  {
    return;
  }
  this->SamplePositions = samplePositions_RAS;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkPoints* vtkMRMLVectorFieldSampler::GetSamplePositions()
{
  return this->SamplePositions;
}

//----------------------------------------------------------------------------
vtkMTimeType vtkMRMLVectorFieldSampler::GetMTime()
{
  vtkMTimeType mTime = this->Superclass::GetMTime();
  vtkMTimeType sampledObjectMTime = this->GetSampledObjectMTime();
  if (sampledObjectMTime > mTime)
  {
    mTime = sampledObjectMTime;
  }
  if (this->SamplePositions && this->SamplePositions->GetMTime() > mTime)
  {
    mTime = this->SamplePositions->GetMTime();
  }
  return mTime;
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldSampler::SetOutputVectors(vtkPointSet* outputPointSet, vtkDataArray* vectors_RAS)
{
  if (!outputPointSet || !vectors_RAS)
  {
    return;
  }
  vectors_RAS->SetName(VectorArrayName);
  outputPointSet->GetPointData()->SetVectors(vectors_RAS);

  vtkIdType numberOfTuples = vectors_RAS->GetNumberOfTuples();
  vtkNew<vtkDoubleArray> magnitudes;
  magnitudes->SetName(MagnitudeArrayName);
  magnitudes->SetNumberOfValues(numberOfTuples);
  for (vtkIdType i = 0; i < numberOfTuples; ++i)
  {
    double vector[3] = { 0.0, 0.0, 0.0 };
    vectors_RAS->GetTuple(i, vector);
    magnitudes->SetValue(i, vtkMath::Norm(vector));
  }
  outputPointSet->GetPointData()->SetScalars(magnitudes);
}

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldSampler::GenerateLatticeCells(vtkUnstructuredGrid* outputGrid, const int latticeSize[3])
{
  if (!outputGrid || !latticeSize)
  {
    return;
  }
  // A lattice that is flat along an axis (a slice) has quads instead of voxels, and a
  // lattice that is a single line or point has no cells at all.
  int numberOfCells[3] = { 0, 0, 0 };
  int numberOfAxesWithCells = 0;
  for (int axis = 0; axis < 3; ++axis)
  {
    numberOfCells[axis] = std::max(0, latticeSize[axis] - 1);
    if (numberOfCells[axis] > 0)
    {
      ++numberOfAxesWithCells;
    }
  }
  if (numberOfAxesWithCells < 2)
  {
    return;
  }

  auto pointId = [&latticeSize](int i, int j, int k)
  { return static_cast<vtkIdType>(i) + static_cast<vtkIdType>(j) * latticeSize[0] + static_cast<vtkIdType>(k) * latticeSize[0] * latticeSize[1]; };

  vtkNew<vtkCellArray> cells;
  if (numberOfAxesWithCells == 3)
  {
    cells->AllocateEstimate(static_cast<vtkIdType>(numberOfCells[0]) * numberOfCells[1] * numberOfCells[2], 8);
    for (int k = 0; k < numberOfCells[2]; ++k)
    {
      for (int j = 0; j < numberOfCells[1]; ++j)
      {
        for (int i = 0; i < numberOfCells[0]; ++i)
        {
          // VTK_VOXEL point order
          vtkIdType voxelPointIds[8] = { pointId(i, j, k),         pointId(i + 1, j, k),         pointId(i, j + 1, k),         pointId(i + 1, j + 1, k),
                                         pointId(i, j, k + 1),     pointId(i + 1, j, k + 1),     pointId(i, j + 1, k + 1),     pointId(i + 1, j + 1, k + 1) };
          cells->InsertNextCell(8, voxelPointIds);
        }
      }
    }
    outputGrid->SetCells(VTK_VOXEL, cells);
    return;
  }

  // Flat lattice (a single slice of samples): quads in the plane of the two axes that have
  // more than one sample.
  int firstAxis = -1;
  int secondAxis = -1;
  for (int axis = 0; axis < 3; ++axis)
  {
    if (numberOfCells[axis] > 0)
    {
      (firstAxis < 0 ? firstAxis : secondAxis) = axis;
    }
  }
  cells->AllocateEstimate(static_cast<vtkIdType>(numberOfCells[firstAxis]) * numberOfCells[secondAxis], 4);
  for (int second = 0; second < numberOfCells[secondAxis]; ++second)
  {
    for (int first = 0; first < numberOfCells[firstAxis]; ++first)
    {
      int corner00[3] = { 0, 0, 0 };
      corner00[firstAxis] = first;
      corner00[secondAxis] = second;
      int corner10[3] = { corner00[0], corner00[1], corner00[2] };
      corner10[firstAxis] += 1;
      int corner11[3] = { corner10[0], corner10[1], corner10[2] };
      corner11[secondAxis] += 1;
      int corner01[3] = { corner00[0], corner00[1], corner00[2] };
      corner01[secondAxis] += 1;
      vtkIdType quadPointIds[4] = { pointId(corner00[0], corner00[1], corner00[2]),
                                    pointId(corner10[0], corner10[1], corner10[2]),
                                    pointId(corner11[0], corner11[1], corner11[2]),
                                    pointId(corner01[0], corner01[1], corner01[2]) };
      cells->InsertNextCell(4, quadPointIds);
    }
  }
  outputGrid->SetCells(VTK_QUAD, cells);
}
