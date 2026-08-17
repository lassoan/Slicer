/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

#ifndef __vtkMRMLVoxelDataProvider_h
#define __vtkMRMLVoxelDataProvider_h

// MRML includes
#include "vtkMRML.h"

// VTK includes
#include <vtkObject.h>

class vtkImageData;

/// \brief Abstract interface that supplies stored voxel values of a volume.
///
/// A voxel data provider is the authoritative representation of a volume's
/// voxel data, decoupled from the materialized vtkImageData that legacy
/// consumers receive from vtkMRMLVolumeNode::GetImageData().
///
/// Stored values are the values held by the backend (for example 16-bit
/// integers read from a file). Physical values are what the user should see
/// (for example cm/s), computed as:
///
///   physical = VoxelValueScale * stored + VoxelValueOffset
///
/// This mirrors DICOM Rescale Slope/Intercept and Real World Value Mapping
/// semantics. VoxelValueScale = 1 and VoxelValueOffset = 0 mean stored and
/// physical values are identical.
///
/// Aware consumers query the provider for regions, single voxels, and
/// precomputed statistics, and apply the value mapping themselves; they never
/// force a full-volume materialization. Unaware consumers keep using
/// vtkMRMLVolumeNode::GetImageData(), which returns physical values.
///
/// Backends:
/// - vtkMRMLInMemoryVoxelDataProvider: keeps all stored voxels in memory
///   (single resolution level).
/// - Future: chunked/multiscale out-of-core backends (e.g. OME-Zarr/NGFF via
///   ITK IOOMEZarrNGFF/TensorStore), which read only requested chunks.
///
/// Backends must invoke Modified() on the provider whenever the stored voxel
/// data changes, so that observers (such as the owning volume node) can
/// regenerate derived data.
class VTK_MRML_EXPORT vtkMRMLVoxelDataProvider : public vtkObject
{
public:
  vtkTypeMacro(vtkMRMLVoxelDataProvider, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Number of resolution levels provided by the backend.
  /// Level 0 is full resolution; each further level is coarser.
  /// In-memory backends provide a single level.
  virtual int GetNumberOfResolutionLevels() { return 1; }

  /// Get the extent of the stored image at a resolution level.
  /// Returns false if the provider has no data or the level is invalid.
  virtual bool GetExtent(int extent[6], int resolutionLevel = 0) = 0;

  /// Scalar type of the stored values (VTK type id, e.g. VTK_SHORT).
  virtual int GetScalarType() = 0;

  /// Number of scalar components of the stored image.
  virtual int GetNumberOfScalarComponents() = 0;

  /// Fill \a output with stored voxel values covering \a extent at
  /// \a resolutionLevel. The extent must be within GetExtent() of the level.
  /// Returns false on failure.
  virtual bool GetRegion(vtkImageData* output, const int extent[6], int resolutionLevel = 0) = 0;

  /// Get the stored value of a single voxel at full resolution.
  virtual double GetVoxelValue(int i, int j, int k, int component = 0) = 0;

  /// Get the range of stored values without forcing a full-volume scan.
  /// Out-of-core backends should return precomputed metadata.
  /// Returns false if the range is not available.
  virtual bool GetStoredScalarRange(double range[2]) = 0;

  /// If the backend keeps the full-resolution stored image in memory as a
  /// vtkImageData, return it (no copy); otherwise return nullptr.
  /// This lets the owning node avoid copies for in-memory backends.
  virtual vtkImageData* GetStoredImageDataIfInMemory() { return nullptr; }

  /// Multiplicative part of the stored-to-physical value mapping.
  /// Must not be 0.
  vtkSetMacro(VoxelValueScale, double);
  vtkGetMacro(VoxelValueScale, double);

  /// Additive part of the stored-to-physical value mapping.
  vtkSetMacro(VoxelValueOffset, double);
  vtkGetMacro(VoxelValueOffset, double);

  /// Returns true if the value mapping is not the identity.
  bool IsVoxelValueScalingActive();

  /// Convert a stored value to a physical value.
  double GetPhysicalValueFromStoredValue(double storedValue);

  /// Convert a physical value to a stored value.
  /// Returns 0 and logs a warning if VoxelValueScale is 0.
  double GetStoredValueFromPhysicalValue(double physicalValue);

protected:
  vtkMRMLVoxelDataProvider() = default;
  ~vtkMRMLVoxelDataProvider() override = default;
  vtkMRMLVoxelDataProvider(const vtkMRMLVoxelDataProvider&) = delete;
  void operator=(const vtkMRMLVoxelDataProvider&) = delete;

  double VoxelValueScale{ 1.0 };
  double VoxelValueOffset{ 0.0 };
};

#endif
