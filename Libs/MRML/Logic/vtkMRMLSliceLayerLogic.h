/*=auto=========================================================================

  Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Program:   3D Slicer
  Module:    $RCSfile: vtkMRMLSliceLayerLogic.h,v $
  Date:      $Date$
  Version:   $Revision$

=========================================================================auto=*/

///  vtkMRMLSliceLayerLogic - slicer logic class for slice manipulation
///
/// This class manages the logic associated with reslicing of volumes
/// (but not the GUI).  Features of the class include:
//
/// - Reslicing
/// -- uses the vtkImageData and IJKToRAS transform from a vtkMRMLVolumeNode
/// -- disp
/// -- uses a current slice view specification (typically set by vtkMRMLSliceLogic)
/// - Outputs
/// -- Colors vtkImageData for the given slice
/// -- image is mapped through current window/level and lookup table
//
/// This class can also be used for resampling volumes for further computation.
//

#ifndef __vtkMRMLSliceLayerLogic_h
#define __vtkMRMLSliceLayerLogic_h

// MRMLLogic includes
#include "vtkMRMLAbstractLogic.h"

// MRML includes
#include "vtkMRMLVolumeNode.h"
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkMRMLVectorVolumeNode.h"
#include "vtkMRMLDiffusionWeightedVolumeNode.h"
#include "vtkMRMLDiffusionTensorVolumeNode.h"

// VTK includes
#include <vtkImageLogic.h>
#include <vtkImageExtractComponents.h>
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>
#include <vtkVersion.h>
#include <vtkWeakPointer.h>

class vtkAssignAttribute;
class vtkImageReslice;
class vtkGeneralTransform;

// STL includes
// #include <cstdlib>

class vtkCallbackCommand;
class vtkImageLabelOutline;
class vtkMRMLScalarVolumeNode;
class vtkMRMLVoxelDataProvider;
class vtkTransform;

class VTK_MRML_LOGIC_EXPORT vtkMRMLSliceLayerLogic : public vtkMRMLAbstractLogic
{
public:
  /// The Usual vtk class functions
  static vtkMRMLSliceLayerLogic* New();
  vtkTypeMacro(vtkMRMLSliceLayerLogic, vtkMRMLAbstractLogic);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///
  /// The volume node to operate on
  vtkGetObjectMacro(VolumeNode, vtkMRMLVolumeNode);
  void SetVolumeNode(vtkMRMLVolumeNode* VolumeNode);

  ///
  /// The volume display node has the render properties of the volume
  /// - this node is set implicitly when the volume is set
  ///   and it is observed by this logic
  vtkGetObjectMacro(VolumeDisplayNode, vtkMRMLVolumeDisplayNode);
  vtkGetObjectMacro(VolumeDisplayNodeUVW, vtkMRMLVolumeDisplayNode);

  ///
  /// The slice node that defines the view
  vtkGetObjectMacro(SliceNode, vtkMRMLSliceNode);
  void SetSliceNode(vtkMRMLSliceNode* SliceNode);

  ///
  /// The image reslice or slice being used
  vtkGetObjectMacro(Reslice, vtkImageReslice);
  vtkGetObjectMacro(ResliceUVW, vtkImageReslice);

  ///
  /// Select if this is a label layer or not (it currently determines if we use
  /// the label outline filter)
  vtkGetMacro(IsLabelLayer, int);
  vtkSetMacro(IsLabelLayer, int);
  vtkBooleanMacro(IsLabelLayer, int);

  ///
  /// The filter that turns the label map into an outline
  vtkGetObjectMacro(LabelOutline, vtkImageLabelOutline);

  ///
  /// Get the output of the pipeline for this layer
  vtkImageData* GetImageData();
  vtkAlgorithmOutput* GetImageDataConnection();

  ///
  /// Get the output of the texture UVW pipeline for this layer
  vtkImageData* GetImageDataUVW();
  vtkAlgorithmOutput* GetImageDataConnectionUVW();

  void UpdateImageDisplay();

  ///
  /// set the Reslice transforms to reflect the current state
  /// of the VolumeNode and the SliceNode
  void UpdateTransforms();

  void UpdateGlyphs();

  ///
  /// Check that we are observing the correct display node
  /// (correct means the same one that the volume node is referencing)
  void UpdateNodeReferences();

  ///
  /// The current reslice transform XYToIJK
  vtkGetObjectMacro(XYToIJKTransform, vtkGeneralTransform);

  ///
  /// Get/set interpolation mode used in image reslice (when interpolation is enabled).
  /// By default it uses VTK_RESLICE_LINEAR and can be set to VTK_RESLICE_CUBIC for higher quality interpolation.
  vtkGetMacro(InterpolationMode, int);
  vtkSetMacro(InterpolationMode, int);

  /// Resolution level of the volume that is currently displayed in this
  /// layer (-1 if variable-resolution display is inactive, i.e. the volume
  /// has a single resolution level). During progressive refinement this may
  /// temporarily be a coarser level than the zoom factor calls for, until
  /// the background retrieval of the target level completes.
  vtkGetMacro(DisplayedResolutionLevel, int);

