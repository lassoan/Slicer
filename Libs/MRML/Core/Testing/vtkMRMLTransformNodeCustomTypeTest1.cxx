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

// Tests the extension points of transform nodes for transform classes that MRML does not know: the plugin of a
// transform class (reading, writing and the key of the input of its transforms), the event for setting the inputs of
// transforms, and the node references to the inputs.

// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLTransformNode.h"
#include "vtkMRMLTransformStorageNode.h"
#include "vtkMRMLTransformPlugin.h"
#include "vtkMRMLTransformPluginFactory.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCollection.h>
#include <vtkGeneralTransform.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkThinPlateSplineTransform.h>
#include <vtkTransform.h>
#include <vtksys/SystemTools.hxx>

// ITK includes
#include <itkTransform.h>
#include <itkTransformFactory.h>

// STD includes
#include <algorithm>
#include <string>
#include <vector>

namespace
{

//----------------------------------------------------------------------------
/// A transform class that MRML does not know. It is a subclass of a class that MRML can write, as custom transform
/// classes may be.
class vtkTestTransform : public vtkThinPlateSplineTransform
{
public:
  static vtkTestTransform* New();
  vtkTypeMacro(vtkTestTransform, vtkThinPlateSplineTransform);
  vtkAbstractTransform* MakeTransform() override { return vtkTestTransform::New(); }

  double Value{ 0.0 };

protected:
  void InternalDeepCopy(vtkAbstractTransform* transform) override
  {
    this->Superclass::InternalDeepCopy(transform);
    this->Value = static_cast<vtkTestTransform*>(transform)->Value;
  }
};
vtkStandardNewMacro(vtkTestTransform);

} // namespace

namespace itk
{
//----------------------------------------------------------------------------
/// Stores a vtkTestTransform in a file: the value is the only fixed parameter
template <typename TParametersValueType, unsigned int NDimensions>
class TestTransform : public Transform<TParametersValueType, NDimensions, NDimensions>
{
public:
  typedef TestTransform Self;
  typedef Transform<TParametersValueType, NDimensions, NDimensions> Superclass;
  typedef SmartPointer<Self> Pointer;
  typedef SmartPointer<const Self> ConstPointer;
  itkTypeMacro(TestTransform, Transform);
  itkNewMacro(Self);

  void SetParameters(const typename Superclass::ParametersType& parameters) override
  {
    this->m_Parameters = parameters;
    this->Modified();
  }
  void SetFixedParameters(const typename Superclass::FixedParametersType& fixedParameters) override
  {
    this->m_FixedParameters = fixedParameters;
    this->Modified();
  }
  using Superclass::TransformPoint;
  typename Superclass::OutputPointType TransformPoint(const typename Superclass::InputPointType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for " << this->GetNameOfClass());
  }
  void ComputeJacobianWithRespectToParameters(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for " << this->GetNameOfClass());
  }
};
} // namespace itk

