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

#include "vtkMRMLThinPlateSplineTransformPlugin.h"

// VTK includes
#include <vtkBSplineTransform.h>
#include <vtkGridTransform.h>
#include <vtkHomogeneousTransform.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOrientedBSplineTransform.h>
#include <vtkOrientedGridTransform.h>
#include <vtkPoints.h>
#include <vtkThinPlateSplineTransform.h>
#include <vtkTransform.h>

// ITK includes
#include <itkThinPlateSplineKernelTransform.h>
#include <itkTransformFactory.h>

// STD includes
#include <sstream>
#include <string>
#include <vector>

//----------------------------------------------------------------------------
// ITK transform classes that store the inverse of a transform in a transform file. They only store the parameters
// of the transform and cannot compute transformations: they are used for writing and reading a transform that is
// defined by its inverse (transform from parent), as ITK has no inverse transform classes.
namespace itk
{

template <class TScalar, unsigned int NDimensions>
class InverseThinPlateSplineKernelTransform : public ThinPlateSplineKernelTransform<TScalar, NDimensions>
{
public:
  /** Standard class typedefs. */
  typedef InverseThinPlateSplineKernelTransform Self;
  typedef ThinPlateSplineKernelTransform<TScalar, NDimensions> Superclass;
  typedef SmartPointer<Self> Pointer;
  typedef SmartPointer<const Self> ConstPointer;

  /** Run-time type information (and related methods). */
  itkTypeMacro(InverseThinPlateSplineKernelTransform, ThinPlateSplineKernelTransform);

  /** New macro for creation of through a Smart Pointer */
  itkNewMacro(Self);

  /** Define all computation methods as unimplemented to make sure they are not used
  (the inherited implementations are for the forward transform, so they would not give correct results) */
  typename Superclass::OutputPointType TransformPoint(const typename Superclass::InputPointType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseThinPlateSplineKernelTransform");
  }
  void ComputeDeformationContribution(const typename Superclass::InputPointType&, typename Superclass::OutputPointType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseThinPlateSplineKernelTransform");
  }
  void ComputeJacobianWithRespectToParameters(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseThinPlateSplineKernelTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseThinPlateSplineKernelTransform");
  }
#endif
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseThinPlateSplineKernelTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseThinPlateSplineKernelTransform");
  }
#endif
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::InverseJacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseThinPlateSplineKernelTransform");
  }
};

} // namespace itk

typedef itk::InverseThinPlateSplineKernelTransform<float, 3> InverseThinPlateSplineTransformFloatType;
typedef itk::InverseThinPlateSplineKernelTransform<double, 3> InverseThinPlateSplineTransformDoubleType;

