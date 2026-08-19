/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

// MRML includes
#include "vtkMRMLOMEZarrVoxelDataProvider.h"

// VTK ITK includes
#include "vtkITKArchetypeImageSeriesScalarReader.h"

// VTK includes
#include <vtkDataArray.h>
#include <vtkErrorCode.h>
#include <vtkExtractVOI.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkInformation.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkStreamingDemandDrivenPipeline.h>

// RapidJSON includes
#include <rapidjson/document.h>

// VTKsys includes
#include <vtksys/SystemInformation.hxx>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace
{

//----------------------------------------------------------------------------
// Diagnostic trace of the background request handling, enabled by setting
// the SLICER_OMEZARR_TRACE environment variable to an output file path.
void ProviderTrace(const std::string& message)
{
  static const char* tracePath = std::getenv("SLICER_OMEZARR_TRACE");
  if (!tracePath)
  {
    return;
  }
  static std::mutex traceMutex;
  std::lock_guard<std::mutex> lock(traceMutex);
  std::ofstream traceFile(tracePath, std::ios::app);
  double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  traceFile.precision(3);
  traceFile << std::fixed << seconds << " " << message << "\n";
}

//----------------------------------------------------------------------------
std::string ProviderTraceExtent(const int extent[6], int level)
{
  std::ostringstream stream;
  stream << "L" << level << " [" << extent[0] << ":" << extent[1] << "," << extent[2] << ":" << extent[3] << "," << extent[4] << ":" << extent[5] << "]";
  return stream.str();
}

//----------------------------------------------------------------------------
bool ReadJsonDocument(const std::string& filePath, rapidjson::Document& document)
{
  std::ifstream jsonFile(filePath, std::ios::binary);
  if (!jsonFile.is_open())
  {
    return false;
  }
  std::stringstream buffer;
  buffer << jsonFile.rdbuf();
  std::string content = buffer.str();
  document.Parse(content.c_str());
  return !document.HasParseError();
}

//----------------------------------------------------------------------------
int ZarrDtypeToVTKScalarType(const std::string& dtype)
{
  // Strip byte order character (<, >, |)
  std::string t = dtype;
  if (!t.empty() && (t[0] == '<' || t[0] == '>' || t[0] == '|'))
  {
    t = t.substr(1);
  }
  if (t == "u1")
  {
    return VTK_UNSIGNED_CHAR;
  }
  if (t == "i1")
  {
    return VTK_SIGNED_CHAR;
  }
  if (t == "u2")
  {
    return VTK_UNSIGNED_SHORT;
  }
  if (t == "i2")
  {
    return VTK_SHORT;
  }
  if (t == "u4")
  {
    return VTK_UNSIGNED_INT;
  }
  if (t == "i4")
  {
    return VTK_INT;
  }
  if (t == "f4")
  {
    return VTK_FLOAT;
  }
  if (t == "f8")
  {
    return VTK_DOUBLE;
  }
  return VTK_VOID;
}

} // namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLOMEZarrVoxelDataProvider);

//----------------------------------------------------------------------------
vtkMRMLOMEZarrVoxelDataProvider::vtkMRMLOMEZarrVoxelDataProvider() = default;

//----------------------------------------------------------------------------
vtkMRMLOMEZarrVoxelDataProvider::~vtkMRMLOMEZarrVoxelDataProvider()
{
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    this->WorkerShouldStop = true;
  }
  this->Condition.notify_all();
  for (std::thread& worker : this->Workers)
  {
    if (worker.joinable())
    {
      worker.join();
    }
  }
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::SetNumberOfWorkerThreads(int numberOfThreads)
{
  this->NumberOfWorkerThreads = std::max(0, std::min(16, numberOfThreads));
  bool hasPendingRequests = false;
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    hasPendingRequests = !this->PendingRequests.empty();
  }
  if (!this->Workers.empty() || hasPendingRequests)
  {
    // Grow the running pool on demand (also when requests queued up while
    // the pool was disabled); shrinking takes effect for providers that
    // have not started their workers yet
    this->EnsureWorkers();
    this->Condition.notify_all();
  }
}

//----------------------------------------------------------------------------
long long vtkMRMLOMEZarrVoxelDataProvider::GetTotalFetchedVoxels()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  return this->FetchedVoxelCount;
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << indent << "FileName: " << this->FileName << "\n";
  os << indent << "NumberOfResolutionLevels: " << this->Levels.size() << "\n";
  os << indent << "ReferenceResolutionLevel: " << this->ReferenceLevel << "\n";
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::SetFileName(const std::string& fileName)
{
  this->FileName = fileName;
  this->Levels.clear();
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    this->LevelImages.clear();
  }

  if (vtkITKArchetypeImageSeriesReader::IsRemoteURL(fileName.c_str()))
  {
    // Remote store: the zarr metadata files cannot be read from the local
    // file system; discover the levels by probing the reader (this fetches
    // only the store metadata over HTTP, no pixel data).
    return this->ProbeRemoteLevels();
  }

  rapidjson::Document zattrs;
  if (!ReadJsonDocument(fileName + "/.zattrs", zattrs))
  {
    vtkErrorMacro("SetFileName: cannot read " << fileName << "/.zattrs");
    return false;
  }
  if (!zattrs.HasMember("multiscales") || !zattrs["multiscales"].IsArray() || zattrs["multiscales"].Empty())
  {
    vtkErrorMacro("SetFileName: no multiscales metadata in " << fileName);
    return false;
  }
  const rapidjson::Value& multiscale = zattrs["multiscales"][0];
  if (!multiscale.HasMember("datasets") || !multiscale["datasets"].IsArray())
  {
    vtkErrorMacro("SetFileName: no datasets in multiscales metadata of " << fileName);
    return false;
  }
  for (const rapidjson::Value& dataset : multiscale["datasets"].GetArray())
  {
    if (!dataset.HasMember("path") || !dataset["path"].IsString())
    {
      continue;
    }
    LevelInfo level;
    level.Path = dataset["path"].GetString();

    // Scale (stored in zyx axis order in NGFF, converted here to ijk=xyz)
    if (dataset.HasMember("coordinateTransformations") && dataset["coordinateTransformations"].IsArray())
    {
      for (const rapidjson::Value& transform : dataset["coordinateTransformations"].GetArray())
      {
        if (transform.HasMember("type") && std::string(transform["type"].GetString()) == "scale" //
            && transform.HasMember("scale") && transform["scale"].IsArray() && transform["scale"].Size() >= 3)
        {
          const rapidjson::Value& scale = transform["scale"];
          int numberOfAxes = static_cast<int>(scale.Size());
          for (int axis = 0; axis < 3; ++axis)
          {
            // last 3 axes are z,y,x; ijk order is x,y,z
            level.SpacingMM[axis] = scale[numberOfAxes - 1 - axis].GetDouble();
          }
        }
        // Translation: physical position of the level's voxel-0 center.
        // Mean-downsampled pyramids offset each coarser level by half a
        // fine voxel; without it the levels appear shifted on screen.
        if (transform.HasMember("type") && std::string(transform["type"].GetString()) == "translation" //
            && transform.HasMember("translation") && transform["translation"].IsArray() && transform["translation"].Size() >= 3)
        {
          const rapidjson::Value& translation = transform["translation"];
          int numberOfAxes = static_cast<int>(translation.Size());
          for (int axis = 0; axis < 3; ++axis)
          {
            level.TranslationMM[axis] = translation[numberOfAxes - 1 - axis].GetDouble();
          }
        }
      }
    }

    // Shape from the level's .zarray (stored in zyx axis order)
    rapidjson::Document zarray;
    if (!ReadJsonDocument(fileName + "/" + level.Path + "/.zarray", zarray))
    {
      vtkErrorMacro("SetFileName: cannot read .zarray of level " << level.Path);
      return false;
    }
    if (!zarray.HasMember("shape") || !zarray["shape"].IsArray() || zarray["shape"].Size() < 3)
    {
      vtkErrorMacro("SetFileName: invalid shape in .zarray of level " << level.Path);
      return false;
    }
    const rapidjson::Value& shape = zarray["shape"];
    int numberOfDims = static_cast<int>(shape.Size());
    for (int axis = 0; axis < 3; ++axis)
    {
      level.Extent[2 * axis] = 0;
      level.Extent[2 * axis + 1] = shape[numberOfDims - 1 - axis].GetInt() - 1;
    }
    if (this->Levels.empty() && zarray.HasMember("dtype") && zarray["dtype"].IsString())
    {
      this->ScalarType = ZarrDtypeToVTKScalarType(zarray["dtype"].GetString());
    }
    this->Levels.push_back(level);
  }
  if (this->Levels.empty())
  {
    vtkErrorMacro("SetFileName: no resolution levels found in " << fileName);
    return false;
  }
  this->ReferenceLevel = 0;
  this->Modified();
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::ProbeRemoteLevels()
{
  const int maximumNumberOfLevels = 16;
  for (int levelIndex = 0; levelIndex < maximumNumberOfLevels; ++levelIndex)
  {
    vtkNew<vtkITKArchetypeImageSeriesScalarReader> reader;
    reader->SetArchetype(this->FileName.c_str());
    reader->SetSingleFile(1);
    reader->SetDatasetIndex(levelIndex);
    reader->SetOutputScalarTypeToNative();
    reader->SetUseNativeOriginOn();
    reader->UpdateInformation();
    if (reader->GetErrorCode() != vtkErrorCode::NoError)
    {
      // First dataset index that cannot be read: end of the pyramid
      break;
    }
    vtkInformation* outInfo = reader->GetOutputInformation(0);
    if (!outInfo || !outInfo->Has(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()))
    {
      break;
    }
    LevelInfo level;
    level.Path = std::to_string(levelIndex);
    outInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), level.Extent);
    if (level.Extent[1] < level.Extent[0])
    {
      break;
    }
    // Per-level spacing (mm) from the reader's RAS to IJK matrix: the
    // reader output image itself uses unit spacing (Slicer convention)
    vtkMatrix4x4* rasToIjk = reader->GetRasToIjkMatrix();
    if (rasToIjk)
    {
      vtkNew<vtkMatrix4x4> ijkToRas;
      vtkMatrix4x4::Invert(rasToIjk, ijkToRas);
      for (int axis = 0; axis < 3; ++axis)
      {
        double columnNorm = std::sqrt(ijkToRas->GetElement(0, axis) * ijkToRas->GetElement(0, axis)     //
                                      + ijkToRas->GetElement(1, axis) * ijkToRas->GetElement(1, axis)   //
                                      + ijkToRas->GetElement(2, axis) * ijkToRas->GetElement(2, axis)); //
        level.SpacingMM[axis] = (columnNorm > 0.0) ? columnNorm : 1.0;
      }
      // Per-level translation (NGFF translation transform) arrives in the
      // reader's origin. Zarr grids are axis-aligned and the reader builds
      // RAS from LPS by negating the first two axes.
      level.TranslationMM[0] = -ijkToRas->GetElement(0, 3);
      level.TranslationMM[1] = -ijkToRas->GetElement(1, 3);
      level.TranslationMM[2] = ijkToRas->GetElement(2, 3);
    }
    if (this->Levels.empty())
    {
      this->ScalarType = reader->GetOutputScalarType();
      this->NumberOfComponents = std::max(1, static_cast<int>(reader->GetNumberOfComponents()));
    }
    this->Levels.push_back(level);
  }
  if (this->Levels.empty())
  {
    vtkErrorMacro("ProbeRemoteLevels: no readable resolution levels found at " << this->FileName);
    return false;
  }
  this->ReferenceLevel = 0;
  this->Modified();
  return true;
}

