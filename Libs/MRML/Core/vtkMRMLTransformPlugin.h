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

#ifndef __vtkMRMLTransformPlugin_h
#define __vtkMRMLTransformPlugin_h

// MRML includes
#include "vtkMRML.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// ITK includes
#include <itkMatrix.h>
#include <itkObject.h>
#include <itkSmartPointer.h>
#include <vnl/algo/vnl_determinant.h>

// STD includes
#include <cmath>
#include <string>
#include <vector>

class vtkAbstractTransform;

/// \brief Makes transform nodes support a transform class that MRML does not know
///
/// A transform node can hold a transform of any vtkAbstractTransform subclass, alone or as a component of a composite
/// transform. A module (for example, in an extension) that adds a transform class registers a plugin of that class,
/// which tells transform nodes and transform storage nodes what they cannot know about the class, so that they can do
/// with its transforms all that they do with the transform classes that MRML provides:
/// - how the transform is written to and read from a transform file (as an ITK transform),
/// - the description of the transform, how it is copied, and the class of the node that holds it after a split.
///
/// Plugins are registered in vtkMRMLTransformPluginFactory.
///
/// A plugin tells how confident it is that it can handle a transform (CanHandleTransform()), a value between 0.0 and
/// 1.0. Several plugins may be able to handle a transform (for example, the plugin of its class and that of a
/// superclass): they are asked in decreasing order of confidence, until one of them does what is asked. The order does
/// not depend on the order that the plugins were registered in, except between plugins of the same confidence.
///
/// The transform classes that MRML provides (linear, B-spline, grid and thin-plate spline transforms) have plugins
/// too, with the default confidence (0.5). A plugin can be registered for a subclass of these classes, with a higher
/// confidence, and leave to them what it does not need to do differently.
class VTK_MRML_EXPORT vtkMRMLTransformPlugin : public vtkObject
{
public:
  vtkTypeMacro(vtkMRMLTransformPlugin, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Name of the VTK transform class that this plugin is for (for example, "vtkMyTransform").
  virtual const char* GetTransformClassName() = 0;

  /// Confidence (0.0-1.0) that this plugin can handle the transform: 0.0 if it cannot, 1.0 if it is certainly the
  /// plugin to use. The plugins of a transform are asked in decreasing order of confidence.
  /// By default it is GetDefaultConfidence() (0.5) for transforms of the transform class of this plugin and of its
  /// subclasses, 0.0 for other transforms. The plugins of the transform classes that MRML provides use this default.
  /// The plugin of a subclass of a class that has a plugin must return more (for example, 0.7), so that it is asked
  /// first; a plugin that others usually handle better can return less.
  /// This is the convention of subject hierarchy plugins and file readers.
  virtual double CanHandleTransform(vtkAbstractTransform* transform);

  /// Confidence of a plugin for the transforms of its transform class, by default: 0.5.
  static double GetDefaultConfidence() { return 0.5; }

  /// Confidence of a plugin that is a fallback, which most other plugins do better: 0.1.
  static double GetFallbackConfidence() { return 0.1; }

  /// \name Description, copy and node class
  ///@{

  /// Get the description of the transform, as vtkMRMLTransformNode::GetTransformToParentInfo() shows it: the name of
  /// the transform type, followed by lines for its properties, each indented by two spaces (for example,
  /// "My transform:\n  Number of points: 12"). If it is empty (the default) then the class name is shown.
  virtual std::string GetTransformInfo(vtkAbstractTransform* transform);

  /// Make dst a deep copy of src (both are of the transform class of this plugin), which shares no data with it.
  /// Returns false if the plugin does not copy the transform (the default): vtkAbstractTransform::DeepCopy() is then
  /// used (and what MRML does for the transform classes that it supports, if it is a subclass of one of those).
  virtual bool DeepCopyTransform(vtkAbstractTransform* dst, vtkAbstractTransform* src);
  ///@}

  /// \name Reading and writing
  /// Transforms are written to file as ITK transforms. The ITK transform class must be registered in
  /// itk::TransformFactory, too. VTK transforms are in RAS, ITK transforms are in LPS coordinate system.
  ///@{

  /// Confidence (0.0-1.0) that this plugin can read ITK transforms of the class itkClassName (as
  /// itk::Object::GetNameOfClass() returns it): 0.0 (the default) if it cannot, GetDefaultConfidence() or more if the
  /// class is one that the plugin writes. The plugins are asked to read a transform in decreasing order of
  /// confidence. (The plugins of the transform classes that MRML provides cannot tell from the class name if they
  /// can read a transform, so they try to read any ITK transform class, with GetFallbackConfidence().)
  virtual double CanReadITKTransformClass(const std::string& itkClassName);

#ifndef __VTK_WRAP__
  /// Create the ITK transform (an itk::TransformBaseTemplate<double>) that stores the VTK transform.
  /// Returns nullptr if the plugin does not write the transform (the default); the next plugin of the transform is
  /// then asked.
  virtual itk::SmartPointer<itk::Object> CreateITKTransformFromVTK(vtkObject* loggerObject, vtkAbstractTransform* transformVtk);

  /// Create the ITK transform that stores the VTK transform, with the options of vtkMRMLTransformStorageNode.
  /// This is what transform storage nodes call; it calls the method above by default, which is all that most
  /// transform classes need.
  /// \param secondaryTransformItk a second transform that is written to the file (only for a legacy file format)
  /// \param preferITKv3CompatibleTransforms write a legacy file format if the transform class has one
  /// \param initialize make the ITK transform usable for computing (may take long; not needed for writing it to file)
  /// \param center_LocalRAS center of the transformation
  virtual itk::SmartPointer<itk::Object> CreateITKTransformFromVTK(vtkObject* loggerObject,
                                                                   vtkAbstractTransform* transformVtk,
                                                                   itk::SmartPointer<itk::Object>& secondaryTransformItk,
                                                                   int preferITKv3CompatibleTransforms,
                                                                   bool initialize,
                                                                   double center_LocalRAS[3]);

  /// Create the VTK transform that is stored in an ITK transform of a class that the plugin can read
  /// (see CanReadITKTransformClass()). The caller takes ownership of the returned transform.
  /// Returns nullptr if it fails or the plugin does not read the transform (the default); the next plugin that
  /// can read the ITK transform class is then asked.
  virtual vtkAbstractTransform* CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItk);

