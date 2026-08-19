/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH)
  All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer

=========================================================================auto=*/

// Tests the multi-resolution slice display guarantees of
// vtkMRMLSliceLayerLogic + vtkMRMLOMEZarrVoxelDataProvider:
//
// - Zooming in never decreases the displayed resolution: while the finer
//   target level is being fetched, the view shows the already downloaded
//   current-resolution data (resampled onto the target grid), not a coarser
//   level.
// - Panning keeps the already downloaded parts at their resolution (the
//   overlap with the previous region stays voxel-identical), and only the
//   newly revealed part of the region is downloaded (measured with the
//   provider's fetched-voxel counter).
// - Zooming out keeps the previously viewed area at its fine resolution
//   while the coarser target level is being fetched.
//
// The test builds a synthetic 3-level OME-Zarr pyramid (int16, uncompressed
// zarr v2) whose levels hold DIFFERENT deterministic patterns, so that the
// level a displayed voxel came from is provable by exact comparison.
// Determinism of intermediate (placeholder) states: background workers are
// disabled (0 worker threads) for the first phase, and the published
// placeholder image is only ever modified by ProcessPendingRegionRequests
// (main thread), which the test does not call between an interaction and
// its assertions.

// MRMLLogic includes
#include "vtkMRMLSliceLayerLogic.h"

// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLOMEZarrVoxelDataProvider.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSliceNode.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

// VTKsys includes
#include <vtksys/SystemTools.hxx>

// STD includes
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

// Pyramid geometry: level 0 is 256x256x8 at 1x1x1 mm; x/y halve per level
const int NumberOfLevels = 3;
const int Level0Dims[3] = { 256, 256, 8 };

//----------------------------------------------------------------------------
int LevelDim(int level, int axis)
{
  return (axis == 2) ? Level0Dims[2] : (Level0Dims[axis] >> level);
}

//----------------------------------------------------------------------------
// Distinct deterministic patterns per level, so that the source level of a
// displayed voxel is identifiable by exact value comparison
short LevelValue(int level, int x, int y, int z)
{
  switch (level)
  {
    case 0: return static_cast<short>(((x * 7919 + y * 104729 + z * 1299709) % 4001) - 2000);
    case 1: return static_cast<short>(((x * 4517 + y * 6301 + z * 7776) % 3001) - 1500);
    default: return static_cast<short>(((x * 911 + y * 3733 + z * 2913) % 2001) - 1000);
  }
}

//----------------------------------------------------------------------------
bool WriteTextFile(const std::string& path, const std::string& content)
{
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open())
  {
    std::cerr << "Cannot write " << path << std::endl;
    return false;
  }
  file << content;
  return true;
}

