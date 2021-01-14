/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLVolumeNode.h,v $
  Date:      $Date: 2006/03/19 17:12:29 $
  Version:   $Revision: 1.13 $

=========================================================================auto=*/

#ifndef __vtkMRMLVolumeNode_h
#define __vtkMRMLVolumeNode_h

// MRML includes
#include "vtkMRMLDisplayableNode.h"
class vtkMRMLVolumeDisplayNode;

// VTK includes
class vtkAlgorithmOutput;
class vtkEventForwarderCommand;
class vtkImageData;
class vtkMatrix4x4;

#include "vtkCodedEntry.h"
#include "vtkCollection.h"

// ITK includes
#include "itkMetaDataDictionary.h"

/// \brief MRML node for representing a volume (image stack).
///
/// Volume nodes describe data sets that can be thought of as stacks of 2D
/// images that form a 3D volume.  Volume nodes describe where the images
/// are stored on disk, how to render the data (window and level), and how
/// to read the files.  This information is extracted from the image
/// headers (if they exist) at the time the MRML file is generated.
/// Consequently, MRML files isolate MRML browsers from understanding how
/// to read the myriad of file formats for medical data.
class VTK_MRML_EXPORT vtkMRMLVolumeNode : public vtkMRMLDisplayableNode
{
public:
  vtkTypeMacro(vtkMRMLVolumeNode,vtkMRMLDisplayableNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  vtkMRMLNode* CreateNodeInstance() override = 0;

  ///
  /// Read node attributes from XML file
  void ReadXMLAttributes( const char** atts) override;

  ///
  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy node content (excludes basic data, such as name and node references).
  /// \sa vtkMRMLNode::CopyContent
  vtkMRMLCopyContentMacro(vtkMRMLVectorVolumeDisplayNode);

  ///
  /// Copy the node's attributes to this object
  void CopyOrientation(vtkMRMLVolumeNode *node);


  ///
  /// Get node XML tag name (like Volume, Model)
  const char* GetNodeTagName() override = 0;

  ///
  /// Finds the storage node and read the data
  void UpdateScene(vtkMRMLScene *scene) override;

  //--------------------------------------------------------------------------
  /// RAS->IJK Matrix Calculation
  //--------------------------------------------------------------------------

  ///
  /// The order of slices in the volume. One of: LR (left-to-right),
  /// RL, AP, PA, IS, SI. This information is encoded in the rasToIJKMatrix.
  /// This matrix can be computed either from corner points, or just he
  /// scanOrder.
  /// Return true on success, false otherwise
  static bool ComputeIJKToRASFromScanOrder(const char *order,
                                           const double* spacing,
                                           const int *dims,
                                           bool centerImage,
                                           vtkMatrix4x4 *IJKToRAS);

  static const char* ComputeScanOrderFromIJKToRAS(vtkMatrix4x4 *IJKToRAS);

  void SetIJKToRASDirections(double dirs[3][3]);
  void SetIJKToRASDirections(double ir, double ia, double is,
                             double jr, double ja, double js,
                             double kr, double ka, double ks);
  void SetIToRASDirection(double ir, double ia, double is);
  void SetJToRASDirection(double jr, double ja, double js);
  void SetKToRASDirection(double kr, double ka, double ks);

  void GetIJKToRASDirections(double dirs[3][3]);
  void GetIToRASDirection(double dirs[3]);
  void GetJToRASDirection(double dirs[3]);
  void GetKToRASDirection(double dirs[3]);

  ///
  /// Spacing and Origin, with the Directions, are the independent
  /// parameters that go to make up the IJKToRAS matrix
  /// In setter methods, StorableModifiedTime may need to be updated,
  /// which cannot be achieved by using vtkGetVector3Macro.
  vtkGetVector3Macro (Spacing, double);
  virtual void SetSpacing(double arg1, double arg2, double arg3);
  virtual void SetSpacing(double arg[3]);
  vtkGetVector3Macro (Origin, double);
  virtual void SetOrigin(double arg1, double arg2, double arg3);
  virtual void SetOrigin(double arg[3]);

  ///
  /// Utility function that returns the min spacing between the 3 orientations
  double GetMinSpacing();

  ///
  /// Utility function that returns the max spacing between the 3 orientations
  double GetMaxSpacing();

  ///
  /// Get the IJKToRAS Matrix that includes the spacing and origin
  /// information (assumes the image data is Origin 0 0 0 and Spacing 1 1 1)
  /// RASToIJK is the inverse of this
  void GetIJKToRASMatrix(vtkMatrix4x4* mat);
  void GetRASToIJKMatrix(vtkMatrix4x4* mat);

  void GetIJKToRASDirectionMatrix(vtkMatrix4x4* mat);
  void SetIJKToRASDirectionMatrix(vtkMatrix4x4* mat);

  ///
  /// Convenience methods to set the directions, spacing, and origin
  /// from a matrix
  void SetIJKToRASMatrix(vtkMatrix4x4* mat);
  void SetRASToIJKMatrix(vtkMatrix4x4* mat);

  ///
  /// Get bounding box in global RAS form (xmin,xmax, ymin,ymax, zmin,zmax).
  /// This method returns the bounds of the object with any transforms that may
  /// be applied to it.
  /// \sa GetSliceBounds(), GetIJKToRASMatrix(), vtkMRMLSliceLogic::GetVolumeSliceBounds()
  /// \sa GetNodeBounds()
  void GetRASBounds(double bounds[6]) override;

  /// Get bounding box in global RAS form (xmin,xmax, ymin,ymax, zmin,zmax).
  /// This method always returns the bounds of the untransformed object.
  /// \sa GetRASBounds()
  void GetBounds(double bounds[6]) override;

  ///
  /// Get bounding box in slice form (xmin,xmax, ymin,ymax, zmin,zmax).
  /// If not rasToSlice is passed, then it returns the bounds in global RAS form.
  /// \sa GetRASBounds()
  /// If useVoxelCenter is set to false (default) then bounds of voxel sides are returned
  /// (otherwise then bounds of voxels centers are returned).
  void GetSliceBounds(double bounds[6], vtkMatrix4x4* rasToSlice, bool useVoxelCenter = false);

  ///
  /// Associated display MRML node
  virtual vtkMRMLVolumeDisplayNode* GetVolumeDisplayNode();

  /// In the ImageData object origin must be set to (0,0,0) and spacing must be set
  /// to (1,1,1). If the variables are set to different values then the application's
  /// behavior is undefined.
  /// The reason for not using origin and spacing in vtkImageData is that vtkImageData
  /// cannot store image orientation, and so it cannot store all the information that
  /// is necessary to compute the mapping between voxel (IJK) and  physical (RAS) coordinate systems.
  /// Instead of storing some information in vtkImageData and some outside, the decision was
  /// made to store all information in the MRML node (vtkMRMLVolumeNode::Origin,
  /// vtkMRMLVolumeNode::Spacing, and vtkMRMLVolumeNode::IJKToRASDirections).
  /// \sa GetImageData(), SetImageDataConnection()
  virtual void SetAndObserveImageData(vtkImageData *ImageData);
  virtual vtkImageData* GetImageData();
  /// Set and observe image data pipeline.
  /// It is propagated to the display nodes.
  /// \sa GetImageDataConnection()
  virtual void SetImageDataConnection(vtkAlgorithmOutput *inputPort);
  /// Return the input image data pipeline.
  vtkGetObjectMacro(ImageDataConnection, vtkAlgorithmOutput);

  ///
  /// Make sure image data of a volume node has extents that start at zero.
  /// This needs to be done for compatibility reasons, as many components assume the extent has a form of
  /// (0,dim[0],0,dim[1],0,dim[2]), which is not the case many times for segmentation merged labelmaps.
  void ShiftImageDataExtentToZeroStart();

  ///
  /// alternative method to propagate events generated in Display nodes
  void ProcessMRMLEvents ( vtkObject * /*caller*/,
                                   unsigned long /*event*/,
                                   void * /*callData*/ ) override;

  /// ImageDataModifiedEvent is generated when image data is changed
  enum
    {
    ImageDataModifiedEvent = 18001
    };

  ///
  /// Set/Get the ITK MetaDataDictionary
  void SetMetaDataDictionary( const itk::MetaDataDictionary& );
  const itk::MetaDataDictionary& GetMetaDataDictionary() const;

  bool CanApplyNonLinearTransforms()const override;

  void ApplyTransform(vtkAbstractTransform* transform) override;

  void ApplyTransformMatrix(vtkMatrix4x4* transformMatrix) override;

  virtual void ApplyNonLinearTransform(vtkAbstractTransform* transform);

  bool GetModifiedSinceRead() override;

  ///
  /// Get background voxel value of the image. It can be used for assigning
  /// intensity value to "empty" voxels when the image is transformed.
  /// It is computed as median value of the 8 corner voxels.
  virtual double GetImageBackgroundScalarComponentAsDouble(int component);

  /// Creates the most appropriate display node class for storing a sequence of these nodes.
  void CreateDefaultSequenceDisplayNodes() override;

  /// Returns true if the volume center is in the origin.
  bool IsCentered();

  /// Add a transform to the scene that puts the center of the volume in the origin.
  /// Returns true if the parent transform is changed.
  bool AddCenteringTransform();

  ///@{
  /// Measured quantity of voxel values, specified as a standard coded entry.
  /// Component argument specifies scalar component for volumes that contain multiple scalar components.
  /// For example: (DCM, 112031, "Attenuation Coefficient")
  /// See definitions at http://dicom.nema.org/medical/dicom/current/output/chtml/part16/sect_CID_7180.html
  void SetVoxelValueQuantity(vtkCodedEntry* quantity, int component=0);
  void SetVoxelValueQuantities(vtkCollection* quantities);
  void GetVoxelValueQuantities(vtkCollection* quantities);
  vtkCodedEntry* GetVoxelValueQuantity(int component=0);
  void RemoveAllVoxelValueQuantities();
  int GetNumberOfVoxelValueQuantities();
  ///@}

  /// Return standard voxel quantity preset: RGB.
  /// (DCM, 110834, "RGB R Component"), (DCM, 110835, "RGB G Component"), (DCM, 110836, "RGB B Component")
  /// \sa SetVoxelValueQuantityToRGB, IsVoxelValueQuantityRGB
  static void GetVoxelValueQuantityPresetRGBColor(vtkCollection* voxelQuantities);

  /// Set measured quantity of voxel values to RGB color.
  /// \sa GetVoxelValueQuantityPresetRGB
  void SetVoxelValueQuantityToRGBColor();

  /// Returns true if voxel values quantity RGB color.
  /// \sa GetVoxelValueQuantityPresetRGB
  bool IsVoxelValueQuantityRGBColor();

  /// Return standard voxel quantity preset: RGBA.
  /// (DCM, 110834, "RGB R Component"), (DCM, 110835, "RGB G Component"), (DCM, 110836, "RGB B Component"), , (SLR, 100000, "RGB A Component")
  /// \sa SetVoxelValueQuantityToRGBA, IsVoxelValueQuantityRGBA
  static void GetVoxelValueQuantityPresetRGBAColor(vtkCollection* voxelQuantities);

  /// Set measured quantity of voxel values to RGBA color.
/// \sa GetVoxelValueQuantityPresetRGBA
  void SetVoxelValueQuantityToRGBAColor();

  /// Returns true if voxel values quantity RGBA color.
  /// \sa GetVoxelValueQuantityPresetRGBA
  bool IsVoxelValueQuantityRGBAColor();

  /// Return standard voxel quantity preset: spatial displacement.
  /// (DCM, 110822, "Spatial Displacement X Component"), (DCM, 110823, "Spatial Displacement Y Component"), (DCM, 110824, "Spatial Displacement Z Component")
  /// \sa SetVoxelValueQuantityToSpatialDisplacement, IsVoxelValueQuantitySpatialDisplacement
  static void GetVoxelValueQuantityPresetSpatialDisplacement(vtkCollection* voxelQuantities);

  /// Set measured quantity of voxel values to spatial displacement.
  /// \sa GetVoxelValueQuantityPresetSpatialDisplacement, IsVoxelValueQuantitySpatialDisplacement
  void SetVoxelValueQuantityToSpatialDisplacement();

  /// Returns true if voxel values quantity is spatial displacement.
  /// \sa SetVoxelValueQuantityToSpatialDisplacement, IsVoxelValueQuantitySpatialDisplacement
  bool IsVoxelValueQuantitySpatialDisplacement();

  ///@{
  /// Measurement unit of voxel value quantity, specified as a standard coded entry.
  /// A single value is stored for each component. Plural (units) name is chosen to be consistent
  /// with nomenclature in the DICOM standard.
  /// Component argument specifies scalar component for volumes that contain multiple scalar components.
  /// For example: (UCUM, [hnsf'U], "Hounsfield unit")
  /// See definitions at http://dicom.nema.org/medical/dicom/current/output/chtml/part16/sect_CID_7180.html
  void SetVoxelValueUnits(vtkCodedEntry* units, int component=0);
  void SetVoxelValueUnits(vtkCollection* units);
  void GetVoxelValueUnits(vtkCollection* units);
  vtkCodedEntry* GetVoxelValueUnits(int component=0);
  void RemoveAllVoxelValueUnits();
  int GetNumberOfVoxelValueUnits();
  ///@}

    /// Helper function to convert coded entries to string
  static std::string GetCodedEntriesAsString(vtkCollection* codedEntries);

  /// Helper function to convert string to coded entries
  static void SetCodedEntriesFromString(vtkCollection* codedEntries, const std::string& str);

protected:
  vtkMRMLVolumeNode();
  ~vtkMRMLVolumeNode() override;
  vtkMRMLVolumeNode(const vtkMRMLVolumeNode&);
  void operator=(const vtkMRMLVolumeNode&);

  /// Set the image data pipeline to all the display nodes.
  void SetImageDataToDisplayNodes();
  void SetImageDataToDisplayNode(vtkMRMLVolumeDisplayNode* displayNode);

  /// Called when a display node is added/removed/modified. Propagate the polydata
  /// to the new display node.
  virtual void UpdateDisplayNodeImageData(vtkMRMLDisplayNode *dnode);

  ///
  /// Called when a node reference ID is added (list size increased).
  void OnNodeReferenceAdded(vtkMRMLNodeReference *reference) override;

  ///
  /// Called when a node reference ID is modified.
  void OnNodeReferenceModified(vtkMRMLNodeReference *reference) override;

  ///
  /// Return the bounds of the node transformed or not depending on
  /// the useTransform parameter and the rasToSlice transform
  /// If useVoxelCenter is set to false (default) then bounds of voxel sides are returned
  /// (otherwise then bounds of voxels centers are returned).
  virtual void GetBoundsInternal(double bounds[6], vtkMatrix4x4* rasToSlice, bool useTransform, bool useVoxelCenter = false);

  /// Returns the origin that would put the volume center in the origin.
  /// If useParentTransform is false then parent transform is ignored.
  void GetCenterPositionRAS(double* centerPositionRAS, bool useParentTransform=true);

  /// Helper function to set value in collection.
  /// Setting nullptr deletes the item (if it is at the end of the collection).
  /// Nullptr elements added as needed if there are not enough entires.
  /// Returns true if the entry is modified.
  static bool SetCodedEntryInCollection(vtkCollection* codedEntries, vtkCodedEntry* codedEntry, int index);

  /// Helper function to set all values in a collection.
  /// Returns true if the entry is modified.
  void SetCodedEntriesFromCollection(vtkCollection* codedEntries, vtkCollection* newEntries);

  /// Helper function to compare all values in a coded entry collection.
  /// Return true if they are equal.
  static bool IsMatchingCodedEntryCollections(vtkCollection* codedEntries1, vtkCollection* codedEntries2);

  /// these are unit length direction cosines
  double IJKToRASDirections[3][3];

  /// these are mappings to mm space
  double Spacing[3];
  double Origin[3];

  vtkAlgorithmOutput* ImageDataConnection;
  vtkEventForwarderCommand* DataEventForwarder;

  /// Collection of vtkCodedEntry, each describing quantity stored in voxel values for a scalar component.
  vtkCollection* VoxelValueQuantities;

  /// Collection of vtkCodedEntry, each describing units stored in voxel values for a scalar component.
  vtkCollection* VoxelValueUnits;

  itk::MetaDataDictionary Dictionary;
};

#endif
