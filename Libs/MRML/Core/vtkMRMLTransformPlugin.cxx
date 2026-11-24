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

#include "vtkMRMLTransformPlugin.h"

// VTK includes
#include <vtkAbstractTransform.h>
#include <vtkSmartPointer.h>

// STD includes
#include <algorithm>
#include <utility>
#include <vector>

//----------------------------------------------------------------------------
vtkMRMLTransformPlugin::vtkMRMLTransformPlugin() = default;

//----------------------------------------------------------------------------
vtkMRMLTransformPlugin::~vtkMRMLTransformPlugin() = default;

//----------------------------------------------------------------------------
void vtkMRMLTransformPlugin::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  const char* transformClassName = this->GetTransformClassName();
  os << indent << "TransformClassName: " << (transformClassName ? transformClassName : "(none)") << "\n";
}

//----------------------------------------------------------------------------
std::string vtkMRMLTransformPlugin::GetTransformInfo(vtkAbstractTransform* vtkNotUsed(transform))
{
  return std::string();
}

//----------------------------------------------------------------------------
bool vtkMRMLTransformPlugin::DeepCopyTransform(vtkAbstractTransform* vtkNotUsed(dst), vtkAbstractTransform* vtkNotUsed(src))
{
  return false;
}

//----------------------------------------------------------------------------
double vtkMRMLTransformPlugin::CanHandleTransform(vtkAbstractTransform* transform)
{
  const char* transformClassName = this->GetTransformClassName();
  return (transform && transformClassName && transform->IsA(transformClassName)) ? vtkMRMLTransformPlugin::GetDefaultConfidence() : 0.0;
}

//----------------------------------------------------------------------------
double vtkMRMLTransformPlugin::CanReadITKTransformClass(const std::string& vtkNotUsed(itkClassName))
{
  return 0.0;
}

//----------------------------------------------------------------------------
itk::SmartPointer<itk::Object> vtkMRMLTransformPlugin::CreateITKTransformFromVTK(vtkObject* vtkNotUsed(loggerObject), vtkAbstractTransform* vtkNotUsed(transformVtk))
{
  return nullptr;
}

//----------------------------------------------------------------------------
itk::SmartPointer<itk::Object> vtkMRMLTransformPlugin::CreateITKTransformFromVTK(vtkObject* loggerObject,
                                                                                 vtkAbstractTransform* transformVtk,
                                                                                 itk::SmartPointer<itk::Object>& vtkNotUsed(secondaryTransformItk),
                                                                                 int vtkNotUsed(preferITKv3CompatibleTransforms),
                                                                                 bool vtkNotUsed(initialize),
                                                                                 double vtkNotUsed(center_LocalRAS)[3])
{
  return this->CreateITKTransformFromVTK(loggerObject, transformVtk);
}

//----------------------------------------------------------------------------
vtkAbstractTransform* vtkMRMLTransformPlugin::CreateVTKTransformFromITK(vtkObject* vtkNotUsed(loggerObject), itk::Object* vtkNotUsed(transformItk))
{
  return nullptr;
}

//----------------------------------------------------------------------------
vtkAbstractTransform* vtkMRMLTransformPlugin::CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItk, double vtkNotUsed(center_LocalRAS)[3])
{
  return this->CreateVTKTransformFromITK(loggerObject, transformItk);
}
