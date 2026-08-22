/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#ifndef __vtkMRMLVectorFieldSliceDisplayableManager_h
#define __vtkMRMLVectorFieldSliceDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAbstractSliceViewDisplayableManager.h"
#include "vtkMRMLDisplayableManagerExport.h"

class vtkMRMLDisplayableNode;

/// \brief Displays glyphs of a model node's point data arrays in slice views.
///
/// Renders the glyphs described by vtkMRMLVectorFieldDisplayNode nodes of model nodes
/// in slice (2D) views, next to the 3D view rendering done by
/// vtkMRMLModelDisplayableManager.
///
/// Only mesh points that are within a slab around the slice plane
/// (vtkMRMLVectorFieldDisplayNode::SliceSlabThicknessMm) are shown, and the glyphs
/// are drawn in the slice view's plane.
class VTK_MRML_DISPLAYABLEMANAGER_EXPORT vtkMRMLVectorFieldSliceDisplayableManager : public vtkMRMLAbstractSliceViewDisplayableManager
{

public:
  static vtkMRMLVectorFieldSliceDisplayableManager* New();
  vtkTypeMacro(vtkMRMLVectorFieldSliceDisplayableManager, vtkMRMLAbstractSliceViewDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  // DisplayableNode handling customizations
  void AddDisplayableNode(vtkMRMLDisplayableNode* displayableNode);
  void RemoveDisplayableNode(vtkMRMLDisplayableNode* displayableNode);

protected:
  vtkMRMLVectorFieldSliceDisplayableManager();
  ~vtkMRMLVectorFieldSliceDisplayableManager() override;

  void UnobserveMRMLScene() override;
  void OnMRMLSceneNodeAdded(vtkMRMLNode* node) override;
  void OnMRMLSceneNodeRemoved(vtkMRMLNode* node) override;
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;

  void UpdateFromMRML() override;
  void OnMRMLSceneStartClose() override;
  void OnMRMLSceneEndClose() override;
  void OnMRMLSceneEndBatchProcess() override;

  /// Initialize the displayable manager based on its associated vtkMRMLSliceNode
  void Create() override;

  int AddingDisplayableNode;

private:
  vtkMRMLVectorFieldSliceDisplayableManager(const vtkMRMLVectorFieldSliceDisplayableManager&) = delete;
  void operator=(const vtkMRMLVectorFieldSliceDisplayableManager&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
  friend class vtkInternal;
};

#endif
