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

#ifndef __vtkMRMLThinPlateSplineTransformPlugin_h
#define __vtkMRMLThinPlateSplineTransformPlugin_h

#include "vtkMRMLTransformPlugin.h"

/// \brief Transform plugin of thin-plate spline transforms (vtkThinPlateSplineTransform)
///
/// It is one of the plugins of the transform classes that MRML provides, which vtkMRMLTransformPluginFactory has
/// without registering them. It describes and copies these transforms, converts them to and from the ITK transforms
/// that store them in transform files.
///
/// A plugin of a subclass of these transform classes (for example, of an extension) only has to do what it does
/// differently: if it returns a higher confidence in CanHandleTransform() then it is asked first, and this plugin does
/// what that one does not.
class VTK_MRML_EXPORT vtkMRMLThinPlateSplineTransformPlugin : public vtkMRMLTransformPlugin
{
public:
  static vtkMRMLThinPlateSplineTransformPlugin* New();
  vtkTypeMacro(vtkMRMLThinPlateSplineTransformPlugin, vtkMRMLTransformPlugin);

  const char* GetTransformClassName() override;

  /// It is not known from the class name if an ITK transform is of this type: reading it is tried, after the plugins
  /// that know the class
  double CanReadITKTransformClass(const std::string& itkClassName) override;

  std::string GetTransformInfo(vtkAbstractTransform* transform) override;

  bool DeepCopyTransform(vtkAbstractTransform* dst, vtkAbstractTransform* src) override;

#ifndef __VTK_WRAP__
  using vtkMRMLTransformPlugin::CreateITKTransformFromVTK;
  itk::Object::Pointer CreateITKTransformFromVTK(vtkObject* loggerObject,
                                                 vtkAbstractTransform* transformVtk,
                                                 itk::Object::Pointer& secondaryTransformItk,
                                                 int preferITKv3CompatibleTransforms,
                                                 bool initialize,
                                                 double center_LocalRAS[3]) override;

  using vtkMRMLTransformPlugin::CreateVTKTransformFromITK;
  vtkAbstractTransform* CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItkObject, double center_LocalRAS[3]) override;
#endif

protected:
  vtkMRMLThinPlateSplineTransformPlugin();
  ~vtkMRMLThinPlateSplineTransformPlugin() override;

private:
  vtkMRMLThinPlateSplineTransformPlugin(const vtkMRMLThinPlateSplineTransformPlugin&) = delete;
  void operator=(const vtkMRMLThinPlateSplineTransformPlugin&) = delete;
};

#endif