//----------------------------------------------------------------------------
// Write an uncompressed zarr v2 multiscale pyramid (zyx axis order,
// millimeter units, mean-downsampling translation transforms)
bool WriteZarrPyramid(const std::string& root)
{
  vtksys::SystemTools::RemoveADirectory(root);
  if (!vtksys::SystemTools::MakeDirectory(root))
  {
    std::cerr << "Cannot create " << root << std::endl;
    return false;
  }
  std::ostringstream zattrs;
  zattrs << "{\"multiscales\":[{\"axes\":["
         << "{\"name\":\"z\",\"type\":\"space\",\"unit\":\"millimeter\"},"
         << "{\"name\":\"y\",\"type\":\"space\",\"unit\":\"millimeter\"},"
         << "{\"name\":\"x\",\"type\":\"space\",\"unit\":\"millimeter\"}],"
         << "\"datasets\":[";
  for (int level = 0; level < NumberOfLevels; ++level)
  {
    double scale = static_cast<double>(1 << level);
    double translation = (scale - 1.0) / 2.0;
    zattrs << (level ? "," : "") << "{\"coordinateTransformations\":["
           << "{\"scale\":[1.0," << scale << "," << scale << "],\"type\":\"scale\"},"
           << "{\"translation\":[0.0," << translation << "," << translation << "],\"type\":\"translation\"}],"
           << "\"path\":\"" << level << "\"}";
  }
  zattrs << "],\"name\":\"test pyramid\",\"version\":\"0.4\"}]}";
  if (!WriteTextFile(root + "/.zattrs", zattrs.str()) || !WriteTextFile(root + "/.zgroup", "{\"zarr_format\":2}"))
  {
    return false;
  }
  const int chunkXY = 32;
  for (int level = 0; level < NumberOfLevels; ++level)
  {
    int dimX = LevelDim(level, 0);
    int dimY = LevelDim(level, 1);
    int dimZ = LevelDim(level, 2);
    std::string levelDir = root + "/" + std::to_string(level);
    if (!vtksys::SystemTools::MakeDirectory(levelDir))
    {
      return false;
    }
    std::ostringstream zarray;
    zarray << "{\"chunks\":[" << dimZ << "," << chunkXY << "," << chunkXY << "],"
           << "\"compressor\":null,\"dtype\":\"<i2\",\"fill_value\":0,\"filters\":null,"
           << "\"order\":\"C\",\"shape\":[" << dimZ << "," << dimY << "," << dimX << "],\"zarr_format\":2}";
    if (!WriteTextFile(levelDir + "/.zarray", zarray.str()))
    {
      return false;
    }
    for (int chunkY = 0; chunkY < dimY / chunkXY; ++chunkY)
    {
      for (int chunkX = 0; chunkX < dimX / chunkXY; ++chunkX)
      {
        std::string chunkPath = levelDir + "/0." + std::to_string(chunkY) + "." + std::to_string(chunkX);
        std::ofstream chunkFile(chunkPath, std::ios::binary);
        if (!chunkFile.is_open())
        {
          std::cerr << "Cannot write " << chunkPath << std::endl;
          return false;
        }
        std::vector<short> buffer;
        buffer.reserve(static_cast<size_t>(dimZ) * chunkXY * chunkXY);
        for (int z = 0; z < dimZ; ++z)
        {
          for (int y = chunkY * chunkXY; y < (chunkY + 1) * chunkXY; ++y)
          {
            for (int x = chunkX * chunkXY; x < (chunkX + 1) * chunkXY; ++x)
            {
              buffer.push_back(LevelValue(level, x, y, z));
            }
          }
        }
        chunkFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(short));
      }
    }
  }
  return true;
}

//----------------------------------------------------------------------------
// The full ground-truth image of a level (unit spacing, zero origin)
vtkSmartPointer<vtkImageData> CreateLevelImage(int level)
{
  vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(LevelDim(level, 0), LevelDim(level, 1), LevelDim(level, 2));
  image->AllocateScalars(VTK_SHORT, 1);
  short* pixel = static_cast<short*>(image->GetScalarPointer());
  for (int z = 0; z < LevelDim(level, 2); ++z)
  {
    for (int y = 0; y < LevelDim(level, 1); ++y)
    {
      for (int x = 0; x < LevelDim(level, 0); ++x)
      {
        *(pixel++) = LevelValue(level, x, y, z);
      }
    }
  }
  return image;
}

//----------------------------------------------------------------------------
// Resample a source level's ground truth onto a target level's grid over
// outputExtent - the same mapping and filter that the provider's placeholder
// composition uses, so equal inputs produce bitwise-equal outputs
vtkSmartPointer<vtkImageData> ResampleLevel(vtkMRMLOMEZarrVoxelDataProvider* provider, //
                                            vtkImageData* sourceImage,
                                            int sourceLevel,
                                            int targetLevel,
                                            const int outputExtent[6])
{
  double sourceScale[3], sourceOffset[3], targetScale[3], targetOffset[3];
  provider->GetLevelScale(sourceLevel, sourceScale);
  provider->GetLevelOffset(sourceLevel, sourceOffset);
  provider->GetLevelScale(targetLevel, targetScale);
  provider->GetLevelOffset(targetLevel, targetOffset);
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
  vtkSmartPointer<vtkImageData> output = vtkSmartPointer<vtkImageData>::New();
  output->DeepCopy(reslice->GetOutput());
  return output;
}

