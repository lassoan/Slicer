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

#include "vtkMRMLTransformPluginFactory.h"

#include "vtkMRMLBSplineTransformPlugin.h"
#include "vtkMRMLGridTransformPlugin.h"
#include "vtkMRMLLinearTransformPlugin.h"
#include "vtkMRMLThinPlateSplineTransformPlugin.h"
#include "vtkMRMLTransformPlugin.h"

// VTK includes
#include <vtkAbstractTransform.h>
#include <vtkObjectFactory.h>

// STD includes
#include <algorithm>
#include <utility>

//----------------------------------------------------------------------------
namespace
{
typedef std::vector<vtkSmartPointer<vtkMRMLTransformPlugin>> PluginsType;

/// Get the plugins in decreasing order of confidence. Plugins of the same confidence keep their order.
void SortByConfidence(std::vector<std::pair<double, vtkMRMLTransformPlugin*>>& pluginsWithConfidence, std::vector<vtkMRMLTransformPlugin*>& plugins)
{
  std::stable_sort(pluginsWithConfidence.begin(),
                   pluginsWithConfidence.end(),
                   [](const std::pair<double, vtkMRMLTransformPlugin*>& a, const std::pair<double, vtkMRMLTransformPlugin*>& b) { return a.first > b.first; });
  for (const std::pair<double, vtkMRMLTransformPlugin*>& pluginWithConfidence : pluginsWithConfidence)
  {
    plugins.push_back(pluginWithConfidence.second);
  }
}
} // namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLTransformPluginFactory);

//----------------------------------------------------------------------------
vtkMRMLTransformPluginFactory* vtkMRMLTransformPluginFactory::GetInstance()
{
  // Created when it is first needed, so that plugins can be registered during static initialization, too
  static vtkSmartPointer<vtkMRMLTransformPluginFactory> instance = vtkSmartPointer<vtkMRMLTransformPluginFactory>::Take(vtkMRMLTransformPluginFactory::New());
  return instance;
}

//----------------------------------------------------------------------------
vtkMRMLTransformPluginFactory::vtkMRMLTransformPluginFactory()
{
  vtkMRMLTransformPluginFactory::CreateBuiltInPlugins(this->BuiltInPlugins);
}

//----------------------------------------------------------------------------
vtkMRMLTransformPluginFactory::~vtkMRMLTransformPluginFactory() = default;

//----------------------------------------------------------------------------
void vtkMRMLTransformPluginFactory::CreateBuiltInPlugins(std::vector<vtkSmartPointer<vtkMRMLTransformPlugin>>& plugins)
{
  // In the order that reading an ITK transform is tried
  plugins.push_back(vtkSmartPointer<vtkMRMLLinearTransformPlugin>::New());
  plugins.push_back(vtkSmartPointer<vtkMRMLGridTransformPlugin>::New());
  plugins.push_back(vtkSmartPointer<vtkMRMLBSplineTransformPlugin>::New());
  plugins.push_back(vtkSmartPointer<vtkMRMLThinPlateSplineTransformPlugin>::New());
}

//----------------------------------------------------------------------------
void vtkMRMLTransformPluginFactory::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Registered plugins:\n";
  for (vtkMRMLTransformPlugin* plugin : this->RegisteredPlugins)
  {
    os << indent.GetNextIndent() << plugin->GetClassName() << " (" << (plugin->GetTransformClassName() ? plugin->GetTransformClassName() : "") << ")\n";
  }
}

//----------------------------------------------------------------------------
void vtkMRMLTransformPluginFactory::RegisterPlugin(vtkMRMLTransformPlugin* plugin)
{
  if (!plugin)
  {
    vtkErrorMacro("RegisterPlugin failed: invalid plugin");
    return;
  }
  if (std::find(this->RegisteredPlugins.begin(), this->RegisteredPlugins.end(), plugin) != this->RegisteredPlugins.end())
  {
    return;
  }
  this->RegisteredPlugins.insert(this->RegisteredPlugins.begin(), plugin);
}

//----------------------------------------------------------------------------
void vtkMRMLTransformPluginFactory::UnregisterPlugin(vtkMRMLTransformPlugin* plugin)
{
  this->RegisteredPlugins.erase(std::remove(this->RegisteredPlugins.begin(), this->RegisteredPlugins.end(), plugin), this->RegisteredPlugins.end());
}

//----------------------------------------------------------------------------
int vtkMRMLTransformPluginFactory::GetNumberOfRegisteredPlugins()
{
  return static_cast<int>(this->RegisteredPlugins.size());
}

//----------------------------------------------------------------------------
vtkMRMLTransformPlugin* vtkMRMLTransformPluginFactory::GetNthRegisteredPlugin(int index)
{
  if (index < 0 || index >= static_cast<int>(this->RegisteredPlugins.size()))
  {
    return nullptr;
  }
  return this->RegisteredPlugins[index];
}

//----------------------------------------------------------------------------
void vtkMRMLTransformPluginFactory::GetPlugins(vtkAbstractTransform* transform, std::vector<vtkMRMLTransformPlugin*>& plugins)
{
  plugins.clear();
  if (!transform)
  {
    return;
  }
  std::vector<std::pair<double, vtkMRMLTransformPlugin*>> pluginsWithConfidence;
  const PluginsType* pluginLists[] = { &this->RegisteredPlugins, &this->BuiltInPlugins };
  for (const PluginsType* pluginList : pluginLists)
  {
    for (vtkMRMLTransformPlugin* plugin : *pluginList)
    {
      double confidence = plugin->CanHandleTransform(transform);
      if (confidence > 0.0)
      {
        pluginsWithConfidence.push_back(std::make_pair(confidence, plugin));
      }
    }
  }
  SortByConfidence(pluginsWithConfidence, plugins);
}

//----------------------------------------------------------------------------
vtkMRMLTransformPlugin* vtkMRMLTransformPluginFactory::GetPlugin(vtkAbstractTransform* transform)
{
  std::vector<vtkMRMLTransformPlugin*> plugins;
  this->GetPlugins(transform, plugins);
  return plugins.empty() ? nullptr : plugins.front();
}

//----------------------------------------------------------------------------
void vtkMRMLTransformPluginFactory::GetPluginsForITKTransformClass(const std::string& itkClassName, std::vector<vtkMRMLTransformPlugin*>& plugins)
{
  plugins.clear();
  std::vector<std::pair<double, vtkMRMLTransformPlugin*>> pluginsWithConfidence;
  const PluginsType* pluginLists[] = { &this->RegisteredPlugins, &this->BuiltInPlugins };
  for (const PluginsType* pluginList : pluginLists)
  {
    for (vtkMRMLTransformPlugin* plugin : *pluginList)
    {
      double confidence = plugin->CanReadITKTransformClass(itkClassName);
      if (confidence > 0.0)
      {
        pluginsWithConfidence.push_back(std::make_pair(confidence, plugin));
      }
    }
  }
  SortByConfidence(pluginsWithConfidence, plugins);
}
