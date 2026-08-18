/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

// MRML includes
#include "vtkMRMLVoxelDataProvider.h"

//----------------------------------------------------------------------------
void vtkMRMLVoxelDataProvider::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << indent << "VoxelValueScale: " << this->VoxelValueScale << "\n";
  os << indent << "VoxelValueOffset: " << this->VoxelValueOffset << "\n";
  os << indent << "NumberOfResolutionLevels: " << this->GetNumberOfResolutionLevels() << "\n";
}

//----------------------------------------------------------------------------
bool vtkMRMLVoxelDataProvider::GetLevelScale(int resolutionLevel, double scale[3])
{
  if (resolutionLevel < 0 || resolutionLevel >= this->GetNumberOfResolutionLevels())
  {
    return false;
  }
  scale[0] = 1.0;
  scale[1] = 1.0;
  scale[2] = 1.0;
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLVoxelDataProvider::GetRegionIfAvailable(vtkImageData* output, const int extent[6], int resolutionLevel /*=0*/)
{
  // Base implementation: data is always available synchronously.
  return this->GetRegion(output, extent, resolutionLevel);
}

//----------------------------------------------------------------------------
bool vtkMRMLVoxelDataProvider::RequestRegionAsync(const int extent[6], int resolutionLevel)
{
  (void)extent;
  (void)resolutionLevel;
  // Base implementation is synchronous: nothing to request.
  return false;
}

//----------------------------------------------------------------------------
bool vtkMRMLVoxelDataProvider::IsVoxelValueScalingActive()
{
  return (this->VoxelValueScale != 1.0 || this->VoxelValueOffset != 0.0);
}

//----------------------------------------------------------------------------
double vtkMRMLVoxelDataProvider::GetPhysicalValueFromStoredValue(double storedValue)
{
  return this->VoxelValueScale * storedValue + this->VoxelValueOffset;
}

//----------------------------------------------------------------------------
double vtkMRMLVoxelDataProvider::GetStoredValueFromPhysicalValue(double physicalValue)
{
  if (this->VoxelValueScale == 0.0)
  {
    vtkWarningMacro("GetStoredValueFromPhysicalValue: invalid VoxelValueScale (0)");
    return 0.0;
  }
  return (physicalValue - this->VoxelValueOffset) / this->VoxelValueScale;
}
