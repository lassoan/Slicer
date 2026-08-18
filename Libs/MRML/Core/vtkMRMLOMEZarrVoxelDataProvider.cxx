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
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace
{

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
  if (this->Worker.joinable())
  {
    this->Worker.join();
  }
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
  if (this->IsRegionAvailable(extent, resolutionLevel))
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
vtkMRMLOMEZarrVoxelDataProvider::CachedRegion* vtkMRMLOMEZarrVoxelDataProvider::FindCoveringCachedRegion(const int extent[6], int level)
{
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
    if (covers)
    {
      return &cachedRegion;
    }
  }
  return nullptr;
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
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  if (this->IsRegionAvailable(extent, resolutionLevel))
  {
    return false; // already available
  }
  // Small levels are fetched and cached whole (reusable by all views);
  // levels above the threshold are accessed chunk-granularly, reading only
  // the requested region.
  bool wholeLevel = this->IsLevelLoadable(resolutionLevel) && this->GetLevelMemoryBytes(resolutionLevel) <= WholeLevelPreferredBytes;
  if (!wholeLevel && !this->IsRegionLoadable(extent, resolutionLevel))
  {
    return false;
  }
  this->EnsureWorker();
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    // Only the most recent request is kept: older pending requests are
    // superseded (e.g. during continued zooming/panning).
    this->PendingRequest.Level = resolutionLevel;
    this->PendingRequest.WholeLevel = wholeLevel;
    for (int i = 0; i < 6; ++i)
    {
      this->PendingRequest.Extent[i] = extent[i];
    }
    this->HasPendingRequest = true;
  }
  this->Condition.notify_all();
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::HasPendingRegionRequests()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  return (this->HasPendingRequest || this->RequestInProgress || !this->CompletedLevels.empty());
}

