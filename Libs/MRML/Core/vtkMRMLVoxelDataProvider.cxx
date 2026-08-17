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
