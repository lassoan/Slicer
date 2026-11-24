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

#include "vtkMRMLLinearTransformPlugin.h"

// VTK includes
#include <vtkBSplineTransform.h>
#include <vtkGridTransform.h>
#include <vtkHomogeneousTransform.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOrientedBSplineTransform.h>
#include <vtkOrientedGridTransform.h>
#include <vtkPoints.h>
#include <vtkThinPlateSplineTransform.h>
#include <vtkTransform.h>

// ITK includes
#include <itkAffineTransform.h>
#include <itkTranslationTransform.h>

// STD includes
#include <sstream>
#include <string>
#include <vector>

namespace
{
const unsigned int VTKDimension = 3;

typedef itk::TransformBaseTemplate<double> TransformItkType;

// Conversion between the VTK transform and the ITK transform that stores it in a file

// Conversion of the transform types, which the plugins of the transform types use
template <typename T>
typename itk::AffineTransform<T, 3>::Pointer ConvertITKLinearTransformFrom2Dto3D(typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS);

template <typename T>
bool SetVTKLinearTransformFromITK(vtkObject* loggerObject,
                                  vtkMatrix4x4* transformVtk_RAS,
                                  typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS,
                                  double center_LocalRAS[3] = nullptr);

bool SetITKLinearTransformFromVTK(vtkObject* loggerObject, itk::Object::Pointer& transformItk_LPS, vtkMatrix4x4* transformVtk_RAS, double center_LocalRAS[3] = nullptr);

//----------------------------------------------------------------------------
template <typename T>
typename itk::AffineTransform<T, 3>::Pointer ConvertITKLinearTransformFrom2Dto3D(typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS)
{
  using Affine3D = itk::AffineTransform<T, 3>;
  using LinearTransformType = itk::MatrixOffsetTransformBase<T, 2, 2>;

  typename LinearTransformType::ConstPointer t2d = dynamic_cast<const LinearTransformType*>(transformItk_LPS.GetPointer());
  if (t2d == nullptr)
  {
    throw std::runtime_error("ConvertITKLinearTransformFrom2Dto3D: Unable to cast ITK transform to 2D linear transform");
  }

  // convert into 3D affine which Slicer can handle
  typename Affine3D::MatrixType m = vtkMRMLTransformPlugin::Matrix2Dto3D<T>(t2d->GetMatrix());
  typename Affine3D::TranslationType t;
  t.Fill(0);
  typename Affine3D::InputPointType c;
  c.Fill(0);
  for (unsigned int i = 0; i < 2; i++)
  {
    t[i] = t2d->GetTranslation()[i];
    c[i] = t2d->GetCenter()[i];
  }

  typename Affine3D::Pointer aTr = Affine3D::New();
  aTr->SetCenter(c);
  aTr->SetMatrix(m);
  aTr->SetTranslation(t);
  return aTr;
}

//----------------------------------------------------------------------------
template <typename T>
bool SetVTKLinearTransformFromITK(vtkObject* /*loggerObject*/,
                                  vtkMatrix4x4* transformVtk_RAS,
                                  typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS,
                                  double center_LocalRAS[3] /*=nullptr*/)
{
  static const unsigned int D = VTKDimension;
  typedef itk::MatrixOffsetTransformBase<T, D, D> LinearTransformType;
  typedef itk::TranslationTransform<T, D> TranslateTransformType;

  vtkSmartPointer<vtkMatrix4x4> transformVtk_LPS = vtkSmartPointer<vtkMatrix4x4>::New();
  const unsigned itkDim = transformItk_LPS->GetOutputSpaceDimension();

  bool convertedToVtkMatrix = false;

  std::string itkTransformClassName = transformItk_LPS->GetNameOfClass();

  // Linear transform of doubles or floats, dimension 2 or 3

  // ITKIO transform libraries are built as shared and dynamic_cast
  // can NOT be used with templated classes that are
  // instantiated in a translation unit different than the one where they are
  // defined. It will work only if the classes are explicitly instantiated
  // and exported.
  // To workaround the issue, instead of using dynamic_cast:
  // (1) to ensure the objects are of the right type string comparison is done
  // (2) static_cast is used instead of dynamic_cast.
  // See InsightSoftwareConsortium/ITK@d1e9fe2
  // and see https://stackoverflow.com/questions/8024010/why-do-template-class-functions-have-to-be-declared-in-the-same-translation-unit
  //
  // The disadvantage of this approach is that each supported class name has to be explicitly listed here and if the class hierarchy changes in ITK
  // then the static cast may produce invalid results. Also, even if the transform class name is matching,
  // we may cast the transform to a wrong type due to mismatch in dimensions (not 3) or data type (not double or float).

  if (itkTransformClassName.find("AffineTransform") != std::string::npos || //
      itkTransformClassName == "MatrixOffsetTransformBase" ||               //
      itkTransformClassName == "Rigid3DTransform" ||                        //
      itkTransformClassName == "Rigid2DTransform" ||                        //
      itkTransformClassName == "Euler3DTransform" ||                        //
      itkTransformClassName == "Euler2DTransform" ||                        //
      itkTransformClassName == "CenteredEuler3DTransform" ||                //
      itkTransformClassName == "CenteredEuler2DTransform" ||                //
      itkTransformClassName == "CenteredSimilarity2DTransform" ||           //
      itkTransformClassName == "QuaternionRigidTransform" ||                //
      itkTransformClassName == "VersorTransform" ||                         //
      itkTransformClassName == "VersorRigid3DTransform" ||                  //
      itkTransformClassName == "ScaleSkewVersor3DTransform" ||              //
      itkTransformClassName == "ScaleVersor3DTransform" ||                  //
      itkTransformClassName == "Similarity3DTransform" ||                   //
      itkTransformClassName == "Similarity2DTransform" ||                   //
      itkTransformClassName == "ScaleTransform" ||                          //
      itkTransformClassName == "ScaleLogarithmicTransform")
  {
    typename LinearTransformType::Pointer dlt;
    if (itkDim == 2)
    {
      dlt = ConvertITKLinearTransformFrom2Dto3D<T>(transformItk_LPS);
    }
    else
    {
      dlt = static_cast<LinearTransformType*>(transformItk_LPS.GetPointer());
    }

    convertedToVtkMatrix = true;
    for (unsigned int i = 0; i < D; i++)
    {
      for (unsigned int j = 0; j < D; j++)
      {
        transformVtk_LPS->SetElement(i, j, dlt->GetMatrix()[i][j]);
      }
      transformVtk_LPS->SetElement(i, D, dlt->GetOffset()[i]);
    }

    if (center_LocalRAS)
    {
      auto center_LocalLPS = dlt->GetCenter();
      center_LocalRAS[0] = -center_LocalLPS[0];
      center_LocalRAS[1] = -center_LocalLPS[1];
      center_LocalRAS[2] = center_LocalLPS[2];
    }
  }

  // Identity transform of doubles or floats, any dimension
  if (itkTransformClassName == "IdentityTransform")
  {
    // nothing to do, matrix is already the identity
    convertedToVtkMatrix = true;
  }

  // Translate transform of doubles or floats, dimension 2 or 3
  // Separated out as it does not inherit from MatrixOffsetTransformBase
  if (itkTransformClassName == "TranslationTransform")
  {
    if (itkDim == 2)
    {
      using Translate2DTransformType = itk::TranslationTransform<T, 2>;
      typename Translate2DTransformType::Pointer dtt = static_cast<Translate2DTransformType*>(transformItk_LPS.GetPointer());
      for (unsigned int i = 0; i < 2; i++)
      {
        transformVtk_LPS->SetElement(i, D, dtt->GetOffset()[i]);
      }
      // third element remains zero
    }
    else
    {
      typename TranslateTransformType::Pointer dtt = static_cast<TranslateTransformType*>(transformItk_LPS.GetPointer());
      for (unsigned int i = 0; i < D; i++)
      {
        transformVtk_LPS->SetElement(i, D, dtt->GetOffset()[i]);
      }
    }
    convertedToVtkMatrix = true;
  }

  // Convert from LPS (ITK) to RAS (Slicer)
  //
  // Tras = lps2ras * Tlps * ras2lps
  //

  vtkSmartPointer<vtkMatrix4x4> lps2ras = vtkSmartPointer<vtkMatrix4x4>::New();
  lps2ras->SetElement(0, 0, -1);
  lps2ras->SetElement(1, 1, -1);
  vtkMatrix4x4* ras2lps = lps2ras; // lps2ras is diagonal therefore the inverse is identical

  vtkMatrix4x4::Multiply4x4(lps2ras, transformVtk_LPS, transformVtk_LPS);
  vtkMatrix4x4::Multiply4x4(transformVtk_LPS, ras2lps, transformVtk_RAS);

  if (center_LocalRAS)
  {
    for (int i = 0; i < 3; ++i)
    {
      // Apply the offset to the center of transformation to account for the expected behavior of the ITK transform center.
      // For image registration, the center of the transform is the center of the fixed image.
      // See ITK software guide section 3.6.2.
      double offset_RAS = transformVtk_RAS->GetElement(i, 3);
      center_LocalRAS[i] = center_LocalRAS[i] + offset_RAS;
    }
  }

  return convertedToVtkMatrix;
}

//----------------------------------------------------------------------------
bool SetITKLinearTransformFromVTK(vtkObject* loggerObject, itk::Object::Pointer& transformItk_LPS, vtkMatrix4x4* transformVtk_RAS, double center_LocalRAS[3] /*=nullptr*/)
{
  typedef itk::AffineTransform<double, VTKDimension> AffineTransformType;

  if (transformVtk_RAS == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "SetITKLinearTransformFromVTK failed: invalid input transform");
    return false;
  }

