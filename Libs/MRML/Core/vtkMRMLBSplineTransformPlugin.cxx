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

#include "vtkMRMLBSplineTransformPlugin.h"
#include "vtkMRMLLinearTransformPlugin.h"

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
#include <itkAffineTransform.h>
#include <itkBSplineDeformableTransform.h>
#include <itkBSplineTransform.h>
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

template <typename TScalar = double, unsigned int NDimensions = 3, unsigned int VSplineOrder = 3>
class InverseBSplineTransform : public BSplineTransform<TScalar, NDimensions, VSplineOrder>
{
public:
  /** Standard class typedefs. */
  typedef InverseBSplineTransform Self;
  typedef BSplineTransform<TScalar, NDimensions, VSplineOrder> Superclass;
  typedef SmartPointer<Self> Pointer;
  typedef SmartPointer<const Self> ConstPointer;

  /** Run-time type information (and related methods). */
  itkTypeMacro(InverseBSplineTransform, BSplineTransform);

  /** New macro for creation of through a Smart Pointer */
  itkNewMacro(Self);

  /** Define all computation methods as unimplemented to make sure they are not used
  (the inherited implementations are for the forward transform, so they would not give correct results) */
  using Superclass::TransformPoint;
  typename Superclass::OutputPointType TransformPoint(const typename Superclass::InputPointType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
  void ComputeJacobianWithRespectToParameters(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#endif
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#endif
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::InverseJacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
  void ComputeJacobianFromBSplineWeightsWithRespectToPosition(const typename Superclass::InputPointType&,
                                                              typename Superclass::WeightsType&,
                                                              typename Superclass::ParameterIndexArrayType&) const
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
};

template <typename TScalar = double, unsigned int NDimensions = 3, unsigned int VSplineOrder = 3>
class InverseBSplineDeformableTransform : public BSplineDeformableTransform<TScalar, NDimensions, VSplineOrder>
{
public:
  /** Standard class typedefs. */
  typedef InverseBSplineDeformableTransform Self;
  typedef BSplineDeformableTransform<TScalar, NDimensions, VSplineOrder> Superclass;
  typedef SmartPointer<Self> Pointer;
  typedef SmartPointer<const Self> ConstPointer;

  /** Run-time type information (and related methods). */
  itkTypeMacro(InverseBSplineDeformableTransform, BSplineDeformableTransform);

  /** New macro for creation of through a Smart Pointer */
  itkNewMacro(Self);

  /** Define all computation methods as unimplemented to make sure they are not used
  (the inherited implementations are for the forward transform, so they would not give correct results) */
  using Superclass::TransformPoint;
  typename Superclass::OutputPointType TransformPoint(const typename Superclass::InputPointType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
  void ComputeJacobianWithRespectToParameters(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#endif
  void ComputeJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#if !defined(ITK_LEGACY_REMOVE)
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::JacobianType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
#endif
  void ComputeInverseJacobianWithRespectToPosition(const typename Superclass::InputPointType&, typename Superclass::InverseJacobianPositionType&) const override
  {
    itkExceptionMacro("Only storage methods are implemented for InverseBSplineTransform");
  }
};

} // namespace itk

// 3D, cubic B-spline transforms
typedef itk::InverseBSplineDeformableTransform<float, 3, 3> InverseBSplineTransformFloatITKv3Type;
typedef itk::InverseBSplineDeformableTransform<double, 3, 3> InverseBSplineTransformDoubleITKv3Type;
typedef itk::InverseBSplineTransform<float, 3, 3> InverseBSplineTransformFloatITKv4Type;
typedef itk::InverseBSplineTransform<double, 3, 3> InverseBSplineTransformDoubleITKv4Type;

namespace
{
const unsigned int VTKDimension = 3;
const int BSPLINE_TRANSFORM_ORDER = 3;

typedef itk::TransformBaseTemplate<double> TransformItkType;

// Conversion between the VTK transform and the ITK transform that stores it in a file

bool IsIdentityMatrix(vtkMatrix4x4* matrix);

template <typename BSplineTransformType>
bool SetVTKBSplineParametersFromITKGeneric(vtkObject* loggerObject,
                                           vtkOrientedBSplineTransform* bsplineVtk,
                                           typename itk::TransformBaseTemplate<typename BSplineTransformType::ScalarType>::Pointer warpTransformItk);

template <typename T>
bool SetVTKBSplineFromITKv3Generic(vtkObject* loggerObject,
                                   vtkOrientedBSplineTransform* bsplineVtk,
                                   typename itk::TransformBaseTemplate<T>::Pointer warpTransformItk,
                                   typename itk::TransformBaseTemplate<T>::Pointer bulkTransformItk);

template <typename T>
bool SetVTKBSplineFromITKv4Generic(vtkObject* loggerObject, vtkOrientedBSplineTransform* bsplineVtk, typename itk::TransformBaseTemplate<T>::Pointer warpTransformItk);

template <typename BSplineTransformType>
bool SetITKBSplineParametersFromVTKGeneric(vtkObject* loggerObject,
                                           typename itk::Transform<typename BSplineTransformType::ScalarType, VTKDimension, VTKDimension>::Pointer& warpTransformItk,
                                           vtkOrientedBSplineTransform* bsplineVtk);

template <typename T>
bool SetITKv3BSplineFromVTKGeneric(vtkObject* loggerObject,
                                   typename itk::Transform<T, VTKDimension, VTKDimension>::Pointer& warpTransformItk,
                                   typename itk::Transform<T, VTKDimension, VTKDimension>::Pointer& bulkTransformItk,
                                   vtkOrientedBSplineTransform* bsplineVtk,
                                   bool alwaysAddBulkTransform);

template <typename T>
bool SetITKv4BSplineFromVTKGeneric(vtkObject* loggerObject,
                                   typename itk::Transform<T, VTKDimension, VTKDimension>::Pointer& warpTransformItk,
                                   vtkOrientedBSplineTransform* bsplineVtk);

bool SetITKv3BSplineFromVTK(vtkObject* loggerObject,
                            itk::Object::Pointer& warpTransformItk,
                            itk::Object::Pointer& bulkTransformItk,
                            vtkOrientedBSplineTransform* bsplineVtk,
                            bool alwaysAddBulkTransform);

bool SetITKv4BSplineFromVTK(vtkObject* loggerObject, itk::Object::Pointer& warpTransformItk, vtkOrientedBSplineTransform* bsplineVtk);

//----------------------------------------------------------------------------
bool IsIdentityMatrix(vtkMatrix4x4* matrix)
{
  static double identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
  int i, j;

  for (i = 0; i < 4; i++)
  {
    for (j = 0; j < 4; j++)
    {
      if (matrix->GetElement(i, j) != identity[4 * i + j])
      {
        return false;
      }
    }
  }
  return true;
}

//----------------------------------------------------------------------------
template <typename BSplineTransformType>
bool SetVTKBSplineParametersFromITKGeneric(vtkObject* loggerObject,
                                           vtkOrientedBSplineTransform* bsplineVtk,
                                           typename itk::TransformBaseTemplate<typename BSplineTransformType::ScalarType>::Pointer warpTransformItk)
{
  //
  // this version uses the itk::BSplineTransform not the itk::BSplineDeformableTransform
  //
  typedef typename BSplineTransformType::ScalarType T;
  if (bsplineVtk == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "vtkMRMLTransformStorageNode::SetVTKBSplineFromITKv4 failed: bsplineVtk is invalid");
    return false;
  }
  bool isDoublePrecisionInput = true;
  if (sizeof(T) == sizeof(float))
  {
    isDoublePrecisionInput = false;
  }
  else if (sizeof(T) == sizeof(double))
  {
    isDoublePrecisionInput = true;
  }
  else
  {
    vtkErrorWithObjectMacro(loggerObject, "Unsupported scalar type in BSpline transform file (only float and double are supported)");
    return false;
  }

  typename BSplineTransformType::Pointer bsplineItk = BSplineTransformType::New();
  std::string warpTransformItkName = warpTransformItk->GetNameOfClass();
  std::string requestedWarpTransformItkName = bsplineItk->GetNameOfClass();
  if (warpTransformItkName != requestedWarpTransformItkName)
  {
    return false;
  }
  const unsigned itkDim = warpTransformItk->GetOutputSpaceDimension();
  constexpr unsigned instDim = BSplineTransformType::SpaceDimension;
  if (itkDim != instDim)
  {
    vtkErrorWithObjectMacro(loggerObject, "Actual and instantiated dimensionality differ: actual = " << itkDim << ", instantiated = " << instDim);
    return false;
  }
  bsplineItk = static_cast<BSplineTransformType*>(warpTransformItk.GetPointer());

  // now get the fixed parameters and map them to the vtk analogs

  // it turns out that for a BSplineTransform, the TransformDomain information
  // is not populated when reading from a file, so instead we access these
  // fields from one of the coefficient images (they are assumed to be
  // identical)
  const typename BSplineTransformType::CoefficientImageArray coefficientImages = bsplineItk->GetCoefficientImages();

  // * mesh size X, Y, Z (including the BSPLINE_TRANSFORM_ORDER=3 boundary nodes,
  //   1 before and 2 after the grid)
  typename BSplineTransformType::MeshSizeType meshSize = coefficientImages[0]->GetLargestPossibleRegion().GetSize();

  // * mesh origin X, Y, Z (position of the boundary node before the grid)
  typename BSplineTransformType::OriginType origin = coefficientImages[0]->GetOrigin();

  // * mesh spacing X, Y, Z
  typename BSplineTransformType::SpacingType spacing = coefficientImages[0]->GetSpacing();

  // * mesh direction 3x3 matrix (first row, second row, third row)
  typename BSplineTransformType::DirectionType direction = coefficientImages[0]->GetDirection();

  itk::Matrix<T, 3, 3> direction3d;
  if constexpr (instDim == 2)
  {
    direction3d = vtkMRMLTransformPlugin::Matrix2Dto3D(direction);
  }
  else
  {
    direction3d = direction;
  }

  vtkNew<vtkMatrix4x4> gridDirectionMatrix_LPS;
  for (unsigned int row = 0; row < VTKDimension; row++)
  {
    for (unsigned int column = 0; column < VTKDimension; column++)
    {
      gridDirectionMatrix_LPS->SetElement(row, column, direction3d[row][column]);
    }
  }

  // Set bspline grid and coefficients
  bsplineVtk->SetBorderModeToZero();

  vtkNew<vtkImageData> bsplineCoefficients;

  if constexpr (instDim == 2)
  {
    bsplineCoefficients->SetExtent(0, meshSize[0] - 1, 0, meshSize[1] - 1, 0, 0);
    bsplineCoefficients->SetSpacing(spacing[0], spacing[1], std::sqrt(spacing[0] * spacing[1]));
  }
  else
  {
    bsplineCoefficients->SetExtent(0, meshSize[0] - 1, 0, meshSize[1] - 1, 0, meshSize[2] - 1);
    bsplineCoefficients->SetSpacing(spacing[0], spacing[1], spacing[2]);
  }

  // convert the direction matrix from LPS (itk) to RAS (slicer)
  vtkNew<vtkMatrix4x4> lpsToRas;
  lpsToRas->SetElement(0, 0, -1);
  lpsToRas->SetElement(1, 1, -1);

  vtkNew<vtkMatrix4x4> rasToLps;
  rasToLps->SetElement(0, 0, -1);
  rasToLps->SetElement(1, 1, -1);

  vtkNew<vtkMatrix4x4> gridDirectionMatrix_RAS;
  vtkMatrix4x4::Multiply4x4(lpsToRas.GetPointer(), gridDirectionMatrix_LPS.GetPointer(), gridDirectionMatrix_RAS.GetPointer());
  bsplineVtk->SetGridDirectionMatrix(gridDirectionMatrix_RAS.GetPointer());

  // convert the origin from LPS (itk) to RAS (slicer)
  double gridOrigin_RAS[3] = { -origin[0], -origin[1], 0 };
  if constexpr (instDim == 3)
  {
    gridOrigin_RAS[2] = origin[2];
  }
  bsplineCoefficients->SetOrigin(gridOrigin_RAS);

  int bsplineCoefficientsScalarType = VTK_FLOAT;
  if (isDoublePrecisionInput)
  {
    bsplineCoefficientsScalarType = VTK_DOUBLE;
  }

  bsplineCoefficients->AllocateScalars(bsplineCoefficientsScalarType, 3);

  const unsigned int expectedNumberOfVectors = meshSize.CalculateProductOfElements();
  const unsigned int expectedNumberOfParameters = expectedNumberOfVectors * instDim;
  const unsigned int actualNumberOfParameters = bsplineItk->GetNumberOfParameters();

  if (actualNumberOfParameters != expectedNumberOfParameters)
  {
    vtkErrorWithObjectMacro(loggerObject, "Mismatch in number of BSpline parameters in the transform file and the MRML node");
    return false;
  }
  const T* itkBSplineParams_LPS = static_cast<const T*>(bsplineItk->GetParameters().data_block());
  T* vtkBSplineParams_RAS = static_cast<T*>(bsplineCoefficients->GetScalarPointer());
  for (unsigned int i = 0; i < expectedNumberOfVectors; i++)
  {
    *(vtkBSplineParams_RAS++) = -(*(itkBSplineParams_LPS));
    *(vtkBSplineParams_RAS++) = -(*(itkBSplineParams_LPS + expectedNumberOfVectors));
    if constexpr (instDim == 3)
    {
      *(vtkBSplineParams_RAS++) = (*(itkBSplineParams_LPS + expectedNumberOfVectors * 2));
    }
    else // 2D
    {
      *(vtkBSplineParams_RAS++) = 0; // Z component
    }
    itkBSplineParams_LPS++;
  }
  bsplineVtk->SetCoefficientData(bsplineCoefficients.GetPointer());

  // Success
  return true;
}

//----------------------------------------------------------------------------
template <typename T>
bool SetVTKBSplineFromITKv3Generic(vtkObject* loggerObject,
                                   vtkOrientedBSplineTransform* bsplineVtk,
                                   typename itk::TransformBaseTemplate<T>::Pointer warpTransformItk,
                                   typename itk::TransformBaseTemplate<T>::Pointer bulkTransformItk)
{
  if (bsplineVtk == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "vtkMRMLTransformStorageNode::SetVTKBSplineFromITK failed: bsplineVtk is invalid");
    return false;
  }

  const unsigned int itkDim = warpTransformItk->GetOutputSpaceDimension();
  if (itkDim < 2 || itkDim > 3)
  {
    vtkErrorWithObjectMacro(loggerObject, "Unsupported number of dimensions in BSpline transform file (expected = 2 or 3, actual = " << itkDim << ")");
    return false;
  }
  bool inverse = false;
  if (itkDim == 2)
  {
    // inverse class is derived from forward class, so it has to be checked first
    if (SetVTKBSplineParametersFromITKGeneric<itk::InverseBSplineDeformableTransform<T, 2, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = true;
    }
    else if (SetVTKBSplineParametersFromITKGeneric<itk::BSplineDeformableTransform<T, 2, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = false;
    }
    else
    {
      vtkDebugWithObjectMacro(loggerObject, "Not an ITKv3 BSpline transform");
      return false;
    }
  }
  else
  {
    // inverse class is derived from forward class, so it has to be checked first
    if (SetVTKBSplineParametersFromITKGeneric<itk::InverseBSplineDeformableTransform<T, 3, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = true;
    }
    else if (SetVTKBSplineParametersFromITKGeneric<itk::BSplineDeformableTransform<T, 3, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = false;
    }
    else
    {
      vtkDebugWithObjectMacro(loggerObject, "Not an ITKv3 BSpline transform");
      return false;
    }
  }

  // Set the bulk transform
  if (bulkTransformItk)
  {
    std::string bulkTransformItkTransformName = bulkTransformItk->GetNameOfClass();

    if (bulkTransformItkTransformName == "AffineTransform")
    {
      vtkNew<vtkMatrix4x4> bulkMatrix_RAS;
      if (!vtkMRMLLinearTransformPlugin::SetVTKMatrixFromITKLinearTransform(loggerObject, bulkMatrix_RAS, bulkTransformItk))
      {
        vtkErrorWithObjectMacro(loggerObject, "Cannot read the bulk transform in BSplineTransform");
        return false;
      }
      bsplineVtk->SetBulkTransformMatrix(bulkMatrix_RAS.GetPointer());
    }
    else if (bulkTransformItkTransformName == "IdentityTransform")
    {
      // bulk transform is identity, which is equivalent to no bulk transform
    }
    else
    {
      vtkErrorWithObjectMacro(loggerObject, "Cannot read the 2nd transform in BSplineTransform (expected AffineTransform_double_3_3 or IdentityTransform)");
      return false;
    }
  }

  if (inverse)
  {
    bsplineVtk->Inverse();
  }

  // Success
  return true;
}

//----------------------------------------------------------------------------
template <typename T>
bool SetVTKBSplineFromITKv4Generic(vtkObject* loggerObject, vtkOrientedBSplineTransform* bsplineVtk, typename itk::TransformBaseTemplate<T>::Pointer warpTransformItk)
{
  bool inverse = false;
  const unsigned int itkDim = warpTransformItk->GetOutputSpaceDimension();

  if (itkDim == 2)
  {
    // inverse class is derived from forward class, so it has to be checked first
    if (SetVTKBSplineParametersFromITKGeneric<itk::InverseBSplineTransform<T, 2, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = true;
    }
    else if (SetVTKBSplineParametersFromITKGeneric<itk::BSplineTransform<T, 2, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = false;
    }
    else
    {
      vtkDebugWithObjectMacro(loggerObject, "Not an ITKv4 BSpline transform");
      return false;
    }
  }
  else
  {
    // inverse class is derived from forward class, so it has to be checked first
    if (SetVTKBSplineParametersFromITKGeneric<itk::InverseBSplineTransform<T, 3, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = true;
    }
    else if (SetVTKBSplineParametersFromITKGeneric<itk::BSplineTransform<T, 3, BSPLINE_TRANSFORM_ORDER>>(loggerObject, bsplineVtk, warpTransformItk))
    {
      inverse = false;
    }
    else
    {
      vtkDebugWithObjectMacro(loggerObject, "Not an ITKv4 BSpline transform");
      return false;
    }
  }

  if (inverse)
  {
    bsplineVtk->Inverse();
  }
  return true;
}

//----------------------------------------------------------------------------
template <typename BSplineTransformType>
bool SetITKBSplineParametersFromVTKGeneric(vtkObject* loggerObject,
                                           typename itk::Transform<typename BSplineTransformType::ScalarType, VTKDimension, VTKDimension>::Pointer& warpTransformItk,
                                           vtkOrientedBSplineTransform* bsplineVtk)
{
  typedef typename BSplineTransformType::ScalarType T;
  if (bsplineVtk == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "vtkMRMLTransformStorageNode::SetITKBSplineFromVTK failed: bsplineVtk is invalid");
    return false;
  }
  vtkImageData* bsplineCoefficients_RAS = bsplineVtk->GetCoefficientData();
  if (bsplineCoefficients_RAS == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file: coefficients are not specified");
    return false;
  }

  typename BSplineTransformType::Pointer bsplineItk = BSplineTransformType::New();
  warpTransformItk = bsplineItk;

  // Fixed parameters:
  // * mesh size X, Y, Z (including the BSPLINE_TRANSFORM_ORDER=3 boundary nodes, 1 before and 2 after the grid)
  // * mesh origin X, Y, Z (position of the boundary node before the grid)
  // * mesh spacing X, Y, Z
  // * mesh direction 3x3 matrix (first row, second row, third row)
  typename BSplineTransformType::FixedParametersType transformFixedParamsItk;
  const unsigned int numberOfFixedParameters = VTKDimension * (VTKDimension + 3);
  transformFixedParamsItk.SetSize(numberOfFixedParameters);

  int* gridExtent = bsplineCoefficients_RAS->GetExtent();
  int gridSize[3] = { gridExtent[1] - gridExtent[0] + 1, gridExtent[3] - gridExtent[2] + 1, gridExtent[5] - gridExtent[4] + 1 };
  transformFixedParamsItk[0] = gridSize[0];
  transformFixedParamsItk[1] = gridSize[1];
  transformFixedParamsItk[2] = gridSize[2];

  double* gridOrigin_RAS = bsplineCoefficients_RAS->GetOrigin();
  double gridOrigin_LPS[3] = { -gridOrigin_RAS[0], -gridOrigin_RAS[1], gridOrigin_RAS[2] };
  transformFixedParamsItk[3] = gridOrigin_LPS[0];
  transformFixedParamsItk[4] = gridOrigin_LPS[1];
  transformFixedParamsItk[5] = gridOrigin_LPS[2];

  double* gridSpacing = bsplineCoefficients_RAS->GetSpacing();
  transformFixedParamsItk[6] = gridSpacing[0];
  transformFixedParamsItk[7] = gridSpacing[1];
  transformFixedParamsItk[8] = gridSpacing[2];

  vtkNew<vtkMatrix4x4> gridDirectionMatrix_RAS;
  if (bsplineVtk->GetGridDirectionMatrix() != nullptr)
  {
    gridDirectionMatrix_RAS->DeepCopy(bsplineVtk->GetGridDirectionMatrix());
  }
  vtkNew<vtkMatrix4x4> lpsToRas;
  lpsToRas->SetElement(0, 0, -1);
  lpsToRas->SetElement(1, 1, -1);
  vtkNew<vtkMatrix4x4> rasToLps;
  rasToLps->SetElement(0, 0, -1);
  rasToLps->SetElement(1, 1, -1);
  vtkNew<vtkMatrix4x4> gridDirectionMatrix_LPS;
  vtkMatrix4x4::Multiply4x4(rasToLps.GetPointer(), gridDirectionMatrix_RAS.GetPointer(), gridDirectionMatrix_LPS.GetPointer());
  int fpIndex = 9;
  for (unsigned int row = 0; row < VTKDimension; row++)
  {
    for (unsigned int column = 0; column < VTKDimension; column++)
    {
      transformFixedParamsItk[fpIndex++] = gridDirectionMatrix_LPS->GetElement(row, column);
    }
  }

  bsplineItk->SetFixedParameters(transformFixedParamsItk);

  // BSpline coefficients

  const unsigned int expectedNumberOfVectors = gridSize[0] * gridSize[1] * gridSize[2];
  const unsigned int expectedNumberOfParameters = expectedNumberOfVectors * VTKDimension;
  if (bsplineItk->GetNumberOfParameters() != expectedNumberOfParameters)
  {
    vtkErrorWithObjectMacro(loggerObject, "Mismatch in number of BSpline parameters in the ITK transform and the VTK transform");
    return false;
  }

  typename BSplineTransformType::ParametersType transformParamsItk(expectedNumberOfParameters);
  T* itkBSplineParams_LPS = static_cast<T*>(transformParamsItk.data_block());
  T* vtkBSplineParams_RAS = static_cast<T*>(bsplineCoefficients_RAS->GetScalarPointer());
  double coefficientScale = bsplineVtk->GetDisplacementScale();
  for (unsigned int i = 0; i < expectedNumberOfVectors; i++)
  {
    *(itkBSplineParams_LPS) = -coefficientScale * (*(vtkBSplineParams_RAS++));
    *(itkBSplineParams_LPS + expectedNumberOfVectors) = -coefficientScale * (*(vtkBSplineParams_RAS++));
    *(itkBSplineParams_LPS + expectedNumberOfVectors * 2) = coefficientScale * (*(vtkBSplineParams_RAS++));
    itkBSplineParams_LPS++;
  }

  bsplineItk->SetParameters(transformParamsItk);
  return true;
}

//----------------------------------------------------------------------------
template <typename T>
bool SetITKv3BSplineFromVTKGeneric(vtkObject* loggerObject,
                                   typename itk::Transform<T, VTKDimension, VTKDimension>::Pointer& warpTransformItk,
                                   typename itk::Transform<T, VTKDimension, VTKDimension>::Pointer& bulkTransformItk,
                                   vtkOrientedBSplineTransform* bsplineVtk,
                                   bool alwaysAddBulkTransform)
{
  if (bsplineVtk == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "vtkMRMLTransformStorageNode::SetITKBSplineFromVTK failed: bsplineVtk is invalid");
    return false;
  }
  // Update is needed because it refreshes the inverse flag (the flag may be out-of-date if the transform depends on its inverse)
  bsplineVtk->Update();
  bool itkTransformSetSuccessfully = false;
  if (bsplineVtk->GetInverseFlag())
  {
    itkTransformSetSuccessfully =
      SetITKBSplineParametersFromVTKGeneric<itk::InverseBSplineDeformableTransform<T, VTKDimension, VTKDimension>>(loggerObject, warpTransformItk, bsplineVtk);
  }
  else
  {
    itkTransformSetSuccessfully = SetITKBSplineParametersFromVTKGeneric<itk::BSplineDeformableTransform<T, VTKDimension, VTKDimension>>(loggerObject, warpTransformItk, bsplineVtk);
  }
  if (!itkTransformSetSuccessfully)
  {
    vtkErrorWithObjectMacro(loggerObject, "vtkMRMLTransformStorageNode::SetITKBSplineFromVTK failed: cannot determine BSpline parameters");
    return false;
  }

