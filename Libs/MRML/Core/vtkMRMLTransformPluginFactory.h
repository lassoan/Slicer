/*=========================================================================

  Program:   3D Slicer

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

#ifndef __vtkMRMLTransformPluginFactory_h
#define __vtkMRMLTransformPluginFactory_h

// MRML includes
#include "vtkMRML.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <string>
#include <vector>

class vtkAbstractTransform;
class vtkMRMLTransformPlugin;

/// \brief Registry of the plugins of transform classes
///
/// A module (for example, in an extension) that adds a transform class registers a plugin for it here
/// (see vtkMRMLTransformPlugin), so that transform nodes and transform storage nodes can do with its transforms all
/// that they do with the transform classes that MRML provides. The factory also has the plugins of those classes
/// (vtkMRMLLinearTransformPlugin, vtkMRMLGridTransformPlugin, vtkMRMLBSplineTransformPlugin and
/// vtkMRMLThinPlateSplineTransformPlugin), which do not need to be registered.
///
/// It is a singleton, which exists as long as the application runs.
class VTK_MRML_EXPORT vtkMRMLTransformPluginFactory : public vtkObject
{
public:
  vtkTypeMacro(vtkMRMLTransformPluginFactory, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Get the singleton instance.
  static vtkMRMLTransformPluginFactory* GetInstance();

  /// Register a plugin. It is kept until it is unregistered.
  void RegisterPlugin(vtkMRMLTransformPlugin* plugin);

  /// Unregister a plugin.
  void UnregisterPlugin(vtkMRMLTransformPlugin* plugin);

  /// Number of the registered plugins (those of the transform classes that MRML provides are not among them).
  int GetNumberOfRegisteredPlugins();

  /// Get a registered plugin.
  vtkMRMLTransformPlugin* GetNthRegisteredPlugin(int index);

  /// Get the plugin that is the most confident that it can handle the transform
  /// (see vtkMRMLTransformPlugin::CanHandleTransform()). Returns nullptr if there is none.
  vtkMRMLTransformPlugin* GetPlugin(vtkAbstractTransform* transform);

#ifndef __VTK_WRAP__
  /// Get the plugins that can handle the transform, in the order they are asked: in decreasing order of confidence
  /// (see vtkMRMLTransformPlugin::CanHandleTransform()). Plugins of the same confidence are in a fixed order: the
  /// registered ones (the one that was registered last is the first), then those of the transform classes that MRML
  /// provides.
  void GetPlugins(vtkAbstractTransform* transform, std::vector<vtkMRMLTransformPlugin*>& plugins);

  /// Get the plugins that can read ITK transforms of the class itkClassName, in the order they are asked:
  /// in decreasing order of confidence (see vtkMRMLTransformPlugin::CanReadITKTransformClass()).
  void GetPluginsForITKTransformClass(const std::string& itkClassName, std::vector<vtkMRMLTransformPlugin*>& plugins);
#endif

protected:
  static vtkMRMLTransformPluginFactory* New();
  vtkMRMLTransformPluginFactory();
  ~vtkMRMLTransformPluginFactory() override;

#ifndef __VTK_WRAP__
  /// Create the plugins of the transform classes that MRML provides (vtkMRMLLinearTransformPlugin,
  /// vtkMRMLGridTransformPlugin, vtkMRMLBSplineTransformPlugin, vtkMRMLThinPlateSplineTransformPlugin).
  static void CreateBuiltInPlugins(std::vector<vtkSmartPointer<vtkMRMLTransformPlugin>>& plugins);

  /// The registered plugins, the one that was registered last is the first
  std::vector<vtkSmartPointer<vtkMRMLTransformPlugin>> RegisteredPlugins;
  /// The plugins of the transform classes that MRML provides
  std::vector<vtkSmartPointer<vtkMRMLTransformPlugin>> BuiltInPlugins;
#endif

private:
  vtkMRMLTransformPluginFactory(const vtkMRMLTransformPluginFactory&) = delete;
  void operator=(const vtkMRMLTransformPluginFactory&) = delete;
};

#endif