//----------------------------------------------------------------------------
// Map a source-level box to the target-level grid, shrunk so that every
// contained target voxel interpolates strictly inside the source box (the
// same rule the provider's placeholder overlay uses)
void MapBoxToLevel(vtkMRMLOMEZarrVoxelDataProvider* provider, const int box[6], int sourceLevel, int targetLevel, int mappedBox[6])
{
  double sourceScale[3], sourceOffset[3], targetScale[3], targetOffset[3];
  provider->GetLevelScale(sourceLevel, sourceScale);
  provider->GetLevelOffset(sourceLevel, sourceOffset);
  provider->GetLevelScale(targetLevel, targetScale);
  provider->GetLevelOffset(targetLevel, targetOffset);
  for (int axis = 0; axis < 3; ++axis)
  {
    double lowTarget = (sourceOffset[axis] - targetOffset[axis] + box[2 * axis] * sourceScale[axis]) / targetScale[axis];
    double highTarget = (sourceOffset[axis] - targetOffset[axis] + (box[2 * axis + 1] - 1) * sourceScale[axis]) / targetScale[axis];
    mappedBox[2 * axis] = static_cast<int>(std::ceil(lowTarget - 1e-6));
    mappedBox[2 * axis + 1] = static_cast<int>(std::floor(highTarget + 1e-6));
  }
}

//----------------------------------------------------------------------------
bool InsideBox(const int box[6], int x, int y, int z, int margin)
{
  return x >= box[0] + margin && x <= box[1] - margin //
         && y >= box[2] + margin && y <= box[3] - margin //
         && z >= box[4] && z <= box[5]; // through-plane axis is not resampled
}

//----------------------------------------------------------------------------
short VoxelAt(vtkImageData* image, int x, int y, int z)
{
  return *static_cast<short*>(image->GetScalarPointer(x, y, z));
}

//----------------------------------------------------------------------------
// Every voxel of `actual` over `extent` must equal the same voxel in one of
// the candidate images. Returns the number of mismatches (0 = pass).
long long CountNonMembers(vtkImageData* actual, const std::vector<vtkImageData*>& candidates, const int extent[6], const char* what)
{
  long long mismatches = 0;
  for (int z = extent[4]; z <= extent[5]; ++z)
  {
    for (int y = extent[2]; y <= extent[3]; ++y)
    {
      for (int x = extent[0]; x <= extent[1]; ++x)
      {
        short value = VoxelAt(actual, x, y, z);
        bool member = false;
        for (vtkImageData* candidate : candidates)
        {
          if (value == VoxelAt(candidate, x, y, z))
          {
            member = true;
            break;
          }
        }
        if (!member)
        {
          if (++mismatches <= 5)
          {
            std::cerr << what << ": voxel (" << x << "," << y << "," << z << ") = " << value << " matches no expected source" << std::endl;
          }
        }
      }
    }
  }
  if (mismatches)
  {
    std::cerr << what << ": " << mismatches << " voxels match no expected source" << std::endl;
  }
  return mismatches;
}

//----------------------------------------------------------------------------
// Every voxel of `actual` inside box (with margin) must equal `expected`
long long CountBoxMismatches(vtkImageData* actual, vtkImageData* expected, const int extent[6], const int box[6], int margin, const char* what)
{
  long long mismatches = 0;
  long long compared = 0;
  for (int z = extent[4]; z <= extent[5]; ++z)
  {
    for (int y = extent[2]; y <= extent[3]; ++y)
    {
      for (int x = extent[0]; x <= extent[1]; ++x)
      {
        if (!InsideBox(box, x, y, z, margin))
        {
          continue;
        }
        ++compared;
        short value = VoxelAt(actual, x, y, z);
        short expectedValue = VoxelAt(expected, x, y, z);
        if (value != expectedValue)
        {
          if (++mismatches <= 5)
          {
            std::cerr << what << ": voxel (" << x << "," << y << "," << z << ") = " << value << ", expected " << expectedValue << std::endl;
          }
        }
      }
    }
  }
  if (mismatches)
  {
    std::cerr << what << ": " << mismatches << " mismatches" << std::endl;
  }
  if (compared == 0)
  {
    std::cerr << what << ": comparison area is empty (test geometry problem)" << std::endl;
    return 1;
  }
  return mismatches;
}

//----------------------------------------------------------------------------
vtkImageData* DisplayedImage(vtkMRMLSliceLayerLogic* layer)
{
  return vtkImageData::SafeDownCast(layer->GetReslice()->GetInput());
}

