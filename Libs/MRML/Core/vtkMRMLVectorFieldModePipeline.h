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

#ifndef __vtkMRMLVectorFieldModePipeline_h
#define __vtkMRMLVectorFieldModePipeline_h

// MRML includes
#include "vtkMRML.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

class vtkAlgorithmOutput;
class vtkAppendPolyData;
class vtkArrayCalculator;
class vtkContourFilter;
class vtkMRMLVectorFieldGridLines;
class vtkMRMLVectorFieldDisplayNode;
class vtkStreamTracer;
class vtkTubeFilter;
class vtkWarpVector;

/// \brief Geometry of the vector field visualization modes that are not glyphs.
///
/// Holds the filters that turn a sampled vector field into the geometry of the grid,
/// contour and streamline modes. One instance is needed per view, because a slice view
/// samples the field in its own plane; the display node owns the one used by 3D views.
///
/// The filters are only rewired and reconfigured by Update(), never executed, so the
/// geometry is computed when the renderer pulls it.
///
/// \sa vtkMRMLVectorFieldDisplayNode, vtkMRMLVectorFieldSampler
class VTK_MRML_EXPORT vtkMRMLVectorFieldModePipeline : public vtkObject
{
public:
  static vtkMRMLVectorFieldModePipeline* New();
  vtkTypeMacro(vtkMRMLVectorFieldModePipeline, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Configure the filters for the visualization mode of the display node, on top of a
  /// connection that produces the sampled field.
  /// Returns the connection that produces the geometry, or nullptr if the mode does not
  /// draw poly data (glyph mode) or the field cannot provide what the mode needs.
  /// flat is true for slice views, where tubes are drawn as lines and isosurfaces as
  /// isolines.
  vtkAlgorithmOutput* Update(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection, bool flat = false);

protected:
  vtkMRMLVectorFieldModePipeline();
  ~vtkMRMLVectorFieldModePipeline() override;
  vtkMRMLVectorFieldModePipeline(const vtkMRMLVectorFieldModePipeline&) = delete;
  void operator=(const vtkMRMLVectorFieldModePipeline&) = delete;

  vtkAlgorithmOutput* UpdateGrid(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection, bool flat);
  vtkAlgorithmOutput* UpdateContour(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection);
  vtkAlgorithmOutput* UpdateStreamline(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection, bool flat);

  vtkSmartPointer<vtkMRMLVectorFieldGridLines> GridLines;
  vtkSmartPointer<vtkWarpVector> Warper;
  vtkSmartPointer<vtkArrayCalculator> NonWarpedGridMagnitude;
  vtkSmartPointer<vtkAppendPolyData> NonWarpedGridAppender;
  vtkSmartPointer<vtkTubeFilter> GridTuber;
  vtkSmartPointer<vtkContourFilter> Contour;
  vtkSmartPointer<vtkStreamTracer> StreamTracer;
  vtkSmartPointer<vtkTubeFilter> StreamlineTuber;
};

#endif