namespace
{
typedef itk::TestTransform<double, 3> TestTransformItkType;

//----------------------------------------------------------------------------
itk::Object::Pointer CreateITKTestTransformFromVTK(vtkObject* vtkNotUsed(loggerObject), vtkAbstractTransform* transformVtk)
{
  vtkTestTransform* testTransformVtk = vtkTestTransform::SafeDownCast(transformVtk);
  if (!testTransformVtk)
  {
    return nullptr;
  }
  TestTransformItkType::Pointer testTransformItk = TestTransformItkType::New();
  TestTransformItkType::FixedParametersType fixedParameters(1);
  fixedParameters[0] = testTransformVtk->Value;
  testTransformItk->SetFixedParameters(fixedParameters);
  return testTransformItk.GetPointer();
}

//----------------------------------------------------------------------------
vtkAbstractTransform* CreateVTKTestTransformFromITK(vtkObject* vtkNotUsed(loggerObject), itk::Object* transformItk)
{
  TestTransformItkType* testTransformItk = dynamic_cast<TestTransformItkType*>(transformItk);
  if (!testTransformItk || testTransformItk->GetFixedParameters().Size() != 1)
  {
    return nullptr;
  }
  vtkTestTransform* testTransformVtk = vtkTestTransform::New();
  testTransformVtk->Value = testTransformItk->GetFixedParameters()[0];
  return testTransformVtk;
}

//----------------------------------------------------------------------------
/// Plugin of the test transform class, as a module registers for its transform class
class vtkTestTransformPlugin : public vtkMRMLTransformPlugin
{
public:
  static vtkTestTransformPlugin* New();
  vtkTypeMacro(vtkTestTransformPlugin, vtkMRMLTransformPlugin);
  const char* GetTransformClassName() override { return "vtkTestTransform"; }
  std::string GetTransformInfo(vtkAbstractTransform* transform) override
  {
    vtkTestTransform* testTransform = vtkTestTransform::SafeDownCast(transform);
    return (testTransform && this->DescribeTransforms ? "Test transform:\n  Value: " + std::to_string(static_cast<int>(testTransform->Value)) : std::string());
  }
  bool DeepCopyTransform(vtkAbstractTransform* dst, vtkAbstractTransform* src) override
  {
    if (!this->CopyTransforms)
    {
      return false;
    }
    this->NumberOfCopies++;
    dst->DeepCopy(src);
    return true;
  }
  bool DescribeTransforms{ false };
  bool CopyTransforms{ false };
  int NumberOfCopies{ 0 };
  double CanHandleTransform(vtkAbstractTransform* transform) override { return vtkTestTransform::SafeDownCast(transform) ? this->Confidence : 0.0; }
  double CanReadITKTransformClass(const std::string& itkClassName) override { return itkClassName == "TestTransform" ? vtkMRMLTransformPlugin::GetDefaultConfidence() : 0.0; }
  /// Confidence that the plugin can handle a test transform. The test transform class is a subclass of a transform
  /// class that MRML provides, so it is more than the default confidence, which the plugin of that class has.
  double Confidence{ 0.7 };
  itk::Object::Pointer CreateITKTransformFromVTK(vtkObject* loggerObject, vtkAbstractTransform* transformVtk) override
  {
    return CreateITKTestTransformFromVTK(loggerObject, transformVtk);
  }
  vtkAbstractTransform* CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItk) override { return CreateVTKTestTransformFromITK(loggerObject, transformItk); }
};
vtkStandardNewMacro(vtkTestTransformPlugin);

//----------------------------------------------------------------------------
/// The first plugin that is asked to read an ITK transform class
vtkMRMLTransformPlugin* GetFirstPluginForITKTransformClass(const std::string& itkClassName)
{
  std::vector<vtkMRMLTransformPlugin*> plugins;
  vtkMRMLTransformPluginFactory::GetInstance()->GetPluginsForITKTransformClass(itkClassName, plugins);
  return plugins.empty() ? nullptr : plugins.front();
}

//----------------------------------------------------------------------------
/// The test transform among the components of the transform that is stored in the node
vtkTestTransform* GetTestTransform(vtkMRMLTransformNode* transformNode, int expectedNumberOfComponents)
{
  vtkNew<vtkCollection> components;
  vtkMRMLTransformNode::FlattenGeneralTransform(components, transformNode->GetTransformFromParent());
  if (components->GetNumberOfItems() != expectedNumberOfComponents)
  {
    std::cerr << "Transform has " << components->GetNumberOfItems() << " components, expected " << expectedNumberOfComponents << std::endl;
    return nullptr;
  }
  for (int index = 0; index < components->GetNumberOfItems(); ++index)
  {
    if (vtkTestTransform* testTransform = vtkTestTransform::SafeDownCast(components->GetItemAsObject(index)))
    {
      return testTransform;
    }
  }
  return nullptr;
}

//----------------------------------------------------------------------------
bool WriteTransform(vtkMRMLScene* scene, vtkAbstractTransform* transformFromParent, const std::string& fileName)
{
  vtkMRMLTransformNode* transformNode = vtkMRMLTransformNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLTransformNode"));
  transformNode->SetAndObserveTransformFromParent(transformFromParent);
  vtkNew<vtkMRMLTransformStorageNode> storageNode;
  storageNode->SetScene(scene);
  storageNode->SetFileName(fileName.c_str());
  return storageNode->WriteData(transformNode) != 0;
}

//----------------------------------------------------------------------------
vtkMRMLTransformNode* ReadTransform(vtkMRMLScene* scene, const std::string& fileName)
{
  vtkMRMLTransformNode* transformNode = vtkMRMLTransformNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLTransformNode"));
  vtkNew<vtkMRMLTransformStorageNode> storageNode;
  storageNode->SetScene(scene);
  storageNode->SetFileName(fileName.c_str());
  return storageNode->ReadData(transformNode) ? transformNode : nullptr;
}