  vtkMatrix4x4* bulkMatrix_RAS = bsplineVtk->GetBulkTransformMatrix();
  if (bulkMatrix_RAS || alwaysAddBulkTransform)
  {
    vtkNew<vtkMatrix4x4> lpsToRas;
    lpsToRas->SetElement(0, 0, -1);
    lpsToRas->SetElement(1, 1, -1);
    vtkNew<vtkMatrix4x4> rasToLps;
    rasToLps->SetElement(0, 0, -1);
    rasToLps->SetElement(1, 1, -1);
    vtkNew<vtkMatrix4x4> bulkMatrix_LPS; // bulk_LPS = rasToLps * bulk_RAS * lpsToRas
    // If bulk transform is available then use it, otherwise just write an identity matrix (we just write it because
    // alwaysAddBulkTransform was requested, due to backward compatibility reasons)
    if (bulkMatrix_RAS != nullptr)
    {
      vtkMatrix4x4::Multiply4x4(rasToLps.GetPointer(), bulkMatrix_RAS, bulkMatrix_LPS.GetPointer());
      vtkMatrix4x4::Multiply4x4(bulkMatrix_LPS.GetPointer(), lpsToRas.GetPointer(), bulkMatrix_LPS.GetPointer());
    }
    typedef itk::AffineTransform<T, VTKDimension> BulkTransformType;
    typename BulkTransformType::Pointer affineItk = BulkTransformType::New();
    bulkTransformItk = affineItk;

    typename BulkTransformType::MatrixType affineMatrix;
    typename BulkTransformType::OffsetType affineOffset;
    for (unsigned int i = 0; i < VTKDimension; i++)
    {
      for (unsigned int j = 0; j < VTKDimension; j++)
      {
        affineMatrix[i][j] = bulkMatrix_LPS->GetElement(i, j);
      }
      affineOffset[i] = bulkMatrix_LPS->GetElement(i, VTKDimension);
    }

    affineItk->SetMatrix(affineMatrix);
    affineItk->SetOffset(affineOffset);
  }
  else
  {
    bulkTransformItk = nullptr;
  }

