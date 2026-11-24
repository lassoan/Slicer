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

#ifndef __vtkMRMLLinearTransformPlugin_h
#define __vtkMRMLLinearTransformPlugin_h

#include "vtkMRMLTransformPlugin.h"

#ifndef __VTK_WRAP__
// ITK includes
# include <itkTransformBase.h>
#endif

class vtkMatrix4x4;

/// \brief Transform plugin of linear transforms (vtkHomogeneousTransform and its subclasses, for example vtkTransform)
///
/// It is one of the plugins of the transform classes that MRML provides, which vtkMRMLTransformPluginFactory has
/// without registering them. It describes and copies these transforms, converts them to and from the ITK transforms
/// that store them in transform files, and tells the class of their node after a split
/// (vtkMRMLLinearTransformNode for linear transforms).
///
/// A plugin of a subclass of these transform classes (for example, of an extension) only has to do what it does
/// differently: if it returns a higher confidence in CanHandleTransform() then it is asked first, and this plugin does
/// what that one does not.
class VTK_MRML_EXPORT vtkMRMLLinearTransformPlugin : public vtkMRMLTransformPlugin
{
public:
  static vtkMRMLLinearTransformPlugin* New();
  vtkTypeMacro(vtkMRMLLinearTransformPlugin, vtkMRMLTransformPlugin);

  const char* GetTransformClassName() override;

  /// It is not known from the class name if an ITK transform is of this type: reading it is tried, after the plugins
  /// that know the class
  double CanReadITKTransformClass(const std::string& itkClassName) override;

  std::string GetTransformInfo(vtkAbstractTransform* transform) override;

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

  /// Set a matrix (in RAS coordinate system) from a linear ITK transform (in LPS coordinate system).
  /// Returns false if the ITK transform is not linear.
  static bool SetVTKMatrixFromITKLinearTransform(vtkObject* loggerObject,
                                                 vtkMatrix4x4* transformVtk_RAS,
                                                 itk::TransformBaseTemplate<double>* transformItk_LPS,
                                                 double center_LocalRAS[3] = nullptr);

  /// Create a linear ITK transform (in LPS coordinate system) from a matrix (in RAS coordinate system).
  static bool SetITKLinearTransformFromVTKMatrix(vtkObject* loggerObject,
                                                 itk::SmartPointer<itk::Object>& transformItk_LPS,
                                                 vtkMatrix4x4* transformVtk_RAS,
                                                 double center_LocalRAS[3] = nullptr);
#endif

protected:
  vtkMRMLLinearTransformPlugin();
  ~vtkMRMLLinearTransformPlugin() override;

private:
  vtkMRMLLinearTransformPlugin(const vtkMRMLLinearTransformPlugin&) = delete;
  void operator=(const vtkMRMLLinearTransformPlugin&) = delete;
};

#endif