//----------------------------------------------------------------------------
int vtkMRMLOMEZarrVoxelDataProvider::PickReferenceResolutionLevel(long long maxNumberOfVoxels)
{
  int pickedLevel = static_cast<int>(this->Levels.size()) - 1;
  for (size_t level = 0; level < this->Levels.size(); ++level)
  {
    const int* extent = this->Levels[level].Extent;
    long long numberOfVoxels = static_cast<long long>(extent[1] - extent[0] + 1)   //
                               * static_cast<long long>(extent[3] - extent[2] + 1) //
                               * static_cast<long long>(extent[5] - extent[4] + 1);
    if (numberOfVoxels <= maxNumberOfVoxels)
    {
      pickedLevel = static_cast<int>(level);
      break;
    }
  }
  return pickedLevel;
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::SetReferenceResolutionLevel(int level)
{
  if (level < 0 || level >= static_cast<int>(this->Levels.size()) || level == this->ReferenceLevel)
  {
    return;
  }
  this->ReferenceLevel = level;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::SetLevelImageData(int level, vtkImageData* image)
{
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    if (image)
    {
      this->LevelImages[level] = image;
      this->LevelAccessOrder[level] = ++this->AccessCounter;
    }
    else
    {
      this->LevelImages.erase(level);
      this->LevelAccessOrder.erase(level);
    }
  }
  this->Modified();
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::IsLevelLoaded(int level)
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  return (this->LevelImages.find(level) != this->LevelImages.end());
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetLevelSpacing(int level, double spacing[3])
{
  if (level < 0 || level >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  for (int axis = 0; axis < 3; ++axis)
  {
    spacing[axis] = this->Levels[level].SpacingMM[axis];
  }
  return true;
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::SetMaximumLevelLoadBytes(long long bytes)
{
  this->MaximumLevelLoadBytes = bytes;
}

//----------------------------------------------------------------------------
long long vtkMRMLOMEZarrVoxelDataProvider::GetMaximumLevelLoadBytes()
{
  if (this->MaximumLevelLoadBytes <= 0)
  {
    // Default: a quarter of the total physical memory. Loading a level
    // transiently needs about twice its data size, so this keeps a single
    // level load below about half of the physical memory.
    vtksys::SystemInformation systemInfo;
    systemInfo.RunMemoryCheck();
    long long totalPhysicalMB = static_cast<long long>(systemInfo.GetTotalPhysicalMemory());
    long long totalPhysicalBytes = totalPhysicalMB * 1024LL * 1024LL;
    this->MaximumLevelLoadBytes = (totalPhysicalBytes > 0) ? totalPhysicalBytes / 4 : 4LL * 1024LL * 1024LL * 1024LL;
  }
  return this->MaximumLevelLoadBytes;
}

//----------------------------------------------------------------------------
long long vtkMRMLOMEZarrVoxelDataProvider::GetLevelMemoryBytes(int resolutionLevel)
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return 0;
  }
  const int* extent = this->Levels[resolutionLevel].Extent;
  long long numberOfVoxels = static_cast<long long>(extent[1] - extent[0] + 1)   //
                             * static_cast<long long>(extent[3] - extent[2] + 1) //
                             * static_cast<long long>(extent[5] - extent[4] + 1);
  int scalarSize = vtkDataArray::GetDataTypeSize(this->ScalarType != VTK_VOID ? this->ScalarType : VTK_SHORT);
  return numberOfVoxels * scalarSize * this->NumberOfComponents;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::IsLevelLoadable(int resolutionLevel)
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  if (this->IsLevelLoaded(resolutionLevel))
  {
    return true;
  }
  // Loading a level transiently needs about twice its data size
  // (ITK reading buffer + VTK image copy).
  return (2 * this->GetLevelMemoryBytes(resolutionLevel) <= this->GetMaximumLevelLoadBytes());
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::ReleaseUnusedLevels(int keepResolutionLevel)
{
  // Levels are only evicted when the total cache size exceeds the memory
  // budget, in least-recently-used order. Levels must never be evicted just
  // because one consumer stopped using them: several views may display
  // different levels of the same volume simultaneously, and eager eviction
  // would make the views endlessly reload each other's evicted levels.
  long long cacheBudget = this->GetMaximumLevelLoadBytes();
  std::lock_guard<std::mutex> lock(this->Mutex);
  for (;;)
  {
    long long totalBytes = 0;
    for (const auto& levelImage : this->LevelImages)
    {
      totalBytes += this->GetLevelMemoryBytes(levelImage.first);
    }
    if (totalBytes <= cacheBudget)
    {
      return;
    }
    // Evict the least recently used level (never the reference level or the
    // level that the caller currently uses)
    int lruLevel = -1;
    long long lruStamp = 0;
    for (const auto& levelImage : this->LevelImages)
    {
      int level = levelImage.first;
      if (level == this->ReferenceLevel || level == keepResolutionLevel)
      {
        continue;
      }
      long long stamp = this->LevelAccessOrder.count(level) ? this->LevelAccessOrder[level] : 0;
      if (lruLevel < 0 || stamp < lruStamp)
      {
        lruLevel = level;
        lruStamp = stamp;
      }
    }
    if (lruLevel < 0)
    {
      return; // nothing evictable
    }
    this->LevelImages.erase(lruLevel);
    this->LevelAccessOrder.erase(lruLevel);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::ReleaseCachedLevels()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  for (auto it = this->LevelImages.begin(); it != this->LevelImages.end();)
  {
    if (it->first != this->ReferenceLevel)
    {
      this->LevelAccessOrder.erase(it->first);
      it = this->LevelImages.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

//----------------------------------------------------------------------------
int vtkMRMLOMEZarrVoxelDataProvider::GetNumberOfResolutionLevels()
{
  return static_cast<int>(this->Levels.size());
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetLevelScale(int resolutionLevel, double scale[3])
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  for (int axis = 0; axis < 3; ++axis)
  {
    scale[axis] = this->Levels[resolutionLevel].SpacingMM[axis] / this->Levels[0].SpacingMM[axis];
  }
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetLevelOffset(int resolutionLevel, double offset[3])
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  for (int axis = 0; axis < 3; ++axis)
  {
    offset[axis] = (this->Levels[resolutionLevel].TranslationMM[axis] - this->Levels[0].TranslationMM[axis]) //
                   / this->Levels[0].SpacingMM[axis];
  }
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetExtent(int extent[6], int resolutionLevel /*=0*/)
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  for (int i = 0; i < 6; ++i)
  {
    extent[i] = this->Levels[resolutionLevel].Extent[i];
  }
  return true;
}

//----------------------------------------------------------------------------
int vtkMRMLOMEZarrVoxelDataProvider::GetScalarType()
{
  return this->ScalarType;
}

//----------------------------------------------------------------------------
int vtkMRMLOMEZarrVoxelDataProvider::GetNumberOfScalarComponents()
{
  return this->NumberOfComponents;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> vtkMRMLOMEZarrVoxelDataProvider::GetCachedLevelImage(int level)
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  auto it = this->LevelImages.find(level);
  if (it == this->LevelImages.end())
  {
    return nullptr;
  }
  this->LevelAccessOrder[level] = ++this->AccessCounter;
  return it->second;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> vtkMRMLOMEZarrVoxelDataProvider::LoadLevelImage(int level)
{
  if (level < 0 || level >= static_cast<int>(this->Levels.size()))
  {
    return nullptr;
  }
  vtkNew<vtkITKArchetypeImageSeriesScalarReader> reader;
  reader->SetArchetype(this->FileName.c_str());
  reader->SetSingleFile(1);
  reader->SetDatasetIndex(level);
  reader->SetOutputScalarTypeToNative();
  reader->SetUseNativeOriginOn();
  reader->Update();
  vtkImageData* readerOutput = vtkImageData::SafeDownCast(reader->GetOutput());
  if (!readerOutput || !readerOutput->GetPointData() || !readerOutput->GetPointData()->GetScalars())
  {
    return nullptr;
  }
  vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  image->ShallowCopy(readerOutput);
  // Slicer convention: geometry is stored in the node, not in the image
  image->SetOrigin(0.0, 0.0, 0.0);
  image->SetSpacing(1.0, 1.0, 1.0);
  return image;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::EnsureLevelLoaded(int level)
{
  if (this->GetCachedLevelImage(level))
  {
    return true;
  }
  vtkSmartPointer<vtkImageData> image = this->LoadLevelImage(level);
  if (!image)
  {
    vtkErrorMacro("EnsureLevelLoaded: failed to load level " << level << " of " << this->FileName);
    return false;
  }
  std::lock_guard<std::mutex> lock(this->Mutex);
  this->LevelImages[level] = image;
  this->LevelAccessOrder[level] = ++this->AccessCounter;
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetRegion(vtkImageData* output, const int extent[6], int resolutionLevel /*=0*/)
{
  if (!output || resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  bool completeDataAvailable = this->IsLevelLoaded(resolutionLevel);
  if (!completeDataAvailable)
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    completeDataAvailable = (this->FindCoveringCachedRegion(extent, resolutionLevel, /*requireComplete=*/true) != nullptr);
  }
  if (completeDataAvailable)
  {
    return this->GetRegionIfAvailable(output, extent, resolutionLevel);
  }
  if (this->IsLevelLoadable(resolutionLevel) && this->GetLevelMemoryBytes(resolutionLevel) <= WholeLevelPreferredBytes)
  {
    // Small level: load and cache it whole
    if (!this->EnsureLevelLoaded(resolutionLevel))
    {
      return false;
    }
    return this->GetRegionIfAvailable(output, extent, resolutionLevel);
  }
  // Large level: chunk-granular synchronous read of just the region
  return vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegion(this->FileName.c_str(), resolutionLevel, extent, output);
}

//----------------------------------------------------------------------------
vtkMRMLOMEZarrVoxelDataProvider::CachedRegion* vtkMRMLOMEZarrVoxelDataProvider::FindCoveringCachedRegion(const int extent[6], int level, bool requireComplete /*=false*/)
{
  CachedRegion* incompleteMatch = nullptr;
  for (CachedRegion& cachedRegion : this->RegionCache)
  {
    if (cachedRegion.Level != level)
    {
      continue;
    }
    bool covers = true;
    for (int axis = 0; axis < 3; ++axis)
    {
      if (extent[2 * axis] < cachedRegion.Extent[2 * axis] || extent[2 * axis + 1] > cachedRegion.Extent[2 * axis + 1])
      {
        covers = false;
        break;
      }
    }
    if (!covers)
    {
      continue;
    }
    if (cachedRegion.Complete)
    {
      return &cachedRegion;
    }
    if (!incompleteMatch)
    {
      incompleteMatch = &cachedRegion;
    }
  }
  return requireComplete ? nullptr : incompleteMatch;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::IsRegionAvailable(const int extent[6], int resolutionLevel)
{
  if (this->IsLevelLoaded(resolutionLevel))
  {
    return true;
  }
  std::lock_guard<std::mutex> lock(this->Mutex);
  return (this->FindCoveringCachedRegion(extent, resolutionLevel) != nullptr);
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::IsRegionComplete(const int extent[6], int resolutionLevel)
{
  if (this->IsLevelLoaded(resolutionLevel))
  {
    return true;
  }
  std::lock_guard<std::mutex> lock(this->Mutex);
  CachedRegion* cachedRegion = this->FindCoveringCachedRegion(extent, resolutionLevel, /*requireComplete=*/true);
  if (cachedRegion)
  {
    // Consumers call this for the region they are displaying (e.g. in the
    // covered-early-return of the slice display update): refresh the LRU
    // stamp so that a displayed region is not evicted just because it did
    // not need to be re-fetched for a while.
    cachedRegion->AccessStamp = ++this->AccessCounter;
    return true;
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetRegionIfAvailable(vtkImageData* output, const int extent[6], int resolutionLevel /*=0*/)
{
  if (!output || resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  const int* levelExtent = this->Levels[resolutionLevel].Extent;
  for (int axis = 0; axis < 3; ++axis)
  {
    if (extent[2 * axis] < levelExtent[2 * axis] || extent[2 * axis + 1] > levelExtent[2 * axis + 1])
    {
      vtkErrorMacro("GetRegionIfAvailable: requested extent is outside the level extent");
      return false;
    }
  }
  // Source: the whole cached level, or a cached region that covers the extent
  vtkSmartPointer<vtkImageData> sourceImage = this->GetCachedLevelImage(resolutionLevel);
  if (!sourceImage)
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    CachedRegion* cachedRegion = this->FindCoveringCachedRegion(extent, resolutionLevel);
    if (!cachedRegion)
    {
      return false;
    }
    cachedRegion->AccessStamp = ++this->AccessCounter;
    sourceImage = cachedRegion->Image;
  }
  const int* sourceExtent = sourceImage->GetExtent();
  bool exactMatch = true;
  for (int axis = 0; axis < 3; ++axis)
  {
    if (extent[2 * axis] != sourceExtent[2 * axis] || extent[2 * axis + 1] != sourceExtent[2 * axis + 1])
    {
      exactMatch = false;
      break;
    }
  }
  if (exactMatch)
  {
    output->ShallowCopy(sourceImage);
    return true;
  }
  vtkNew<vtkExtractVOI> extractVOI;
  extractVOI->SetInputData(sourceImage);
  extractVOI->SetVOI(extent[0], extent[1], extent[2], extent[3], extent[4], extent[5]);
  extractVOI->Update();
  output->ShallowCopy(extractVOI->GetOutput());
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::RequestRegionAsync(const int extent[6], int resolutionLevel)
{
  return this->RequestRegionAsync(extent, resolutionLevel, nullptr);
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::RequestRegionAsync(const int extent[6], int resolutionLevel, vtkObject* requester)
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  bool placeholderNeeded = false;
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    // Record the requester's current region of interest FIRST (even if the
    // request itself is deduplicated below): it withdraws the requester's
    // interest in whatever it asked for before, letting queued requests that
    // nobody wants anymore be dropped here and executing ones abandon
    // between tiles (IsRequestWanted in the worker loop).
    if (requester)
    {
      RegionRequest interest;
      interest.Level = resolutionLevel;
      for (int i = 0; i < 6; ++i)
      {
        interest.Extent[i] = extent[i];
      }
      this->RequesterRegionsOfInterest[requester] = interest;
      ProviderTrace("roi " + ProviderTraceExtent(extent, resolutionLevel) + " requester=" + std::to_string(reinterpret_cast<uintptr_t>(requester)));
      this->DropUnwantedQueuedRequests();
    }
    // Complete data already cached: nothing to do. (An INCOMPLETE covering
    // region is only a placeholder that is being - or was - streamed; the
    // request is re-issued unless it is already being served, so that a
    // placeholder whose streaming got superseded is eventually completed.)
    if (this->LevelImages.find(resolutionLevel) != this->LevelImages.end() //
        || this->FindCoveringCachedRegion(extent, resolutionLevel, /*requireComplete=*/true) != nullptr)
    {
      return false;
    }
    // Already being served by an executing or queued request
    auto requestCovers = [&](const RegionRequest& request)
    {
      if (request.Level != resolutionLevel)
      {
        return false;
      }
      if (request.WholeLevel)
      {
        return true;
      }
      for (int axis = 0; axis < 3; ++axis)
      {
        if (extent[2 * axis] < request.Extent[2 * axis] || extent[2 * axis + 1] > request.Extent[2 * axis + 1])
        {
          return false;
        }
      }
      return true;
    };
    for (const std::shared_ptr<ActiveRequest>& active : this->ActiveRequests)
    {
      if (requestCovers(active->Request))
      {
        return true;
      }
    }
    for (const RegionRequest& queued : this->PendingRequests)
    {
      if (requestCovers(queued))
      {
        return true;
      }
    }
    placeholderNeeded = (this->FindCoveringCachedRegion(extent, resolutionLevel) == nullptr);
  }
  // Small levels are fetched and cached whole (reusable by all views);
  // levels above the threshold are accessed chunk-granularly, reading only
  // the requested region.
  bool wholeLevel = this->IsLevelLoadable(resolutionLevel) && this->GetLevelMemoryBytes(resolutionLevel) <= WholeLevelPreferredBytes;
  if (!wholeLevel && !this->IsRegionLoadable(extent, resolutionLevel))
  {
    return false;
  }
  if (placeholderNeeded)
  {
    // Publish the placeholder for the region SYNCHRONOUSLY (composed from
    // all cached data), so that the caller's view can swap to the target
    // region in this very update without ever dropping below the resolution
    // that is already on screen (waiting for a worker to publish it would
    // show a coarser fallback for a few frames - or, for whole-level
    // requests, until the whole level finished loading). Very large regions
    // (e.g. volume rendering at high zoom) are left to the worker so that
    // the caller is not blocked by a big allocation and resampling.
    long long numberOfVoxels = static_cast<long long>(extent[1] - extent[0] + 1)   //
                               * static_cast<long long>(extent[3] - extent[2] + 1) //
                               * static_cast<long long>(extent[5] - extent[4] + 1);
    long long bytesPerVoxel = vtkDataArray::GetDataTypeSize(this->ScalarType) * this->NumberOfComponents;
    const long long maximumSynchronousPlaceholderBytes = 64LL * 1024LL * 1024LL;
    if (numberOfVoxels * bytesPerVoxel <= maximumSynchronousPlaceholderBytes)
    {
      this->PublishPlaceholderRegion(extent, resolutionLevel);
    }
  }
  this->EnsureWorkers();
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    RegionRequest request;
    request.Level = resolutionLevel;
    request.WholeLevel = wholeLevel;
    request.Requester = requester;
    for (int i = 0; i < 6; ++i)
    {
      request.Extent[i] = extent[i];
    }
    this->PendingRequests.push_back(request);
    // Keep the queue SHORT (worker count): every live consumer re-requests
    // its target on each update, so a dropped stale entry costs at most one
    // event cycle, while a long queue of stale requests keeps the worker
    // pool busy long after the interaction that created them has moved on.
    const size_t maximumQueuedRequests = 4;
    while (this->PendingRequests.size() > maximumQueuedRequests)
    {
      // Notify so that the dropped request's consumer re-evaluates and can
      // re-request (a silent drop could leave a view waiting until the next
      // interaction)
      this->CompletedLevels.push_back(this->PendingRequests.front().Level);
      this->PendingRequests.pop_front();
    }
  }
  this->Condition.notify_one();
  return true;
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::CancelRegionRequests(vtkObject* requester)
{
  if (!requester)
  {
    return;
  }
  std::lock_guard<std::mutex> lock(this->Mutex);
  this->RequesterRegionsOfInterest.erase(requester);
  this->DropUnwantedQueuedRequests();
  // Executing requests re-evaluate IsRequestWanted between tiles and abandon
  // on their own.
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::IsRequestWanted(const RegionRequest& request)
{
  if (request.Requester == nullptr || request.WholeLevel)
  {
    // Anonymous requests carry no interest information, and whole levels are
    // cached permanently and reusable by every view: always complete them.
    return true;
  }
  for (const auto& requesterAndInterest : this->RequesterRegionsOfInterest)
  {
    const RegionRequest& interest = requesterAndInterest.second;
    if (interest.Level != request.Level)
    {
      continue;
    }
    bool intersects = true;
    for (int axis = 0; axis < 3 && intersects; ++axis)
    {
      intersects = (interest.Extent[2 * axis] <= request.Extent[2 * axis + 1] //
                    && interest.Extent[2 * axis + 1] >= request.Extent[2 * axis]);
    }
    if (intersects)
    {
      // Partial overlap keeps the request alive: its completed tiles are
      // reused from the region cache by the overlapping request.
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::DropUnwantedQueuedRequests()
{
  for (auto it = this->PendingRequests.begin(); it != this->PendingRequests.end();)
  {
    if (!this->IsRequestWanted(*it))
    {
      // Notify so that any consumer that was (unexpectedly) still waiting
      // for this region re-evaluates instead of stalling
      ProviderTrace("drop-queued " + ProviderTraceExtent(it->Extent, it->Level));
      this->CompletedLevels.push_back(it->Level);
      it = this->PendingRequests.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::HasPendingRegionRequests()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  return (!this->PendingRequests.empty() || !this->ActiveRequests.empty() || !this->CompletedLevels.empty());
}

//----------------------------------------------------------------------------
namespace
{
double vtkMRMLOMEZarrActiveRequestProgress(int tilesCompleted,
                                           int tilesTotal,
                                           double bytes,
                                           const std::chrono::steady_clock::time_point& startTime,
                                           double throughputBytesPerSecond)
{
  // Estimate from the request size and the observed retrieval throughput
  double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
  double estimate;
  if (throughputBytesPerSecond <= 0.0 || bytes <= 0.0)
  {
    // Unknown speed: fill gradually over ~10 seconds
    estimate = std::min(0.9, elapsedSeconds / 10.0);
  }
  else
  {
    double expectedSeconds = bytes / throughputBytesPerSecond;
    estimate = std::max(0.02, std::min(0.95, elapsedSeconds / expectedSeconds));
  }
  if (tilesTotal > 0)
  {
    // Tile progress, blended with the time estimate: the download cost is
    // often very unevenly distributed over the tiles (the tile that first
    // touches a chunk fetches it, later tiles reuse it from the cache), so
    // pure tile counting can sit still for a long time during the dominant
    // transfer and then jump to complete. The time estimate is allowed to
    // run ahead of the tile count by up to 30%, which keeps the indicator
    // moving without letting it drift arbitrarily far from real progress.
    double tileFraction = static_cast<double>(tilesCompleted) / tilesTotal;
    return std::min(0.98, std::max(tileFraction, std::min(tileFraction + 0.3, estimate)));
  }
  return estimate;
}
} // namespace

//----------------------------------------------------------------------------
double vtkMRMLOMEZarrVoxelDataProvider::GetPendingRegionRequestProgress()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  double progress = -1.0;
  for (const std::shared_ptr<ActiveRequest>& active : this->ActiveRequests)
  {
    double activeProgress =
      vtkMRMLOMEZarrActiveRequestProgress(active->TilesCompleted, active->TilesTotal, active->Bytes, active->StartTime, this->ThroughputBytesPerSecond);
    progress = std::max(progress, activeProgress);
  }
  if (progress < 0.0 && !this->PendingRequests.empty())
  {
    progress = 0.0;
  }
  return progress;
}

//----------------------------------------------------------------------------
double vtkMRMLOMEZarrVoxelDataProvider::GetRegionRequestProgress(const int extent[6], int resolutionLevel)
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  auto requestCovers = [&](const RegionRequest& request)
  {
    if (request.Level != resolutionLevel)
    {
      return false;
    }
    if (request.WholeLevel)
    {
      return true;
    }
    for (int axis = 0; axis < 3; ++axis)
    {
      if (extent[2 * axis] < request.Extent[2 * axis] || extent[2 * axis + 1] > request.Extent[2 * axis + 1])
      {
        return false;
      }
    }
    return true;
  };
  for (const std::shared_ptr<ActiveRequest>& active : this->ActiveRequests)
  {
    if (requestCovers(active->Request))
    {
      return vtkMRMLOMEZarrActiveRequestProgress(active->TilesCompleted, active->TilesTotal, active->Bytes, active->StartTime, this->ThroughputBytesPerSecond);
    }
  }
  for (const RegionRequest& queued : this->PendingRequests)
  {
    if (requestCovers(queued))
    {
      return 0.0;
    }
  }
  return -1.0;
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::ProcessPendingRegionRequests()
{
  std::vector<int> completed;
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    // Progressive (tile-by-tile) display: copy the tiles that the workers
    // completed since the last call from their private images into the
    // published placeholder images that views are displaying. The copy is
    // done on the main thread so that it cannot race with rendering.
    for (auto activeIt = this->ActiveRequests.begin(); activeIt != this->ActiveRequests.end();)
    {
      ActiveRequest* active = activeIt->get();
      if (!active->CompletedTileExtents.empty() && active->PublishedImage && active->PrivateImage)
      {
        long long bytesPerVoxel = active->PrivateImage->GetScalarSize() * active->PrivateImage->GetNumberOfScalarComponents();
        const int* privateExtent = active->PrivateImage->GetExtent();
        long long rowBytes = static_cast<long long>(privateExtent[1] - privateExtent[0] + 1) * bytesPerVoxel;
        for (const std::array<int, 6>& tileExtent : active->CompletedTileExtents)
        {
          for (int k = tileExtent[4]; k <= tileExtent[5]; ++k)
          {
            for (int j = tileExtent[2]; j <= tileExtent[3]; ++j)
            {
              memcpy(active->PublishedImage->GetScalarPointer(privateExtent[0], j, k), //
                     active->PrivateImage->GetScalarPointer(privateExtent[0], j, k),
                     rowBytes);
            }
          }
          this->CompletedLevels.push_back(active->Request.Level);
        }
        active->CompletedTileExtents.clear();
        active->PublishedImage->GetPointData()->GetScalars()->Modified();
      }
      // Once the worker finished successfully and all tiles were copied,
      // the region is complete
      if (active->Finished && active->Succeeded && active->CompletedTileExtents.empty())
      {
        ProviderTrace("finalize " + ProviderTraceExtent(active->Request.Extent, active->Request.Level));
        if (active->PublishedImage)
        {
          for (CachedRegion& cachedRegion : this->RegionCache)
          {
            if (cachedRegion.Image == active->PublishedImage)
            {
              cachedRegion.Complete = true;
            }
          }
        }
        activeIt = this->ActiveRequests.erase(activeIt);
      }
      else
      {
        ++activeIt;
      }
    }
    completed.swap(this->CompletedLevels);
  }
  for (int level : completed)
  {
    this->InvokeEvent(vtkMRMLVoxelDataProvider::RegionReadyEvent, &level);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::EnsureWorkers()
{
  this->WorkerShouldStop = false;
  while (this->Workers.size() < static_cast<size_t>(this->NumberOfWorkerThreads))
  {
    this->Workers.emplace_back(&vtkMRMLOMEZarrVoxelDataProvider::WorkerLoop, this);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::TrimRegionCache()
{
  long long maximumBytes = this->GetMaximumLevelLoadBytes() / 8;
  auto regionBytes = [](const CachedRegion& region) -> long long
  {
    if (!region.Image || !region.Image->GetPointData() || !region.Image->GetPointData()->GetScalars())
    {
      return 0;
    }
    return static_cast<long long>(region.Image->GetPointData()->GetScalars()->GetNumberOfTuples()) //
           * region.Image->GetPointData()->GetScalars()->GetNumberOfComponents() * region.Image->GetScalarSize();
  };
  auto isBeingStreamed = [this](const CachedRegion& region)
  {
    for (const std::shared_ptr<ActiveRequest>& active : this->ActiveRequests)
    {
      if (active->PublishedImage && active->PublishedImage == region.Image)
      {
        return true;
      }
    }
    return false;
  };
  long long totalBytes = 0;
  for (const CachedRegion& region : this->RegionCache)
  {
    totalBytes += regionBytes(region);
  }
  while ((totalBytes > maximumBytes || this->RegionCache.size() > MaximumCachedRegions) && !this->RegionCache.empty())
  {
    // Least recently used region that is not being streamed
    int oldestIndex = -1;
    for (size_t i = 0; i < this->RegionCache.size(); ++i)
    {
      if (isBeingStreamed(this->RegionCache[i]))
      {
        continue;
      }
      if (oldestIndex < 0 || this->RegionCache[i].AccessStamp < this->RegionCache[oldestIndex].AccessStamp)
      {
        oldestIndex = static_cast<int>(i);
      }
    }
    if (oldestIndex < 0)
    {
      break; // everything is being streamed
    }
    totalBytes -= regionBytes(this->RegionCache[oldestIndex]);
    this->RegionCache.erase(this->RegionCache.begin() + oldestIndex);
  }
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> vtkMRMLOMEZarrVoxelDataProvider::PublishPlaceholderRegion(const int extent[6], int level)
{
  // Sources: base = the finest cached WHOLE level (covers the full region);
  // overlays = complete cached regions of other levels (for example the
  // region that was displayed before a zoom), resampled into their overlap
  // and applied coarsest to finest so that the best data wins. Composing
  // from all cached data guarantees that swapping the view to this
  // placeholder never shows lower resolution than what was already on
  // screen.
  struct PlaceholderSource
  {
    int Level{ -1 };
    int Extent[6]{ 0, -1, 0, -1, 0, -1 };
    vtkSmartPointer<vtkImageData> Image;
    double VoxelVolume{ 1.0 }; // product of the level scales (coarseness)
  };
  auto levelVoxelVolume = [this](int sourceLevel) -> double
  {
    double scale[3] = { 1.0, 1.0, 1.0 };
    this->GetLevelScale(sourceLevel, scale);
    return scale[0] * scale[1] * scale[2];
  };
  vtkSmartPointer<vtkImageData> baseImage;
  int baseLevel = -1;
  double baseVoxelVolume = 0.0;
  std::vector<PlaceholderSource> overlays;
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    for (const auto& levelAndImage : this->LevelImages)
    {
      if (levelAndImage.first == level || !levelAndImage.second)
      {
        continue;
      }
      double voxelVolume = levelVoxelVolume(levelAndImage.first);
      if (!baseImage || voxelVolume < baseVoxelVolume)
      {
        baseImage = levelAndImage.second;
        baseLevel = levelAndImage.first;
        baseVoxelVolume = voxelVolume;
      }
    }
    for (CachedRegion& cachedRegion : this->RegionCache)
    {
      if (cachedRegion.Level == level || !cachedRegion.Complete || !cachedRegion.Image)
      {
        continue;
      }
      PlaceholderSource source;
      source.Level = cachedRegion.Level;
      for (int i = 0; i < 6; ++i)
      {
        source.Extent[i] = cachedRegion.Extent[i];
      }
      source.Image = cachedRegion.Image;
      source.VoxelVolume = levelVoxelVolume(cachedRegion.Level);
      overlays.push_back(source);
    }
  }
  if (!baseImage)
  {
    return nullptr;
  }
  // Coarsest first, so that finer overlays overwrite coarser ones
  std::sort(overlays.begin(), overlays.end(), [](const PlaceholderSource& a, const PlaceholderSource& b) { return a.VoxelVolume > b.VoxelVolume; });

  double targetScale[3] = { 1.0, 1.0, 1.0 };
  double targetOffset[3] = { 0.0, 0.0, 0.0 };
  if (!this->GetLevelScale(level, targetScale) || !this->GetLevelOffset(level, targetOffset))
  {
    return nullptr;
  }
  // Resample a source level onto the requested level's grid over
  // outputExtent: target index i_t maps to source index
  // (targetOffset - sourceOffset + i_t * targetScale) / sourceScale
  // (offsets are the level translations, i.e. the voxel-0 center positions,
  // in level-0 voxel units; all images use unit spacing and zero origin).
  auto resampleSource = [this, &targetScale, &targetOffset](vtkImageData* sourceImage, int sourceLevel, const int outputExtent[6]) -> vtkSmartPointer<vtkImageData>
  {
    double sourceScale[3] = { 1.0, 1.0, 1.0 };
    double sourceOffset[3] = { 0.0, 0.0, 0.0 };
    this->GetLevelScale(sourceLevel, sourceScale);
    this->GetLevelOffset(sourceLevel, sourceOffset);
    vtkNew<vtkMatrix4x4> resliceAxes;
    for (int axis = 0; axis < 3; ++axis)
    {
      resliceAxes->SetElement(axis, axis, targetScale[axis] / sourceScale[axis]);
      resliceAxes->SetElement(axis, 3, (targetOffset[axis] - sourceOffset[axis]) / sourceScale[axis]);
    }
    vtkNew<vtkImageReslice> reslice;
    reslice->SetInputData(sourceImage);
    reslice->SetResliceAxes(resliceAxes);
    reslice->SetInterpolationModeToLinear();
    reslice->SetOutputExtent(outputExtent[0], outputExtent[1], outputExtent[2], outputExtent[3], outputExtent[4], outputExtent[5]);
    reslice->SetOutputOrigin(0.0, 0.0, 0.0);
    reslice->SetOutputSpacing(1.0, 1.0, 1.0);
    reslice->Update();
    return reslice->GetOutput();
  };
  vtkSmartPointer<vtkImageData> placeholder = vtkSmartPointer<vtkImageData>::New();
  placeholder->DeepCopy(resampleSource(baseImage, baseLevel, extent));
  long long placeholderBytesPerVoxel = placeholder->GetScalarSize() * placeholder->GetNumberOfScalarComponents();
  for (const PlaceholderSource& overlay : overlays)
  {
    if (!overlay.Image || overlay.Image->GetScalarType() != placeholder->GetScalarType())
    {
      continue;
    }
    double sourceScale[3] = { 1.0, 1.0, 1.0 };
    double sourceOffset[3] = { 0.0, 0.0, 0.0 };
    this->GetLevelScale(overlay.Level, sourceScale);
    this->GetLevelOffset(overlay.Level, sourceOffset);
    // Target voxels whose source sample stays strictly inside the overlay
    // region (one source voxel is left off the high side so that linear
    // interpolation never has to clamp at the region border)
    int overlapExtent[6];
    bool overlaps = true;
    for (int axis = 0; axis < 3 && overlaps; ++axis)
    {
      double lowTarget = (sourceOffset[axis] - targetOffset[axis] + overlay.Extent[2 * axis] * sourceScale[axis]) / targetScale[axis];
      double highTarget = (sourceOffset[axis] - targetOffset[axis] + (overlay.Extent[2 * axis + 1] - 1) * sourceScale[axis]) / targetScale[axis];
      if (overlay.Extent[2 * axis + 1] == overlay.Extent[2 * axis])
      {
        // Single-voxel-thick source axis (e.g. the through-plane axis):
        // nothing to interpolate, allow the exact position
        highTarget = lowTarget;
      }
      overlapExtent[2 * axis] = std::max(extent[2 * axis], static_cast<int>(std::ceil(lowTarget - 1e-6)));
      overlapExtent[2 * axis + 1] = std::min(extent[2 * axis + 1], static_cast<int>(std::floor(highTarget + 1e-6)));
      overlaps = (overlapExtent[2 * axis] <= overlapExtent[2 * axis + 1]);
    }
    if (!overlaps)
    {
      continue;
    }
    vtkSmartPointer<vtkImageData> resampled = resampleSource(overlay.Image, overlay.Level, overlapExtent);
    long long overlayRowBytes = static_cast<long long>(overlapExtent[1] - overlapExtent[0] + 1) * placeholderBytesPerVoxel;
    for (int k = overlapExtent[4]; k <= overlapExtent[5]; ++k)
    {
      for (int j = overlapExtent[2]; j <= overlapExtent[3]; ++j)
      {
        memcpy(placeholder->GetScalarPointer(overlapExtent[0], j, k), resampled->GetScalarPointer(overlapExtent[0], j, k), overlayRowBytes);
      }
    }
  }
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    // Seed the placeholder with real data already cached for overlapping
    // regions of the same level (e.g. the previous pan position): the
    // overlapping parts appear at full quality immediately instead of
    // dropping back to the upsampled coarse level.
    long long bytesPerVoxel = placeholder->GetScalarSize() * placeholder->GetNumberOfScalarComponents();
    for (CachedRegion& sourceRegion : this->RegionCache)
    {
      if (sourceRegion.Level != level || !sourceRegion.Complete || !sourceRegion.Image)
      {
        continue;
      }
      int overlap[6];
      bool overlaps = true;
      for (int axis = 0; axis < 3 && overlaps; ++axis)
      {
        overlap[2 * axis] = std::max(extent[2 * axis], sourceRegion.Extent[2 * axis]);
        overlap[2 * axis + 1] = std::min(extent[2 * axis + 1], sourceRegion.Extent[2 * axis + 1]);
        overlaps = (overlap[2 * axis] <= overlap[2 * axis + 1]);
      }
      if (!overlaps || sourceRegion.Image->GetScalarType() != placeholder->GetScalarType())
      {
        continue;
      }
      long long rowBytes = static_cast<long long>(overlap[1] - overlap[0] + 1) * bytesPerVoxel;
      for (int k = overlap[4]; k <= overlap[5]; ++k)
      {
        for (int j = overlap[2]; j <= overlap[3]; ++j)
        {
          memcpy(placeholder->GetScalarPointer(overlap[0], j, k), sourceRegion.Image->GetScalarPointer(overlap[0], j, k), rowBytes);
        }
      }
    }
    CachedRegion cachedRegion;
    cachedRegion.Level = level;
    for (int i = 0; i < 6; ++i)
    {
      cachedRegion.Extent[i] = extent[i];
    }
    cachedRegion.Image = placeholder;
    cachedRegion.AccessStamp = ++this->AccessCounter;
    cachedRegion.Complete = false;
    this->RegionCache.push_back(cachedRegion);
    this->TrimRegionCache();
    // Announce so that views swap from the coarse fallback to the (initially
    // identical-looking, progressively sharpening) placeholder immediately
    this->CompletedLevels.push_back(level);
  }
  return placeholder;
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::WorkerLoop()
{
  for (;;)
  {
    std::shared_ptr<ActiveRequest> active;
    {
      std::unique_lock<std::mutex> lock(this->Mutex);
      this->Condition.wait(lock, [this] { return this->WorkerShouldStop || !this->PendingRequests.empty(); });
      if (this->WorkerShouldStop)
      {
        return;
      }
      // Serve the NEWEST queued request first: during continued interaction
      // (e.g. slice scrolling) the queue accumulates requests for slabs that
      // the user has already moved past; the currently visible one must not
      // wait behind them. Started reads still always run to completion, and
      // stale queued requests are either served later or dropped by the
      // queue bound (their requesters re-request if still interested).
      RegionRequest request = this->PendingRequests.back();
      this->PendingRequests.pop_back();
      ProviderTrace("pop " + ProviderTraceExtent(request.Extent, request.Level) + (request.WholeLevel ? " whole" : ""));
      bool alreadyAvailable = (this->LevelImages.find(request.Level) != this->LevelImages.end()) //
                              || (!request.WholeLevel && this->FindCoveringCachedRegion(request.Extent, request.Level, /*requireComplete=*/true) != nullptr);
      bool alreadyBeingServed = false;
      for (const std::shared_ptr<ActiveRequest>& other : this->ActiveRequests)
      {
        if (other->Request.Level == request.Level && other->Request.WholeLevel == request.WholeLevel)
        {
          bool sameExtent = true;
          for (int i = 0; i < 6 && sameExtent; ++i)
          {
            sameExtent = (other->Request.Extent[i] == request.Extent[i]);
          }
          if (sameExtent || request.WholeLevel)
          {
            alreadyBeingServed = true;
            break;
          }
        }
      }
      if (alreadyAvailable || alreadyBeingServed)
      {
        if (alreadyAvailable)
        {
          // Became available meanwhile: report as completed
          this->CompletedLevels.push_back(request.Level);
        }
        continue;
      }
      active = std::make_shared<ActiveRequest>();
      active->Request = request;
      active->StartTime = std::chrono::steady_clock::now();
      if (request.WholeLevel)
      {
        active->Bytes = static_cast<double>(this->GetLevelMemoryBytes(request.Level));
      }
      else
      {
        double numberOfVoxels = static_cast<double>(request.Extent[1] - request.Extent[0] + 1)   //
                                * static_cast<double>(request.Extent[3] - request.Extent[2] + 1) //
                                * static_cast<double>(request.Extent[5] - request.Extent[4] + 1);
        active->Bytes = numberOfVoxels * vtkDataArray::GetDataTypeSize(this->ScalarType) * this->NumberOfComponents;
      }
      this->ActiveRequests.push_back(active);
    }
    const RegionRequest request = active->Request;
    // Load outside the lock (this is the expensive part)
    auto finishRequest = [this, &active](bool success)
    {
      // The caller must hold Mutex. Update the observed throughput (EMA)
      // that the progress estimate of future requests is computed from.
      double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - active->StartTime).count();
      if (success && elapsedSeconds > 0.05 && active->Bytes > 0.0)
      {
        double bytesPerSecond = active->Bytes / elapsedSeconds;
        this->ThroughputBytesPerSecond = (this->ThroughputBytesPerSecond > 0.0) ? (0.5 * this->ThroughputBytesPerSecond + 0.5 * bytesPerSecond) : bytesPerSecond;
      }
      active->Finished = true;
      active->Succeeded = success;
    };
    // The region (or the whole level) is read in multiple smaller tiles:
    // TensorStore fetches only the chunks intersecting each tile, progress is
    // reported per tile, and a request that got superseded (e.g. the user
    // zoomed or panned further) is abandoned between tiles.
    int regionExtent[6];
    if (request.WholeLevel)
    {
      for (int i = 0; i < 6; ++i)
      {
        regionExtent[i] = this->Levels[request.Level].Extent[i];
      }
    }
    else
    {
      for (int i = 0; i < 6; ++i)
      {
        regionExtent[i] = request.Extent[i];
      }
    }
    // Open the store once for the whole request (opening fetches store
    // metadata, which for a remote store means several HTTP requests per
    // open; the tiles are read through the same session)
    int scalarType = VTK_VOID;
    std::unique_ptr<void, void (*)(void*)> sessionReader( //
      vtkITKArchetypeImageSeriesReader::OpenOMEZarrRegionReader(this->FileName.c_str(), request.Level, scalarType),
      &vtkITKArchetypeImageSeriesReader::CloseOMEZarrRegionReader);
    bool success = (sessionReader != nullptr);
    vtkSmartPointer<vtkImageData> image;
    bool abandoned = false;
    bool progressive = false;
    if (success)
    {
      image = vtkSmartPointer<vtkImageData>::New();
      image->SetExtent(regionExtent);
      image->SetOrigin(0.0, 0.0, 0.0);
      image->SetSpacing(1.0, 1.0, 1.0);
      image->AllocateScalars(scalarType, 1);

      if (!request.WholeLevel)
      {
        // Progressive display: the placeholder of the region is replaced
        // tile by tile with real data (the tiles are copied into the
        // published image by the main thread). RequestRegionAsync usually
        // published the placeholder already (synchronously, so that the
        // requesting view could swap to it immediately) - reuse it, but only
        // on an EXACT extent match: completing the request marks the
        // published cache entry complete, which must not cover a larger
        // area than the data that actually arrived.
        vtkSmartPointer<vtkImageData> placeholder;
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          for (CachedRegion& cachedRegion : this->RegionCache)
          {
            if (cachedRegion.Level == request.Level && !cachedRegion.Complete && cachedRegion.Image //
                && std::equal(cachedRegion.Extent, cachedRegion.Extent + 6, regionExtent))
            {
              placeholder = cachedRegion.Image;
              break;
            }
          }
        }
        if (!placeholder)
        {
          placeholder = this->PublishPlaceholderRegion(regionExtent, request.Level);
        }
        if (placeholder)
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          active->PublishedImage = placeholder;
          active->PrivateImage = image;
          progressive = true;
        }
      }

      // Tile plan: split along the LARGER of the j/k axes, so that for a
      // slab-shaped region (a slice view: thin k, large j) the tiles are
      // strips ACROSS the viewed plane and the progressive display visibly
      // sharpens strip by strip (splitting a slab along its thin axis would
      // deliver all visible content in a single tile). Thick regions
      // (volume rendering) split along k, which is contiguous in the output
      // buffer. ~4 MB per tile; chunks shared between tiles (e.g. j-strips
      // crossing the same chunk row) are served from the session's chunk
      // cache, not re-fetched.
      const long long tileTargetBytes = 4LL * 1024LL * 1024LL;
      int xDim = regionExtent[1] - regionExtent[0] + 1;
      int yDim = regionExtent[3] - regionExtent[2] + 1;
      int zDim = regionExtent[5] - regionExtent[4] + 1;
      long long bytesPerVoxel = image->GetScalarSize();
      int splitAxis = (zDim >= 8 && zDim >= yDim) ? 2 : 1;
      long long bytesPerUnit = (splitAxis == 2) ? (static_cast<long long>(xDim) * yDim * bytesPerVoxel) //
                                                : (static_cast<long long>(xDim) * zDim * bytesPerVoxel);
      int axisDim = (splitAxis == 2) ? zDim : yDim;
      int unitsPerTile = std::max(1, static_cast<int>(tileTargetBytes / std::max(1LL, bytesPerUnit)));
      int numberOfTiles = (axisDim + unitsPerTile - 1) / unitsPerTile;
      {
        std::lock_guard<std::mutex> lock(this->Mutex);
        active->TilesTotal = numberOfTiles;
        active->TilesCompleted = 0;
      }
      // Note: a started read always runs to completion, except at shutdown
      // and when NO requester is interested in it anymore (see below).
      // Several views request regions of the same provider; only the
      // guarantee that every read some view still waits for completes (and
      // its result is cached, triggering the unserved views to re-request)
      // makes the request handling converge.
      for (int tileIndex = 0; tileIndex < numberOfTiles && success; ++tileIndex)
      {
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          if (this->WorkerShouldStop)
          {
            return;
          }
          // Requester-aware cancellation: a request that no requester is
          // interested in anymore (every requester has since recorded a
          // different region of interest) is abandoned between tiles,
          // freeing the worker and the bandwidth for the regions the views
          // want NOW. The interest is re-evaluated on every tile rather
          // than latched, so a view that returns to the region before the
          // next tile keeps the request alive. A request that any requester
          // still wants ALWAYS runs to completion (abandoning wanted
          // requests is a proven livelock), and so do anonymous requests,
          // which carry no interest information. Abandoned requests notify
          // their (former) requesters, which re-request if somehow still
          // interested.
          if (!this->IsRequestWanted(active->Request))
          {
            ProviderTrace("abandon " + ProviderTraceExtent(active->Request.Extent, active->Request.Level) //
                          + " tiles=" + std::to_string(active->TilesCompleted) + "/" + std::to_string(active->TilesTotal));
            abandoned = true;
          }
        }
        if (abandoned)
        {
          break;
        }
        int tileExtent[6];
        for (int i = 0; i < 6; ++i)
        {
          tileExtent[i] = regionExtent[i];
        }
        int axisMin = regionExtent[2 * splitAxis] + tileIndex * unitsPerTile;
        tileExtent[2 * splitAxis] = axisMin;
        tileExtent[2 * splitAxis + 1] = std::min(regionExtent[2 * splitAxis + 1], axisMin + unitsPerTile - 1);
        bool tileSuccess = false;
        // If complete cached data covers this tile (e.g. the overlap with
        // the previous pan position), copy it instead of fetching: panning
        // then only downloads the newly exposed part of the region. If the
        // cache covers the tile except for a remainder box along ONE axis
        // (the typical pan overlap when the tiles span the panned axis),
        // copy the covered slab and shrink the fetch to the remainder.
        int fetchExtent[6];
        for (int i = 0; i < 6; ++i)
        {
          fetchExtent[i] = tileExtent[i];
        }
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          CachedRegion* cachedCover = this->FindCoveringCachedRegion(tileExtent, request.Level, /*requireComplete=*/true);
          if (cachedCover && cachedCover->Image && cachedCover->Image->GetScalarType() == image->GetScalarType())
          {
            long long rowBytes = static_cast<long long>(tileExtent[1] - tileExtent[0] + 1) * bytesPerVoxel;
            for (int k = tileExtent[4]; k <= tileExtent[5]; ++k)
            {
              for (int j = tileExtent[2]; j <= tileExtent[3]; ++j)
              {
                memcpy(image->GetScalarPointer(tileExtent[0], j, k), cachedCover->Image->GetScalarPointer(tileExtent[0], j, k), rowBytes);
              }
            }
            tileSuccess = true;
          }
          else
          {
            CachedRegion* bestRegion = nullptr;
            int bestOverlap[6] = { 0, -1, 0, -1, 0, -1 };
            int bestAxis = -1;
            long long bestVolume = 0;
            for (CachedRegion& cachedRegion : this->RegionCache)
            {
              if (cachedRegion.Level != request.Level || !cachedRegion.Complete || !cachedRegion.Image //
                  || cachedRegion.Image->GetScalarType() != image->GetScalarType())
              {
                continue;
              }
              int overlap[6];
              bool overlaps = true;
              int partialAxis = -1;
              bool usable = true;
              for (int axis = 0; axis < 3 && overlaps && usable; ++axis)
              {
                overlap[2 * axis] = std::max(tileExtent[2 * axis], cachedRegion.Extent[2 * axis]);
                overlap[2 * axis + 1] = std::min(tileExtent[2 * axis + 1], cachedRegion.Extent[2 * axis + 1]);
                overlaps = (overlap[2 * axis] <= overlap[2 * axis + 1]);
                bool fullAxis = (overlap[2 * axis] == tileExtent[2 * axis] && overlap[2 * axis + 1] == tileExtent[2 * axis + 1]);
                if (!fullAxis)
                {
                  // The remainder must be a single box: the overlap must
                  // reach one side of the tile, along exactly one axis
                  bool touchesSide = (overlap[2 * axis] == tileExtent[2 * axis] || overlap[2 * axis + 1] == tileExtent[2 * axis + 1]);
                  if (partialAxis < 0 && touchesSide)
                  {
                    partialAxis = axis;
                  }
                  else
                  {
                    usable = false;
                  }
                }
              }
              if (!overlaps || !usable || partialAxis < 0)
              {
                continue;
              }
              long long overlapVolume = static_cast<long long>(overlap[1] - overlap[0] + 1)   //
                                        * static_cast<long long>(overlap[3] - overlap[2] + 1) //
                                        * static_cast<long long>(overlap[5] - overlap[4] + 1);
              if (overlapVolume > bestVolume)
              {
                bestRegion = &cachedRegion;
                for (int i = 0; i < 6; ++i)
                {
                  bestOverlap[i] = overlap[i];
                }
                bestAxis = partialAxis;
                bestVolume = overlapVolume;
              }
            }
            if (bestRegion)
            {
              long long rowBytes = static_cast<long long>(bestOverlap[1] - bestOverlap[0] + 1) * bytesPerVoxel;
              for (int k = bestOverlap[4]; k <= bestOverlap[5]; ++k)
              {
                for (int j = bestOverlap[2]; j <= bestOverlap[3]; ++j)
                {
                  memcpy(image->GetScalarPointer(bestOverlap[0], j, k), bestRegion->Image->GetScalarPointer(bestOverlap[0], j, k), rowBytes);
                }
              }
              if (bestOverlap[2 * bestAxis] == tileExtent[2 * bestAxis])
              {
                fetchExtent[2 * bestAxis] = bestOverlap[2 * bestAxis + 1] + 1;
              }
              else
              {
                fetchExtent[2 * bestAxis + 1] = bestOverlap[2 * bestAxis] - 1;
              }
            }
          }
        }
        bool fetched = false;
        bool fullTileFetch = std::equal(fetchExtent, fetchExtent + 6, tileExtent);
        if (tileSuccess)
        {
          // served from cache
        }
        else if (splitAxis == 2 && fullTileFetch)
        {
          // k-slab of the full xy region: contiguous in the image buffer
          void* tileBuffer = image->GetScalarPointer(regionExtent[0], regionExtent[2], tileExtent[4]);
          tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegionWithReader(sessionReader.get(), tileExtent, tileBuffer);
          if (!tileSuccess)
          {
            // Transient read failures have been observed (remote hiccup,
            // stalled transfer aborted by the low speed limit): retry once
            tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegionWithReader(sessionReader.get(), tileExtent, tileBuffer);
          }
          fetched = tileSuccess;
        }
        else
        {
          // Read the (possibly cache-shrunk) fetch box into a temporary tile
          // and copy row by row
          vtkNew<vtkImageData> tile;
          tile->SetExtent(fetchExtent);
          tile->AllocateScalars(scalarType, 1);
          tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegionWithReader(sessionReader.get(), fetchExtent, tile->GetScalarPointer());
          if (!tileSuccess)
          {
            tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegionWithReader(sessionReader.get(), fetchExtent, tile->GetScalarPointer());
          }
          if (tileSuccess)
          {
            long long rowBytes = static_cast<long long>(fetchExtent[1] - fetchExtent[0] + 1) * bytesPerVoxel;
            for (int k = fetchExtent[4]; k <= fetchExtent[5]; ++k)
            {
              for (int j = fetchExtent[2]; j <= fetchExtent[3]; ++j)
              {
                memcpy(image->GetScalarPointer(fetchExtent[0], j, k), tile->GetScalarPointer(fetchExtent[0], j, k), rowBytes);
              }
            }
          }
          fetched = tileSuccess;
        }
        if (fetched)
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          this->FetchedVoxelCount += static_cast<long long>(fetchExtent[1] - fetchExtent[0] + 1)   //
                                     * static_cast<long long>(fetchExtent[3] - fetchExtent[2] + 1) //
                                     * static_cast<long long>(fetchExtent[5] - fetchExtent[4] + 1);
        }
        success = tileSuccess;
        if (!tileSuccess)
        {
          vtkWarningMacro("WorkerLoop: failed to read tile [" << tileExtent[0] << "-" << tileExtent[1] << ", " << tileExtent[2] << "-" << tileExtent[3] << ", " //
                                                              << tileExtent[4] << "-" << tileExtent[5] << "] of level " << request.Level << " of " << this->FileName);
        }
        ProviderTrace("tile " + std::to_string(tileIndex + 1) + "/" + std::to_string(numberOfTiles) //
                      + (tileSuccess ? "" : " FAILED") + " " + ProviderTraceExtent(request.Extent, request.Level));
        if (success)
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          active->TilesCompleted = tileIndex + 1;
          if (progressive)
          {
            // Publish this tile: the main thread copies it into the
            // displayed placeholder image and notifies the views
            std::array<int, 6> completedTile;
            for (int i = 0; i < 6; ++i)
            {
              completedTile[i] = tileExtent[i];
            }
            active->CompletedTileExtents.push_back(completedTile);
          }
        }
      }
      image->GetPointData()->GetScalars()->Modified();
    }
    if (!success)
    {
      // Transient read failures happen (e.g. concurrent store access, remote
      // hiccup). Back off briefly before notifying, so that the automatic
      // retry (consumers re-request on the notification) does not turn into
      // a hot loop when the failure persists.
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    ProviderTrace("finish " + ProviderTraceExtent(request.Extent, request.Level) + " success=" + std::to_string(success) //
                  + " abandoned=" + std::to_string(abandoned) + " progressive=" + std::to_string(progressive));
    {
      std::lock_guard<std::mutex> lock(this->Mutex);
      if (progressive)
      {
        finishRequest(success && !abandoned);
        if (!active->Succeeded)
        {
          // Discard the partial read; the placeholder region stays cached
          // (incomplete). Notify consumers so that they re-evaluate and
          // re-issue the request: a silent failure would leave the views
          // showing the placeholder forever.
          active->CompletedTileExtents.clear();
          active->PublishedImage = nullptr;
          active->PrivateImage = nullptr;
          this->ActiveRequests.erase(std::remove(this->ActiveRequests.begin(), this->ActiveRequests.end(), active), this->ActiveRequests.end());
          this->CompletedLevels.push_back(request.Level);
        }
        // Successful progressive requests stay in ActiveRequests until the
        // main thread copied the remaining tiles and marked the published
        // region complete (in ProcessPendingRegionRequests).
      }
      else if (success && !abandoned)
      {
        if (request.WholeLevel)
        {
          this->LevelImages[request.Level] = image;
          this->LevelAccessOrder[request.Level] = ++this->AccessCounter;
        }
        else
        {
          CachedRegion cachedRegion;
          cachedRegion.Level = request.Level;
          for (int i = 0; i < 6; ++i)
          {
            cachedRegion.Extent[i] = request.Extent[i];
          }
          cachedRegion.Image = image;
          cachedRegion.AccessStamp = ++this->AccessCounter;
          this->RegionCache.push_back(cachedRegion);
        }
        this->CompletedLevels.push_back(request.Level);
      }
      if (!progressive)
      {
        finishRequest(success && !abandoned);
        this->ActiveRequests.erase(std::remove(this->ActiveRequests.begin(), this->ActiveRequests.end(), active), this->ActiveRequests.end());
        if (!success || abandoned)
        {
          // Notify consumers so that they re-evaluate and re-issue the
          // failed or abandoned request (see the note above)
          this->CompletedLevels.push_back(request.Level);
        }
      }
      this->TrimRegionCache();
    }
  }
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::IsRegionLoadable(const int extent[6], int resolutionLevel)
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  if (extent[1] < extent[0] || extent[3] < extent[2] || extent[5] < extent[4])
  {
    return false;
  }
  long long numberOfVoxels = static_cast<long long>(extent[1] - extent[0] + 1)   //
                             * static_cast<long long>(extent[3] - extent[2] + 1) //
                             * static_cast<long long>(extent[5] - extent[4] + 1);
  long long bytesPerVoxel = vtkDataArray::GetDataTypeSize(this->ScalarType) * this->NumberOfComponents;
  // Reading a region transiently needs about twice its data size
  return (2 * numberOfVoxels * bytesPerVoxel) <= this->GetMaximumLevelLoadBytes();
}

//----------------------------------------------------------------------------
double vtkMRMLOMEZarrVoxelDataProvider::GetVoxelValue(int i, int j, int k, int component /*=0*/)
{
  // Voxel values are provided at the reference resolution level (the grid of
  // the owning volume node).
  if (!this->EnsureLevelLoaded(this->ReferenceLevel))
  {
    return 0.0;
  }
  vtkSmartPointer<vtkImageData> image = this->GetCachedLevelImage(this->ReferenceLevel);
  return image ? image->GetScalarComponentAsDouble(i, j, k, component) : 0.0;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetStoredScalarRange(double range[2])
{
  vtkSmartPointer<vtkImageData> image = this->GetCachedLevelImage(this->ReferenceLevel);
  if (!image)
  {
    return false;
  }
  image->GetScalarRange(range);
  return true;
}

//----------------------------------------------------------------------------
vtkImageData* vtkMRMLOMEZarrVoxelDataProvider::GetStoredImageDataIfInMemory()
{
  // The stored image of the owning node is the reference level image.
  return this->GetCachedLevelImage(this->ReferenceLevel);
}
