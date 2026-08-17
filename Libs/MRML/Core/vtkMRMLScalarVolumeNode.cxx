/*=auto=========================================================================

Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Program:   3D Slicer
Module:    $RCSfile: vtkMRMLVolumeNode.cxx,v $
Date:      $Date: 2006/03/17 17:01:53 $
Version:   $Revision: 1.14 $

=========================================================================auto=*/
// MRML includes
#include "vtkCodedEntry.h"
#include "vtkMRMLInMemoryVoxelDataProvider.h"
#include "vtkMRMLScalarVolumeDisplayNode.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLSelectionNode.h"
#include "vtkMRMLUnitNode.h"
#include "vtkMRMLVolumeArchetypeStorageNode.h"
#include "vtkMRMLVolumeSequenceStorageNode.h"
#include "vtkMRMLVoxelDataProvider.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkDataArray.h>
#include <vtkImageShiftScale.h>
#include <vtkMatrix4x4.h>
#include <vtkObjectFactory.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTrivialProducer.h>

// STD includes
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLScalarVolumeNode);
vtkCxxSetObjectMacro(vtkMRMLScalarVolumeNode, VoxelValueQuantity, vtkCodedEntry);
vtkCxxSetObjectMacro(vtkMRMLScalarVolumeNode, VoxelValueUnits, vtkCodedEntry);

//----------------------------------------------------------------------------
vtkMRMLScalarVolumeNode::vtkMRMLScalarVolumeNode()
{
  this->TypeDisplayName = vtkMRMLTr("vtkMRMLScalarVolumeNode", "Volume");

  this->DefaultSequenceStorageNodeClassName = "vtkMRMLVolumeSequenceStorageNode";

  this->VoxelDataProviderObserver = vtkSmartPointer<vtkCallbackCommand>::New();
  this->VoxelDataProviderObserver->SetClientData(this);
  this->VoxelDataProviderObserver->SetCallback(vtkMRMLScalarVolumeNode::OnVoxelDataProviderModified);

  this->PhysicalImageDataObserver = vtkSmartPointer<vtkCallbackCommand>::New();
  this->PhysicalImageDataObserver->SetClientData(this);
  this->PhysicalImageDataObserver->SetCallback(vtkMRMLScalarVolumeNode::OnPhysicalImageDataModified);
}

