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

  /// Grid scale of a resolution level relative to level 0 (full resolution),
  /// in ijk axis order. For example {2.0, 2.0, 1.0} for a level downsampled
  /// by 2 in-plane. Returns false for an invalid level.
  virtual bool GetLevelScale(int resolutionLevel, double scale[3]);

  /// Offset of the voxel-0 CENTER of a resolution level relative to the
  /// voxel-0 center of level 0, in level-0 voxel units (ijk axis order):
  ///   level-0 index = offset + level index * scale
  /// Multiscale stores declare this as per-dataset translation transforms:
  /// mean-downsampling places a coarser level's voxel-0 center half a fine
  /// voxel past the fine voxel-0 center along each downsampled axis.
  /// Consumers that map between levels must include this offset, otherwise
  /// the levels appear slightly shifted against each other.
  /// Returns false for an invalid level. The base implementation returns a
  /// zero offset.
  virtual bool GetLevelOffset(int resolutionLevel, double offset[3]);

  /// The resolution level that corresponds to the geometry (extent, spacing,
  /// IJK to RAS matrix) of the owning volume node. Multi-resolution backends
  /// may use a coarser preview level as reference so that the volume can be
  /// loaded and displayed quickly; finer levels are then retrieved on demand
  /// (e.g. by slice views, according to the current zoom).
  virtual int GetReferenceResolutionLevel() { return 0; }

  //@{
  /// Asynchronous region retrieval with progressive refinement support.
  ///
  /// GetRegionIfAvailable() returns the region immediately if the backend
  /// can produce it without expensive work (level data already cached);
  /// otherwise it returns false and the caller may call RequestRegionAsync()
  /// to fetch it in the background and use a coarser available level in the
  /// meantime. When a background request completes, the provider invokes
  /// RegionReadyEvent (from the main thread, during
  /// ProcessPendingRegionRequests()).
  ///
  /// The base implementation is synchronous: GetRegionIfAvailable() forwards
  /// to GetRegion() and RequestRegionAsync() does nothing (the data is
  /// always available).
  virtual bool GetRegionIfAvailable(vtkImageData* output, const int extent[6], int resolutionLevel = 0);

  /// Returns true if GetRegionIfAvailable() would succeed for this region
  /// (without producing the data).
  virtual bool IsRegionAvailable(const int extent[6], int resolutionLevel)
  {
    (void)extent;
    (void)resolutionLevel;
    return true;
  }

  /// Returns true if COMPLETE data of this region is available. A region can
  /// be available but incomplete: backends with progressive (tile-by-tile)
  /// streaming publish a placeholder that is upsampled from a coarser level
  /// and gets replaced by real data as it arrives. Consumers must not treat
  /// an available region as final unless it is complete (otherwise a
  /// displayed placeholder whose streaming got interrupted would never be
  /// replaced). The base implementation equals IsRegionAvailable().
  virtual bool IsRegionComplete(const int extent[6], int resolutionLevel) { return this->IsRegionAvailable(extent, resolutionLevel); }

  /// Returns true if the resolution level can be retrieved AS A WHOLE within
  /// the provider's memory budget. Whole-level consumers (e.g. loading the
  /// volume node grid at a chosen level) must not request levels that are
  /// not loadable. Display consumers should gate on IsRegionLoadable()
  /// instead: a backend with chunk-granular access can serve regions of
  /// levels that are too large to load whole.
  /// The base implementation returns true (data is already in memory).
  virtual bool IsLevelLoadable(int resolutionLevel)
  {
    (void)resolutionLevel;
    return true;
  }

  /// Returns true if a region of this size can be retrieved at the level
  /// within the provider's memory budget (regardless of whether the whole
  /// level would fit). The base implementation returns true.
  virtual bool IsRegionLoadable(const int extent[6], int resolutionLevel)
  {
    (void)extent;
    (void)resolutionLevel;
    return true;
  }

  /// Allow the provider to release cached data that is not needed anymore.
  /// \a keepResolutionLevel is the level currently displayed/used (in
  /// addition to any level the provider must always keep, such as the
  /// reference level). The base implementation does nothing.
  virtual void ReleaseUnusedLevels(int keepResolutionLevel) { (void)keepResolutionLevel; }

  /// Request that the data needed for the given region/level is fetched in
  /// the background. Returns true if a request was queued (or is pending).
  virtual bool RequestRegionAsync(const int extent[6], int resolutionLevel);

  /// Request asynchronous fetching on behalf of a specific requester (for
  /// example a slice view layer or a volume rendering pipeline).
  /// A requester has one current region of interest per provider: this call
  /// records it, withdrawing the requester's interest in whatever it
  /// requested before. Backends can then cancel queued or executing
  /// background work that no requester is interested in anymore (the user
  /// moved on), instead of spending bandwidth on completing it.
  /// The requester pointer is used only as an identity key; it is never
  /// dereferenced and no reference is held.
  /// The base implementation ignores the requester.
  virtual bool RequestRegionAsync(const int extent[6], int resolutionLevel, vtkObject* requester)
  {
    (void)requester;
    return this->RequestRegionAsync(extent, resolutionLevel);
  }

  /// Withdraw a requester's region of interest (the requester is being
  /// destroyed or switched to a different data source), allowing the backend
  /// to cancel background work that only this requester was waiting for.
  /// The base implementation does nothing.
  virtual void CancelRegionRequests(vtkObject* requester) { (void)requester; }

  /// Returns true if a background request is pending.
  virtual bool HasPendingRegionRequests() { return false; }

  /// Progress of the current background region request: -1 if no request is
  /// pending or executing, otherwise an estimate between 0 and 1 (backends
  /// typically estimate from the request size and the observed retrieval
  /// throughput). Used by views to display a loading indicator.
  virtual double GetPendingRegionRequestProgress() { return -1.0; }

  /// Progress of the background request that serves this specific region:
  /// -1 if no pending or executing request covers the region at this level,
  /// otherwise 0..1. Views use this to display a loading indicator only for
  /// the region they are showing.
  virtual double GetRegionRequestProgress(const int extent[6], int resolutionLevel)
  {
    (void)extent;
    (void)resolutionLevel;
    return -1.0;
  }

  /// Called periodically on the main thread (by the application) to finalize
  /// completed background requests and invoke RegionReadyEvent.
  virtual void ProcessPendingRegionRequests() {}

  enum
  {
    /// Invoked (on the main thread) when a background region request
    /// completed and the data can now be retrieved with
    /// GetRegionIfAvailable().
    RegionReadyEvent = 24000
  };
  //@}

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