  vtkSmartPointer<vtkMatrix4x4> lps2ras = vtkSmartPointer<vtkMatrix4x4>::New();
  lps2ras->SetElement(0, 0, -1);
  lps2ras->SetElement(1, 1, -1);
  vtkMatrix4x4* ras2lps = lps2ras; // lps2ras is diagonal therefore the inverse is identical

  // Convert from RAS (Slicer) to LPS (ITK)
  //
  // Tlps = ras2lps * Tras * lps2ras
  //
  vtkSmartPointer<vtkMatrix4x4> vtkmat = vtkSmartPointer<vtkMatrix4x4>::New();

  vtkMatrix4x4::Multiply4x4(ras2lps, transformVtk_RAS, vtkmat);
  vtkMatrix4x4::Multiply4x4(vtkmat, lps2ras, vtkmat);

  typedef AffineTransformType::MatrixType MatrixType;
  typedef AffineTransformType::OutputVectorType OffsetType;

  MatrixType itkmat;
  OffsetType itkoffset;

  for (unsigned int i = 0; i < VTKDimension; i++)
  {
    for (unsigned int j = 0; j < VTKDimension; j++)
    {
      itkmat[i][j] = vtkmat->GetElement(i, j);
    }
    itkoffset[i] = vtkmat->GetElement(i, VTKDimension);
  }

