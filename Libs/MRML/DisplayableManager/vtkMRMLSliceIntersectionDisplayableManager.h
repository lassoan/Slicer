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

class vtkMRMLCrosshairNode;
class vtkMRMLScene;
class vtkMRMLSliceIntersectionInteractionWidget;

/// \brief Displays interaction handles in 2D views to control slice intersection
///
/// Interaction handles to enable users to navigate slices changing position and orientation
///
class VTK_MRML_DISPLAYABLEMANAGER_EXPORT vtkMRMLSliceIntersectionDisplayableManager :
  public vtkMRMLAbstractSliceViewDisplayableManager
{
  public:
    static vtkMRMLSliceIntersectionDisplayableManager* New();
    vtkTypeMacro(vtkMRMLSliceIntersectionDisplayableManager,
      vtkMRMLAbstractSliceViewDisplayableManager);
    void PrintSelf(ostream& os, vtkIndent indent) override;

    bool CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& closestDistance2) override;
    bool ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData) override;

    void SetActionsEnabled(int actions);
    int GetActionsEnabled();

    vtkMRMLSliceIntersectionInteractionWidget* GetSliceIntersectionInteractionWidget();

  protected:
    void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;

    void UnobserveMRMLScene() override;
    void ObserveMRMLScene() override;
    void UpdateFromMRMLScene() override;
    void OnMRMLNodeModified(vtkMRMLNode* node) override;

  protected:
    vtkMRMLSliceIntersectionDisplayableManager();
    ~vtkMRMLSliceIntersectionDisplayableManager() override;

  private:
    vtkMRMLSliceIntersectionDisplayableManager(const vtkMRMLSliceIntersectionDisplayableManager&) = delete;
    void operator=(const vtkMRMLSliceIntersectionDisplayableManager&) = delete;

    class vtkInternal;
    vtkInternal* Internal;
};

#endif