namespace
{
typedef itk::ThinPlateSplineKernelTransform<double, 3> ThinPlateSplineTransformDoubleType;

typedef itk::TransformBaseTemplate<double> TransformItkType;

// Conversion between the VTK transform and the ITK transform that stores it in a file

template <typename T, unsigned VDimension>
bool SetVTKThinPlateSplineTransformFromITK(vtkObject* loggerObject,
                                           vtkThinPlateSplineTransform* transformVtk_RAS,
                                           typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS);

bool SetITKThinPlateSplineTransformFromVTK(vtkObject* loggerObject, itk::Object::Pointer& transformItk_LPS, vtkThinPlateSplineTransform* transformVtk_RAS, bool initialize = true);

//----------------------------------------------------------------------------
template <typename T, unsigned VDimension>
bool SetVTKThinPlateSplineTransformFromITK(vtkObject* loggerObject, vtkThinPlateSplineTransform* transformVtk_RAS, typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS)
{
  typedef itk::ThinPlateSplineKernelTransform<T, VDimension> ThinPlateSplineTransformType;
  typedef itk::InverseThinPlateSplineKernelTransform<T, VDimension> InverseThinPlateSplineTransformType;

  std::string transformItkClassName = transformItk_LPS->GetNameOfClass();

  bool inverse = false;
  typename ThinPlateSplineTransformType::PointSetType::Pointer sourceLandmarksItk_Lps;
  typename ThinPlateSplineTransformType::PointSetType::Pointer targetLandmarksItk_Lps;
  if (transformItkClassName == "InverseThinPlateSplineKernelTransform") // inverse class is derived from forward class, so it has to be checked first
  {
    ThinPlateSplineTransformType* inverseTpsTransform = static_cast<InverseThinPlateSplineTransformType*>(transformItk_LPS.GetPointer());
    inverse = true;
    sourceLandmarksItk_Lps = inverseTpsTransform->GetSourceLandmarks();
    targetLandmarksItk_Lps = inverseTpsTransform->GetTargetLandmarks();
  }
  else if (transformItkClassName == "ThinPlateSplineKernelTransform")
  {
    ThinPlateSplineTransformType* tpsTransform = static_cast<ThinPlateSplineTransformType*>(transformItk_LPS.GetPointer());
    inverse = false;
    sourceLandmarksItk_Lps = tpsTransform->GetSourceLandmarks();
    targetLandmarksItk_Lps = tpsTransform->GetTargetLandmarks();
  }
  else
  {
    vtkDebugWithObjectMacro(loggerObject, "Not a ThinPlateSpline transform");
    return false;
  }

  vtkNew<vtkPoints> sourceLandmarksVtk_Ras;
  unsigned int numberOfSourceLandmarks = sourceLandmarksItk_Lps->GetNumberOfPoints();
  vtkNew<vtkPoints> targetLandmarksVtk_Ras;
  unsigned int numberOfTargetLandmarks = targetLandmarksItk_Lps->GetNumberOfPoints();
  if constexpr (std::is_same_v<T, float>)
  {
    sourceLandmarksVtk_Ras->SetDataTypeToFloat();
    sourceLandmarksVtk_Ras->Allocate(numberOfSourceLandmarks);

    targetLandmarksVtk_Ras->SetDataTypeToFloat();
    targetLandmarksVtk_Ras->Allocate(numberOfTargetLandmarks);
  }
  else if constexpr (std::is_same_v<T, double>)
  {
    sourceLandmarksVtk_Ras->SetDataTypeToDouble();
    sourceLandmarksVtk_Ras->Allocate(numberOfSourceLandmarks);

    targetLandmarksVtk_Ras->SetDataTypeToDouble();
    targetLandmarksVtk_Ras->Allocate(numberOfTargetLandmarks);
  }
  else
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot load 2D TPS transform: unsupported pixel type. Only float and double are supported.");
    return false;
  }

  for (unsigned int i = 0; i < numberOfSourceLandmarks; i++)
  {
    typename ThinPlateSplineTransformType::InputPointType pointItk_Lps;
    bool pointExists = sourceLandmarksItk_Lps->GetPoint(i, &pointItk_Lps);
    if (!pointExists)
    {
      continue;
    }
    T pointVtk_Ras[3] = { 0 };
    pointVtk_Ras[0] = -pointItk_Lps[0];
    pointVtk_Ras[1] = -pointItk_Lps[1];
    if constexpr (VDimension == 2)
    {
      pointVtk_Ras[2] = 0.0;
    }
    else
    {
      pointVtk_Ras[2] = pointItk_Lps[2];
    }
    sourceLandmarksVtk_Ras->InsertNextPoint(pointVtk_Ras);
  }

  for (unsigned int i = 0; i < numberOfTargetLandmarks; i++)
  {
    typename ThinPlateSplineTransformType::InputPointType pointItk_Lps;
    bool pointExists = targetLandmarksItk_Lps->GetPoint(i, &pointItk_Lps);
    if (!pointExists)
    {
      continue;
    }
    T pointVtk_Ras[3] = { 0 };
    pointVtk_Ras[0] = -pointItk_Lps[0];
    pointVtk_Ras[1] = -pointItk_Lps[1];
    if constexpr (VDimension == 2)
    {
      pointVtk_Ras[2] = 0.0;
    }
    else
    {
      pointVtk_Ras[2] = pointItk_Lps[2];
    }
    targetLandmarksVtk_Ras->InsertNextPoint(pointVtk_Ras);
  }