  /// Create the VTK transform that is stored in an ITK transform, getting the center of the transformation, too
  /// (it is not changed if the transform has none). This is what transform storage nodes call; it calls the method
  /// above by default.
  virtual vtkAbstractTransform* CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItk, double center_LocalRAS[3]);

  /// Convert the matrix of a 2D ITK transform to a 3D matrix, for reading 2D transforms as 3D transforms.
  /// Spacing for the third dimension is geometric mean of the other two dimensions, to minimize geometric distortion.
  /// Sign of the third dimension is chosen to create a right-handed coordinate system.
  template <typename T>
  static itk::Matrix<T, 3, 3> Matrix2Dto3D(const itk::Matrix<T, 2, 2>& m2D)
  {
    itk::Matrix<T, 3, 3> direction3d;
    direction3d.SetIdentity();
    for (unsigned int i = 0; i < 2; i++)
    {
      for (unsigned int j = 0; j < 2; j++)
      {
        direction3d[i][j] = m2D(i, j);
      }
    }
    const T scaleX = std::sqrt(direction3d[0][0] * direction3d[0][0] + direction3d[0][1] * direction3d[0][1]);
    const T scaleY = std::sqrt(direction3d[1][0] * direction3d[1][0] + direction3d[1][1] * direction3d[1][1]);
    const T scaleZ = std::sqrt(scaleX * scaleY); // geometric mean
    const T det = vnl_determinant(m2D.GetVnlMatrix());
    direction3d(2, 2) = (det < 0.0) ? -scaleZ : scaleZ;
    return direction3d;
  }
#endif
  ///@}

protected:
  vtkMRMLTransformPlugin();
  ~vtkMRMLTransformPlugin() override;

private:
  vtkMRMLTransformPlugin(const vtkMRMLTransformPlugin&) = delete;
  void operator=(const vtkMRMLTransformPlugin&) = delete;
};

#endif
