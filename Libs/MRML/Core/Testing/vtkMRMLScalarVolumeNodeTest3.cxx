/*=auto=========================================================================

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH)
  All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer

=========================================================================auto=*/

// Tests voxel value scaling (stored vs physical voxel values) of
// vtkMRMLScalarVolumeNode and vtkMRMLInMemoryVoxelDataProvider.

#include "vtkCodedEntry.h"
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLInMemoryVoxelDataProvider.h"
#include "vtkMRMLScalarVolumeDisplayNode.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLScene.h"

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

namespace
{

//----------------------------------------------------------------------------
vtkSmartPointer<vtkImageData> CreateStoredImage(short fillValue)
{
  vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  image->SetDimensions(4, 4, 4);
  image->AllocateScalars(VTK_SHORT, 1);
  image->GetPointData()->GetScalars()->FillComponent(0, fillValue);
  return image;
}

} // namespace

//----------------------------------------------------------------------------
int vtkMRMLScalarVolumeNodeTest3(int, char*[])
{
  TESTING_OUTPUT_INIT();

  // Defaults: scaling inactive
  vtkNew<vtkMRMLScalarVolumeNode> node;
  CHECK_BOOL(node->IsVoxelValueScalingActive(), false);
  CHECK_DOUBLE(node->GetVoxelValueScale(), 1.0);
  CHECK_DOUBLE(node->GetVoxelValueOffset(), 0.0);
  CHECK_NULL(node->GetVoxelDataProvider());
  // Conversions are identity when scaling is inactive
  CHECK_DOUBLE(node->GetPhysicalValueFromStoredValue(123.0), 123.0);
  CHECK_DOUBLE(node->GetStoredValueFromPhysicalValue(123.0), 123.0);

  // Activate packing: stored int16, physical = 0.01 * stored
  vtkSmartPointer<vtkImageData> stored = CreateStoredImage(250);
  node->SetStoredImageData(stored, 0.01, 0.0);
  CHECK_BOOL(node->IsVoxelValueScalingActive(), true);
  CHECK_NOT_NULL(node->GetVoxelDataProvider());
  CHECK_DOUBLE(node->GetVoxelValueScale(), 0.01);
  CHECK_POINTER(node->GetStoredImageData(), stored.GetPointer());

  // The physical image is lazy: presence/extent queries and the stored
  // connection do not generate it
  CHECK_BOOL(node->IsPhysicalImageDataMaterialized(), false);
  CHECK_BOOL(node->HasImageData(), true);
  int lazyExtent[6] = { 0, -1, 0, -1, 0, -1 };
  CHECK_BOOL(node->GetImageExtent(lazyExtent), true);
  CHECK_INT(lazyExtent[1], 3);
  CHECK_NOT_NULL(node->GetStoredImageDataConnection());
  CHECK_BOOL(node->IsPhysicalImageDataMaterialized(), false);
  // Physical corner value (median of stored corners, converted) is available
  // without generating the physical image
  CHECK_DOUBLE_TOLERANCE(node->GetImageBackgroundScalarComponentAsDouble(0), 2.5, 1e-6);
  CHECK_BOOL(node->IsPhysicalImageDataMaterialized(), false);

  // GetImageData() generates the physical (float32) image on demand
  vtkImageData* physical = node->GetImageData();
  CHECK_BOOL(node->IsPhysicalImageDataMaterialized(), true);
  CHECK_NOT_NULL(physical);
  CHECK_POINTER_DIFFERENT(physical, stored.GetPointer());
  CHECK_INT(physical->GetScalarType(), VTK_FLOAT);
  CHECK_DOUBLE_TOLERANCE(physical->GetScalarComponentAsDouble(1, 2, 3, 0), 2.5, 1e-6);
  // Stored image is untouched
  CHECK_INT(stored->GetScalarType(), VTK_SHORT);
  CHECK_DOUBLE(stored->GetScalarComponentAsDouble(1, 2, 3, 0), 250.0);

  // Value conversion helpers
  CHECK_DOUBLE_TOLERANCE(node->GetPhysicalValueFromStoredValue(200.0), 2.0, 1e-9);
  CHECK_DOUBLE_TOLERANCE(node->GetStoredValueFromPhysicalValue(2.0), 200.0, 1e-9);

  // Provider interface
  vtkMRMLVoxelDataProvider* provider = node->GetVoxelDataProvider();
  CHECK_INT(provider->GetNumberOfResolutionLevels(), 1);
  CHECK_INT(provider->GetScalarType(), VTK_SHORT);
  CHECK_INT(provider->GetNumberOfScalarComponents(), 1);
  CHECK_DOUBLE(provider->GetVoxelValue(1, 2, 3, 0), 250.0);
  double storedRange[2] = { 0.0, 0.0 };
  CHECK_BOOL(provider->GetStoredScalarRange(storedRange), true);
  CHECK_DOUBLE(storedRange[0], 250.0);
  CHECK_DOUBLE(storedRange[1], 250.0);
  int extent[6] = { 0, -1, 0, -1, 0, -1 };
  CHECK_BOOL(provider->GetExtent(extent), true);
  CHECK_INT(extent[1], 3);
  // Region extraction (a sub-extent)
  vtkNew<vtkImageData> region;
  int subExtent[6] = { 1, 2, 1, 2, 1, 2 };
  CHECK_BOOL(provider->GetRegion(region.GetPointer(), subExtent), true);
  CHECK_DOUBLE(region->GetScalarComponentAsDouble(1, 1, 1, 0), 250.0);

  // Voxel value string with units
  vtkNew<vtkCodedEntry> units;
  units->SetValueSchemeMeaning("cm/s", "UCUM", "centimeter per second");
  node->SetVoxelValueUnits(units.GetPointer());
  CHECK_STD_STRING(node->GetVoxelValueAsString(1, 2, 3), "2.5 cm/s");
  CHECK_STD_STRING(node->GetVoxelValueAsString(100, 0, 0), ""); // out of extent

  // Aware modification of the stored image regenerates the physical image
  stored->GetPointData()->GetScalars()->FillComponent(0, 500);
  stored->Modified();
  CHECK_BOOL(node->IsVoxelValueScalingActive(), true);
  physical = node->GetImageData();
  CHECK_NOT_NULL(physical);
  CHECK_DOUBLE_TOLERANCE(physical->GetScalarComponentAsDouble(0, 0, 0, 0), 5.0, 1e-6);

  // Unaware in-place modification of the physical image promotes it to
  // primary: scaling deactivates, values are preserved
  physical->GetPointData()->GetScalars()->FillComponent(0, 42);
  physical->Modified();
  CHECK_BOOL(node->IsVoxelValueScalingActive(), false);
  CHECK_NULL(node->GetVoxelDataProvider());
  CHECK_DOUBLE(node->GetVoxelValueScale(), 1.0);
  CHECK_POINTER(node->GetImageData(), physical);
  CHECK_DOUBLE(node->GetImageData()->GetScalarComponentAsDouble(0, 0, 0, 0), 42.0);
  // Units metadata survives promotion
  CHECK_NOT_NULL(node->GetVoxelValueUnits());

  // Unaware replacement of the image data deactivates scaling
  vtkNew<vtkMRMLScalarVolumeNode> node2;
  vtkSmartPointer<vtkImageData> stored2 = CreateStoredImage(100);
  node2->SetStoredImageData(stored2, 0.5, 10.0);
  CHECK_BOOL(node2->IsVoxelValueScalingActive(), true);
  CHECK_DOUBLE_TOLERANCE(node2->GetImageData()->GetScalarComponentAsDouble(0, 0, 0, 0), 60.0, 1e-6);
  vtkSmartPointer<vtkImageData> plainImage = CreateStoredImage(7);
  node2->SetAndObserveImageData(plainImage);
  CHECK_BOOL(node2->IsVoxelValueScalingActive(), false);
  CHECK_NULL(node2->GetVoxelDataProvider());
  CHECK_POINTER(node2->GetImageData(), plainImage.GetPointer());

  // Invalid scale is rejected
  vtkNew<vtkMRMLScalarVolumeNode> node3;
  vtkSmartPointer<vtkImageData> stored3 = CreateStoredImage(10);
  TESTING_OUTPUT_ASSERT_ERRORS_BEGIN();
  node3->SetStoredImageData(stored3, 0.0, 0.0);
  TESTING_OUTPUT_ASSERT_ERRORS_END();
  CHECK_BOOL(node3->IsVoxelValueScalingActive(), false);

  // CopyContent copies metadata and packing state
  vtkNew<vtkMRMLScalarVolumeNode> source;
  vtkSmartPointer<vtkImageData> sourceStored = CreateStoredImage(300);
  source->SetStoredImageData(sourceStored, 0.01, -1.0);
  vtkNew<vtkCodedEntry> quantity;
  quantity->SetValueSchemeMeaning("G-A19E", "SRT", "Velocity");
  source->SetVoxelValueQuantity(quantity.GetPointer());
  vtkNew<vtkCodedEntry> units2;
  units2->SetValueSchemeMeaning("cm/s", "UCUM", "centimeter per second");
  source->SetVoxelValueUnits(units2.GetPointer());

  vtkNew<vtkMRMLScalarVolumeNode> copied;
  copied->CopyContent(source.GetPointer(), /*deepCopy=*/true);
  // Copy does not generate the physical image on the source or the target
  CHECK_BOOL(source->IsPhysicalImageDataMaterialized(), false);
  CHECK_BOOL(copied->IsPhysicalImageDataMaterialized(), false);
  CHECK_BOOL(copied->IsVoxelValueScalingActive(), true);
  CHECK_DOUBLE(copied->GetVoxelValueScale(), 0.01);
  CHECK_DOUBLE(copied->GetVoxelValueOffset(), -1.0);
  CHECK_NOT_NULL(copied->GetVoxelValueQuantity());
  CHECK_STD_STRING(copied->GetVoxelValueQuantity()->GetCodeMeaning(), "Velocity");
  CHECK_NOT_NULL(copied->GetVoxelValueUnits());
  CHECK_NOT_NULL(copied->GetStoredImageData());
  CHECK_POINTER_DIFFERENT(copied->GetStoredImageData(), sourceStored.GetPointer()); // deep copy
  CHECK_DOUBLE(copied->GetStoredImageData()->GetScalarComponentAsDouble(0, 0, 0, 0), 300.0);
  CHECK_DOUBLE_TOLERANCE(copied->GetImageData()->GetScalarComponentAsDouble(0, 0, 0, 0), 2.0, 1e-6);

  // CopyContent from an unpacked source deactivates packing on the target
  vtkNew<vtkMRMLScalarVolumeNode> plainSource;
  plainSource->SetAndObserveImageData(plainImage);
  copied->CopyContent(plainSource.GetPointer(), /*deepCopy=*/true);
  CHECK_BOOL(copied->IsVoxelValueScalingActive(), false);
  CHECK_NULL(copied->GetVoxelDataProvider());

  // Display node receives the stored connection and the value mapping
  {
    vtkNew<vtkMRMLScene> scene;
    vtkMRMLScalarVolumeNode* displayedNode = vtkMRMLScalarVolumeNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLScalarVolumeNode"));
    CHECK_NOT_NULL(displayedNode);
    vtkSmartPointer<vtkImageData> displayedStored = CreateStoredImage(400);
    displayedNode->SetStoredImageData(displayedStored, 0.01, 0.0);
    displayedNode->CreateDefaultDisplayNodes();
    vtkMRMLScalarVolumeDisplayNode* displayNode = vtkMRMLScalarVolumeDisplayNode::SafeDownCast(displayedNode->GetDisplayNode());
    CHECK_NOT_NULL(displayNode);
    CHECK_DOUBLE(displayNode->GetVoxelValueScale(), 0.01);
    CHECK_DOUBLE(displayNode->GetVoxelValueOffset(), 0.0);
    // Display pipeline input is the stored image; the physical image is not
    // generated for display
    CHECK_POINTER(displayNode->GetInputImageData(), displayedStored.GetPointer());
    CHECK_BOOL(displayedNode->IsPhysicalImageDataMaterialized(), false);
    // Display scalar range is the stored range
    double displayRange[2] = { 0.0, 0.0 };
    displayNode->GetDisplayScalarRange(displayRange);
    CHECK_DOUBLE(displayRange[0], 400.0);
    CHECK_DOUBLE(displayRange[1], 400.0);
  }

  return EXIT_SUCCESS;
}