  transformVtk_RAS->SetBasisToR(); // need to set this because data is 3D
  transformVtk_RAS->SetSourceLandmarks(sourceLandmarksVtk_Ras.GetPointer());
  transformVtk_RAS->SetTargetLandmarks(targetLandmarksVtk_Ras.GetPointer());

  if (inverse)
  {
    transformVtk_RAS->Inverse();
  }
  return true;
}

//----------------------------------------------------------------------------
bool SetITKThinPlateSplineTransformFromVTK(vtkObject* loggerObject,
                                           itk::Object::Pointer& transformItk_LPS,
                                           vtkThinPlateSplineTransform* transformVtk_RAS,
                                           bool initialize /*= true*/)
{
  if (transformVtk_RAS == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot set ITK thin-plate spline transform from VTK: the input vtkThinPlateSplineTransform is invalid");
    return false;
  }

  // Update is needed because the Basis value and Inverse flag may be out-of-date if the transform depends on its inverse
  transformVtk_RAS->Update();

  if (transformVtk_RAS->GetBasis() != VTK_RBF_R)
  {
    vtkErrorWithObjectMacro(loggerObject,
                            "Cannot set ITK thin-plate spline transform from VTK: basis function must be R."
                            " Call SetBasisToR() method of the vtkThinPlateSplineTransform object before attempting to write it to file.");
    return false;
  }

  ThinPlateSplineTransformDoubleType::PointSetType::Pointer sourceLandmarksItk_Lps = ThinPlateSplineTransformDoubleType::PointSetType::New();
  vtkPoints* sourceLandmarksVtk_Ras = transformVtk_RAS->GetSourceLandmarks();
  if (sourceLandmarksVtk_Ras != nullptr)
  {
    for (int i = 0; i < sourceLandmarksVtk_Ras->GetNumberOfPoints(); i++)
    {
      double posVtk_Ras[3] = { 0 };
      sourceLandmarksVtk_Ras->GetPoint(i, posVtk_Ras);
      ThinPlateSplineTransformDoubleType::InputPointType posItk_Lps;
      posItk_Lps[0] = -posVtk_Ras[0];
      posItk_Lps[1] = -posVtk_Ras[1];
      posItk_Lps[2] = posVtk_Ras[2];
      sourceLandmarksItk_Lps->GetPoints()->InsertElement(i, posItk_Lps);
    }
  }
  ThinPlateSplineTransformDoubleType::PointSetType::Pointer targetLandmarksItk_Lps = ThinPlateSplineTransformDoubleType::PointSetType::New();
  vtkPoints* targetLandmarksVtk_Ras = transformVtk_RAS->GetTargetLandmarks();
  if (targetLandmarksVtk_Ras != nullptr)
  {
    for (int i = 0; i < targetLandmarksVtk_Ras->GetNumberOfPoints(); i++)
    {
      double posVtk_Ras[3] = { 0 };
      targetLandmarksVtk_Ras->GetPoint(i, posVtk_Ras);
      ThinPlateSplineTransformDoubleType::InputPointType posItk_Lps;
      posItk_Lps[0] = -posVtk_Ras[0];
      posItk_Lps[1] = -posVtk_Ras[1];
      posItk_Lps[2] = posVtk_Ras[2];
      targetLandmarksItk_Lps->GetPoints()->InsertElement(i, posItk_Lps);
    }
  }

  if (transformVtk_RAS->GetInverseFlag())
  {
    InverseThinPlateSplineTransformDoubleType::Pointer tpsTransformItk = InverseThinPlateSplineTransformDoubleType::New();
    tpsTransformItk->SetSourceLandmarks(sourceLandmarksItk_Lps);
    tpsTransformItk->SetTargetLandmarks(targetLandmarksItk_Lps);
    if (initialize)
    {
      tpsTransformItk->ComputeWMatrix();
    }
    transformItk_LPS = tpsTransformItk;
  }
  else
  {
    ThinPlateSplineTransformDoubleType::Pointer tpsTransformItk = ThinPlateSplineTransformDoubleType::New();
    tpsTransformItk->SetSourceLandmarks(sourceLandmarksItk_Lps);
    tpsTransformItk->SetTargetLandmarks(targetLandmarksItk_Lps);
    if (initialize)
    {
      tpsTransformItk->ComputeWMatrix();
    }
    transformItk_LPS = tpsTransformItk;
  }
  return true;
}

