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

// MRML includes
#include "vtkMRMLVectorFieldModePipeline.h"
#include "vtkMRMLVectorFieldDisplayNode.h"
#include "vtkMRMLVectorFieldGridLines.h"
#include "vtkMRMLVectorFieldSampler.h"

// VTK includes
#include <vtkAlgorithmOutput.h>
#include <vtkAppendPolyData.h>
#include <vtkArrayCalculator.h>
#include <vtkContourFilter.h>
#include <vtkObjectFactory.h>
#include <vtkStreamTracer.h>
#include <vtkTubeFilter.h>
#include <vtkWarpVector.h>

// STD includes
#include <algorithm>
#include <vector>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLVectorFieldModePipeline);

//----------------------------------------------------------------------------
vtkMRMLVectorFieldModePipeline::vtkMRMLVectorFieldModePipeline()
{
  this->GridLines = vtkSmartPointer<vtkMRMLVectorFieldGridLines>::New();
  this->Warper = vtkSmartPointer<vtkWarpVector>::New();
  this->NonWarpedGridMagnitude = vtkSmartPointer<vtkArrayCalculator>::New();
  this->NonWarpedGridMagnitude->SetAttributeTypeToPointData();
  this->NonWarpedGridAppender = vtkSmartPointer<vtkAppendPolyData>::New();
  this->GridTuber = vtkSmartPointer<vtkTubeFilter>::New();
  this->Contour = vtkSmartPointer<vtkContourFilter>::New();
  this->StreamTracer = vtkSmartPointer<vtkStreamTracer>::New();
  this->StreamlineTuber = vtkSmartPointer<vtkTubeFilter>::New();

  this->GridTuber->SetNumberOfSides(8);
  this->GridTuber->CappingOn();

  this->StreamlineTuber->SetInputConnection(this->StreamTracer->GetOutputPort());
  this->StreamlineTuber->SetNumberOfSides(8);
  this->StreamlineTuber->CappingOn();
}

//----------------------------------------------------------------------------
vtkMRMLVectorFieldModePipeline::~vtkMRMLVectorFieldModePipeline() = default;

//----------------------------------------------------------------------------
void vtkMRMLVectorFieldModePipeline::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorFieldModePipeline::Update(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection, bool flat /*=false*/)
{
  if (!displayNode || !fieldConnection)
  {
    return nullptr;
  }
  switch (displayNode->GetVisualizationMode())
  {
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid: return this->UpdateGrid(displayNode, fieldConnection, flat);
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeContour: return this->UpdateContour(displayNode, fieldConnection);
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeStreamline: return this->UpdateStreamline(displayNode, fieldConnection, flat);
    case vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph:
    default: return nullptr;
  }
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorFieldModePipeline::UpdateGrid(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection, bool flat)
{
  // The lattice that the field was sampled on is the undeformed grid; warping its lines by
  // the vectors shows the deformation. Grid lines are drawn every GridSpacingMm, but they
  // follow every sampled point, so a finer sampling shows how the grid curves in between.
  this->GridLines->SetInputConnection(fieldConnection);
  this->GridLines->SetSubdivision(displayNode->GetGridSubdivision());

  this->Warper->SetInputConnection(this->GridLines->GetOutputPort());
  this->Warper->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, vtkMRMLVectorFieldSampler::GetVectorArrayName());
  this->Warper->SetScaleFactor(displayNode->GetGridScalePercent() * 0.01);

  vtkAlgorithmOutput* gridConnection = this->Warper->GetOutputPort();

  // Showing the undeformed grid next to the deformed one makes the deformation easier to
  // read. It is only offered in slice views: in a 3D view it makes the image unreadable.
  if (flat && displayNode->GetGridShowNonWarped())
  {
    // The undeformed grid is drawn in the color that the color map gives to a zero
    // displacement, which sets it apart from the deformed grid without needing a second
    // actor: its magnitudes are replaced by zeros, the deformed grid keeps its own.
    this->NonWarpedGridMagnitude->SetInputConnection(this->GridLines->GetOutputPort());
    this->NonWarpedGridMagnitude->SetResultArrayName(vtkMRMLVectorFieldSampler::GetMagnitudeArrayName());
    this->NonWarpedGridMagnitude->SetFunction("0");
    this->NonWarpedGridAppender->RemoveAllInputConnections(0);
    this->NonWarpedGridAppender->AddInputConnection(this->Warper->GetOutputPort());
    this->NonWarpedGridAppender->AddInputConnection(this->NonWarpedGridMagnitude->GetOutputPort());
    gridConnection = this->NonWarpedGridAppender->GetOutputPort();
  }
  else
  {
    this->NonWarpedGridAppender->RemoveAllInputConnections(0);
    this->NonWarpedGridMagnitude->SetInputConnection(nullptr);
  }

  double lineDiameterMm = displayNode->GetGridLineDiameterMm();
  if (flat || lineDiameterMm <= 0.0)
  {
    // Slice views draw the grid as lines: a tube would be seen edge-on and would only
    // clutter the image.
    this->GridTuber->SetInputConnection(nullptr);
    return gridConnection;
  }
  this->GridTuber->SetInputConnection(gridConnection);
  this->GridTuber->SetRadius(0.5 * lineDiameterMm);
  return this->GridTuber->GetOutputPort();
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorFieldModePipeline::UpdateContour(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection)
{
  // Isosurfaces of the magnitude. In a slice view the sampled field is a single layer of
  // cells, so the same filter produces isolines.
  this->Contour->SetInputConnection(fieldConnection);
  this->Contour->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, vtkMRMLVectorFieldSampler::GetMagnitudeArrayName());
  std::vector<double> levels;
  displayNode->GetEffectiveContourLevelsMm(levels);
  this->Contour->SetNumberOfContours(static_cast<int>(levels.size()));
  for (size_t levelIndex = 0; levelIndex < levels.size(); ++levelIndex)
  {
    this->Contour->SetValue(static_cast<int>(levelIndex), levels[levelIndex]);
  }
  return this->Contour->GetOutputPort();
}

//----------------------------------------------------------------------------
vtkAlgorithmOutput* vtkMRMLVectorFieldModePipeline::UpdateStreamline(vtkMRMLVectorFieldDisplayNode* displayNode, vtkAlgorithmOutput* fieldConnection, bool flat)
{
  // Curves that follow the field. The sampled points are used as seeds, which puts a
  // streamline everywhere a glyph would be.
  this->StreamTracer->SetInputConnection(fieldConnection);
  this->StreamTracer->SetSourceConnection(fieldConnection);
  this->StreamTracer->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, vtkMRMLVectorFieldSampler::GetVectorArrayName());
  this->StreamTracer->SetIntegrationDirectionToBoth();
  this->StreamTracer->SetIntegratorTypeToRungeKutta45();
  this->StreamTracer->SetMaximumPropagation(displayNode->GetMaximumPropagationMm());

  double tubeDiameterMm = displayNode->GetStreamlineTubeDiameterMm();
  if (flat || tubeDiameterMm <= 0.0)
  {
    this->StreamlineTuber->SetInputConnection(nullptr);
    return this->StreamTracer->GetOutputPort();
  }
  this->StreamlineTuber->SetInputConnection(this->StreamTracer->GetOutputPort());
  this->StreamlineTuber->SetRadius(0.5 * tubeDiameterMm);
  return this->StreamlineTuber->GetOutputPort();
}
