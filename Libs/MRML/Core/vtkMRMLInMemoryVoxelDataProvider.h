/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

#ifndef __vtkMRMLInMemoryVoxelDataProvider_h
#define __vtkMRMLInMemoryVoxelDataProvider_h

// MRML includes
#include "vtkMRMLVoxelDataProvider.h"

// VTK includes
#include <vtkSmartPointer.h>

class vtkCallbackCommand;

/// \brief Voxel data provider that keeps all stored voxels in memory.
///
/// The simplest backend of vtkMRMLVoxelDataProvider: the full-resolution
/// stored image (for example an int16 volume loaded from file) is held in a
/// single vtkImageData, together with the stored-to-physical value mapping
/// (VoxelValueScale/VoxelValueOffset).
///
/// The provider observes the stored image and invokes Modified() on itself
/// whenever the image changes, so the owning volume node can regenerate
/// derived (physical) data. Aware code that modifies stored voxels in place
/// must call Modified() on the stored image (standard VTK practice).
class VTK_MRML_EXPORT vtkMRMLInMemoryVoxelDataProvider : public vtkMRMLVoxelDataProvider
{
public:
  static vtkMRMLInMemoryVoxelDataProvider* New();
  vtkTypeMacro(vtkMRMLInMemoryVoxelDataProvider, vtkMRMLVoxelDataProvider);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set/get the stored image. The provider keeps a reference (no copy).
  void SetStoredImageData(vtkImageData* imageData);
  vtkImageData* GetStoredImageData();

  bool GetExtent(int extent[6], int resolutionLevel = 0) override;
  int GetScalarType() override;
  int GetNumberOfScalarComponents() override;
  bool GetRegion(vtkImageData* output, const int extent[6], int resolutionLevel = 0) override;
  double GetVoxelValue(int i, int j, int k, int component = 0) override;
  bool GetStoredScalarRange(double range[2]) override;
  vtkImageData* GetStoredImageDataIfInMemory() override;

protected:
  vtkMRMLInMemoryVoxelDataProvider();
  ~vtkMRMLInMemoryVoxelDataProvider() override;
  vtkMRMLInMemoryVoxelDataProvider(const vtkMRMLInMemoryVoxelDataProvider&) = delete;
  void operator=(const vtkMRMLInMemoryVoxelDataProvider&) = delete;

  static void OnStoredImageDataModified(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

  vtkSmartPointer<vtkImageData> StoredImageData;
  vtkSmartPointer<vtkCallbackCommand> StoredImageDataObserver;
};

#endif