//----------------------------------------------------------------------------
/// Register the ITK transform classes of this transform type that ITK does not register by default, so that
/// transform files that contain them can be read
bool RegisterITKTransformClasses()
{
  itk::TransformFactory<InverseThinPlateSplineTransformFloatType>::RegisterTransform();
  itk::TransformFactory<InverseThinPlateSplineTransformDoubleType>::RegisterTransform();
  itk::TransformFactory<itk::InverseThinPlateSplineKernelTransform<float, 2>>::RegisterTransform();
  itk::TransformFactory<itk::InverseThinPlateSplineKernelTransform<double, 2>>::RegisterTransform();
  itk::TransformFactory<itk::ThinPlateSplineKernelTransform<float, 3>>::RegisterTransform();
  itk::TransformFactory<itk::ThinPlateSplineKernelTransform<double, 3>>::RegisterTransform();
  itk::TransformFactory<itk::ThinPlateSplineKernelTransform<float, 2>>::RegisterTransform();
  itk::TransformFactory<itk::ThinPlateSplineKernelTransform<double, 2>>::RegisterTransform();
  return true;
}

} // namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLThinPlateSplineTransformPlugin);

//----------------------------------------------------------------------------
vtkMRMLThinPlateSplineTransformPlugin::vtkMRMLThinPlateSplineTransformPlugin()
{
  static const bool itkTransformClassesRegistered = RegisterITKTransformClasses();
  (void)itkTransformClassesRegistered;
}

//----------------------------------------------------------------------------
vtkMRMLThinPlateSplineTransformPlugin::~vtkMRMLThinPlateSplineTransformPlugin() = default;

//----------------------------------------------------------------------------
const char* vtkMRMLThinPlateSplineTransformPlugin::GetTransformClassName()
{
  return "vtkThinPlateSplineTransform";
}

//----------------------------------------------------------------------------
double vtkMRMLThinPlateSplineTransformPlugin::CanReadITKTransformClass(const std::string& vtkNotUsed(itkClassName))
{
  return vtkMRMLTransformPlugin::GetFallbackConfidence();
}

//----------------------------------------------------------------------------
std::string vtkMRMLThinPlateSplineTransformPlugin::GetTransformInfo(vtkAbstractTransform* transform)
{
  vtkThinPlateSplineTransform* tpsTransform = vtkThinPlateSplineTransform::SafeDownCast(transform);
  if (!tpsTransform)
  {
    return std::string();
  }
  std::stringstream ss;
  ss << " Thin plate spline:";
  tpsTransform->Update(); // compute if inverse
  int numberOfSourceLandmarks = 0;
  if (tpsTransform->GetSourceLandmarks() != nullptr)
  {
    numberOfSourceLandmarks = tpsTransform->GetSourceLandmarks()->GetNumberOfPoints();
  }
  int numberOfTargetLandmarks = 0;
  if (tpsTransform->GetTargetLandmarks() != nullptr)
  {
    numberOfTargetLandmarks = tpsTransform->GetTargetLandmarks()->GetNumberOfPoints();
  }
  ss << std::endl << "  Number of source landmarks: " << numberOfSourceLandmarks;
  ss << std::endl << "  Number of target landmarks: " << numberOfTargetLandmarks;
  if (tpsTransform->GetInverseFlag())
  {
    ss << std::endl << "  Computed from its inverse.";
  }
  // The description starts with the name of the transform type
  std::string info = ss.str();
  return (!info.empty() && info[0] == ' ') ? info.substr(1) : info;
}