//----------------------------------------------------------------------------
int TestReadWrite(const std::string& tempDir)
{
  vtkNew<vtkMRMLScene> scene;
  const std::string singleFileName = tempDir + "/vtkMRMLTransformNodeCustomTypeTest1_single.h5";
  const std::string compositeFileName = tempDir + "/vtkMRMLTransformNodeCustomTypeTest1_composite.h5";
  vtksys::SystemTools::RemoveFile(singleFileName);
  vtksys::SystemTools::RemoveFile(compositeFileName);

  vtkNew<vtkTestTransform> testTransform;
  testTransform->Value = 12.5;
  vtkNew<vtkTransform> linearTransform;
  linearTransform->Translate(10.0, 20.0, 30.0);
  vtkNew<vtkGeneralTransform> compositeTransform;
  compositeTransform->Concatenate(linearTransform);
  compositeTransform->Concatenate(testTransform);

  // Without its own plugin the test transform has the plugin of its superclass, a transform class that MRML
  // provides; reading an ITK transform of an unknown class is tried by the plugins of those classes
  CHECK_NOT_NULL(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(testTransform));
  CHECK_STRING(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(testTransform)->GetTransformClassName(), "vtkThinPlateSplineTransform");
  CHECK_NOT_NULL(GetFirstPluginForITKTransformClass("TestTransform"));
  CHECK_STRING(GetFirstPluginForITKTransformClass("TestTransform")->GetTransformClassName(), "vtkHomogeneousTransform");

  itk::TransformFactory<TestTransformItkType>::RegisterTransform();
  vtkNew<vtkTestTransformPlugin> plugin;
  vtkMRMLTransformPluginFactory::GetInstance()->RegisterPlugin(plugin);
  // Registering it again changes nothing
  vtkMRMLTransformPluginFactory::GetInstance()->RegisterPlugin(plugin);
  CHECK_POINTER(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(testTransform), plugin.GetPointer());
  CHECK_POINTER(GetFirstPluginForITKTransformClass("TestTransform"), plugin.GetPointer());
  CHECK_STRING(GetFirstPluginForITKTransformClass("OtherTransform")->GetTransformClassName(), "vtkHomogeneousTransform");
  // The plugin of the superclass comes after it
  std::vector<vtkMRMLTransformPlugin*> testTransformPlugins;
  vtkMRMLTransformPluginFactory::GetInstance()->GetPlugins(testTransform, testTransformPlugins);
  CHECK_INT(static_cast<int>(testTransformPlugins.size()), 2);
  CHECK_POINTER(testTransformPlugins[0], plugin.GetPointer());
  CHECK_STRING(testTransformPlugins[1]->GetTransformClassName(), "vtkThinPlateSplineTransform");
  CHECK_DOUBLE(plugin->CanHandleTransform(testTransform), 0.7);
  vtkNew<vtkThinPlateSplineTransform> plainThinPlateSplineTransform;
  CHECK_DOUBLE(plugin->CanHandleTransform(plainThinPlateSplineTransform), 0.0);
  CHECK_DOUBLE(plugin->CanHandleTransform(nullptr), 0.0);
  CHECK_DOUBLE(testTransformPlugins[1]->CanHandleTransform(testTransform), vtkMRMLTransformPlugin::GetDefaultConfidence());
  // The plugins are in decreasing order of confidence, whatever order they were registered in:
  // a second plugin, registered later, is first only if it is more confident
  vtkNew<vtkTestTransformPlugin> otherPlugin;
  otherPlugin->Confidence = 0.4;
  vtkMRMLTransformPluginFactory::GetInstance()->RegisterPlugin(otherPlugin);
  CHECK_POINTER(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(testTransform), plugin.GetPointer());
  otherPlugin->Confidence = 0.9;
  CHECK_POINTER(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(testTransform), otherPlugin.GetPointer());
  // Less confident than the plugin of the superclass: that one is the first
  plugin->Confidence = 0.1;
  otherPlugin->Confidence = 0.05;
  vtkMRMLTransformPluginFactory::GetInstance()->GetPlugins(testTransform, testTransformPlugins);
  CHECK_INT(static_cast<int>(testTransformPlugins.size()), 3);
  CHECK_STRING(testTransformPlugins[0]->GetTransformClassName(), "vtkThinPlateSplineTransform");
  CHECK_POINTER(testTransformPlugins[1], plugin.GetPointer());
  CHECK_POINTER(testTransformPlugins[2], otherPlugin.GetPointer());
  // A plugin that cannot handle the transform is not among its plugins
  otherPlugin->Confidence = 0.0;
  vtkMRMLTransformPluginFactory::GetInstance()->GetPlugins(testTransform, testTransformPlugins);
  CHECK_INT(static_cast<int>(testTransformPlugins.size()), 2);
  vtkMRMLTransformPluginFactory::GetInstance()->UnregisterPlugin(otherPlugin);
  plugin->Confidence = 0.7;
  // Only for the class and its subclasses
  vtkNew<vtkThinPlateSplineTransform> superclassTransform;
  CHECK_STRING(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(superclassTransform)->GetTransformClassName(), "vtkThinPlateSplineTransform");
  CHECK_NULL(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(nullptr));

  // The transform alone
  CHECK_BOOL(WriteTransform(scene, testTransform, singleFileName), true);
  vtkMRMLTransformNode* singleTransformNode = ReadTransform(scene, singleFileName);
  CHECK_NOT_NULL(singleTransformNode);
  vtkTestTransform* singleTestTransform = GetTestTransform(singleTransformNode, 1);
  CHECK_NOT_NULL(singleTestTransform);
  CHECK_DOUBLE(singleTestTransform->Value, 12.5);

  // The transform as a component of a composite transform
  CHECK_BOOL(WriteTransform(scene, compositeTransform, compositeFileName), true);
  vtkMRMLTransformNode* compositeTransformNode = ReadTransform(scene, compositeFileName);
  CHECK_NOT_NULL(compositeTransformNode);
  vtkTestTransform* compositeTestTransform = GetTestTransform(compositeTransformNode, 2);
  CHECK_NOT_NULL(compositeTestTransform);
  CHECK_DOUBLE(compositeTestTransform->Value, 12.5);
  // The test transform is the identity, so only the linear component moves the point
  double point[3] = { 1.0, 2.0, 3.0 };
  double transformedPoint[3] = { 0.0, 0.0, 0.0 };
  compositeTransformNode->GetTransformFromParent()->TransformPoint(point, transformedPoint);
  CHECK_DOUBLE_TOLERANCE(transformedPoint[0], 11.0, 1e-6);
  CHECK_DOUBLE_TOLERANCE(transformedPoint[1], 22.0, 1e-6);
  CHECK_DOUBLE_TOLERANCE(transformedPoint[2], 33.0, 1e-6);

  // Without the plugin the transform cannot be read: it is an error also if it is in a composite transform,
  // the rest of the composite transform is not used without it
  vtkMRMLTransformPluginFactory::GetInstance()->UnregisterPlugin(plugin);
  // Without its own plugin the test transform has the plugin of its superclass, a transform class that MRML
  // provides; reading an ITK transform of an unknown class is tried by the plugins of those classes
  CHECK_NOT_NULL(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(testTransform));
  CHECK_STRING(vtkMRMLTransformPluginFactory::GetInstance()->GetPlugin(testTransform)->GetTransformClassName(), "vtkThinPlateSplineTransform");
  CHECK_NOT_NULL(GetFirstPluginForITKTransformClass("TestTransform"));
  CHECK_STRING(GetFirstPluginForITKTransformClass("TestTransform")->GetTransformClassName(), "vtkHomogeneousTransform");
  TESTING_OUTPUT_ASSERT_ERRORS_BEGIN();
  CHECK_NULL(ReadTransform(scene, singleFileName));
  TESTING_OUTPUT_ASSERT_ERRORS_END();
  TESTING_OUTPUT_ASSERT_ERRORS_BEGIN();
  CHECK_NULL(ReadTransform(scene, compositeFileName));
  TESTING_OUTPUT_ASSERT_ERRORS_END();

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
/// The plugin of a transform class describes and copies its transforms, and tells the class of their nodes
int TestInfoCopyAndNodeClass()
{
  vtkNew<vtkMRMLScene> scene;
  vtkMRMLTransformNode* transformNode = vtkMRMLTransformNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLTransformNode"));
  vtkNew<vtkTestTransform> testTransform;
  testTransform->Value = 7.0;
  vtkNew<vtkTransform> linearTransform;
  linearTransform->Translate(1.0, 2.0, 3.0);
  vtkNew<vtkGeneralTransform> compositeTransform;
  compositeTransform->Concatenate(linearTransform);
  compositeTransform->Concatenate(testTransform);
  transformNode->SetAndObserveTransformToParent(compositeTransform);

  // Without a plugin: described as the transform class that MRML supports and it is a subclass of
  std::string info = transformNode->GetTransformToParentInfo();
  CHECK_BOOL(info.find("Thin plate spline") != std::string::npos, true);

  vtkNew<vtkTestTransformPlugin> plugin;
  vtkMRMLTransformPluginFactory::GetInstance()->RegisterPlugin(plugin);
  // A plugin that does not describe, copy or choose the node class of its transforms changes nothing
  info = transformNode->GetTransformToParentInfo();
  CHECK_BOOL(info.find("Thin plate spline") != std::string::npos, true);
  vtkMRMLTransformNode* copiedNode = vtkMRMLTransformNode::SafeDownCast(scene->AddNewNodeByClass("vtkMRMLTransformNode"));
  copiedNode->CopyContent(transformNode);
  CHECK_INT(plugin->NumberOfCopies, 0);
  CHECK_NOT_NULL(GetTestTransform(copiedNode, 2));
  CHECK_DOUBLE(GetTestTransform(copiedNode, 2)->Value, 7.0);

  // Description
  plugin->DescribeTransforms = true;
  info = transformNode->GetTransformToParentInfo();
  CHECK_BOOL(info.find("Test transform:") != std::string::npos, true);
  CHECK_BOOL(info.find("Value: 7") != std::string::npos, true);
  CHECK_BOOL(info.find("Thin plate spline") != std::string::npos, false);
  // The other components are described as before
  CHECK_BOOL(info.find("Linear") != std::string::npos, true);

  // Copy
  plugin->CopyTransforms = true;
  copiedNode->CopyContent(transformNode);
  CHECK_INT(plugin->NumberOfCopies, 1);
  vtkTestTransform* copiedTestTransform = GetTestTransform(copiedNode, 2);
  CHECK_NOT_NULL(copiedTestTransform);
  CHECK_POINTER_DIFFERENT(copiedTestTransform, testTransform.GetPointer());
  CHECK_DOUBLE(copiedTestTransform->Value, 7.0);

  // Split: a component that gets a new node is in a generic transform node
  CHECK_BOOL(copiedNode->Split() != 0, true);
  int numberOfTestTransformNodes = 0;
  for (vtkMRMLTransformNode* componentNode = copiedNode; componentNode; componentNode = componentNode->GetParentTransformNode())
  {
    if (componentNode != copiedNode && componentNode->GetTransformToParentAs("vtkTestTransform", false))
    {
      numberOfTestTransformNodes++;
      CHECK_STRING(componentNode->GetClassName(), "vtkMRMLTransformNode");
    }
  }
  CHECK_BOOL(transformNode->Split() != 0, true);
  for (vtkMRMLTransformNode* componentNode = transformNode; componentNode; componentNode = componentNode->GetParentTransformNode())
  {
    if (componentNode != transformNode && componentNode->GetTransformToParentAs("vtkTestTransform", false))
    {
      numberOfTestTransformNodes++;
      CHECK_STRING(componentNode->GetClassName(), "vtkMRMLTransformNode");
    }
  }
  // In each of the two splits either the test transform or the linear transform got a new node
  CHECK_BOOL(numberOfTestTransformNodes == 0 || numberOfTestTransformNodes == 2, true);

  vtkMRMLTransformPluginFactory::GetInstance()->UnregisterPlugin(plugin);
  return EXIT_SUCCESS;
}

} // namespace

//----------------------------------------------------------------------------
int vtkMRMLTransformNodeCustomTypeTest1(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " /path/to/temp" << std::endl;
    return EXIT_FAILURE;
  }
  CHECK_EXIT_SUCCESS(TestReadWrite(argv[1]));
  CHECK_EXIT_SUCCESS(TestInfoCopyAndNodeClass());
  return EXIT_SUCCESS;
}
