/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#ifndef __vtkMRMLTransformFieldSampler_h
#define __vtkMRMLTransformFieldSampler_h

// MRML includes
#include "vtkMRMLVectorFieldSampler.h"

class vtkMRMLTransformNode;

/// \brief Samples the displacement field of a transform.
///
/// The vector at a position is the displacement that the transform applies there, so the
/// same sampler works for any transform: linear, B-spline, thin plate spline, grid, or a
/// composite of those.
///
/// A transform is defined everywhere, so it has no extent of its own: nothing is sampled
/// until a region, a slice plane, or explicit sample positions are given.
///
/// \sa vtkMRMLTransformDisplayNode, vtkMRMLVectorFieldSampler
class VTK_MRML_EXPORT vtkMRMLTransformFieldSampler : public vtkMRMLVectorFieldSampler
{
public:
  static vtkMRMLTransformFieldSampler* New();
  vtkTypeMacro(vtkMRMLTransformFieldSampler, vtkMRMLVectorFieldSampler);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  ///@{
  /// Transform whose displacement field is sampled.
  virtual void SetTransformNode(vtkMRMLTransformNode* transformNode);
  vtkMRMLTransformNode* GetTransformNode();
  ///@}

  ///@{
  /// If true, the displacement of the transform to world is sampled, otherwise the
  /// displacement of the transform from world.
  /// Default is true.
  vtkSetMacro(TransformToWorld, bool);
  vtkGetMacro(TransformToWorld, bool);
  vtkBooleanMacro(TransformToWorld, bool);
  ///@}

  /// A transform can be evaluated anywhere, including in the plane of a slice view.
  bool CanSampleOnSlice() override { return true; }

protected:
  vtkMRMLTransformFieldSampler();
  ~vtkMRMLTransformFieldSampler() override;
  vtkMRMLTransformFieldSampler(const vtkMRMLTransformFieldSampler&) = delete;
  void operator=(const vtkMRMLTransformFieldSampler&) = delete;

  int RequestData(vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector) override;

  /// Includes the modified time of the transform, so that dragging a transform re-samples
  /// the field on the next render instead of on every mouse move.
  vtkMTimeType GetSampledObjectMTime() override;

  vtkWeakPointer<vtkMRMLTransformNode> TransformNode;
  bool TransformToWorld{ true };
};

#endif
