/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

#ifndef __vtkMRMLOMEZarrVoxelDataProvider_h
#define __vtkMRMLOMEZarrVoxelDataProvider_h

// MRML includes
#include "vtkMRMLVoxelDataProvider.h"

// VTK includes
#include <vtkSmartPointer.h>

// STD includes
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/// \brief Multi-resolution voxel data provider backed by an OME-Zarr (NGFF)
/// image stored as a zarr v2 directory.
///
/// The provider parses the multiscale metadata (level paths, shapes, scales)
/// without reading voxel data. Each resolution level is loaded lazily (whole
/// level at a time, through ITK's OMEZarrNGFFImageIO) and cached in memory.
///
/// The owning volume node's geometry corresponds to the reference resolution
/// level (a coarser preview level for large images, so that loading and
/// initial display is fast). Slice views retrieve finer levels on demand
/// according to the current zoom, using the asynchronous region API for
/// progressive refinement: a coarser cached level is displayed immediately
/// and RegionReadyEvent signals when the requested level became available.
///
/// Small levels are loaded and cached whole. Levels that are too large to
/// cache (or too expensive to fetch, e.g. from a remote store) are served
/// chunk-granularly: only the chunks intersecting the requested region are
/// read, and recently used regions are kept in a small region cache.
class VTK_MRML_EXPORT vtkMRMLOMEZarrVoxelDataProvider : public vtkMRMLVoxelDataProvider
{
public:
  static vtkMRMLOMEZarrVoxelDataProvider* New();
  vtkTypeMacro(vtkMRMLOMEZarrVoxelDataProvider, vtkMRMLVoxelDataProvider);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set the OME-Zarr directory and parse the multiscale metadata
  /// (no voxel data is read). Returns false if the metadata cannot be parsed.
  bool SetFileName(const std::string& fileName);
  const std::string& GetFileName() const { return this->FileName; }

  /// Returns the finest resolution level whose total number of voxels does
  /// not exceed the specified limit (the coarsest level if all are larger).
  int PickReferenceResolutionLevel(long long maxNumberOfVoxels);
  void SetReferenceResolutionLevel(int level);
  int GetReferenceResolutionLevel() override { return this->ReferenceLevel; }

  /// Seed the level cache with an already-loaded image (e.g. the reference
  /// level image that the storage node read).
  void SetLevelImageData(int level, vtkImageData* image);
  bool IsLevelLoaded(int level);

  /// Physical voxel spacing of a level from the file metadata (ijk order).
  bool GetLevelSpacing(int level, double spacing[3]);

  /// Release cached levels other than the reference level.
  void ReleaseCachedLevels();

  //@{
  /// Maximum amount of memory (bytes) that loading a single resolution level
  /// may require. Loading a level transiently needs about twice its data
  /// size (ITK reading buffer + VTK image copy), which is accounted for
  /// here. Levels above the budget are reported as not loadable
  /// (IsLevelLoadable) and are never fetched; views then use the finest
  /// loadable level. Default: a quarter of the total physical memory.
  void SetMaximumLevelLoadBytes(long long bytes);
  long long GetMaximumLevelLoadBytes();

  /// Uncompressed in-memory size of a level (bytes).
  long long GetLevelMemoryBytes(int resolutionLevel);
  //@}