  /// Extent (in the displayed level's IJK indices) of the region that is
  /// currently used as the reslice input.
  vtkGetVector6Macro(DisplayedRegionExtent, int);

  //@{
  /// Resolution level and region that this layer wants to display at the
  /// current zoom (-1: variable resolution inactive). May be finer than the
  /// displayed level while a background retrieval is in progress. Views use
  /// this together with
  /// vtkMRMLVoxelDataProvider::GetRegionRequestProgress() to show a loading
  /// indicator only for the region they are displaying.
  vtkGetMacro(TargetResolutionLevel, int);
  vtkGetVector6Macro(TargetRegionExtent, int);
  //@}

protected:
  vtkMRMLSliceLayerLogic();
  ~vtkMRMLSliceLayerLogic() override;
  vtkMRMLSliceLayerLogic(const vtkMRMLSliceLayerLogic&);
  void operator=(const vtkMRMLSliceLayerLogic&);

  // Initialize listening to MRML events
  void SetMRMLSceneInternal(vtkMRMLScene* newScene) override;

  ///
  /// provide the virtual method that updates this Logic based
  /// on mrml changes
  void ProcessMRMLSceneEvents(vtkObject* caller, unsigned long event, void* callData) override;
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;
  void UpdateLogic();
  void OnMRMLNodeModified(vtkMRMLNode* node) override;
  vtkAlgorithmOutput* GetSliceImageDataConnection();
  vtkAlgorithmOutput* GetSliceImageDataConnectionUVW();

  // Copy VolumeDisplayNodeObserved into VolumeDisplayNode
  void UpdateVolumeDisplayNode();

  ///
  /// the MRML Nodes that define this Logic's parameters
  vtkMRMLVolumeNode* VolumeNode;
  vtkMRMLVolumeDisplayNode* VolumeDisplayNode;
  vtkMRMLVolumeDisplayNode* VolumeDisplayNodeUVW;
  vtkMRMLVolumeDisplayNode* VolumeDisplayNodeObserved;
  vtkMRMLSliceNode* SliceNode;

  ///
  /// the VTK class instances that implement this Logic's operations
  vtkImageReslice* Reslice;
  vtkImageReslice* ResliceUVW;
  vtkImageLabelOutline* LabelOutline;
  vtkImageLabelOutline* LabelOutlineUVW;

  vtkAssignAttribute* AssignAttributeTensorsToScalars;
  vtkAssignAttribute* AssignAttributeScalarsToTensors;
  vtkAssignAttribute* AssignAttributeScalarsToTensorsUVW;

  /// TODO: make this a vtkAbstractTransform for non-linear
  vtkGeneralTransform* XYToIJKTransform;
  vtkGeneralTransform* UVWToIJKTransform;

  int IsLabelLayer;

  int UpdatingTransforms;

  int InterpolationMode;

  //@{
  /// Variable-resolution display of multi-resolution volumes (volumes whose
  /// voxel data provider offers more than one resolution level):
  /// the reslice input is the region of the volume that is displayed in the
  /// slice view, at the resolution level matching the current zoom factor.
  /// If the target level is not available yet, it is requested in the
  /// background and the best already-available coarser level is displayed in
  /// the meantime (progressive refinement).

  /// Choose resolution level and region for the current view and update the
  /// reslice input accordingly.
  void UpdateVariableResolutionInput(vtkMRMLScalarVolumeNode* scalarVolumeNode, vtkMRMLVoxelDataProvider* provider);

  /// Compute the displayed region of the volume (padded, clamped) at the
  /// requested resolution level. Returns false if it cannot be determined
  /// (the full level extent should be used then).
  bool ComputeDisplayedRegion(vtkMRMLScalarVolumeNode* scalarVolumeNode, vtkMRMLVoxelDataProvider* provider, int resolutionLevel, int regionExtent[6]);

  /// Compute the view (XY) to reference-level IJK matrix. Returns false for
  /// non-linearly transformed volumes (region restriction is not supported).
  bool GetXYToReferenceIJKMatrix(vtkMRMLScalarVolumeNode* scalarVolumeNode, vtkMatrix4x4* xyToRefIJK);

  /// Keep observing the volume's provider for background request completion.
  void UpdateVoxelDataProviderObserver(vtkMRMLVoxelDataProvider* provider);

  static void OnVoxelDataProviderModified(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

  /// Maps reference-level IJK coordinates to the currently displayed level's
  /// IJK coordinates (identity for single-resolution volumes).
  vtkSmartPointer<vtkMatrix4x4> ResolutionScaleMatrix;
  vtkSmartPointer<vtkCallbackCommand> VoxelDataProviderObserver;
  vtkWeakPointer<vtkMRMLVoxelDataProvider> ObservedVoxelDataProvider;
  /// Currently displayed resolution level (-1: variable resolution inactive)
  int DisplayedResolutionLevel{ -1 };
  int DisplayedRegionExtent[6]{ 0, -1, 0, -1, 0, -1 };
  /// Level/region that the layer wants to display at the current zoom
  int TargetResolutionLevel{ -1 };
  int TargetRegionExtent[6]{ 0, -1, 0, -1, 0, -1 };
  //@}
};

#endif