//----------------------------------------------------------------------------
bool WaitForRegionComplete(vtkMRMLOMEZarrVoxelDataProvider* provider, const int extent[6], int level, double timeoutSeconds)
{
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < timeoutSeconds)
  {
    provider->ProcessPendingRegionRequests();
    if (provider->IsRegionComplete(extent, level))
    {
      // One more pump so that pending notifications reach the views
      provider->ProcessPendingRegionRequests();
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::cerr << "Timeout waiting for region [" << extent[0] << ":" << extent[1] << "," << extent[2] << ":" << extent[3] << "," //
            << extent[4] << ":" << extent[5] << "] of level " << level << std::endl;
  return false;
}

//----------------------------------------------------------------------------
void UpdateView(vtkMRMLSliceNode* sliceNode, vtkMRMLSliceLayerLogic* layer, double fieldOfView, double centerRAS[3])
{
  // Batch the geometry change into a single modification, so that no
  // intermediate view state (new zoom with the old center) triggers a
  // region request of its own
  int wasModifying = sliceNode->StartModify();
  sliceNode->SetFieldOfView(fieldOfView, fieldOfView, 4.0);
  vtkMatrix4x4* sliceToRAS = sliceNode->GetSliceToRAS();
  sliceToRAS->Identity();
  sliceToRAS->SetElement(0, 3, centerRAS[0]);
  sliceToRAS->SetElement(1, 3, centerRAS[1]);
  sliceToRAS->SetElement(2, 3, centerRAS[2]);
  sliceNode->UpdateMatrices();
  sliceNode->EndModify(wasModifying);
  layer->UpdateTransforms();
  layer->UpdateImageDisplay();
}

} // namespace

