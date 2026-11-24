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

#include "vtkMRMLGridTransformPlugin.h"

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
#include <itkDisplacementFieldTransform.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>
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
class InverseDisplacementFieldTransform : public DisplacementFieldTransform<TScalar, NDimensions>
{
public:
  /** Standard class typedefs. */
  typedef InverseDisplacementFieldTransform Self;
  typedef DisplacementFieldTransform<TScalar, NDimensions> Superclass;
  typedef SmartPointer<Self> Pointer;
  typedef SmartPointer<const Self> ConstPointer;

  /** Run-time type information (and related methods). */
  itkTypeMacro(InverseDisplacementFieldTransform, DisplacementFieldTransform);

  /** New macro for creation of through a Smart Pointer */
  itkNewMacro(Self);

  /** Define all computation methods as unimplemented to make sure they are not used
  (the inherited implementations are for the forward transform, so they would not give correct results) */
  typename Superclass::OutputPointType TransformPoint(const typename Superclass::InputPointType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
  void ComputeJacobianWithRespectToParameters(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
  void ComputeJacobianWithRespectToParameters(const typename Superclass::IndexType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
#endif
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
  void ComputeJacobianWithRespectToPosition(const typename Superclass::IndexType&, typename Superclass::JacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
#endif
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::InverseJacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
  void GetInverseJacobianOfForwardFieldWithRespectToPosition(const typename Superclass::InputPointType&,
                                                             typename Superclass::JacobianPositionType&,
                                                             bool useSVD = false) const override
  {
    (void)useSVD; // unused
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
  void GetInverseJacobianOfForwardFieldWithRespectToPosition(const typename Superclass::IndexType&, typename Superclass::JacobianPositionType&, bool useSVD = false) const override
  {
    (void)useSVD; // unused
    itkExceptionMacro("Only storage methods are implemented for InverseDisplacementFieldTransform");
  }
};

} // namespace itk

typedef itk::InverseDisplacementFieldTransform<float, 3> InverseDisplacementFieldTransformFloatType;
typedef itk::InverseDisplacementFieldTransform<double, 3> InverseDisplacementFieldTransformDoubleType;

namespace
{
const unsigned int VTKDimension = 3;

typedef itk::DisplacementFieldTransform<double, 3> DisplacementFieldTransformDoubleType;
typedef DisplacementFieldTransformDoubleType::DisplacementFieldType GridImageDoubleType;

typedef itk::TransformBaseTemplate<double> TransformItkType;

// Conversion between the VTK transform and the ITK transform that stores it in a file

template <typename T, unsigned Dimension>
bool SetVTKOrientedGridTransformFromITK(vtkObject* loggerObject, vtkOrientedGridTransform* transformVtk_RAS, typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS);

bool SetITKOrientedGridTransformFromVTK(vtkObject* loggerObject, itk::Object::Pointer& transformItk_LPS, vtkOrientedGridTransform* transformVtk_RAS);

template <typename T>
bool SetVTKOrientedGridTransformFromITKImage(vtkObject* loggerObject,
                                             vtkOrientedGridTransform* grid_Ras,
                                             typename itk::DisplacementFieldTransform<T, 3>::DisplacementFieldType::Pointer gridImage_Lps);

template <typename T>
bool SetVTKOrientedGridTransformFrom2DITKImage(vtkObject* loggerObject,
                                               vtkOrientedGridTransform* grid_Ras,
                                               typename itk::DisplacementFieldTransform<T, 2>::DisplacementFieldType::Pointer gridImage_Lps);

bool SetITKImageFromVTKOrientedGridTransform(vtkObject* loggerObject, GridImageDoubleType::Pointer& gridImage_Lps, vtkOrientedGridTransform* grid_Ras);

//----------------------------------------------------------------------------
template <typename T, unsigned Dimension>
bool SetVTKOrientedGridTransformFromITK(vtkObject* loggerObject, vtkOrientedGridTransform* transformVtk_RAS, typename itk::TransformBaseTemplate<T>::Pointer transformItk_LPS)
{
  typedef itk::DisplacementFieldTransform<T, Dimension> DisplacementFieldTransformType;
  typedef itk::InverseDisplacementFieldTransform<T, Dimension> InverseDisplacementFieldTransformType;

  std::string transformItkClassName = transformItk_LPS->GetNameOfClass();

  bool inverse = false;
  typename DisplacementFieldTransformType::DisplacementFieldType* gridImageItk_Lps = nullptr;
  if (transformItkClassName == "InverseDisplacementFieldTransform") // inverse class is derived from forward class, so it has to be checked first
  {
    DisplacementFieldTransformType* inverseDisplacementFieldTransform = static_cast<InverseDisplacementFieldTransformType*>(transformItk_LPS.GetPointer());
    inverse = true;
    gridImageItk_Lps = inverseDisplacementFieldTransform->GetDisplacementField();
  }
  else if (transformItkClassName == "DisplacementFieldTransform")
  {
    DisplacementFieldTransformType* displacementFieldTransform = static_cast<DisplacementFieldTransformType*>(transformItk_LPS.GetPointer());
    inverse = false;
    gridImageItk_Lps = displacementFieldTransform->GetDisplacementField();
  }
  else
  {
    vtkDebugWithObjectMacro(loggerObject, "Not a grid transform");
    return false;
  }
  if constexpr (Dimension != VTKDimension)
  {
    if (!SetVTKOrientedGridTransformFrom2DITKImage<T>(loggerObject, transformVtk_RAS, gridImageItk_Lps))
    {
      return false;
    }
  }
  else
  {
    if (!SetVTKOrientedGridTransformFromITKImage<T>(loggerObject, transformVtk_RAS, gridImageItk_Lps))
    {
      return false;
    }
  }
  if (inverse)
  {
    transformVtk_RAS->Inverse();
  }
  return true;
}

//----------------------------------------------------------------------------
bool SetITKOrientedGridTransformFromVTK(vtkObject* loggerObject, itk::Object::Pointer& transformItk_LPS, vtkOrientedGridTransform* transformVtk_RAS)
{
  GridImageDoubleType::Pointer gridImageItk_Lps;
  if (!SetITKImageFromVTKOrientedGridTransform(loggerObject, gridImageItk_Lps, transformVtk_RAS))
  {
    vtkErrorWithObjectMacro(loggerObject, "SetITKOrientedGridTransformFromVTK failed: input transform is invalid");
    return false;
  }
  // Update is needed because it refreshes the inverse flag (the flag may be out-of-date if the transform depends on its inverse)
  transformVtk_RAS->Update();
  if (transformVtk_RAS->GetInverseFlag())
  {
    InverseDisplacementFieldTransformDoubleType::Pointer gridTransformItk = InverseDisplacementFieldTransformDoubleType::New();
    gridTransformItk->SetDisplacementField(gridImageItk_Lps);
    transformItk_LPS = gridTransformItk;
  }
  else
  {
    DisplacementFieldTransformDoubleType::Pointer gridTransformItk = DisplacementFieldTransformDoubleType::New();
    gridTransformItk->SetDisplacementField(gridImageItk_Lps);
    transformItk_LPS = gridTransformItk;
  }
  return true;
}

//----------------------------------------------------------------------------
template <typename T>
bool SetVTKOrientedGridTransformFromITKImage(vtkObject* loggerObject,
                                             vtkOrientedGridTransform* grid_Ras,
                                             typename itk::DisplacementFieldTransform<T, 3>::DisplacementFieldType::Pointer gridImage_Lps)
{
  typedef itk::DisplacementFieldTransform<T, 3> DisplacementFieldTransformType;
  typedef typename DisplacementFieldTransformType::DisplacementFieldType GridImageType;

  vtkNew<vtkImageData> gridImage_Ras;

  // Origin
  gridImage_Ras->SetOrigin(-gridImage_Lps->GetOrigin()[0], -gridImage_Lps->GetOrigin()[1], gridImage_Lps->GetOrigin()[2]);

  // Spacing
  gridImage_Ras->SetSpacing(gridImage_Lps->GetSpacing()[0], gridImage_Lps->GetSpacing()[1], gridImage_Lps->GetSpacing()[2]);

  // Direction
  vtkNew<vtkMatrix4x4> gridDirectionMatrix_LPS;
  for (unsigned int row = 0; row < VTKDimension; row++)
  {
    for (unsigned int column = 0; column < VTKDimension; column++)
    {
      gridDirectionMatrix_LPS->SetElement(row, column, gridImage_Lps->GetDirection()(row, column));
    }
  }
  vtkNew<vtkMatrix4x4> lpsToRas;
  lpsToRas->SetElement(0, 0, -1);
  lpsToRas->SetElement(1, 1, -1);
  vtkNew<vtkMatrix4x4> gridDirectionMatrix_RAS;
  vtkMatrix4x4::Multiply4x4(lpsToRas.GetPointer(), gridDirectionMatrix_LPS.GetPointer(), gridDirectionMatrix_RAS.GetPointer());
  grid_Ras->SetGridDirectionMatrix(gridDirectionMatrix_RAS.GetPointer());

  // Vectors
  typename GridImageType::SizeType size = gridImage_Lps->GetBufferedRegion().GetSize();
  gridImage_Ras->SetDimensions(size[0], size[1], size[2]);
  unsigned int numberOfScalarComponents = GridImageType::PixelType::Dimension;
  if (numberOfScalarComponents != VTKDimension)
  {
    vtkErrorWithObjectMacro(loggerObject,
                            "Cannot load grid transform: the input displacement field expected to contain " << VTKDimension << " components but it actually contains "
                                                                                                            << numberOfScalarComponents);
    return false;
  }
  gridImage_Ras->AllocateScalars(VTK_DOUBLE, 3);

  double* displacementVectors_Ras = reinterpret_cast<double*>(gridImage_Ras->GetScalarPointer());
  itk::ImageRegionConstIterator<GridImageType> inputIt(gridImage_Lps, gridImage_Lps->GetRequestedRegion());
  while (!inputIt.IsAtEnd())
  {
    typename GridImageType::PixelType displacementVectorLps = inputIt.Get();
    *(displacementVectors_Ras++) = -displacementVectorLps[0];
    *(displacementVectors_Ras++) = -displacementVectorLps[1];
    *(displacementVectors_Ras++) = displacementVectorLps[2];
    ++inputIt;
  }

  grid_Ras->SetDisplacementGridData(gridImage_Ras.GetPointer());

  // Set the interpolation to cubic to have smooth derivatives
  grid_Ras->SetInterpolationModeToCubic();

  return true;
}

//----------------------------------------------------------------------------
template <typename T>
bool SetVTKOrientedGridTransformFrom2DITKImage(vtkObject* loggerObject,
                                               vtkOrientedGridTransform* grid_Ras,
                                               typename itk::DisplacementFieldTransform<T, 2>::DisplacementFieldType::Pointer gridImage_Lps)
{
  typedef itk::DisplacementFieldTransform<T, 2> DisplacementFieldTransformType;
  typedef typename DisplacementFieldTransformType::DisplacementFieldType GridImageType;

  vtkNew<vtkImageData> gridImage_Ras;

  // Origin (convert LPS -> RAS for X and Y, set Z to 0)
  const auto& itkOrigin = gridImage_Lps->GetOrigin();
  double originRasX = -itkOrigin[0];
  double originRasY = -itkOrigin[1];
  double originRasZ = 0.0;
  gridImage_Ras->SetOrigin(originRasX, originRasY, originRasZ);

  // Spacing (X,Y from ITK, Z=geometric mean)
  const auto& itkSpacing = gridImage_Lps->GetSpacing();
  gridImage_Ras->SetSpacing(itkSpacing[0], itkSpacing[1], std::sqrt(itkSpacing[0] * itkSpacing[1]));

  itk::Matrix<itk::SpacePrecisionType, 3, 3> direction3d = vtkMRMLTransformPlugin::Matrix2Dto3D(gridImage_Lps->GetDirection());

  vtkNew<vtkMatrix4x4> gridDirectionMatrix_LPS;
  for (unsigned int row = 0; row < 3; ++row)
  {
    for (unsigned int col = 0; col < 3; ++col)
    {
      gridDirectionMatrix_LPS->SetElement(row, col, direction3d(row, col));
    }
  }

  // Convert direction from LPS to RAS
  vtkNew<vtkMatrix4x4> lpsToRas;
  lpsToRas->SetElement(0, 0, -1);
  lpsToRas->SetElement(1, 1, -1);
  vtkNew<vtkMatrix4x4> gridDirectionMatrix_RAS;
  vtkMatrix4x4::Multiply4x4(lpsToRas.GetPointer(), gridDirectionMatrix_LPS.GetPointer(), gridDirectionMatrix_RAS.GetPointer());
  grid_Ras->SetGridDirectionMatrix(gridDirectionMatrix_RAS.GetPointer());

  // Expand from 2->3 components
  typename GridImageType::SizeType size = gridImage_Lps->GetBufferedRegion().GetSize();
  int dimX = static_cast<int>(size[0]);
  int dimY = static_cast<int>(size[1]);
  gridImage_Ras->SetDimensions(dimX, dimY, 1);

  unsigned int numberOfScalarComponents = GridImageType::PixelType::Dimension;
  if (numberOfScalarComponents != 2)
  {
    vtkErrorWithObjectMacro(loggerObject,
                            "Cannot load 2D grid transform: the input displacement field expected to contain 2 components but it actually contains " << numberOfScalarComponents);
    return false;
  }

  if constexpr (std::is_same_v<T, float>)
  {
    gridImage_Ras->AllocateScalars(VTK_FLOAT, 3);
  }
  else if constexpr (std::is_same_v<T, double>)
  {
    gridImage_Ras->AllocateScalars(VTK_DOUBLE, 3);
  }
  else
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot load 2D grid transform: unsupported pixel type. Only float and double are supported.");
    return false;
  }

  // Vectors: copy displacement vectors, converting LPS -> RAS and expand from 2->3 components
  T* displacementVectors_Ras = reinterpret_cast<T*>(gridImage_Ras->GetScalarPointer());
  itk::ImageRegionConstIterator<GridImageType> inputIt(gridImage_Lps, gridImage_Lps->GetRequestedRegion());
  while (!inputIt.IsAtEnd())
  {
    typename GridImageType::PixelType displacementVectorLps = inputIt.Get();
    // Convert LPS -> RAS for X and Y, Z is zero for 2D
    *(displacementVectors_Ras++) = -static_cast<T>(displacementVectorLps[0]);
    *(displacementVectors_Ras++) = -static_cast<T>(displacementVectorLps[1]);
    *(displacementVectors_Ras++) = 0.0;
    ++inputIt;
  }

  grid_Ras->SetDisplacementGridData(gridImage_Ras.GetPointer());

  // Use cubic interpolation for smooth derivatives
  grid_Ras->SetInterpolationModeToCubic();

  return true;
}

//----------------------------------------------------------------------------
bool SetITKImageFromVTKOrientedGridTransform(vtkObject* loggerObject, GridImageDoubleType::Pointer& gridImage_Lps, vtkOrientedGridTransform* grid_Ras)
{
  if (grid_Ras == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot save grid transform: the input vtkOrientedGridTransform is invalid");
    return false;
  }

  // Update is needed because DisplacementGrid may be out-of-date if the transform depends on its inverse
  grid_Ras->Update();

  vtkImageData* gridImage_Ras = grid_Ras->GetDisplacementGrid();
  if (gridImage_Ras == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot save grid transform: the input vtkOrientedGridTransform does not contain a valid displacement grid");
    return false;
  }
  if (gridImage_Ras->GetNumberOfScalarComponents() != static_cast<int>(VTKDimension))
  {
    vtkErrorWithObjectMacro(loggerObject,
                            "Cannot save grid transform: the input vtkOrientedGridTransform expected to contain " << VTKDimension << " components but it actually contains "
                                                                                                                  << gridImage_Ras->GetNumberOfScalarComponents());
    return false;
  }

  gridImage_Lps = GridImageDoubleType::New();

  // Origin
  double* origin_Ras = gridImage_Ras->GetOrigin();
  double origin_Lps[3] = { -origin_Ras[0], -origin_Ras[1], origin_Ras[2] };
  gridImage_Lps->SetOrigin(origin_Lps);

  // Spacing
  double* spacing = gridImage_Ras->GetSpacing();
  // GridType::SpacingType spacing( spacing );
  gridImage_Lps->SetSpacing(spacing);

  // Direction
  vtkNew<vtkMatrix4x4> gridDirectionMatrix_Ras;
  if (grid_Ras->GetGridDirectionMatrix() != nullptr)
  {
    gridDirectionMatrix_Ras->DeepCopy(grid_Ras->GetGridDirectionMatrix());
  }
  vtkNew<vtkMatrix4x4> rasToLps;
  rasToLps->SetElement(0, 0, -1);
  rasToLps->SetElement(1, 1, -1);
  vtkNew<vtkMatrix4x4> gridDirectionMatrix_Lps;
  vtkMatrix4x4::Multiply4x4(rasToLps.GetPointer(), gridDirectionMatrix_Ras.GetPointer(), gridDirectionMatrix_Lps.GetPointer());
  GridImageDoubleType::DirectionType gridDirectionMatrixItk_Lps;
  for (unsigned int row = 0; row < VTKDimension; row++)
  {
    for (unsigned int column = 0; column < VTKDimension; column++)
    {
      gridDirectionMatrixItk_Lps(row, column) = gridDirectionMatrix_Lps->GetElement(row, column);
    }
  }
  gridImage_Lps->SetDirection(gridDirectionMatrixItk_Lps);

  // Vectors
  GridImageDoubleType::IndexType start;
  start[0] = start[1] = start[2] = 0;
  int* Nijk = gridImage_Ras->GetDimensions();
  GridImageDoubleType::SizeType size;
  size[0] = Nijk[0];
  size[1] = Nijk[1];
  size[2] = Nijk[2];
  GridImageDoubleType::RegionType region;
  region.SetSize(size);
  region.SetIndex(start);
  gridImage_Lps->SetRegions(region);
  gridImage_Lps->Allocate();
  itk::ImageRegionIterator<GridImageDoubleType> gridImageIt_Lps(gridImage_Lps, region);
  gridImageIt_Lps.GoToBegin();
  GridImageDoubleType::PixelType displacementVectorLps;
  double displacementScale = grid_Ras->GetDisplacementScale();
  double displacementShift = grid_Ras->GetDisplacementShift();

  if (gridImage_Ras->GetScalarType() == VTK_DOUBLE)
  {
    double* displacementVectors_Ras = reinterpret_cast<double*>(gridImage_Ras->GetScalarPointer());
    while (!gridImageIt_Lps.IsAtEnd())
    {
      displacementVectorLps[0] = -(displacementScale * (*(displacementVectors_Ras++)) + displacementShift);
      displacementVectorLps[1] = -(displacementScale * (*(displacementVectors_Ras++)) + displacementShift);
      displacementVectorLps[2] = (displacementScale * (*(displacementVectors_Ras++)) + displacementShift);
      gridImageIt_Lps.Set(displacementVectorLps);
      ++gridImageIt_Lps;
    }
  }
  else if (gridImage_Ras->GetScalarType() == VTK_FLOAT)
  {
    float* displacementVectors_Ras = reinterpret_cast<float*>(gridImage_Ras->GetScalarPointer());
    while (!gridImageIt_Lps.IsAtEnd())
    {
      displacementVectorLps[0] = -(displacementScale * (*(displacementVectors_Ras++)) + displacementShift);
      displacementVectorLps[1] = -(displacementScale * (*(displacementVectors_Ras++)) + displacementShift);
      displacementVectorLps[2] = (displacementScale * (*(displacementVectors_Ras++)) + displacementShift);
      gridImageIt_Lps.Set(displacementVectorLps);
      ++gridImageIt_Lps;
    }
  }
  else
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot save grid transform: only float and double scalar types are supported");
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
/// Register the ITK transform classes of this transform type that ITK does not register by default, so that
/// transform files that contain them can be read
bool RegisterITKTransformClasses()
{
  itk::TransformFactory<InverseDisplacementFieldTransformFloatType>::RegisterTransform();
  itk::TransformFactory<InverseDisplacementFieldTransformDoubleType>::RegisterTransform();
  return true;
}

} // namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLGridTransformPlugin);

//----------------------------------------------------------------------------
vtkMRMLGridTransformPlugin::vtkMRMLGridTransformPlugin()
{
  static const bool itkTransformClassesRegistered = RegisterITKTransformClasses();
  (void)itkTransformClassesRegistered;
}

//----------------------------------------------------------------------------
vtkMRMLGridTransformPlugin::~vtkMRMLGridTransformPlugin() = default;

//----------------------------------------------------------------------------
const char* vtkMRMLGridTransformPlugin::GetTransformClassName()
{
  return "vtkGridTransform";
}

//----------------------------------------------------------------------------
double vtkMRMLGridTransformPlugin::CanReadITKTransformClass(const std::string& vtkNotUsed(itkClassName))
{
  return vtkMRMLTransformPlugin::GetFallbackConfidence();
}

//----------------------------------------------------------------------------
std::string vtkMRMLGridTransformPlugin::GetTransformInfo(vtkAbstractTransform* transform)
{
  vtkGridTransform* gridTransform = vtkGridTransform::SafeDownCast(transform);
  if (!gridTransform)
  {
    return std::string();
  }
  std::stringstream ss;
  ss << " Displacement field:";
  gridTransform->Update(); // compute if inverse
  vtkImageData* displacementField = gridTransform->GetDisplacementGrid();
  if (displacementField != nullptr)
  {
    int* extent = displacementField->GetExtent();
    ss << std::endl << "  Grid size: " << (extent[1] - extent[0] + 1) << " " << (extent[3] - extent[2] + 1) << " " << (extent[5] - extent[4] + 1);
    double* origin = displacementField->GetOrigin();
    ss << std::endl << "  Grid origin: " << origin[0] << " " << origin[1] << " " << origin[2];
    double* spacing = displacementField->GetSpacing();
    ss << std::endl << "  Grid spacing: " << spacing[0] << " " << spacing[1] << " " << spacing[2];
    vtkOrientedGridTransform* orientedGridTransform = vtkOrientedGridTransform::SafeDownCast(transform);
    if (orientedGridTransform != nullptr)
    {
      vtkMatrix4x4* gridOrientation = orientedGridTransform->GetGridDirectionMatrix();
      if (gridOrientation != nullptr)
      {
        ss << std::endl << "  Grid orientation:";
        for (int i = 0; i < 3; i++)
        {
          ss << std::endl << "    " << gridOrientation->GetElement(i, 0) << "  " << gridOrientation->GetElement(i, 1) << "  " << gridOrientation->GetElement(i, 2);
        }
      }
    }
  }
  else
  {
    ss << std::endl << "  Displacement field is invalid.";
  }
  if (gridTransform->GetInverseFlag())
  {
    ss << std::endl << "  Computed from its inverse.";
  }
  // The description starts with the name of the transform type
  std::string info = ss.str();
  return (!info.empty() && info[0] == ' ') ? info.substr(1) : info;
}

//----------------------------------------------------------------------------
bool vtkMRMLGridTransformPlugin::DeepCopyTransform(vtkAbstractTransform* dst, vtkAbstractTransform* src)
{
  // Fix up the DeepCopy for vtkGridTransform (it performs only a ShallowCopy on the displacement grid)
  dst->DeepCopy(src);
  if (vtkGridTransform::SafeDownCast(dst) == nullptr)
  {
    vtkGenericWarningMacro("vtkMRMLTransformNode::DeepCopyTransform failed: destination transform has to be vtkGridTransform");
    return false;
  }
  vtkImageData* srcDisplacementGrid = vtkGridTransform::SafeDownCast(src)->GetDisplacementGrid();
  if (srcDisplacementGrid)
  {
    vtkNew<vtkImageData> dstDisplacementGrid;
    dstDisplacementGrid->DeepCopy(srcDisplacementGrid);
    vtkGridTransform::SafeDownCast(dst)->SetDisplacementGridData(dstDisplacementGrid.GetPointer());
  }
  return true;
}

//----------------------------------------------------------------------------
itk::Object::Pointer vtkMRMLGridTransformPlugin::CreateITKTransformFromVTK(vtkObject* loggerObject,
                                                                           vtkAbstractTransform* transformVtk,
                                                                           itk::Object::Pointer& secondaryTransformItk,
                                                                           int preferITKv3CompatibleTransforms,
                                                                           bool initialize,
                                                                           double center_LocalRAS[3])
{
  vtkOrientedGridTransform* gridTransformVtk = vtkOrientedGridTransform::SafeDownCast(transformVtk);
  itk::Object::Pointer transformItk;
  if (!gridTransformVtk || !SetITKOrientedGridTransformFromVTK(loggerObject, transformItk, gridTransformVtk))
  {
    return nullptr;
  }
  return transformItk;
}

//----------------------------------------------------------------------------
vtkAbstractTransform* vtkMRMLGridTransformPlugin::CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItkObject, double center_LocalRAS[3])
{
  // Transform files are read with double precision
  TransformItkType* transformItk = dynamic_cast<TransformItkType*>(transformItkObject);
  if (!transformItk)
  {
    return nullptr;
  }
  const unsigned int itkDim = transformItk->GetOutputSpaceDimension();
  vtkNew<vtkOrientedGridTransform> gridTransformVtk;
  bool success = false;
  if (itkDim == 3)
  {
    success = SetVTKOrientedGridTransformFromITK<double, 3>(loggerObject, gridTransformVtk, transformItk);
  }
  else if (itkDim == 2)
  {
    success = SetVTKOrientedGridTransformFromITK<double, 2>(loggerObject, gridTransformVtk, transformItk);
  }
  if (!success)
  {
    return nullptr;
  }
  gridTransformVtk->Register(nullptr);
  return gridTransformVtk;
}

//----------------------------------------------------------------------------
bool vtkMRMLGridTransformPlugin::SetVTKOrientedGridTransformFromITKImage(vtkObject* loggerObject, vtkOrientedGridTransform* grid_Ras, GridImageType::Pointer gridImage_Lps)
{
  return ::SetVTKOrientedGridTransformFromITKImage<double>(loggerObject, grid_Ras, gridImage_Lps);
}

//----------------------------------------------------------------------------
bool vtkMRMLGridTransformPlugin::SetITKImageFromVTKOrientedGridTransform(vtkObject* loggerObject, GridImageType::Pointer& gridImage_Lps, vtkOrientedGridTransform* grid_Ras)
{
  return ::SetITKImageFromVTKOrientedGridTransform(loggerObject, gridImage_Lps, grid_Ras);
}
