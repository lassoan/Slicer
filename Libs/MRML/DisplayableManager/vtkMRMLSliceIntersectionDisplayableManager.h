/*==============================================================================

  Program: 3D Slicer

  Portions (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All Rights Reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

==============================================================================*/

#ifndef __vtkMRMLSliceIntersectionDisplayableManager_h
#define __vtkMRMLSliceIntersectionDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAbstractSliceViewDisplayableManager.h"
#include "vtkMRMLDisplayableManagerExport.h"

/// \brief Displays interaction handles in 2D views to control slice intersection
///
/// Interaction handles to enable users to navigate slices changing position and orientation
///
class VTK_MRML_DISPLAYABLEMANAGER_EXPORT vtkMRMLSliceIntersectionDisplayableManager
  : public vtkMRMLAbstractSliceViewDisplayableManager
{

public:

  static vtkMRMLSliceIntersectionDisplayableManager* New();
  vtkTypeMacro(vtkMRMLSliceIntersectionDisplayableManager, vtkMRMLAbstractSliceViewDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent) override;

protected:

  vtkMRMLSliceIntersectionDisplayableManager();
  ~vtkMRMLSliceIntersectionDisplayableManager() override;

  void OnMRMLSceneNodeAdded(vtkMRMLNode* node) override;
  void OnMRMLSceneNodeRemoved(vtkMRMLNode* node) override;
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;

  /// Initialize the displayable manager based on its associated
  /// vtkMRMLSliceNode
  void Create() override;

  /// Called each time the view node is modified.
  /// Internally update the renderer from the view node.
  /// \sa UpdateFromMRMLViewNode()
  void OnMRMLDisplayableNodeModifiedEvent(vtkObject* caller) override;

  bool CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& closestDistance2) override;
  bool ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData) override;

private:

  vtkMRMLSliceIntersectionDisplayableManager(const vtkMRMLSliceIntersectionDisplayableManager&) = delete;
  void operator=(const vtkMRMLSliceIntersectionDisplayableManager&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
  friend class vtkInternal;
};

#endif