  AffineTransformType::Pointer affine = AffineTransformType::New();
  // Matrix and offset are stored last as they represent desired transform of the VTK rotation and translation.
  if (center_LocalRAS)
  {
    // For itk::AffineTransform, the center is not normally specified with matrix/offset, however it can be set explicitly before setting the matrix/offset.
    // When using matrix/offset it is only safe to explicitly set the center of transformation before setting the matrix/offset. Setting the center after
    // the offset would change the contents of the transform.
    // Translation will be recomputed from the offset to represent the specified center of the transform.
    // For more information, see the documentation of itk::AffineTransform::SetCenter.

    // Convert the center from the local RAS to the parent RAS
    double center_LocalLPS[3] = { -center_LocalRAS[0], -center_LocalRAS[1], center_LocalRAS[2] };

    // Account for the offset to the center of transformation to account for the expected behavior of the ITK transform center.
    // For image registration, the center of the transform is the center of the fixed image.
    // See ITK software guide section 3.6.2.
    vtkMath::Subtract(center_LocalLPS, itkoffset, center_LocalLPS);
    affine->SetCenter(center_LocalLPS);
  }
  affine->SetMatrix(itkmat);
  affine->SetOffset(itkoffset);

  transformItk_LPS = affine;
  return true;
}

} // namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLLinearTransformPlugin);

//----------------------------------------------------------------------------
vtkMRMLLinearTransformPlugin::vtkMRMLLinearTransformPlugin() = default;

//----------------------------------------------------------------------------
vtkMRMLLinearTransformPlugin::~vtkMRMLLinearTransformPlugin() = default;

//----------------------------------------------------------------------------
const char* vtkMRMLLinearTransformPlugin::GetTransformClassName()
{
  return "vtkHomogeneousTransform";
}

