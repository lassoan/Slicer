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
#include "vtkMRMLVectorFieldSampler.h"

// VTK includes
#include <vtkAlgorithmOutput.h>
#include <vtkContourFilter.h>
#include <vtkExtractEdges.h>
#include <vtkObjectFactory.h>
#include <vtkStreamTracer.h>
#include <vtkTubeFilter.h>
#include <vtkWarpVector.h>

// STD includes
#include <vector>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLVectorFieldModePipeline);

//----------------------------------------------------------------------------
vtkMRMLVectorFieldModePipeline::vtkMRMLVectorFieldModePipeline()
{
  this->EdgeExtractor = vtkSmartPointer<vtkExtractEdges>::New();
  this->Warper = vtkSmartPointer<vtkWarpVector>::New();
  this->GridTuber = vtkSmartPointer<vtkTubeFilter>::New();
  this->Contour = vtkSmartPointer<vtkContourFilter>::New();
  this->StreamTracer = vtkSmartPointer<vtkStreamTracer>::New();
  this->StreamlineTuber = vtkSmartPointer<vtkTubeFilter>::New();

  // The deformed grid is the edges of the sampling lattice, moved by the vectors
  this->Warper->SetInputConnection(this->EdgeExtractor->GetOutputPort());
  this->GridTuber->SetInputConnection(this->Warper->GetOutputPort());
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
  // The lattice that the field was sampled on is the undeformed grid; its edges are the
  // grid lines, and warping them by the vectors shows the deformation.
  this->EdgeExtractor->SetInputConnection(fieldConnection);
  this->Warper->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, vtkMRMLVectorFieldSampler::GetVectorArrayName());
  this->Warper->SetScaleFactor(displayNode->GetGridScalePercent() * 0.01);

  double lineDiameterMm = displayNode->GetGridLineDiameterMm();
  if (flat || lineDiameterMm <= 0.0)
  {
    // Slice views draw the grid as lines: a tube would be seen edge-on and would only
    // clutter the image.
    this->GridTuber->SetInputConnection(nullptr);
    return this->Warper->GetOutputPort();
  }
  this->GridTuber->SetInputConnection(this->Warper->GetOutputPort());
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
  displayNode->GetContourLevelsMm(levels);
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