//----------------------------------------------------------------------------
double vtkMRMLOMEZarrVoxelDataProvider::GetPendingRegionRequestProgress()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  if (!this->RequestInProgress)
  {
    return this->HasPendingRequest ? 0.0 : -1.0;
  }
  if (this->TilesTotal > 0)
  {
    // Real progress: completed tiles of the tiled region read
    return std::min(0.98, static_cast<double>(this->TilesCompleted) / this->TilesTotal);
  }
  // Estimate from the request size and the observed retrieval throughput
  double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - this->RequestStartTime).count();
  if (this->ThroughputBytesPerSecond <= 0.0 || this->RequestInProgressBytes <= 0.0)
  {
    // Unknown speed: fill gradually over ~10 seconds
    return std::min(0.9, elapsedSeconds / 10.0);
  }
  double expectedSeconds = this->RequestInProgressBytes / this->ThroughputBytesPerSecond;
  return std::max(0.02, std::min(0.95, elapsedSeconds / expectedSeconds));
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::ProcessPendingRegionRequests()
{
  std::vector<int> completed;
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    completed.swap(this->CompletedLevels);
  }
  for (int level : completed)
  {
    this->InvokeEvent(vtkMRMLVoxelDataProvider::RegionReadyEvent, &level);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::EnsureWorker()
{
  if (this->Worker.joinable())
  {
    return;
  }
  this->WorkerShouldStop = false;
  this->Worker = std::thread(&vtkMRMLOMEZarrVoxelDataProvider::WorkerLoop, this);
}

//----------------------------------------------------------------------------
void vtkMRMLOMEZarrVoxelDataProvider::WorkerLoop()
{
  for (;;)
  {
    RegionRequest request;
    {
      std::unique_lock<std::mutex> lock(this->Mutex);
      this->Condition.wait(lock, [this] { return this->WorkerShouldStop || this->HasPendingRequest; });
      if (this->WorkerShouldStop)
      {
        return;
      }
      request = this->PendingRequest;
      this->HasPendingRequest = false;
      bool alreadyAvailable = (this->LevelImages.find(request.Level) != this->LevelImages.end()) //
                              || (!request.WholeLevel && this->FindCoveringCachedRegion(request.Extent, request.Level) != nullptr);
      if (alreadyAvailable)
      {
        // Became available meanwhile: report as completed
        this->CompletedLevels.push_back(request.Level);
        continue;
      }
      // Bookkeeping for the loading progress estimate
      this->RequestInProgress = true;
      this->RequestStartTime = std::chrono::steady_clock::now();
      if (request.WholeLevel)
      {
        this->RequestInProgressBytes = static_cast<double>(this->GetLevelMemoryBytes(request.Level));
      }
      else
      {
        double numberOfVoxels = static_cast<double>(request.Extent[1] - request.Extent[0] + 1)   //
                                * static_cast<double>(request.Extent[3] - request.Extent[2] + 1) //
                                * static_cast<double>(request.Extent[5] - request.Extent[4] + 1);
        this->RequestInProgressBytes = numberOfVoxels * vtkDataArray::GetDataTypeSize(this->ScalarType) * this->NumberOfComponents;
      }
    }
    // Load outside the lock (this is the expensive part)
    auto finishRequest = [this](bool success)
    {
      // The caller must hold Mutex. Update the observed throughput (EMA)
      // that the progress estimate of future requests is computed from.
      double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - this->RequestStartTime).count();
      if (success && elapsedSeconds > 0.05 && this->RequestInProgressBytes > 0.0)
      {
        double bytesPerSecond = this->RequestInProgressBytes / elapsedSeconds;
        this->ThroughputBytesPerSecond = (this->ThroughputBytesPerSecond > 0.0) ? (0.5 * this->ThroughputBytesPerSecond + 0.5 * bytesPerSecond) : bytesPerSecond;
      }
      this->RequestInProgress = false;
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
    int scalarType = VTK_VOID;
    bool success = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegionIntoBuffer(this->FileName.c_str(), request.Level, regionExtent, nullptr, scalarType);
    vtkSmartPointer<vtkImageData> image;
    bool abandoned = false;
    if (success)
    {
      image = vtkSmartPointer<vtkImageData>::New();
      image->SetExtent(regionExtent);
      image->SetOrigin(0.0, 0.0, 0.0);
      image->SetSpacing(1.0, 1.0, 1.0);
      image->AllocateScalars(scalarType, 1);

      // Tile plan: split along k (contiguous memory) when the region is
      // thick, otherwise along j; ~8 MB per tile.
      const long long tileTargetBytes = 8LL * 1024LL * 1024LL;
      int xDim = regionExtent[1] - regionExtent[0] + 1;
      int yDim = regionExtent[3] - regionExtent[2] + 1;
      int zDim = regionExtent[5] - regionExtent[4] + 1;
      long long bytesPerVoxel = image->GetScalarSize();
      int splitAxis = (zDim >= 8) ? 2 : 1;
      long long bytesPerUnit = (splitAxis == 2) ? (static_cast<long long>(xDim) * yDim * bytesPerVoxel) //
                                                : (static_cast<long long>(xDim) * zDim * bytesPerVoxel);
      int axisDim = (splitAxis == 2) ? zDim : yDim;
      int unitsPerTile = std::max(1, static_cast<int>(tileTargetBytes / std::max(1LL, bytesPerUnit)));
      int numberOfTiles = (axisDim + unitsPerTile - 1) / unitsPerTile;
      {
        std::lock_guard<std::mutex> lock(this->Mutex);
        this->TilesTotal = numberOfTiles;
        this->TilesCompleted = 0;
      }
      for (int tileIndex = 0; tileIndex < numberOfTiles && success; ++tileIndex)
      {
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          if (this->WorkerShouldStop)
          {
            return;
          }
          if (this->HasPendingRequest)
          {
            bool samePendingRequest = (this->PendingRequest.Level == request.Level && this->PendingRequest.WholeLevel == request.WholeLevel);
            for (int i = 0; i < 6 && samePendingRequest; ++i)
            {
              samePendingRequest = (this->PendingRequest.Extent[i] == request.Extent[i]);
            }
            if (samePendingRequest)
            {
              // Duplicate of what is being read: absorb it
              this->HasPendingRequest = false;
            }
            // Note: a different pending request must NOT abandon the current
            // read. Several views request regions of the same provider and
            // supersede each other in the single-slot request queue; only the
            // guarantee that every started read completes (and its result is
            // cached, triggering the unserved views to re-request) makes this
            // converge. Abandoning superseded reads would need per-requester
            // queues to avoid a livelock.
          }
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
        if (splitAxis == 2)
        {
          // k-slab of the full xy region: contiguous in the image buffer
          void* tileBuffer = image->GetScalarPointer(regionExtent[0], regionExtent[2], tileExtent[4]);
          tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegionIntoBuffer(this->FileName.c_str(), request.Level, tileExtent, tileBuffer, scalarType);
          if (!tileSuccess)
          {
            // Transient read failures have been observed (e.g. store handle
            // contention, remote hiccup): retry once
            tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegionIntoBuffer(this->FileName.c_str(), request.Level, tileExtent, tileBuffer, scalarType);
          }
        }
        else
        {
          // j-slab: read into a temporary tile and copy row by row
          vtkNew<vtkImageData> tile;
          tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegion(this->FileName.c_str(), request.Level, tileExtent, tile.GetPointer());
          if (!tileSuccess)
          {
            tileSuccess = vtkITKArchetypeImageSeriesReader::ReadOMEZarrRegion(this->FileName.c_str(), request.Level, tileExtent, tile.GetPointer());
          }
          if (tileSuccess)
          {
            long long rowBytes = static_cast<long long>(xDim) * bytesPerVoxel;
            for (int k = tileExtent[4]; k <= tileExtent[5]; ++k)
            {
              for (int j = tileExtent[2]; j <= tileExtent[3]; ++j)
              {
                memcpy(image->GetScalarPointer(regionExtent[0], j, k), tile->GetScalarPointer(regionExtent[0], j, k), rowBytes);
              }
            }
          }
        }
        success = tileSuccess;
        if (success)
        {
          std::lock_guard<std::mutex> lock(this->Mutex);
          this->TilesCompleted = tileIndex + 1;
        }
      }
      image->GetPointData()->GetScalars()->Modified();
    }
    {
      std::lock_guard<std::mutex> lock(this->Mutex);
      this->TilesTotal = 0;
      this->TilesCompleted = 0;
      if (success && !abandoned)
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
          // Keep only the most recently used regions
          while (this->RegionCache.size() > MaximumCachedRegions)
          {
            size_t oldestIndex = 0;
            for (size_t i = 1; i < this->RegionCache.size(); ++i)
            {
              if (this->RegionCache[i].AccessStamp < this->RegionCache[oldestIndex].AccessStamp)
              {
                oldestIndex = i;
              }
            }
            this->RegionCache.erase(this->RegionCache.begin() + oldestIndex);
          }
        }
        this->CompletedLevels.push_back(request.Level);
      }
      finishRequest(success && !abandoned);
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
