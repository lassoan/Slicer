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
#include "vtkMRMLVolumeArchetypeStorageNode.h"
#include "vtkMRMLVolumeSequenceStorageNode.h"
#include "vtkMRMLVoxelDataProvider.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkDataArray.h>
#include <vtkImageShiftScale.h>
#include <vtkObjectFactory.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

// STD includes
#include <sstream>

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
  // Superclass copies the (physical) image data. Our SetAndObserveImageData
  // override deactivates any provider on this node when the image changes.
  Superclass::CopyContent(anode, deepCopy);

  vtkMRMLScalarVolumeNode* node = vtkMRMLScalarVolumeNode::SafeDownCast(anode);
  if (!node)
  {
    return;
  }

  this->CopyVoxelValueMetadata(node);

  // Copy voxel value scaling state (stored image + mapping)
  vtkMRMLVoxelDataProvider* sourceProvider = node->GetVoxelDataProvider();
  if (!sourceProvider)
  {
    this->SetVoxelDataProvider(nullptr);
    return;
  }
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
  if (this->VoxelDataProvider)
  {
    this->VoxelDataProvider->AddObserver(vtkCommand::ModifiedEvent, this->VoxelDataProviderObserver);
    this->UpdatePhysicalImageDataFromProvider();
  }
  else
  {
    this->RemovePhysicalImageDataObservers();
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
  std::ostringstream value;
  value.precision(precision < 0 ? 6 : precision);
  value << physicalValue;
  if (this->GetVoxelValueUnits() && this->GetVoxelValueUnits()->GetCodeValue() //
      && strlen(this->GetVoxelValueUnits()->GetCodeValue()) > 0)
  {
    // UCUM code value is the short displayable form of the unit (e.g. "cm/s").
    // "1" is the UCUM code for dimensionless ("no units").
    if (strcmp(this->GetVoxelValueUnits()->GetCodeValue(), "1") != 0)
    {
      value << " " << this->GetVoxelValueUnits()->GetCodeValue();
    }
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
  if (!this->InternalPhysicalImageDataUpdate //
      && this->VoxelDataProvider             //
      && imageData != this->GetImageData())
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
  // Stored voxel data (or the value mapping) changed: regenerate the
  // physical image.
  self->UpdatePhysicalImageDataFromProvider();
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