  return true;
}

//----------------------------------------------------------------------------
template <typename T>
bool SetITKv4BSplineFromVTKGeneric(vtkObject* loggerObject,
                                   typename itk::Transform<T, VTKDimension, VTKDimension>::Pointer& warpTransformItk,
                                   vtkOrientedBSplineTransform* bsplineVtk)
{
  // Update is needed because it refreshes the inverse flag (the flag may be out-of-date if the transform depends on its inverse)
  bsplineVtk->Update();
  bool itkTransformSetSuccessfully = false;
  if (bsplineVtk->GetInverseFlag())
  {
    itkTransformSetSuccessfully = SetITKBSplineParametersFromVTKGeneric<itk::InverseBSplineTransform<T, VTKDimension, VTKDimension>>(loggerObject, warpTransformItk, bsplineVtk);
  }
  else
  {
    itkTransformSetSuccessfully = SetITKBSplineParametersFromVTKGeneric<itk::BSplineTransform<T, VTKDimension, VTKDimension>>(loggerObject, warpTransformItk, bsplineVtk);
  }
  if (!itkTransformSetSuccessfully)
  {
    vtkErrorWithObjectMacro(loggerObject, "vtkMRMLTransformStorageNode::SetITKv4BSplineFromVTKGeneric failed: cannot determine BSpline parameters");
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
bool SetITKv3BSplineFromVTK(vtkObject* loggerObject,
                            itk::Object::Pointer& warpTransformItk,
                            itk::Object::Pointer& bulkTransformItk,
                            vtkOrientedBSplineTransform* bsplineVtk,
                            bool alwaysAddBulkTransform)
{
  if (bsplineVtk == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot retrieve BSpline transform from node");
    return false;
  }

  // An inverse transform gets its coefficients from the forward transform when it is updated,
  // for example the transform from parent of a B-spline that is stored as transform to parent
  bsplineVtk->Update();
  vtkImageData* bsplineCoefficients = bsplineVtk->GetCoefficientData();

  if (bsplineCoefficients == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file: coefficients are not specified");
    return false;
  }

  if (bsplineCoefficients->GetScalarType() == VTK_FLOAT)
  {
    typedef itk::Transform<float, VTKDimension, VTKDimension> ITKTransformType;
    ITKTransformType::Pointer floatWarpTransformItk;
    ITKTransformType::Pointer floatBulkTransformItk;
    if (!SetITKv3BSplineFromVTKGeneric<float>(loggerObject, floatWarpTransformItk, floatBulkTransformItk, bsplineVtk, alwaysAddBulkTransform))
    {
      vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file");
      return false;
    }
    warpTransformItk = floatWarpTransformItk.GetPointer();
    bulkTransformItk = floatBulkTransformItk.GetPointer();
  }
  else if (bsplineCoefficients->GetScalarType() == VTK_DOUBLE)
  {
    typedef itk::Transform<double, VTKDimension, VTKDimension> ITKTransformType;
    ITKTransformType::Pointer doubleWarpTransformItk;
    ITKTransformType::Pointer doubleBulkTransformItk;
    if (!SetITKv3BSplineFromVTKGeneric<double>(loggerObject, doubleWarpTransformItk, doubleBulkTransformItk, bsplineVtk, alwaysAddBulkTransform))
    {
      vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file");
      return false;
    }
    warpTransformItk = doubleWarpTransformItk;
    bulkTransformItk = doubleBulkTransformItk;
  }
  else
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file: only float and double coefficient types are supported");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
bool SetITKv4BSplineFromVTK(vtkObject* loggerObject, itk::Object::Pointer& warpTransformItk, vtkOrientedBSplineTransform* bsplineVtk)
{
  if (bsplineVtk == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot retrieve BSpline transform from node");
    return false;
  }

  // An inverse transform gets its coefficients from the forward transform when it is updated,
  // for example the transform from parent of a B-spline that is stored as transform to parent
  bsplineVtk->Update();
  vtkImageData* bsplineCoefficients = bsplineVtk->GetCoefficientData();

  if (bsplineCoefficients == nullptr)
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file: coefficients are not specified");
    return false;
  }

  if (bsplineCoefficients->GetScalarType() == VTK_FLOAT)
  {
    typedef itk::Transform<float, VTKDimension, VTKDimension> ITKTransformType;
    ITKTransformType::Pointer floatWarpTransformItk;
    if (!SetITKv4BSplineFromVTKGeneric<float>(loggerObject, floatWarpTransformItk, bsplineVtk))
    {
      vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file");
      return false;
    }
    warpTransformItk = floatWarpTransformItk.GetPointer();
  }
  else if (bsplineCoefficients->GetScalarType() == VTK_DOUBLE)
  {
    typedef itk::Transform<double, VTKDimension, VTKDimension> ITKTransformType;
    ITKTransformType::Pointer doubleWarpTransformItk;
    if (!SetITKv4BSplineFromVTKGeneric<double>(loggerObject, doubleWarpTransformItk, bsplineVtk))
    {
      vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file");
      return false;
    }
    warpTransformItk = doubleWarpTransformItk;
  }
  else
  {
    vtkErrorWithObjectMacro(loggerObject, "Cannot write BSpline transform to file: only float and double coefficient types are supported");
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
/// Register the ITK transform classes of this transform type that ITK does not register by default, so that
/// transform files that contain them can be read
bool RegisterITKTransformClasses()
{
  itk::TransformFactory<InverseBSplineTransformFloatITKv3Type>::RegisterTransform();
  itk::TransformFactory<InverseBSplineTransformFloatITKv4Type>::RegisterTransform();
  itk::TransformFactory<InverseBSplineTransformDoubleITKv3Type>::RegisterTransform();
  itk::TransformFactory<InverseBSplineTransformDoubleITKv4Type>::RegisterTransform();
  return true;
}

} // namespace

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLBSplineTransformPlugin);

//----------------------------------------------------------------------------
vtkMRMLBSplineTransformPlugin::vtkMRMLBSplineTransformPlugin()
{
  static const bool itkTransformClassesRegistered = RegisterITKTransformClasses();
  (void)itkTransformClassesRegistered;
}

//----------------------------------------------------------------------------
vtkMRMLBSplineTransformPlugin::~vtkMRMLBSplineTransformPlugin() = default;

//----------------------------------------------------------------------------
const char* vtkMRMLBSplineTransformPlugin::GetTransformClassName()
{
  return "vtkBSplineTransform";
}

//----------------------------------------------------------------------------
double vtkMRMLBSplineTransformPlugin::CanReadITKTransformClass(const std::string& vtkNotUsed(itkClassName))
{
  return vtkMRMLTransformPlugin::GetFallbackConfidence();
}

//----------------------------------------------------------------------------
std::string vtkMRMLBSplineTransformPlugin::GetTransformInfo(vtkAbstractTransform* transform)
{
  vtkBSplineTransform* bsplineTransform = vtkBSplineTransform::SafeDownCast(transform);
  if (!bsplineTransform)
  {
    return std::string();
  }
  std::stringstream ss;
  ss << " B-spline:";
  bsplineTransform->Update(); // compute if inverse
  vtkImageData* coefficients = bsplineTransform->GetCoefficientData();
  if (coefficients != nullptr)
  {
    int* extent = coefficients->GetExtent();
    int gridSize[3] = { extent[1] - extent[0] + 1, extent[3] - extent[2] + 1, extent[5] - extent[4] + 1 };
    ss << std::endl << "  Grid size: " << gridSize[0] << " " << gridSize[1] << " " << gridSize[2];
    double* gridOrigin = coefficients->GetOrigin();
    ss << std::endl << "  Grid origin: " << gridOrigin[0] << " " << gridOrigin[1] << " " << gridOrigin[2];
    double* gridSpacing = coefficients->GetSpacing();
    ss << std::endl << "  Grid spacing: " << gridSpacing[0] << " " << gridSpacing[1] << " " << gridSpacing[2];
    vtkOrientedBSplineTransform* orientedBsplineTransform = vtkOrientedBSplineTransform::SafeDownCast(transform);
    if (orientedBsplineTransform != nullptr)
    {
      vtkMatrix4x4* gridOrientation = orientedBsplineTransform->GetGridDirectionMatrix();
      if (gridOrientation != nullptr)
      {
        ss << std::endl << "  Grid orientation:";
        for (int i = 0; i < 3; i++)
        {
          ss << std::endl << "    " << gridOrientation->GetElement(i, 0) << "  " << gridOrientation->GetElement(i, 1) << "  " << gridOrientation->GetElement(i, 2);
        }
      }
      vtkMatrix4x4* bulkTransform = orientedBsplineTransform->GetBulkTransformMatrix();
      if (bulkTransform != nullptr)
      {
        ss << std::endl << "  Additive bulk transform:";
        for (int i = 0; i < 4; i++)
        {
          ss << std::endl
             << "    " << bulkTransform->GetElement(i, 0) << "  " << bulkTransform->GetElement(i, 1) << "  " << bulkTransform->GetElement(i, 2) << "  "
             << bulkTransform->GetElement(i, 3);
        }
      }
    }
  }
  if (bsplineTransform->GetInverseFlag())
  {
    ss << std::endl << "  Computed from its inverse.";
  }
  // The description starts with the name of the transform type
  std::string info = ss.str();
  return (!info.empty() && info[0] == ' ') ? info.substr(1) : info;
}

//----------------------------------------------------------------------------
bool vtkMRMLBSplineTransformPlugin::DeepCopyTransform(vtkAbstractTransform* dst, vtkAbstractTransform* src)
{
  // Fix up the DeepCopy for vtkBSplineTransform (it performs only a ShallowCopy on the coefficient grid)
  dst->DeepCopy(src);
  if (vtkBSplineTransform::SafeDownCast(dst) == nullptr)
  {
    vtkGenericWarningMacro("vtkMRMLTransformNode::DeepCopyTransform failed: destination transform has to be vtkBSplineTransform");
    return false;
  }
  vtkImageData* srcCoefficients = vtkBSplineTransform::SafeDownCast(src)->GetCoefficientData();
  if (srcCoefficients)
  {
    vtkNew<vtkImageData> dstCoefficients;
    dstCoefficients->DeepCopy(srcCoefficients);
    vtkBSplineTransform::SafeDownCast(dst)->SetCoefficientData(dstCoefficients.GetPointer());
  }
  return true;
}

//----------------------------------------------------------------------------
itk::Object::Pointer vtkMRMLBSplineTransformPlugin::CreateITKTransformFromVTK(vtkObject* loggerObject,
                                                                              vtkAbstractTransform* transformVtk,
                                                                              itk::Object::Pointer& secondaryTransformItk,
                                                                              int preferITKv3CompatibleTransforms,
                                                                              bool initialize,
                                                                              double center_LocalRAS[3])
{
  vtkOrientedBSplineTransform* bsplineTransformVtk = vtkOrientedBSplineTransform::SafeDownCast(transformVtk);
  if (!bsplineTransformVtk)
  {
    return nullptr;
  }
  itk::Object::Pointer transformItk;
  vtkMatrix4x4* bulkMatrix = bsplineTransformVtk->GetBulkTransformMatrix(); // non-zero for ITKv3 bspline transform only
  if (preferITKv3CompatibleTransforms || (bulkMatrix != nullptr && !IsIdentityMatrix(bulkMatrix)))
  {
    if (!SetITKv3BSplineFromVTK(loggerObject, transformItk, secondaryTransformItk, bsplineTransformVtk, preferITKv3CompatibleTransforms))
    {
      return nullptr;
    }
  }
  else if (!SetITKv4BSplineFromVTK(loggerObject, transformItk, bsplineTransformVtk))
  {
    return nullptr;
  }
  return transformItk;
}

//----------------------------------------------------------------------------
vtkAbstractTransform* vtkMRMLBSplineTransformPlugin::CreateVTKTransformFromITK(vtkObject* loggerObject, itk::Object* transformItkObject, double center_LocalRAS[3])
{
  // Transform files are read with double precision
  TransformItkType* transformItk = dynamic_cast<TransformItkType*>(transformItkObject);
  if (!transformItk)
  {
    return nullptr;
  }
  vtkNew<vtkOrientedBSplineTransform> bsplineTransformVtk;
  if (!SetVTKBSplineFromITKv4Generic<double>(loggerObject, bsplineTransformVtk, transformItk))
  {
    return nullptr;
  }
  bsplineTransformVtk->Register(nullptr);
  return bsplineTransformVtk;
}

//----------------------------------------------------------------------------
bool vtkMRMLBSplineTransformPlugin::SetVTKBSplineFromITKv3(vtkObject* loggerObject,
                                                           vtkOrientedBSplineTransform* bsplineVtk,
                                                           itk::TransformBaseTemplate<double>* warpTransformItk,
                                                           itk::TransformBaseTemplate<double>* bulkTransformItk)
{
  return SetVTKBSplineFromITKv3Generic<double>(loggerObject, bsplineVtk, warpTransformItk, bulkTransformItk);
}