//----------------------------------------------------------------------------
bool vtkMRMLThinPlateSplineTransformPlugin::DeepCopyTransform(vtkAbstractTransform* dst, vtkAbstractTransform* src)
{
  // Fix up the DeepCopy for vtkThinPlateSplineTransform (it performs only a ShallowCopy on the landmark points)
  dst->DeepCopy(src);
  if (vtkThinPlateSplineTransform::SafeDownCast(dst) == nullptr)
  {
    vtkGenericWarningMacro("vtkMRMLTransformNode::DeepCopyTransform failed: destination transform has to be vtkThinPlateSplineTransform");
    return false;
  }
  vtkPoints* srcSourceLandmarks = vtkThinPlateSplineTransform::SafeDownCast(src)->GetSourceLandmarks();
  if (srcSourceLandmarks)
  {
    vtkNew<vtkPoints> dstSourceLandmarks;
    dstSourceLandmarks->DeepCopy(srcSourceLandmarks);
    vtkThinPlateSplineTransform::SafeDownCast(dst)->SetSourceLandmarks(dstSourceLandmarks.GetPointer());
  }
  vtkPoints* srcTargetLandmarks = vtkThinPlateSplineTransform::SafeDownCast(src)->GetTargetLandmarks();
  if (srcTargetLandmarks)
  {
    vtkNew<vtkPoints> dstTargetLandmarks;
    dstTargetLandmarks->DeepCopy(srcTargetLandmarks);
    vtkThinPlateSplineTransform::SafeDownCast(dst)->SetTargetLandmarks(dstTargetLandmarks.GetPointer());
  }
  return true;
}

//----------------------------------------------------------------------------
itk::Object::Pointer vtkMRMLThinPlateSplineTransformPlugin::CreateITKTransformFromVTK(vtkObject* loggerObject,
                                                                                      vtkAbstractTransform* transformVtk,
                                                                                      itk::Object::Pointer& secondaryTransformItk,
                                                                                      int preferITKv3CompatibleTransforms,
                                                                                      bool initialize,
                                                                                      double center_LocalRAS[3])
{
  vtkThinPlateSplineTransform* tpsTransformVtk = vtkThinPlateSplineTransform::SafeDownCast(transformVtk);
  itk::Object::Pointer transformItk;
  if (!tpsTransformVtk || !SetITKThinPlateSplineTransformFromVTK(loggerObject, transformItk, tpsTransformVtk, initialize))
  {
    return nullptr;
  }
  return transformItk;
}

//----------------------------------------------------------------------------
vtkAbstractTransform* vtkMRMLThinPlateSplineTransformPlugin::CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItkObject, double center_LocalRAS[3])
{
  // Transform files are read with double precision
  TransformItkType* transformItk = dynamic_cast<TransformItkType*>(transformItkObject);
  if (!transformItk)
  {
    return nullptr;
  }
  const unsigned int itkDim = transformItk->GetOutputSpaceDimension();
  vtkNew<vtkThinPlateSplineTransform> tpsTransformVtk;
  bool success = false;
  if (itkDim == 3)
  {
    success = SetVTKThinPlateSplineTransformFromITK<double, 3>(loggerObject, tpsTransformVtk, transformItk);
  }
  else if (itkDim == 2)
  {
    success = SetVTKThinPlateSplineTransformFromITK<double, 2>(loggerObject, tpsTransformVtk, transformItk);
  }
  if (!success)
  {
    return nullptr;
  }
  tpsTransformVtk->Register(nullptr);
  return tpsTransformVtk;
}