  //@{
  /// vtkMRMLVoxelDataProvider interface
  int GetNumberOfResolutionLevels() override;
  bool GetLevelScale(int resolutionLevel, double scale[3]) override;
  bool GetExtent(int extent[6], int resolutionLevel = 0) override;
  int GetScalarType() override;
  int GetNumberOfScalarComponents() override;
  bool GetRegion(vtkImageData* output, const int extent[6], int resolutionLevel = 0) override;
  bool GetRegionIfAvailable(vtkImageData* output, const int extent[6], int resolutionLevel = 0) override;
  bool IsRegionAvailable(const int extent[6], int resolutionLevel) override;
  bool RequestRegionAsync(const int extent[6], int resolutionLevel) override;
  bool HasPendingRegionRequests() override;
  void ProcessPendingRegionRequests() override;
  double GetVoxelValue(int i, int j, int k, int component = 0) override;
  bool GetStoredScalarRange(double range[2]) override;
  vtkImageData* GetStoredImageDataIfInMemory() override;
  bool IsLevelLoadable(int resolutionLevel) override;
  bool IsRegionLoadable(const int extent[6], int resolutionLevel) override;
  void ReleaseUnusedLevels(int keepResolutionLevel) override;
  //@}

protected:
  vtkMRMLOMEZarrVoxelDataProvider();
  ~vtkMRMLOMEZarrVoxelDataProvider() override;
  vtkMRMLOMEZarrVoxelDataProvider(const vtkMRMLOMEZarrVoxelDataProvider&) = delete;
  void operator=(const vtkMRMLOMEZarrVoxelDataProvider&) = delete;

  struct LevelInfo
  {
    std::string Path;
    int Extent[6]{ 0, -1, 0, -1, 0, -1 }; // ijk order
    double SpacingMM[3]{ 1.0, 1.0, 1.0 }; // ijk order
  };

  /// Load a level synchronously (used on the main thread and by the worker).
  /// Does not touch the cache; returns the loaded image.
  vtkSmartPointer<vtkImageData> LoadLevelImage(int level);

  /// Discover the resolution levels of a remote (HTTP/HTTPS) store by probing
  /// increasing dataset indices with the ITK reader (only metadata is
  /// fetched). Used by SetFileName() for remote stores, where the zarr
  /// metadata files cannot be read from the local file system.
  bool ProbeRemoteLevels();

  /// Ensure a level is present in the cache (synchronous).
  bool EnsureLevelLoaded(int level);

  vtkSmartPointer<vtkImageData> GetCachedLevelImage(int level);

  void EnsureWorker();
  void WorkerLoop();

  std::string FileName;
  std::vector<LevelInfo> Levels;
  int ReferenceLevel{ 0 };
  int ScalarType{ VTK_VOID };
  int NumberOfComponents{ 1 };
  /// 0 = not initialized yet (computed from physical memory on first use)
  long long MaximumLevelLoadBytes{ 0 };

  struct RegionRequest
  {
    int Level{ -1 };
    int Extent[6]{ 0, -1, 0, -1, 0, -1 };
    /// Load and cache the whole level (small levels) instead of just the
    /// requested region (chunk-granular access to large levels)
    bool WholeLevel{ false };
  };

  struct CachedRegion
  {
    int Level{ -1 };
    int Extent[6]{ 0, -1, 0, -1, 0, -1 };
    vtkSmartPointer<vtkImageData> Image;
    long long AccessStamp{ 0 };
  };

  /// Find a cached region that covers the extent (nullptr if none).
  /// The caller must hold Mutex.
  CachedRegion* FindCoveringCachedRegion(const int extent[6], int level);

  /// Largest level (bytes) that is loaded and cached whole instead of
  /// serving chunk-granular region reads
  static constexpr long long WholeLevelPreferredBytes = 512LL * 1024LL * 1024LL;
  /// Number of recently used regions kept per provider
  static constexpr size_t MaximumCachedRegions = 4;

  std::mutex Mutex; // guards LevelImages, LevelAccessOrder, RegionCache, PendingRequest, HasPendingRequest, CompletedLevels, WorkerShouldStop
  std::map<int, vtkSmartPointer<vtkImageData>> LevelImages;
  /// Monotonic access stamps for LRU eviction (kept in sync with LevelImages)
  std::map<int, long long> LevelAccessOrder;
  long long AccessCounter{ 0 };
  std::vector<CachedRegion> RegionCache;
  std::thread Worker;
  std::condition_variable Condition;
  bool WorkerShouldStop{ false };
  RegionRequest PendingRequest;
  bool HasPendingRequest{ false };
  std::vector<int> CompletedLevels;
};

#endif
