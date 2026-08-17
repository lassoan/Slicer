/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

// MRML includes
#include "vtkMRMLInMemoryVoxelDataProvider.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkDataArray.h>
#include <vtkExtractVOI.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLInMemoryVoxelDataProvider);

//----------------------------------------------------------------------------
vtkMRMLInMemoryVoxelDataProvider::vtkMRMLInMemoryVoxelDataProvider()
{
  this->StoredImageDataObserver = vtkSmartPointer<vtkCallbackCommand>::New();
  this->StoredImageDataObserver->SetClientData(this);
  this->StoredImageDataObserver->SetCallback(vtkMRMLInMemoryVoxelDataProvider::OnStoredImageDataModified);
}

//----------------------------------------------------------------------------
vtkMRMLInMemoryVoxelDataProvider::~vtkMRMLInMemoryVoxelDataProvider()
{
  this->SetStoredImageData(nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLInMemoryVoxelDataProvider::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << indent << "StoredImageData: " << (this->StoredImageData ? "set" : "(none)") << "\n";
}

//----------------------------------------------------------------------------
void vtkMRMLInMemoryVoxelDataProvider::SetStoredImageData(vtkImageData* imageData)
{
  if (imageData == this->StoredImageData)
  {
    return;
  }
  if (this->StoredImageData)
  {
    this->StoredImageData->RemoveObserver(this->StoredImageDataObserver);
    vtkDataArray* previousScalars = this->StoredImageData->GetPointData() ? this->StoredImageData->GetPointData()->GetScalars() : nullptr;
    if (previousScalars)
    {
      previousScalars->RemoveObserver(this->StoredImageDataObserver);
    }
  }
  this->StoredImageData = imageData;
  if (this->StoredImageData)
  {
    this->StoredImageData->AddObserver(vtkCommand::ModifiedEvent, this->StoredImageDataObserver);
    // The scalar array is observed in addition to the image because in-place
    // modification is commonly signaled by invoking Modified() on the scalar
    // array only (e.g. slicer.util.arrayFromVolumeModified).
    vtkDataArray* scalars = this->StoredImageData->GetPointData() ? this->StoredImageData->GetPointData()->GetScalars() : nullptr;
    if (scalars)
    {
      scalars->AddObserver(vtkCommand::ModifiedEvent, this->StoredImageDataObserver);
    }
  }
  this->Modified();
}

//----------------------------------------------------------------------------
vtkImageData* vtkMRMLInMemoryVoxelDataProvider::GetStoredImageData()
{
  return this->StoredImageData;
}

//----------------------------------------------------------------------------
vtkImageData* vtkMRMLInMemoryVoxelDataProvider::GetStoredImageDataIfInMemory()
{
  return this->StoredImageData;
}

//----------------------------------------------------------------------------
bool vtkMRMLInMemoryVoxelDataProvider::GetExtent(int extent[6], int resolutionLevel /*=0*/)
{
  if (!this->StoredImageData || resolutionLevel != 0)
  {
    return false;
  }
  this->StoredImageData->GetExtent(extent);
  return true;
}

//----------------------------------------------------------------------------
int vtkMRMLInMemoryVoxelDataProvider::GetScalarType()
{
  return this->StoredImageData ? this->StoredImageData->GetScalarType() : VTK_VOID;
}

//----------------------------------------------------------------------------
int vtkMRMLInMemoryVoxelDataProvider::GetNumberOfScalarComponents()
{
  return this->StoredImageData ? this->StoredImageData->GetNumberOfScalarComponents() : 0;
}

//----------------------------------------------------------------------------
bool vtkMRMLInMemoryVoxelDataProvider::GetRegion(vtkImageData* output, const int extent[6], int resolutionLevel /*=0*/)
{
  if (!output || !this->StoredImageData || resolutionLevel != 0)
  {
    return false;
  }
  int fullExtent[6] = { 0, -1, 0, -1, 0, -1 };
  this->StoredImageData->GetExtent(fullExtent);
  bool fullRegionRequested = true;
  for (int axis = 0; axis < 3; ++axis)
  {
    if (extent[2 * axis] < fullExtent[2 * axis] || extent[2 * axis + 1] > fullExtent[2 * axis + 1])
    {
      vtkErrorMacro("GetRegion: requested extent is outside the stored image extent");
      return false;
    }
    if (extent[2 * axis] != fullExtent[2 * axis] || extent[2 * axis + 1] != fullExtent[2 * axis + 1])
    {
      fullRegionRequested = false;
    }
  }
  if (fullRegionRequested)
  {
    output->ShallowCopy(this->StoredImageData);
    return true;
  }
  vtkNew<vtkExtractVOI> extractVOI;
  extractVOI->SetInputData(this->StoredImageData);
  extractVOI->SetVOI(extent[0], extent[1], extent[2], extent[3], extent[4], extent[5]);
  extractVOI->Update();
  output->ShallowCopy(extractVOI->GetOutput());
  return true;
}

//----------------------------------------------------------------------------
double vtkMRMLInMemoryVoxelDataProvider::GetVoxelValue(int i, int j, int k, int component /*=0*/)
{
  if (!this->StoredImageData)
  {
    return 0.0;
  }
  return this->StoredImageData->GetScalarComponentAsDouble(i, j, k, component);
}

//----------------------------------------------------------------------------
bool vtkMRMLInMemoryVoxelDataProvider::GetStoredScalarRange(double range[2])
{
  if (!this->StoredImageData)
  {
    return false;
  }
  this->StoredImageData->GetScalarRange(range);
  return true;
}

//----------------------------------------------------------------------------
void vtkMRMLInMemoryVoxelDataProvider::OnStoredImageDataModified(vtkObject* vtkNotUsed(caller),
                                                                 unsigned long vtkNotUsed(eid),
                                                                 void* clientData,
                                                                 void* vtkNotUsed(callData))
{
  vtkMRMLInMemoryVoxelDataProvider* self = reinterpret_cast<vtkMRMLInMemoryVoxelDataProvider*>(clientData);
  if (self)
  {
    // Forward stored image changes as provider modification so that the
    // owning volume node can regenerate derived (physical) data.
    self->Modified();
  }
}
