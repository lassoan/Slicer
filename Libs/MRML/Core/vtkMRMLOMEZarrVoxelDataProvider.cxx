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
#include <vtkExtractVOI.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

// RapidJSON includes
#include <rapidjson/document.h>

// STD includes
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
    }
    else
    {
      this->LevelImages.erase(level);
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
void vtkMRMLOMEZarrVoxelDataProvider::ReleaseCachedLevels()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  for (auto it = this->LevelImages.begin(); it != this->LevelImages.end();)
  {
    if (it->first != this->ReferenceLevel)
    {
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
  return (it != this->LevelImages.end()) ? it->second : nullptr;
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
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetRegion(vtkImageData* output, const int extent[6], int resolutionLevel /*=0*/)
{
  if (!output || !this->EnsureLevelLoaded(resolutionLevel))
  {
    return false;
  }
  return this->GetRegionIfAvailable(output, extent, resolutionLevel);
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::GetRegionIfAvailable(vtkImageData* output, const int extent[6], int resolutionLevel /*=0*/)
{
  if (!output)
  {
    return false;
  }
  vtkSmartPointer<vtkImageData> levelImage = this->GetCachedLevelImage(resolutionLevel);
  if (!levelImage)
  {
    return false;
  }
  const int* levelExtent = this->Levels[resolutionLevel].Extent;
  bool fullRegion = true;
  for (int axis = 0; axis < 3; ++axis)
  {
    if (extent[2 * axis] < levelExtent[2 * axis] || extent[2 * axis + 1] > levelExtent[2 * axis + 1])
    {
      vtkErrorMacro("GetRegionIfAvailable: requested extent is outside the level extent");
      return false;
    }
    if (extent[2 * axis] != levelExtent[2 * axis] || extent[2 * axis + 1] != levelExtent[2 * axis + 1])
    {
      fullRegion = false;
    }
  }
  if (fullRegion)
  {
    output->ShallowCopy(levelImage);
    return true;
  }
  vtkNew<vtkExtractVOI> extractVOI;
  extractVOI->SetInputData(levelImage);
  extractVOI->SetVOI(extent[0], extent[1], extent[2], extent[3], extent[4], extent[5]);
  extractVOI->Update();
  output->ShallowCopy(extractVOI->GetOutput());
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::RequestRegionAsync(const int vtkNotUsed(extent)[6], int resolutionLevel)
{
  if (resolutionLevel < 0 || resolutionLevel >= static_cast<int>(this->Levels.size()))
  {
    return false;
  }
  if (this->IsLevelLoaded(resolutionLevel))
  {
    return false; // already available
  }
  this->EnsureWorker();
  {
    std::lock_guard<std::mutex> lock(this->Mutex);
    // Only the most recent request is kept: older pending requests are
    // superseded (e.g. during continued zooming/panning).
    this->PendingLevel = resolutionLevel;
  }
  this->Condition.notify_all();
  return true;
}

//----------------------------------------------------------------------------
bool vtkMRMLOMEZarrVoxelDataProvider::HasPendingRegionRequests()
{
  std::lock_guard<std::mutex> lock(this->Mutex);
  return (this->PendingLevel >= 0 || !this->CompletedLevels.empty());
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
    int levelToLoad = -1;
    {
      std::unique_lock<std::mutex> lock(this->Mutex);
      this->Condition.wait(lock, [this] { return this->WorkerShouldStop || this->PendingLevel >= 0; });
      if (this->WorkerShouldStop)
      {
        return;
      }
      levelToLoad = this->PendingLevel;
      this->PendingLevel = -1;
      if (this->LevelImages.find(levelToLoad) != this->LevelImages.end())
      {
        // Already loaded meanwhile: report as completed
        this->CompletedLevels.push_back(levelToLoad);
        continue;
      }
    }
    // Load outside the lock (this is the expensive part)
    vtkSmartPointer<vtkImageData> image = this->LoadLevelImage(levelToLoad);
    {
      std::lock_guard<std::mutex> lock(this->Mutex);
      if (image)
      {
        this->LevelImages[levelToLoad] = image;
        this->CompletedLevels.push_back(levelToLoad);
      }
    }
  }
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
