/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLVolumeNode.h,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.13 $

=========================================================================auto=*/

#ifndef __vtkMRMLScalarVolumeNode_h
#define __vtkMRMLScalarVolumeNode_h

// MRML includes
#include "vtkMRMLVolumeNode.h"
class vtkMRMLScalarVolumeDisplayNode;
class vtkMRMLVoxelDataProvider;
class vtkCodedEntry;

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

// STD includes
#include <string>

class vtkCallbackCommand;
class vtkImageData;

/// \brief MRML node for representing a volume (image stack).
///
/// Volume nodes describe data sets that can be thought of as stacks of 2D
/// images that form a 3D volume. Volume nodes contain only the image data,
/// where it is store on disk and how to read the files is controlled by
/// the volume storage node, how to render the data (window and level) is
/// controlled by the volume display nodes. Image information is extracted
/// from the image headers (if they exist) at the time the MRML file is
/// generated.
/// Consequently, MRML files isolate MRML browsers from understanding how
/// to read the myriad of file formats for medical data.
class VTK_MRML_EXPORT vtkMRMLScalarVolumeNode : public vtkMRMLVolumeNode
{
public:
  static vtkMRMLScalarVolumeNode* New();
  vtkTypeMacro(vtkMRMLScalarVolumeNode, vtkMRMLVolumeNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  vtkMRMLNode* CreateNodeInstance() override;

  ///
  /// Set node attributes
  void ReadXMLAttributes(const char** atts) override;

  ///
  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLScalarVolumeNode);

  ///
  /// Get node XML tag name (like Volume, Model)
  const char* GetNodeTagName() override { return "Volume"; }

  ///
  /// Make a 'None' volume node with blank image data
  static void CreateNoneNode(vtkMRMLScene* scene);

  ///
  /// Associated display MRML node
  virtual vtkMRMLScalarVolumeDisplayNode* GetScalarVolumeDisplayNode();

  ///
  /// Create default storage node or nullptr if does not have one
  vtkMRMLStorageNode* CreateDefaultStorageNode() override;

  ///
  /// Create and observe default display node
  void CreateDefaultDisplayNodes() override;

  /// Measured quantity of voxel values, specified as a standard coded entry.
  /// For example: (DCM, 112031, "Attenuation Coefficient")
  void SetVoxelValueQuantity(vtkCodedEntry*);
  vtkGetObjectMacro(VoxelValueQuantity, vtkCodedEntry);

  /// Measurement unit of voxel value quantity, specified as a standard coded entry.
  /// For example: (UCUM, [hnsf'U], "Hounsfield unit")
  /// It stores a single unit. Plural (units) name is chosen to be consistent
  /// with nomenclature in the DICOM standard.
  void SetVoxelValueUnits(vtkCodedEntry*);
  vtkGetObjectMacro(VoxelValueUnits, vtkCodedEntry);

  //@{
  /// Voxel value scaling: stored values vs physical values.
  ///
  /// A volume may internally keep "stored" voxel values (for example 16-bit
  /// integers as read from file) along with a linear mapping to "physical"
  /// values (what the user should see, for example cm/s):
  ///
  ///   physical = VoxelValueScale * stored + VoxelValueOffset
  ///
  /// This mirrors DICOM Rescale Slope/Intercept and Real World Value Mapping.
  ///
  /// Correct by default, efficient when aware:
  /// - GetImageData() (and all existing generic access) always returns
  ///   physical values, so modules that know nothing about voxel value scaling
  ///   remain correct. When scaling is active, the physical image is a float32
  ///   image derived from the stored image.
  /// - Aware consumers use GetVoxelDataProvider()/GetStoredImageData() and
  ///   the value conversion methods to work directly with stored values.
  /// - If generic code modifies the physical image in place (and calls
  ///   Modified() on it, as required for views to update), the physical image
  ///   is promoted to primary: the stored image is discarded and scaling
  ///   becomes inactive. Values remain correct; only the memory optimization
  ///   and the scaling metadata are lost for this node.
  /// - Replacing the image via SetAndObserveImageData() deactivates scaling
  ///   (the new image is taken as plain physical values, as before).
  ///
  /// Note: when the volume is written to file by a storage node, physical
  /// values are written (the materialized image), therefore re-loaded scenes
  /// are numerically correct with scaling simply inactive. Persisting stored
  /// values + scaling in image files is not yet implemented.