//----------------------------------------------------------------------------
int vtkMRMLSliceLayerLogicResolutionTest(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: vtkMRMLSliceLayerLogicResolutionTest <temporary directory>" << std::endl;
    return EXIT_FAILURE;
  }
  std::string root = std::string(argv[1]) + "/vtkMRMLSliceLayerLogicResolutionTest.zarr";
  CHECK_BOOL(WriteZarrPyramid(root), true);

  // Ground truth and provider
  vtkSmartPointer<vtkImageData> truth[NumberOfLevels];
  for (int level = 0; level < NumberOfLevels; ++level)
  {
    truth[level] = CreateLevelImage(level);
  }
  vtkNew<vtkMRMLOMEZarrVoxelDataProvider> provider;
  CHECK_BOOL(provider->SetFileName(root), true);
  CHECK_INT(provider->GetNumberOfResolutionLevels(), NumberOfLevels);
  double offset[3] = { 0.0, 0.0, 0.0 };
  CHECK_BOOL(provider->GetLevelOffset(1, offset), true);
  CHECK_DOUBLE(offset[0], 0.5);
  CHECK_DOUBLE(offset[1], 0.5);
  CHECK_DOUBLE(offset[2], 0.0);

  // Reference level 2 (the coarse preview grid, like a real large-image load)
  const int referenceLevel = 2;
  provider->SetReferenceResolutionLevel(referenceLevel);
  int referenceExtent[6];
  CHECK_BOOL(provider->GetExtent(referenceExtent, referenceLevel), true);
  vtkNew<vtkImageData> referenceImage;
  CHECK_BOOL(provider->GetRegion(referenceImage.GetPointer(), referenceExtent, referenceLevel), true);
  CHECK_BOOL(provider->IsLevelLoaded(referenceLevel), true);
  // Verify the pyramid reads back exactly
  CHECK_INT(static_cast<int>(CountBoxMismatches(referenceImage.GetPointer(), truth[2], referenceExtent, referenceExtent, 0, "reference level")), 0);

  // Force chunk-granular region streaming (the machinery under test) even
  // though this test pyramid is small, and make intermediate states
  // deterministic by disabling the background workers for now
  provider->SetWholeLevelPreferredBytes(0);
  provider->SetNumberOfWorkerThreads(0);

  // Scene: volume node on the reference grid (spacing 4 mm in-plane)
  vtkNew<vtkMRMLScene> scene;
  vtkMRMLScalarVolumeNode* volumeNode = vtkMRMLScalarVolumeNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLScalarVolumeNode"));
  CHECK_NOT_NULL(volumeNode);
  volumeNode->SetAndObserveImageData(referenceImage.GetPointer());
  volumeNode->SetSpacing(4.0, 4.0, 1.0);
  volumeNode->SetVoxelDataProvider(provider.GetPointer());
  volumeNode->CreateDefaultDisplayNodes();

  vtkMRMLSliceNode* sliceNode = vtkMRMLSliceNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLSliceNode"));
  CHECK_NOT_NULL(sliceNode);
  sliceNode->SetSliceResolutionMode(vtkMRMLSliceNode::SliceResolutionMatch2DView);
  sliceNode->SetDimensions(64, 64, 1);

  vtkNew<vtkMRMLSliceLayerLogic> layer;
  layer->SetMRMLScene(scene.GetPointer());
  layer->SetSliceNode(sliceNode);
  layer->SetVolumeNode(volumeNode);

  const int margin = 3;
  double center[3] = { 62.0, 62.0, 4.0 };

  //--------------------------------------------------------------------------
  // Phase A: mid zoom targets level 1. With no workers, the view must
  // immediately display the synchronously published placeholder: level 2
  // (all that is cached) resampled onto the level 1 target grid.
  UpdateView(sliceNode, layer, 120.0, center);
  CHECK_INT(layer->GetTargetResolutionLevel(), 1);
  CHECK_INT(layer->GetDisplayedResolutionLevel(), 1);
  int level1Extent[6];
  layer->GetTargetRegionExtent(level1Extent);
  vtkSmartPointer<vtkImageData> level2on1 = ResampleLevel(provider.GetPointer(), truth[2], 2, 1, level1Extent);
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), level2on1, level1Extent, level1Extent, 0, "A: placeholder from reference")), 0);

  // Enable the workers and let the level 1 region complete
  provider->SetNumberOfWorkerThreads(2);
  CHECK_BOOL(WaitForRegionComplete(provider.GetPointer(), level1Extent, 1, 30.0), true);
  layer->UpdateImageDisplay();
  CHECK_INT(layer->GetDisplayedResolutionLevel(), 1);
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[1], level1Extent, level1Extent, 0, "A: converged level 1")), 0);

  //--------------------------------------------------------------------------
  // Phase B (zooming test): zooming in targets level 0. The displayed image
  // must NEVER be coarser than the level 1 data that is already downloaded:
  // in the same update (before any background work is processed) the view
  // must show level 1 data resampled onto the level 0 grid.
  UpdateView(sliceNode, layer, 44.0, center);
  CHECK_INT(layer->GetTargetResolutionLevel(), 0);
  CHECK_INT(layer->GetDisplayedResolutionLevel(), 0);
  int level0Extent[6];
  layer->GetTargetRegionExtent(level0Extent);
  vtkSmartPointer<vtkImageData> level1on0 = ResampleLevel(provider.GetPointer(), truth[1], 1, 0, level0Extent);
  vtkSmartPointer<vtkImageData> level2on0 = ResampleLevel(provider.GetPointer(), truth[2], 2, 0, level0Extent);
  int level1BoxOn0[6];
  MapBoxToLevel(provider.GetPointer(), level1Extent, 1, 0, level1BoxOn0);
  // Strictly level 1 quality wherever the downloaded level 1 region covers
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), level1on0, level0Extent, level1BoxOn0, margin, "B: zoom-in keeps level 1 quality")), 0);
  // Everywhere: only current-resolution or reference data (never garbage)
  {
    std::vector<vtkImageData*> candidates = { truth[0].GetPointer(), level1on0.GetPointer(), level2on0.GetPointer() };
    CHECK_INT(static_cast<int>(CountNonMembers(DisplayedImage(layer), candidates, level0Extent, "B: zoom-in membership")), 0);
  }
  // Sanity: the assertion is meaningful (level 1 and level 2 expectations differ)
  {
    long long differing = 0;
    for (int z = level0Extent[4]; z <= level0Extent[5]; ++z)
    {
      for (int y = level0Extent[2]; y <= level0Extent[3]; ++y)
      {
        for (int x = level0Extent[0]; x <= level0Extent[1]; ++x)
        {
          if (InsideBox(level1BoxOn0, x, y, z, margin) && VoxelAt(level1on0, x, y, z) != VoxelAt(level2on0, x, y, z))
          {
            ++differing;
          }
        }
      }
    }
    CHECK_BOOL(differing > 1000, true);
  }

  CHECK_BOOL(WaitForRegionComplete(provider.GetPointer(), level0Extent, 0, 30.0), true);
  layer->UpdateImageDisplay();
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[0], level0Extent, level0Extent, 0, "B: converged level 0")), 0);

  //--------------------------------------------------------------------------
  // Phase C (panning test): pan by half a view. The overlap with the
  // previous region must stay voxel-identical to the full-resolution data
  // (never decrease resolution), and completing the pan must only download
  // the newly revealed part of the region.
  long long fetchedBeforePan = provider->GetTotalFetchedVoxels();
  double pannedCenter[3] = { center[0] + 22.0, center[1], center[2] };
  UpdateView(sliceNode, layer, 44.0, pannedCenter);
  CHECK_INT(layer->GetTargetResolutionLevel(), 0);
  CHECK_INT(layer->GetDisplayedResolutionLevel(), 0);
  int pannedExtent[6];
  layer->GetTargetRegionExtent(pannedExtent);
  int overlapExtent[6];
  bool panMovedRegion = false;
  for (int axis = 0; axis < 3; ++axis)
  {
    overlapExtent[2 * axis] = std::max(level0Extent[2 * axis], pannedExtent[2 * axis]);
    overlapExtent[2 * axis + 1] = std::min(level0Extent[2 * axis + 1], pannedExtent[2 * axis + 1]);
    CHECK_BOOL(overlapExtent[2 * axis] <= overlapExtent[2 * axis + 1], true);
    panMovedRegion = panMovedRegion || (pannedExtent[2 * axis] != level0Extent[2 * axis]) || (pannedExtent[2 * axis + 1] != level0Extent[2 * axis + 1]);
  }
  CHECK_BOOL(panMovedRegion, true);
  // Already downloaded tiles keep their (full) resolution in the overlap
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[0], pannedExtent, overlapExtent, 0, "C: pan keeps downloaded resolution")), 0);
  // Newly revealed area shows the best already-downloaded data meanwhile
  {
    vtkSmartPointer<vtkImageData> level1onPanned = ResampleLevel(provider.GetPointer(), truth[1], 1, 0, pannedExtent);
    vtkSmartPointer<vtkImageData> level2onPanned = ResampleLevel(provider.GetPointer(), truth[2], 2, 0, pannedExtent);
    std::vector<vtkImageData*> candidates = { truth[0].GetPointer(), level1onPanned.GetPointer(), level2onPanned.GetPointer() };
    CHECK_INT(static_cast<int>(CountNonMembers(DisplayedImage(layer), candidates, pannedExtent, "C: pan membership")), 0);
  }

  CHECK_BOOL(WaitForRegionComplete(provider.GetPointer(), pannedExtent, 0, 30.0), true);
  layer->UpdateImageDisplay();
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[0], pannedExtent, pannedExtent, 0, "C: converged panned level 0")), 0);
  // Only the newly revealed voxels were downloaded
  long long pannedVoxels = static_cast<long long>(pannedExtent[1] - pannedExtent[0] + 1)   //
                           * static_cast<long long>(pannedExtent[3] - pannedExtent[2] + 1) //
                           * static_cast<long long>(pannedExtent[5] - pannedExtent[4] + 1);
  long long overlapVoxels = static_cast<long long>(overlapExtent[1] - overlapExtent[0] + 1)   //
                            * static_cast<long long>(overlapExtent[3] - overlapExtent[2] + 1) //
                            * static_cast<long long>(overlapExtent[5] - overlapExtent[4] + 1);
  long long fetchedByPan = provider->GetTotalFetchedVoxels() - fetchedBeforePan;
  std::cout << "Pan fetched " << fetchedByPan << " voxels (region " << pannedVoxels << ", newly revealed " << (pannedVoxels - overlapVoxels) << ")" << std::endl;
  CHECK_BOOL(fetchedByPan <= pannedVoxels - overlapVoxels, true);
  CHECK_BOOL(fetchedByPan > 0, true);

  //--------------------------------------------------------------------------
  // Phase C2 (setup for the zoom-out test): jump to a far area (beyond the
  // level 1 region downloaded in phase A) and let full resolution complete
  // there, so that phase D has fine data that only exists at level 0.
  double jumpedCenter[3] = { 200.0, 62.0, 4.0 };
  UpdateView(sliceNode, layer, 44.0, jumpedCenter);
  CHECK_INT(layer->GetTargetResolutionLevel(), 0);
  int jumpedExtent[6];
  layer->GetTargetRegionExtent(jumpedExtent);
  CHECK_BOOL(WaitForRegionComplete(provider.GetPointer(), jumpedExtent, 0, 30.0), true);
  layer->UpdateImageDisplay();
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[0], jumpedExtent, jumpedExtent, 0, "C2: converged jumped level 0")), 0);

  //--------------------------------------------------------------------------
  // Phase D (zoom out): a coarser target whose region is not downloaded yet
  // must still show the already downloaded data in the previously viewed
  // areas (composed placeholder), not drop to the reference level: level 1
  // data where level 1 was downloaded, downsampled level 0 data where only
  // level 0 was downloaded.
  double zoomedOutCenter[3] = { 100.0, 62.0, 4.0 };
  UpdateView(sliceNode, layer, 120.0, zoomedOutCenter);
  CHECK_INT(layer->GetTargetResolutionLevel(), 1);
  CHECK_INT(layer->GetDisplayedResolutionLevel(), 1);
  int zoomedOutExtent[6];
  layer->GetTargetRegionExtent(zoomedOutExtent);
  vtkSmartPointer<vtkImageData> level0on1 = ResampleLevel(provider.GetPointer(), truth[0], 0, 1, zoomedOutExtent);
  vtkSmartPointer<vtkImageData> level2on1Wide = ResampleLevel(provider.GetPointer(), truth[2], 2, 1, zoomedOutExtent);
  // Where the downloaded level 1 region covers, the actual level 1 data
  // (the canonical content of the displayed level) is shown
  int level1KeptBox[6];
  for (int axis = 0; axis < 3; ++axis)
  {
    level1KeptBox[2 * axis] = std::max(level1Extent[2 * axis], zoomedOutExtent[2 * axis]);
    level1KeptBox[2 * axis + 1] = std::min(level1Extent[2 * axis + 1], zoomedOutExtent[2 * axis + 1]);
  }
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[1], zoomedOutExtent, level1KeptBox, margin, "D: zoom-out keeps level 1 data")), 0);
  // Where only level 0 was downloaded (the jumped area, beyond the level 1
  // region), the fine data is shown downsampled - never the coarse reference
  int fineOnlyBox[6];
  MapBoxToLevel(provider.GetPointer(), jumpedExtent, 0, 1, fineOnlyBox);
  for (int axis = 0; axis < 3; ++axis)
  {
    fineOnlyBox[2 * axis] = std::max(fineOnlyBox[2 * axis], zoomedOutExtent[2 * axis]);
    fineOnlyBox[2 * axis + 1] = std::min(fineOnlyBox[2 * axis + 1], zoomedOutExtent[2 * axis + 1]);
  }
  fineOnlyBox[0] = std::max(fineOnlyBox[0], level1Extent[1] + 1 + margin);
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), level0on1, zoomedOutExtent, fineOnlyBox, margin, "D: zoom-out keeps fine data")), 0);
  // Everywhere: composed from downloaded data of some level (never garbage)
  {
    std::vector<vtkImageData*> candidates = { level0on1.GetPointer(), truth[1].GetPointer(), level2on1Wide.GetPointer() };
    CHECK_INT(static_cast<int>(CountNonMembers(DisplayedImage(layer), candidates, zoomedOutExtent, "D: zoom-out membership")), 0);
  }
  CHECK_BOOL(WaitForRegionComplete(provider.GetPointer(), zoomedOutExtent, 1, 30.0), true);
  layer->UpdateImageDisplay();
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[1], zoomedOutExtent, zoomedOutExtent, 0, "D: converged level 1")), 0);

  //--------------------------------------------------------------------------
  // Phase E (zoom out DURING loading): zoom into a fresh area, let the fine
  // stream publish only its first strips (small tiles + inter-tile delay),
  // then zoom out while the stream is still running (which abandons it).
  // The strips that were already displayed at full resolution must survive
  // in the zoomed-out view - already displayed data is never downgraded,
  // even though the rest of the region is not available at high resolution.
  provider->SetTileTargetBytes(4096);
  provider->SetInterTileDelayMilliseconds(30);
  double freshCenter[3] = { 230.0, 62.0, 4.0 };
  UpdateView(sliceNode, layer, 44.0, freshCenter);
  CHECK_INT(layer->GetTargetResolutionLevel(), 0);
  CHECK_INT(layer->GetDisplayedResolutionLevel(), 0);
  int streamedExtent[6];
  layer->GetTargetRegionExtent(streamedExtent);
  // Pump until at least a few full-resolution strips are displayed: a row is
  // "arrived" when it matches the level 0 truth across the full region width
  auto rowIsFullResolution = [&](int y) -> bool
  {
    vtkImageData* displayed = DisplayedImage(layer);
    for (int z = streamedExtent[4]; z <= streamedExtent[5]; ++z)
    {
      for (int x = streamedExtent[0]; x <= streamedExtent[1]; ++x)
      {
        if (VoxelAt(displayed, x, y, z) != VoxelAt(truth[0], x, y, z))
        {
          return false;
        }
      }
    }
    return true;
  };
  int arrivedY = streamedExtent[2] - 1;
  {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < 30.0)
    {
      provider->ProcessPendingRegionRequests();
      while (arrivedY + 1 <= streamedExtent[3] && rowIsFullResolution(arrivedY + 1))
      {
        ++arrivedY;
      }
      if (arrivedY >= streamedExtent[2] + 23)
      {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
  CHECK_BOOL(arrivedY >= streamedExtent[2] + 23, true);
  // The stream must still be mid-flight (that is the scenario under test)
  CHECK_BOOL(provider->IsRegionComplete(streamedExtent, 0), false);

  // Zoom out NOW: the layer's interest moves to the coarser target, which
  // abandons the running fine stream - but its already displayed strips
  // must be preserved in the composed zoomed-out view
  UpdateView(sliceNode, layer, 120.0, freshCenter);
  CHECK_INT(layer->GetTargetResolutionLevel(), 1);
  CHECK_INT(layer->GetDisplayedResolutionLevel(), 1);
  int interruptedExtent[6];
  layer->GetTargetRegionExtent(interruptedExtent);
  vtkSmartPointer<vtkImageData> level0onInterrupted = ResampleLevel(provider.GetPointer(), truth[0], 0, 1, interruptedExtent);
  vtkSmartPointer<vtkImageData> level2onInterrupted = ResampleLevel(provider.GetPointer(), truth[2], 2, 1, interruptedExtent);
  int arrivedBox[6] = { streamedExtent[0], streamedExtent[1], streamedExtent[2], arrivedY, streamedExtent[4], streamedExtent[5] };
  int arrivedBoxOn1[6];
  MapBoxToLevel(provider.GetPointer(), arrivedBox, 0, 1, arrivedBoxOn1);
  for (int axis = 0; axis < 3; ++axis)
  {
    arrivedBoxOn1[2 * axis] = std::max(arrivedBoxOn1[2 * axis], interruptedExtent[2 * axis]);
    arrivedBoxOn1[2 * axis + 1] = std::min(arrivedBoxOn1[2 * axis + 1], interruptedExtent[2 * axis + 1]);
  }
  // Stay clear of the completed level 1 regions (phases A and D): canonical
  // same-level data is preferred where it exists
  arrivedBoxOn1[0] = std::max(arrivedBoxOn1[0], std::max(level1Extent[1], zoomedOutExtent[1]) + 1 + margin);
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), level0onInterrupted, interruptedExtent, arrivedBoxOn1, margin, //
                                                "E: interrupted stream data survives zoom-out")),
            0);
  {
    std::vector<vtkImageData*> candidates = { level0onInterrupted.GetPointer(), truth[1].GetPointer(), level2onInterrupted.GetPointer() };
    CHECK_INT(static_cast<int>(CountNonMembers(DisplayedImage(layer), candidates, interruptedExtent, "E: zoom-out membership")), 0);
  }
  provider->SetInterTileDelayMilliseconds(0);
  CHECK_BOOL(WaitForRegionComplete(provider.GetPointer(), interruptedExtent, 1, 30.0), true);
  layer->UpdateImageDisplay();
  CHECK_INT(static_cast<int>(CountBoxMismatches(DisplayedImage(layer), truth[1], interruptedExtent, interruptedExtent, 0, "E: converged level 1")), 0);

  // The scene owns the nodes; disconnect the layer before teardown
  layer->SetVolumeNode(nullptr);
  layer->SetSliceNode(nullptr);
  vtksys::SystemTools::RemoveADirectory(root);
  return EXIT_SUCCESS;
}