//----------------------------------------------------------------------------
vtkMRMLScalarVolumeNode::~vtkMRMLScalarVolumeNode()
{
  if (this->VoxelDataProvider)
  {
    this->VoxelDataProvider->RemoveObserver(this->VoxelDataProviderObserver);
    this->VoxelDataProvider = nullptr;
  }
  this->RemovePhysicalImageDataObservers();
  this->SetVoxelValueQuantity(nullptr);
  this->SetVoxelValueUnits(nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::RemovePhysicalImageDataObservers()
{
  if (this->ObservedPhysicalImageData)
  {
    this->ObservedPhysicalImageData->RemoveObserver(this->PhysicalImageDataObserver);
    this->ObservedPhysicalImageData = nullptr;
  }
  if (this->ObservedPhysicalImageScalars)
  {
    this->ObservedPhysicalImageScalars->RemoveObserver(this->PhysicalImageDataObserver);
    this->ObservedPhysicalImageScalars = nullptr;
  }
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
  if (this->GetVoxelValueQuantity())
  {
    of << " voxelValueQuantity=\"" << vtkMRMLNode::URLEncodeString(this->GetVoxelValueQuantity()->GetAsString().c_str()) << "\"";
  }
  if (this->GetVoxelValueUnits())
  {
    of << " voxelValueUnits=\"" << vtkMRMLNode::URLEncodeString(this->GetVoxelValueUnits()->GetAsString().c_str()) << "\"";
  }
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::ReadXMLAttributes(const char** atts)
{
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  // For backward compatibility, we read the labelMap attribute and save it as a custom attribute.
  // This allows scene reader to detect that this node has to be converted to a segmentation node.
  const char* attName;
  const char* attValue;
  while (*atts != nullptr)
  {
    attName = *(atts++);
    attValue = *(atts++);
    if (!strcmp(attName, "labelMap"))
    {
      std::stringstream ss;
      int val;
      ss << attValue;
      ss >> val;
      if (val)
      {
        this->SetAttribute("LabelMap", "1");
      }
    }
    else if (!strcmp(attName, "voxelValueQuantity"))
    {
      vtkNew<vtkCodedEntry> entry;
      entry->SetFromString(vtkMRMLNode::URLDecodeString(attValue));
      this->SetVoxelValueQuantity(entry.GetPointer());
    }
    else if (!strcmp(attName, "voxelValueUnits"))
    {
      vtkNew<vtkCodedEntry> entry;
      entry->SetFromString(vtkMRMLNode::URLDecodeString(attValue));
      this->SetVoxelValueUnits(entry.GetPointer());
    }
  }

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::CopyContent(vtkMRMLNode* anode, bool deepCopy /*=true*/)
{
  MRMLNodeModifyBlocker blocker(this);
  // The image data (stored or physical) is copied by the CopyImageData
  // override, which the superclass calls.
  Superclass::CopyContent(anode, deepCopy);

  vtkMRMLScalarVolumeNode* node = vtkMRMLScalarVolumeNode::SafeDownCast(anode);
  if (!node)
  {
    return;
  }
  this->CopyVoxelValueMetadata(node);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::CopyImageData(vtkMRMLVolumeNode* sourceNode, bool deepCopy)
{
  vtkMRMLScalarVolumeNode* scalarSource = vtkMRMLScalarVolumeNode::SafeDownCast(sourceNode);
  vtkMRMLVoxelDataProvider* sourceProvider = scalarSource ? scalarSource->GetVoxelDataProvider() : nullptr;
  if (!sourceProvider)
  {
    this->SetVoxelDataProvider(nullptr);
    Superclass::CopyImageData(sourceNode, deepCopy);
    return;
  }
  // Copy the packed representation without generating the physical image on
  // the source (important e.g. when volumes are copied while browsing a
  // sequence).
  vtkImageData* sourceStored = sourceProvider->GetStoredImageDataIfInMemory();
  if (sourceStored)
  {
    vtkSmartPointer<vtkImageData> stored = sourceStored;
    if (deepCopy)
    {
      stored = vtkSmartPointer<vtkImageData>::New();
      stored->DeepCopy(sourceStored);
    }
    this->SetStoredImageData(stored, sourceProvider->GetVoxelValueScale(), sourceProvider->GetVoxelValueOffset());
  }
  else
  {
    // Non-in-memory backend: share the provider.
    this->SetVoxelDataProvider(sourceProvider);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::CopyVoxelValueMetadata(vtkMRMLScalarVolumeNode* source)
{
  if (!source)
  {
    return;
  }
  if (source->GetVoxelValueQuantity())
  {
    vtkNew<vtkCodedEntry> quantity;
    quantity->Copy(source->GetVoxelValueQuantity());
    this->SetVoxelValueQuantity(quantity.GetPointer());
  }
  else
  {
    this->SetVoxelValueQuantity(nullptr);
  }
  if (source->GetVoxelValueUnits())
  {
    vtkNew<vtkCodedEntry> units;
    units->Copy(source->GetVoxelValueUnits());
    this->SetVoxelValueUnits(units.GetPointer());
  }
  else
  {
    this->SetVoxelValueUnits(nullptr);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::SetVoxelDataProvider(vtkMRMLVoxelDataProvider* provider)
{
  if (provider == this->VoxelDataProvider)
  {
    return;
  }
  if (this->VoxelDataProvider)
  {
    this->VoxelDataProvider->RemoveObserver(this->VoxelDataProviderObserver);
  }
  this->VoxelDataProvider = provider;
  this->PhysicalImageDataUpToDate = false;
  if (this->VoxelDataProvider)
  {
    this->VoxelDataProvider->AddObserver(vtkCommand::ModifiedEvent, this->VoxelDataProviderObserver);
    // The provider becomes the authoritative representation: drop any
    // previously held (physical) image. It is regenerated lazily, only when
    // scaling-unaware code requests it via GetImageData()/
    // GetImageDataConnection(). Scaling-aware consumers (display pipeline,
    // Data Probe, storage) use the stored tier and never trigger generation.
    this->RemovePhysicalImageDataObservers();
    bool wasInternal = this->InternalPhysicalImageDataUpdate;
    this->InternalPhysicalImageDataUpdate = true;
    this->Superclass::SetAndObserveImageData(nullptr);
    this->InternalPhysicalImageDataUpdate = wasInternal;
    this->UpdateStoredImageDataConnection();
    // Route display nodes to the stored tier (with the new value mapping)
    this->SetImageDataToDisplayNodes();
    this->InvokeCustomModifiedEvent(vtkMRMLVolumeNode::ImageDataModifiedEvent);
  }
  else
  {
    this->RemovePhysicalImageDataObservers();
    this->StoredImageDataProducer = nullptr;
    // Route display nodes back to the (physical) image data
    this->SetImageDataToDisplayNodes();
  }
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLVoxelDataProvider* vtkMRMLScalarVolumeNode::GetVoxelDataProvider()
{
  return this->VoxelDataProvider;
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::SetStoredImageData(vtkImageData* storedImageData, double voxelValueScale, double voxelValueOffset)
{
  if (!storedImageData)
  {
    this->SetVoxelDataProvider(nullptr);
    this->SetAndObserveImageData(nullptr);
    return;
  }
  if (voxelValueScale == 0.0)
  {
    vtkErrorMacro("SetStoredImageData: voxelValueScale must not be 0");
    return;
  }
  if (voxelValueScale == 1.0 && voxelValueOffset == 0.0)
  {
    // Identity mapping: no need for a provider.
    this->SetVoxelDataProvider(nullptr);
    this->SetAndObserveImageData(storedImageData);
    return;
  }
  vtkNew<vtkMRMLInMemoryVoxelDataProvider> provider;
  provider->SetStoredImageData(storedImageData);
  provider->SetVoxelValueScale(voxelValueScale);
  provider->SetVoxelValueOffset(voxelValueOffset);
  this->SetVoxelDataProvider(provider.GetPointer());
}

//----------------------------------------------------------------------------
vtkImageData* vtkMRMLScalarVolumeNode::GetStoredImageData()
{
  if (this->VoxelDataProvider)
  {
    return this->VoxelDataProvider->GetStoredImageDataIfInMemory();
  }
  return this->GetImageData();
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLScalarVolumeNode::GetStoredImageDataConnection()
{
  if (!this->VoxelDataProvider)
  {
    return this->GetImageDataConnection();
  }
  if (!this->StoredImageDataProducer)
  {
    this->UpdateStoredImageDataConnection();
  }
  return this->StoredImageDataProducer ? this->StoredImageDataProducer->GetOutputPort() : nullptr;
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::UpdateStoredImageDataConnection()
{
  if (!this->VoxelDataProvider)
  {
    this->StoredImageDataProducer = nullptr;
    return;
  }
  vtkSmartPointer<vtkImageData> storedImage = this->VoxelDataProvider->GetStoredImageDataIfInMemory();
  if (!storedImage)
  {
    // Non-in-memory backend: materialize the full-resolution stored image.
    // (Region/level-based streaming for out-of-core backends is future work.)
    int extent[6] = { 0, -1, 0, -1, 0, -1 };
    if (!this->VoxelDataProvider->GetExtent(extent))
    {
      this->StoredImageDataProducer = nullptr;
      return;
    }
    storedImage = vtkSmartPointer<vtkImageData>::New();
    if (!this->VoxelDataProvider->GetRegion(storedImage, extent))
    {
      this->StoredImageDataProducer = nullptr;
      return;
    }
  }
  if (!this->StoredImageDataProducer)
  {
    this->StoredImageDataProducer = vtkSmartPointer<vtkTrivialProducer>::New();
  }
  if (this->StoredImageDataProducer->GetOutputDataObject(0) != storedImage)
  {
    this->StoredImageDataProducer->SetOutput(storedImage);
  }
}

//----------------------------------------------------------------------------
bool vtkMRMLScalarVolumeNode::IsPhysicalImageDataMaterialized()
{
  if (!this->VoxelDataProvider)
  {
    return this->Superclass::GetImageData() != nullptr;
  }
  return (this->PhysicalImageDataUpToDate && this->Superclass::GetImageData() != nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::EnsurePhysicalImageData()
{
  if (!this->VoxelDataProvider)
  {
    return;
  }
  if (this->PhysicalImageDataUpToDate && this->Superclass::GetImageData() != nullptr)
  {
    return;
  }
  this->UpdatePhysicalImageDataFromProvider();
}

//----------------------------------------------------------------------------
vtkImageData* vtkMRMLScalarVolumeNode::GetImageData()
{
  if (this->VoxelDataProvider && !this->InternalPhysicalImageDataUpdate)
  {
    this->EnsurePhysicalImageData();
  }
  return this->Superclass::GetImageData();
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLScalarVolumeNode::GetImageDataConnection()
{
  if (this->VoxelDataProvider && !this->InternalPhysicalImageDataUpdate)
  {
    this->EnsurePhysicalImageData();
  }
  return this->Superclass::GetImageDataConnection();
}

//----------------------------------------------------------------------------
bool vtkMRMLScalarVolumeNode::HasImageData()
{
  if (this->VoxelDataProvider)
  {
    int extent[6] = { 0, -1, 0, -1, 0, -1 };
    return this->VoxelDataProvider->GetExtent(extent);
  }
  return this->Superclass::HasImageData();
}

//----------------------------------------------------------------------------
bool vtkMRMLScalarVolumeNode::GetImageExtent(int extent[6])
{
  if (this->VoxelDataProvider)
  {
    return this->VoxelDataProvider->GetExtent(extent);
  }
  return this->Superclass::GetImageExtent(extent);
}

//----------------------------------------------------------------------------
double vtkMRMLScalarVolumeNode::GetImageBackgroundScalarComponentAsDouble(int component)
{
  if (!this->VoxelDataProvider)
  {
    return this->Superclass::GetImageBackgroundScalarComponentAsDouble(component);
  }
  if (component >= this->VoxelDataProvider->GetNumberOfScalarComponents())
  {
    return 0.0;
  }
  int extent[6] = { 0, -1, 0, -1, 0, -1 };
  if (!this->VoxelDataProvider->GetExtent(extent) || extent[0] > extent[1] || extent[2] > extent[3] || extent[4] > extent[5])
  {
    return 0.0;
  }
  std::vector<double> scalarValues;
  for (int i = 0; i < 2; ++i)
  {
    for (int j = 0; j < 2; ++j)
    {
      for (int k = 0; k < 2; ++k)
      {
        scalarValues.push_back(this->VoxelDataProvider->GetVoxelValue(extent[i], extent[2 + j], extent[4 + k], component));
      }
    }
  }
  const int medianElementIndex = 3;
  std::nth_element(scalarValues.begin(), scalarValues.begin() + medianElementIndex, scalarValues.end());
  // Return the physical value (this method is part of the generic API)
  return this->VoxelDataProvider->GetPhysicalValueFromStoredValue(scalarValues[medianElementIndex]);
}

//----------------------------------------------------------------------------
bool vtkMRMLScalarVolumeNode::GetModifiedSinceRead()
{
  if (this->VoxelDataProvider)
  {
    vtkImageData* storedImage = this->VoxelDataProvider->GetStoredImageDataIfInMemory();
    return this->vtkMRMLStorableNode::GetModifiedSinceRead() || //
           (storedImage && storedImage->GetMTime() > this->GetStoredTime());
  }
  return this->Superclass::GetModifiedSinceRead();
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::UpdateScene(vtkMRMLScene* scene)
{
  if (this->VoxelDataProvider)
  {
    // Skip the superclass image data poke (SetAndObserveImageData(
    // GetImageData())), which would generate the physical image; refresh the
    // display node connections instead.
    this->vtkMRMLDisplayableNode::UpdateScene(scene);
    this->SetImageDataToDisplayNodes();
    return;
  }
  this->Superclass::UpdateScene(scene);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::SetImageDataToDisplayNode(vtkMRMLVolumeDisplayNode* displayNode)
{
  vtkMRMLScalarVolumeDisplayNode* scalarDisplayNode = vtkMRMLScalarVolumeDisplayNode::SafeDownCast(displayNode);
  if (scalarDisplayNode)
  {
    if (this->VoxelDataProvider)
    {
      // Stored-tier display: the display pipeline consumes stored values and
      // keeps window/level/threshold in stored units; presentation layers
      // convert using the value mapping. Rendering output is pixel-identical
      // because the value mapping and window/level are both affine.
      scalarDisplayNode->SetVoxelValueScale(this->VoxelDataProvider->GetVoxelValueScale());
      scalarDisplayNode->SetVoxelValueOffset(this->VoxelDataProvider->GetVoxelValueOffset());
      scalarDisplayNode->SetInputImageDataConnection(this->GetStoredImageDataConnection());
      return;
    }
    scalarDisplayNode->SetVoxelValueScale(1.0);
    scalarDisplayNode->SetVoxelValueOffset(0.0);
  }
  this->Superclass::SetImageDataToDisplayNode(displayNode);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::ApplyNonLinearTransform(vtkAbstractTransform* transform)
{
  if (!this->VoxelDataProvider)
  {
    Superclass::ApplyNonLinearTransform(transform);
    return;
  }
  if (!this->CanApplyNonLinearTransforms())
  {
    return;
  }
  double scale = this->VoxelDataProvider->GetVoxelValueScale();
  double offset = this->VoxelDataProvider->GetVoxelValueOffset();

  vtkNew<vtkImageData> transformedImage;
  vtkNew<vtkMatrix4x4> transformedIJKToRAS;
  // GetTransformedImageData resamples the stored image for volumes with
  // active voxel value scaling.
  if (!vtkMRMLVolumeNode::GetTransformedImageData(this, transform, transformedImage, transformedIJKToRAS))
  {
    vtkErrorMacro("ApplyNonLinearTransform: failed to get transformed image data");
    return;
  }

  int wasModified = this->StartModify();
  // Hardening preserves the packed representation: the resampled image holds
  // stored values with an unchanged value mapping.
  this->SetStoredImageData(transformedImage, scale, offset);
  this->SetIJKToRASMatrix(transformedIJKToRAS);
  this->EndModify(wasModified);
}

//----------------------------------------------------------------------------
vtkMRMLUnitNode* vtkMRMLScalarVolumeNode::GetVoxelValueUnitNode()
{
  if (!this->GetScene() || !this->GetVoxelValueQuantity() //
      || !this->GetVoxelValueQuantity()->GetCodeMeaning() //
      || strlen(this->GetVoxelValueQuantity()->GetCodeMeaning()) == 0)
  {
    return nullptr;
  }
  vtkMRMLSelectionNode* selectionNode = vtkMRMLSelectionNode::SafeDownCast(this->GetScene()->GetNodeByID("vtkMRMLSelectionNodeSingleton"));
  if (!selectionNode)
  {
    return nullptr;
  }
  // Unit quantities are registered with lowercase names (e.g. "velocity"),
  // coded entries use title case meanings (e.g. "Velocity").
  std::string quantity = this->GetVoxelValueQuantity()->GetCodeMeaning();
  std::transform(quantity.begin(), quantity.end(), quantity.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return selectionNode->GetUnitNode(quantity.c_str());
}

//----------------------------------------------------------------------------
bool vtkMRMLScalarVolumeNode::IsVoxelValueScalingActive()
{
  return (this->VoxelDataProvider && this->VoxelDataProvider->IsVoxelValueScalingActive());
}

//----------------------------------------------------------------------------
double vtkMRMLScalarVolumeNode::GetVoxelValueScale()
{
  return this->VoxelDataProvider ? this->VoxelDataProvider->GetVoxelValueScale() : 1.0;
}

//----------------------------------------------------------------------------
double vtkMRMLScalarVolumeNode::GetVoxelValueOffset()
{
  return this->VoxelDataProvider ? this->VoxelDataProvider->GetVoxelValueOffset() : 0.0;
}

//----------------------------------------------------------------------------
double vtkMRMLScalarVolumeNode::GetPhysicalValueFromStoredValue(double storedValue)
{
  return this->VoxelDataProvider ? this->VoxelDataProvider->GetPhysicalValueFromStoredValue(storedValue) : storedValue;
}

//----------------------------------------------------------------------------
double vtkMRMLScalarVolumeNode::GetStoredValueFromPhysicalValue(double physicalValue)
{
  return this->VoxelDataProvider ? this->VoxelDataProvider->GetStoredValueFromPhysicalValue(physicalValue) : physicalValue;
}

//----------------------------------------------------------------------------
std::string vtkMRMLScalarVolumeNode::GetPhysicalValueAsString(double physicalValue, int precision /*=-1*/)
{
  // Bridge to the application units: the unit node matching the voxel value
  // quantity provides the display precision (and a fallback suffix).
  // Its DisplayCoefficient is not applied: physical voxel values are already
  // expressed in VoxelValueUnits.
  vtkMRMLUnitNode* unitNode = this->GetVoxelValueUnitNode();
  if (precision < 0)
  {
    precision = unitNode ? unitNode->GetPrecision() : 6;
  }
  std::ostringstream value;
  value.precision(precision);
  value << physicalValue;
  const char* suffix = nullptr;
  if (this->GetVoxelValueUnits() && this->GetVoxelValueUnits()->GetCodeValue() //
      && strlen(this->GetVoxelValueUnits()->GetCodeValue()) > 0)
  {
    // UCUM code value is the short displayable form of the unit (e.g. "cm/s").
    // "1" is the UCUM code for dimensionless ("no units").
    if (strcmp(this->GetVoxelValueUnits()->GetCodeValue(), "1") != 0)
    {
      suffix = this->GetVoxelValueUnits()->GetCodeValue();
    }
  }
  else if (unitNode && unitNode->GetSuffix() && strlen(unitNode->GetSuffix()) > 0)
  {
    suffix = unitNode->GetSuffix();
  }
  if (suffix)
  {
    value << " " << suffix;
  }
  return value.str();
}

//----------------------------------------------------------------------------
std::string vtkMRMLScalarVolumeNode::GetVoxelValueAsString(int i, int j, int k, int component /*=0*/, int precision /*=-1*/)
{
  double physicalValue = 0.0;
  if (this->VoxelDataProvider)
  {
    int extent[6] = { 0, -1, 0, -1, 0, -1 };
    if (!this->VoxelDataProvider->GetExtent(extent)     //
        || i < extent[0] || i > extent[1]               //
        || j < extent[2] || j > extent[3]               //
        || k < extent[4] || k > extent[5]               //
        || component < 0                                //
        || component >= this->VoxelDataProvider->GetNumberOfScalarComponents())
    {
      return std::string();
    }
    physicalValue = this->VoxelDataProvider->GetPhysicalValueFromStoredValue( //
      this->VoxelDataProvider->GetVoxelValue(i, j, k, component));
  }
  else
  {
    vtkImageData* imageData = this->GetImageData();
    if (!imageData)
    {
      return std::string();
    }
    int* extent = imageData->GetExtent();
    if (i < extent[0] || i > extent[1]    //
        || j < extent[2] || j > extent[3] //
        || k < extent[4] || k > extent[5] //
        || component < 0                  //
        || component >= imageData->GetNumberOfScalarComponents())
    {
      return std::string();
    }
    physicalValue = imageData->GetScalarComponentAsDouble(i, j, k, component);
  }
  return this->GetPhysicalValueAsString(physicalValue, precision);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::SetAndObserveImageData(vtkImageData* imageData)
{
  // Compare against the superclass image (without triggering lazy physical
  // image generation).
  if (!this->InternalPhysicalImageDataUpdate //
      && this->VoxelDataProvider             //
      && imageData != this->Superclass::GetImageData())
  {
    // Generic (scaling-unaware) code replaces the image data: the new image
    // contains plain physical values, therefore voxel value scaling becomes
    // inactive.
    this->SetVoxelDataProvider(nullptr);
  }
  Superclass::SetAndObserveImageData(imageData);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::UpdatePhysicalImageDataFromProvider()
{
  if (!this->VoxelDataProvider)
  {
    return;
  }
  double scale = this->VoxelDataProvider->GetVoxelValueScale();
  double offset = this->VoxelDataProvider->GetVoxelValueOffset();
  if (scale == 0.0)
  {
    vtkErrorMacro("UpdatePhysicalImageDataFromProvider: invalid VoxelValueScale (0)");
    return;
  }

  vtkSmartPointer<vtkImageData> storedImage = this->VoxelDataProvider->GetStoredImageDataIfInMemory();
  if (!storedImage)
  {
    int extent[6] = { 0, -1, 0, -1, 0, -1 };
    if (!this->VoxelDataProvider->GetExtent(extent))
    {
      vtkErrorMacro("UpdatePhysicalImageDataFromProvider: provider has no data");
      return;
    }
    storedImage = vtkSmartPointer<vtkImageData>::New();
    if (!this->VoxelDataProvider->GetRegion(storedImage, extent))
    {
      vtkErrorMacro("UpdatePhysicalImageDataFromProvider: failed to get voxel data from provider");
      return;
    }
  }

  vtkSmartPointer<vtkImageData> physicalImage;
  if (scale == 1.0 && offset == 0.0)
  {
    physicalImage = storedImage;
  }
  else
  {
    // physical = scale * stored + offset  ==  ((stored + offset/scale) * scale)
    vtkNew<vtkImageShiftScale> shiftScale;
    shiftScale->SetInputData(storedImage);
    shiftScale->SetShift(offset / scale);
    shiftScale->SetScale(scale);
    shiftScale->SetOutputScalarTypeToFloat();
    shiftScale->Update();
    physicalImage = vtkSmartPointer<vtkImageData>::New();
    physicalImage->ShallowCopy(shiftScale->GetOutput());
  }

  // Stop watching the previous physical image before it is replaced, so that
  // a still-referenced old image cannot trigger a spurious promotion later.
  if (this->ObservedPhysicalImageData.GetPointer() != physicalImage.GetPointer())
  {
    this->RemovePhysicalImageDataObservers();
  }

  bool wasInternal = this->InternalPhysicalImageDataUpdate;
  this->InternalPhysicalImageDataUpdate = true;
  this->SetAndObserveImageData(physicalImage);
  this->InternalPhysicalImageDataUpdate = wasInternal;

  // Watch the physical image so that in-place modification by generic code
  // (which would diverge from the stored image) promotes it to primary.
  // The scalar array is observed in addition to the image because the
  // canonical in-place modification pattern
  // (slicer.util.arrayFromVolumeModified) invokes Modified() on the scalar
  // array only.
  if (this->ObservedPhysicalImageData.GetPointer() != physicalImage.GetPointer())
  {
    physicalImage->AddObserver(vtkCommand::ModifiedEvent, this->PhysicalImageDataObserver);
    this->ObservedPhysicalImageData = physicalImage;
    vtkDataArray* scalars = physicalImage->GetPointData() ? physicalImage->GetPointData()->GetScalars() : nullptr;
    if (scalars)
    {
      scalars->AddObserver(vtkCommand::ModifiedEvent, this->PhysicalImageDataObserver);
      this->ObservedPhysicalImageScalars = scalars;
    }
  }

  this->PhysicalImageDataUpToDate = true;
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::PromotePhysicalImageDataToPrimary()
{
  if (!this->VoxelDataProvider)
  {
    return;
  }
  vtkDebugMacro("PromotePhysicalImageDataToPrimary: physical image was modified in place;"
                << " discarding stored image and deactivating voxel value scaling");
  this->SetVoxelDataProvider(nullptr);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::OnVoxelDataProviderModified(vtkObject* vtkNotUsed(caller),
                                                          unsigned long vtkNotUsed(eid),
                                                          void* clientData,
                                                          void* vtkNotUsed(callData))
{
  vtkMRMLScalarVolumeNode* self = reinterpret_cast<vtkMRMLScalarVolumeNode*>(clientData);
  if (!self || self->InternalPhysicalImageDataUpdate)
  {
    return;
  }
  // Stored voxel data (or the value mapping) changed.
  bool physicalWasMaterialized = self->IsPhysicalImageDataMaterialized();
  self->PhysicalImageDataUpToDate = false;
  // The stored image instance may have been replaced: refresh the stored
  // connection (in-place edits keep the same image and need no refresh).
  self->UpdateStoredImageDataConnection();
  if (physicalWasMaterialized)
  {
    // Keep already-handed-out physical data consistent; if the physical
    // image was never generated, stay lazy.
    self->UpdatePhysicalImageDataFromProvider();
  }
  // Refresh display node inputs (the value mapping may have changed)
  self->SetImageDataToDisplayNodes();
  // Notify pipelines/views observing this node that the voxel data changed.
  self->InvokeCustomModifiedEvent(vtkMRMLVolumeNode::ImageDataModifiedEvent);
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::OnPhysicalImageDataModified(vtkObject* caller,
                                                          unsigned long vtkNotUsed(eid),
                                                          void* clientData,
                                                          void* vtkNotUsed(callData))
{
  vtkMRMLScalarVolumeNode* self = reinterpret_cast<vtkMRMLScalarVolumeNode*>(clientData);
  if (!self || self->InternalPhysicalImageDataUpdate)
  {
    return;
  }
  vtkImageData* currentImage = self->GetImageData();
  vtkObject* currentScalars = (currentImage && currentImage->GetPointData()) ? currentImage->GetPointData()->GetScalars() : nullptr;
  if (caller != currentImage && caller != currentScalars)
  {
    // Event from an image (or scalar array) that is no longer this node's
    // physical image.
    return;
  }
  self->PromotePhysicalImageDataToPrimary();
}

//-----------------------------------------------------------
void vtkMRMLScalarVolumeNode::CreateNoneNode(vtkMRMLScene* scene)
{
  // Create a None volume RGBA of 0, 0, 0 so the filters won't complain
  // about missing input
  vtkNew<vtkImageData> id;
  id->SetDimensions(1, 1, 1);
  id->AllocateScalars(VTK_DOUBLE, 4);
  id->GetPointData()->GetScalars()->FillComponent(0, 0.0);
  id->GetPointData()->GetScalars()->FillComponent(1, 125.0);
  id->GetPointData()->GetScalars()->FillComponent(2, 0.0);
  id->GetPointData()->GetScalars()->FillComponent(3, 0.0);

  vtkNew<vtkMRMLScalarVolumeNode> n;
  n->SetName("None");
  // the scene will set the id
  n->SetAndObserveImageData(id.GetPointer());
  scene->AddNode(n.GetPointer());
}

//----------------------------------------------------------------------------
vtkMRMLScalarVolumeDisplayNode* vtkMRMLScalarVolumeNode::GetScalarVolumeDisplayNode()
{
  return vtkMRMLScalarVolumeDisplayNode::SafeDownCast(this->GetVolumeDisplayNode());
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  if (this->GetVoxelValueQuantity())
  {
    os << indent << "VoxelValueQuantity: " << this->GetVoxelValueQuantity()->GetAsPrintableString() << "\n";
  }
  if (this->GetVoxelValueUnits())
  {
    os << indent << "VoxelValueUnits: " << this->GetVoxelValueUnits()->GetAsPrintableString() << "\n";
  }
  os << indent << "VoxelValueScalingActive: " << (this->IsVoxelValueScalingActive() ? "true" : "false") << "\n";
  if (this->VoxelDataProvider)
  {
    os << indent << "VoxelDataProvider:\n";
    this->VoxelDataProvider->PrintSelf(os, indent.GetNextIndent());
  }
}

//---------------------------------------------------------------------------
vtkMRMLStorageNode* vtkMRMLScalarVolumeNode::CreateDefaultStorageNode()
{
  vtkMRMLScene* scene = this->GetScene();
  if (scene == nullptr)
  {
    vtkErrorMacro("CreateDefaultStorageNode failed: scene is invalid");
    return nullptr;
  }
  return vtkMRMLStorageNode::SafeDownCast(scene->CreateNodeByClass("vtkMRMLVolumeArchetypeStorageNode"));
}

//----------------------------------------------------------------------------
void vtkMRMLScalarVolumeNode::CreateDefaultDisplayNodes()
{
  if (vtkMRMLScalarVolumeDisplayNode::SafeDownCast(this->GetDisplayNode()) != nullptr)
  {
    // display node already exists
    return;
  }
  if (this->GetScene() == nullptr)
  {
    vtkErrorMacro("vtkMRMLScalarVolumeNode::CreateDefaultDisplayNodes failed: scene is invalid");
    return;
  }
  vtkMRMLScalarVolumeDisplayNode* dispNode = vtkMRMLScalarVolumeDisplayNode::SafeDownCast(this->GetScene()->AddNewNodeByClass("vtkMRMLScalarVolumeDisplayNode"));
  // If color node is already specified (via default display node mechanism) then use that,
  // otherwise set the default color specified in this class.
  if (!dispNode->GetColorNodeID())
  {
    dispNode->SetDefaultColorMap();
  }
  this->SetAndObserveDisplayNodeID(dispNode->GetID());
}