//----------------------------------------------------------------------------
double vtkMRMLLinearTransformPlugin::CanReadITKTransformClass(const std::string& vtkNotUsed(itkClassName))
{
  return vtkMRMLTransformPlugin::GetFallbackConfidence();
}

//----------------------------------------------------------------------------
std::string vtkMRMLLinearTransformPlugin::GetTransformInfo(vtkAbstractTransform* transform)
{
  vtkHomogeneousTransform* linearTransform = vtkHomogeneousTransform::SafeDownCast(transform);
  if (!linearTransform)
  {
    return std::string();
  }
  std::stringstream ss;
  ss << " Linear";
  vtkMatrix4x4* m = linearTransform->GetMatrix();
  ss << std::endl << "    " << m->GetElement(0, 0) << "  " << m->GetElement(0, 1) << "  " << m->GetElement(0, 2) << "  " << m->GetElement(0, 3);
  ss << std::endl << "    " << m->GetElement(1, 0) << "  " << m->GetElement(1, 1) << "  " << m->GetElement(1, 2) << "  " << m->GetElement(1, 3);
  ss << std::endl << "    " << m->GetElement(2, 0) << "  " << m->GetElement(2, 1) << "  " << m->GetElement(2, 2) << "  " << m->GetElement(2, 3);
  ss << std::endl << "    " << m->GetElement(3, 0) << "  " << m->GetElement(3, 1) << "  " << m->GetElement(3, 2) << "  " << m->GetElement(3, 3);
  // The description starts with the name of the transform type
  std::string info = ss.str();
  return (!info.empty() && info[0] == ' ') ? info.substr(1) : info;
}

//----------------------------------------------------------------------------
itk::Object::Pointer vtkMRMLLinearTransformPlugin::CreateITKTransformFromVTK(vtkObject* loggerObject,
                                                                             vtkAbstractTransform* transformVtk,
                                                                             itk::Object::Pointer& secondaryTransformItk,
                                                                             int preferITKv3CompatibleTransforms,
                                                                             bool initialize,
                                                                             double center_LocalRAS[3])
{
  vtkHomogeneousTransform* linearTransformVtk = vtkHomogeneousTransform::SafeDownCast(transformVtk);
  itk::Object::Pointer transformItk;
  if (!linearTransformVtk || !SetITKLinearTransformFromVTK(loggerObject, transformItk, linearTransformVtk->GetMatrix(), center_LocalRAS))
  {
    return nullptr;
  }
  return transformItk;
}

//----------------------------------------------------------------------------
vtkAbstractTransform* vtkMRMLLinearTransformPlugin::CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItkObject, double center_LocalRAS[3])
{
  // Transform files are read with double precision
  TransformItkType* transformItk = dynamic_cast<TransformItkType*>(transformItkObject);
  if (!transformItk)
  {
    return nullptr;
  }
  vtkNew<vtkMatrix4x4> transformMatrixVtk;
  if (!SetVTKLinearTransformFromITK<double>(loggerObject, transformMatrixVtk, transformItk, center_LocalRAS))
  {
    return nullptr;
  }
  vtkTransform* linearTransformVtk = vtkTransform::New();
  linearTransformVtk->SetMatrix(transformMatrixVtk);
  return linearTransformVtk;
}

//----------------------------------------------------------------------------
bool vtkMRMLLinearTransformPlugin::SetVTKMatrixFromITKLinearTransform(vtkObject* loggerObject,
                                                                      vtkMatrix4x4* transformVtk_RAS,
                                                                      itk::TransformBaseTemplate<double>* transformItk_LPS,
                                                                      double center_LocalRAS[3] /*=nullptr*/)
{
  return SetVTKLinearTransformFromITK<double>(loggerObject, transformVtk_RAS, transformItk_LPS, center_LocalRAS);
}

//----------------------------------------------------------------------------
bool vtkMRMLLinearTransformPlugin::SetITKLinearTransformFromVTKMatrix(vtkObject* loggerObject,
                                                                      itk::Object::Pointer& transformItk_LPS,
                                                                      vtkMatrix4x4* transformVtk_RAS,
                                                                      double center_LocalRAS[3] /*=nullptr*/)
{
  return SetITKLinearTransformFromVTK(loggerObject, transformItk_LPS, transformVtk_RAS, center_LocalRAS);
}
