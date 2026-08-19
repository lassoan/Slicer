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
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
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
  /// Number of worker threads that serve background region requests
  /// concurrently (several views can stream different regions at the same
  /// time). Default: 4. The pool grows on demand up to this count; reducing
  /// the count takes effect for providers that have not started their
  /// workers yet. Setting 0 (before the first request) disables background
  /// execution entirely: requests stay queued and only the synchronously
  /// published placeholders serve the views - intended for tests that need
  /// deterministic intermediate states.
  void SetNumberOfWorkerThreads(int numberOfThreads);
  int GetNumberOfWorkerThreads() { return this->NumberOfWorkerThreads; }
  //@}

  /// Total number of voxels read from the backing store so far (excluding
  /// voxels served from the caches). Diagnostic/testing aid: panning a view
  /// must only fetch the newly revealed part of its region, which this
  /// counter makes measurable.
  long long GetTotalFetchedVoxels();

  //@{
  /// Largest level (bytes) that is loaded and cached whole instead of
  /// serving chunk-granular region reads. Default: 512 MB. Tests lower it
  /// to exercise the region streaming machinery on small images.
  void SetWholeLevelPreferredBytes(long long bytes) { this->WholeLevelPreferredBytes = bytes; }
  long long GetWholeLevelPreferredBytes() { return this->WholeLevelPreferredBytes; }
  //@}

  //@{
  /// Approximate size (bytes) of the tiles that a region is read in
  /// (progress granularity and progressive display granularity).
  /// Default: 4 MB. Tests lower it to create multi-tile requests on small
  /// images.
  void SetTileTargetBytes(long long bytes) { this->TileTargetBytes = bytes; }
  long long GetTileTargetBytes() { return this->TileTargetBytes; }
  //@}

  //@{
  /// Pause between tile reads of a background request (milliseconds).
  /// Default: 0. Can be used to throttle background bandwidth/IO use;
  /// tests use it to observe and interrupt mid-stream states
  /// deterministically.
  void SetInterTileDelayMilliseconds(int milliseconds) { this->InterTileDelayMilliseconds = milliseconds; }
  int GetInterTileDelayMilliseconds() { return this->InterTileDelayMilliseconds; }
  //@}

  //@{
  /// vtkMRMLVoxelDataProvider interface
  int GetNumberOfResolutionLevels() override;
  bool GetLevelScale(int resolutionLevel, double scale[3]) override;
  bool GetLevelOffset(int resolutionLevel, double offset[3]) override;
  bool GetExtent(int extent[6], int resolutionLevel = 0) override;
  int GetScalarType() override;
  int GetNumberOfScalarComponents() override;
  bool GetRegion(vtkImageData* output, const int extent[6], int resolutionLevel = 0) override;
  bool GetRegionIfAvailable(vtkImageData* output, const int extent[6], int resolutionLevel = 0) override;
  bool IsRegionAvailable(const int extent[6], int resolutionLevel) override;
  bool IsRegionComplete(const int extent[6], int resolutionLevel) override;
  bool RequestRegionAsync(const int extent[6], int resolutionLevel) override;
  bool RequestRegionAsync(const int extent[6], int resolutionLevel, vtkObject* requester) override;
  void CancelRegionRequests(vtkObject* requester) override;
  bool HasPendingRegionRequests() override;
  double GetPendingRegionRequestProgress() override;
  double GetRegionRequestProgress(const int extent[6], int resolutionLevel) override;
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
    /// Physical position of the voxel-0 center in the multiscale coordinate
    /// space (the dataset's translation transform; ijk order). Coarser
    /// levels of a mean-downsampled pyramid are offset by half a fine voxel
    /// per downsampling step; ignoring this shifts the levels against each
    /// other on screen.
    double TranslationMM[3]{ 0.0, 0.0, 0.0 }; // ijk order
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

  void EnsureWorkers();
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
    /// Identity of the consumer the request was made for (nullptr for
    /// anonymous requests). Used only as a key into
    /// RequesterRegionsOfInterest; never dereferenced.
    vtkObject* Requester{ nullptr };
  };

  struct CachedRegion
  {
    int Level{ -1 };
    int Extent[6]{ 0, -1, 0, -1, 0, -1 };
    vtkSmartPointer<vtkImageData> Image;
    long long AccessStamp{ 0 };
    /// False while the region is being streamed in tiles: the image starts
    /// as an upsampled placeholder computed from a coarser level and tiles
    /// of real data replace it progressively.
    bool Complete{ true };
    /// Sub-boxes of an INCOMPLETE region that already hold real data (the
    /// tiles published so far). Data that was already displayed must never
    /// be downgraded: when a placeholder for another region is composed,
    /// these boxes contribute real data even if the stream that produced
    /// them was interrupted (e.g. abandoned because the user zoomed out).
    /// Empty for complete regions (the whole extent is valid).
    std::vector<std::array<int, 6>> ValidExtents;
  };

  /// Returns true if some requester is still interested in the request:
  /// its current region of interest is at the same level and intersects the
  /// request (partial overlap keeps the request alive because completed
  /// tiles are reused from the cache). Anonymous requests (no requester)
  /// and whole-level requests (cached permanently, reusable by every view)
  /// are always wanted. The caller must hold Mutex.
  bool IsRequestWanted(const RegionRequest& request);

  /// Remove queued requests that no requester is interested in anymore,
  /// notifying their (former) consumers. The caller must hold Mutex.
  void DropUnwantedQueuedRequests();

  /// Find a cached region that covers the extent (nullptr if none).
  /// Prefers a complete region; requireComplete skips incomplete ones.
  /// The caller must hold Mutex.
  CachedRegion* FindCoveringCachedRegion(const int extent[6], int level, bool requireComplete = false);

  /// Create the progressive placeholder for a region request and publish it
  /// to the region cache (marked incomplete) so that views can display it
  /// immediately while the real data is fetched. The placeholder is composed
  /// from ALL cached data, so that swapping to it never shows lower
  /// resolution than what is already on screen: the finest cached whole
  /// level as the base (covers the full region), overlaid with complete
  /// cached regions of other levels resampled into their overlap (applied
  /// coarsest to finest so the best data wins), overlaid with exact copies
  /// from complete same-level cached regions.
  /// Returns the published image (nullptr if no whole level is cached).
  vtkSmartPointer<vtkImageData> PublishPlaceholderRegion(const int extent[6], int level);

  /// Largest level (bytes) that is loaded and cached whole instead of
  /// serving chunk-granular region reads (see SetWholeLevelPreferredBytes)
  long long WholeLevelPreferredBytes{ 512LL * 1024LL * 1024LL };
  /// Approximate tile size of region reads (see SetTileTargetBytes)
  long long TileTargetBytes{ 4LL * 1024LL * 1024LL };
  /// Pause between tile reads (see SetInterTileDelayMilliseconds)
  int InterTileDelayMilliseconds{ 0 };
  /// Hard cap on the number of cached regions (the effective limit is the
  /// byte budget in TrimRegionCache: several views each keep their own
  /// display regions cached, and evicting a region that a view still shows
  /// forces that view to re-fetch it on its next update)
  static constexpr size_t MaximumCachedRegions = 32;

  /// Evict least recently used cached regions while the region cache exceeds
  /// its byte budget (an eighth of the level load budget). Regions that
  /// belong to an executing request are never evicted.
  /// The caller must hold Mutex.
  void TrimRegionCache();

  /// A background request being executed by a worker thread. The region is
  /// read in multiple smaller tiles: the tile counters provide real
  /// progress, and for progressive (tile-by-tile) display the worker reads
  /// tiles into the private image while the main thread
  /// (ProcessPendingRegionRequests) copies completed tiles into the
  /// published (displayed) placeholder image.
  struct ActiveRequest
  {
    RegionRequest Request;
    int TilesTotal{ 0 };
    int TilesCompleted{ 0 };
    double Bytes{ 0.0 };
    std::chrono::steady_clock::time_point StartTime;
    vtkSmartPointer<vtkImageData> PublishedImage;
    vtkSmartPointer<vtkImageData> PrivateImage;
    std::vector<std::array<int, 6>> CompletedTileExtents;
    /// Worker finished (success or failure); for progressive requests the
    /// main thread then copies the remaining tiles and finalizes.
    bool Finished{ false };
    bool Succeeded{ false };
  };

  std::mutex Mutex; // guards LevelImages, LevelAccessOrder, RegionCache, PendingRequests, ActiveRequests, CompletedLevels, RequesterRegionsOfInterest, WorkerShouldStop
  std::map<int, vtkSmartPointer<vtkImageData>> LevelImages;
  /// Monotonic access stamps for LRU eviction (kept in sync with LevelImages)
  std::map<int, long long> LevelAccessOrder;
  long long AccessCounter{ 0 };
  std::vector<CachedRegion> RegionCache;
  int NumberOfWorkerThreads{ 4 };
  std::vector<std::thread> Workers;
  std::condition_variable Condition;
  bool WorkerShouldStop{ false };
  std::deque<RegionRequest> PendingRequests;
  std::vector<std::shared_ptr<ActiveRequest>> ActiveRequests;
  /// Current region of interest of each known requester (WholeLevel unused).
  /// Requests that intersect no requester's current interest are cancelled
  /// (queued: dropped; executing: abandoned between tiles). The keys are
  /// identity-only pointers (never dereferenced); a stale entry of a
  /// destroyed requester merely keeps matching requests running, which is
  /// the pre-cancellation behavior.
  std::map<vtkObject*, RegionRequest> RequesterRegionsOfInterest;
  std::vector<int> CompletedLevels;
  /// Exponential moving average of the observed retrieval throughput
  double ThroughputBytesPerSecond{ 0.0 };
  /// Total voxels read from the store (cache hits excluded); see
  /// GetTotalFetchedVoxels()
  long long FetchedVoxelCount{ 0 };
};

#endif