  /// Set the voxel data provider that supplies stored voxel values and the
  /// stored-to-physical mapping. Setting a provider (re)generates the
  /// physical image data of this node. Setting nullptr deactivates voxel
  /// value scaling (the current image data is kept as-is).
  void SetVoxelDataProvider(vtkMRMLVoxelDataProvider* provider);
  vtkMRMLVoxelDataProvider* GetVoxelDataProvider();

  /// Convenience method: wrap \a storedImageData in an in-memory voxel data
  /// provider with the given scale/offset and set it as this node's provider.
  /// If scale is 1 and offset is 0, the image is simply set as this node's
  /// (physical) image data and no provider is created.
  void SetStoredImageData(vtkImageData* storedImageData, double voxelValueScale, double voxelValueOffset);

  /// Get the stored image. If voxel value scaling is inactive, this is the
  /// same as GetImageData(). If the provider is not an in-memory provider,
  /// nullptr may be returned (use the provider's GetRegion() instead).
  vtkImageData* GetStoredImageData();

  /// Returns true if a voxel data provider with a non-identity value mapping
  /// is active on this node.
  bool IsVoxelValueScalingActive();

  /// Multiplicative part of the stored-to-physical mapping (1.0 when scaling
  /// is inactive).
  double GetVoxelValueScale();

  /// Additive part of the stored-to-physical mapping (0.0 when scaling is
  /// inactive).
  double GetVoxelValueOffset();

  /// Convert a stored voxel value to a physical value.
  double GetPhysicalValueFromStoredValue(double storedValue);

  /// Convert a physical value to a stored voxel value.
  double GetStoredValueFromPhysicalValue(double physicalValue);

  /// Get the physical value of a voxel as a formatted string, with the unit
  /// suffix (from VoxelValueUnits code value, e.g. "cm/s") appended if set.
  /// \a precision is the number of significant digits (-1 uses default: 6).
  std::string GetVoxelValueAsString(int i, int j, int k, int component = 0, int precision = -1);

  /// Format a physical value with the unit suffix appended if set.
  /// \a precision is the number of significant digits (-1 uses default: 6).
  std::string GetPhysicalValueAsString(double physicalValue, int precision = -1);

  /// Copy voxel value quantity and units from another volume node.
  /// Intended for processing modules whose operation preserves the meaning of
  /// the voxel values (e.g. blurring, resampling). Does not copy scale/offset
  /// (those travel with the stored image via SetStoredImageData/provider).
  void CopyVoxelValueMetadata(vtkMRMLScalarVolumeNode* source);

  /// Reimplemented to deactivate voxel value scaling when generic code
  /// replaces the image data (the new image is plain physical values).
  void SetAndObserveImageData(vtkImageData* imageData) override;
  //@}

protected:
  vtkMRMLScalarVolumeNode();
  ~vtkMRMLScalarVolumeNode() override;
  vtkMRMLScalarVolumeNode(const vtkMRMLScalarVolumeNode&);
  void operator=(const vtkMRMLScalarVolumeNode&);

  /// Regenerate the physical image data of this node from the voxel data
  /// provider (full resolution).
  void UpdatePhysicalImageDataFromProvider();

  /// Deactivate voxel value scaling, keeping the current (physical) image.
  void PromotePhysicalImageDataToPrimary();

  /// Remove promote-on-write observers from the physical image and its
  /// scalar array.
  void RemovePhysicalImageDataObservers();

  static void OnVoxelDataProviderModified(vtkObject* caller, unsigned long eid, void* clientData, void* callData);
  static void OnPhysicalImageDataModified(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

  vtkCodedEntry* VoxelValueQuantity{ nullptr };

  vtkCodedEntry* VoxelValueUnits{ nullptr };

  vtkSmartPointer<vtkMRMLVoxelDataProvider> VoxelDataProvider;
  vtkSmartPointer<vtkCallbackCommand> VoxelDataProviderObserver;
  vtkSmartPointer<vtkCallbackCommand> PhysicalImageDataObserver;
  /// Image that PhysicalImageDataObserver is currently added to.
  vtkWeakPointer<vtkImageData> ObservedPhysicalImageData;
  /// Scalar array of the physical image that PhysicalImageDataObserver is
  /// currently added to. Observed in addition to the image itself because
  /// the canonical in-place modification pattern
  /// (slicer.util.arrayFromVolumeModified) invokes Modified() on the scalar
  /// array, not on the image.
  vtkWeakPointer<vtkObject> ObservedPhysicalImageScalars;
  /// True while this node itself is generating/updating the physical image.
  bool InternalPhysicalImageDataUpdate{ false };
};

#endif
