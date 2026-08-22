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

#ifndef __vtkMRMLVectorFieldDisplayableManager_h
#define __vtkMRMLVectorFieldDisplayableManager_h

// MRMLDisplayableManager includes
#include "vtkMRMLAbstractThreeDViewDisplayableManager.h"
#include "vtkMRMLDisplayableManagerExport.h"

class vtkMRMLDisplayableNode;

/// \brief Displays vector fields as glyphs in 3D views.
///
/// Renders the vector fields described by vtkMRMLVectorFieldDisplayNode nodes, whatever
/// displayable node stores the field: the point data of a model, a vector volume, or a
/// transform. The display node provides the point set through
/// vtkMRMLVectorFieldDisplayNode::GetFieldConnection(), which this manager connects to a
/// vtkGlyph3DMapper; the field is therefore only sampled when the renderer pulls the data.
///
/// \sa vtkMRMLVectorFieldSliceDisplayableManager
class VTK_MRML_DISPLAYABLEMANAGER_EXPORT vtkMRMLVectorFieldDisplayableManager : public vtkMRMLAbstractThreeDViewDisplayableManager
{
public:
  static vtkMRMLVectorFieldDisplayableManager* New();
  vtkTypeMacro(vtkMRMLVectorFieldDisplayableManager, vtkMRMLAbstractThreeDViewDisplayableManager);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  // DisplayableNode handling customizations
  void AddDisplayableNode(vtkMRMLDisplayableNode* displayableNode);
  void RemoveDisplayableNode(vtkMRMLDisplayableNode* displayableNode);

protected:
  vtkMRMLVectorFieldDisplayableManager();
  ~vtkMRMLVectorFieldDisplayableManager() override;

  void UnobserveMRMLScene() override;
  void OnMRMLSceneNodeAdded(vtkMRMLNode* node) override;
  void OnMRMLSceneNodeRemoved(vtkMRMLNode* node) override;
  void ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData) override;

  void UpdateFromMRML() override;
  void OnMRMLSceneStartClose() override;
  void OnMRMLSceneEndClose() override;
  void OnMRMLSceneEndBatchProcess() override;

  /// Initialize the displayable manager
  void Create() override;

  int AddingDisplayableNode;

private:
  vtkMRMLVectorFieldDisplayableManager(const vtkMRMLVectorFieldDisplayableManager&) = delete;
  void operator=(const vtkMRMLVectorFieldDisplayableManager&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
  friend class vtkInternal;
};

#endif
