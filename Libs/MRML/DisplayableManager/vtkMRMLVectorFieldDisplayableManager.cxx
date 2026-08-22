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

// MRMLDisplayableManager includes
#include "vtkMRMLVectorFieldDisplayableManager.h"
#include "vtkMRMLVectorFieldGlyphSource.h"

// MRML includes
#include <vtkEventBroker.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLDisplayableNode.h>
#include <vtkMRMLFolderDisplayNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLVectorFieldDisplayNode.h>
#include <vtkMRMLVectorFieldSampler.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLVolumeNode.h>

// VTK includes
#include <vtkActor.h>
#include <vtkAlgorithmOutput.h>
#include <vtkDataArray.h>
#include <vtkGeneralTransform.h>
#include <vtkGeometryFilter.h>
#include <vtkGlyph3DMapper.h>
#include <vtkPolyDataMapper.h>
#include <vtkLookupTable.h>
#include <vtkMapper.h>
#include <vtkMaskPoints.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkThresholdPoints.h>
#include <vtkTransformFilter.h>

// STD includes
#include <deque>
#include <map>
#include <set>
#include <string>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLVectorFieldDisplayableManager);

//---------------------------------------------------------------------------
class vtkMRMLVectorFieldDisplayableManager::vtkInternal
{
public:
  struct Pipeline
  {
    vtkSmartPointer<vtkGeneralTransform> NodeToWorld;
    vtkSmartPointer<vtkTransformFilter> NodeWarper;
    vtkSmartPointer<vtkGeometryFilter> SurfaceFilter;
    vtkSmartPointer<vtkThresholdPoints> ScalarThreshold;
    vtkSmartPointer<vtkMaskPoints> MaskPoints;
    vtkSmartPointer<vtkGlyph3DMapper> Glypher;
    /// Draws the geometry of the grid, contour and streamline modes; the glyph mapper is
    /// only used in glyph mode.
    vtkSmartPointer<vtkPolyDataMapper> PolyDataMapper;
    vtkSmartPointer<vtkActor> Actor;
  };

  typedef std::map<vtkMRMLDisplayNode*, const Pipeline*> PipelinesCacheType;
  PipelinesCacheType DisplayPipelines;

  typedef std::map<vtkMRMLDisplayableNode*, std::set<vtkMRMLDisplayNode*>> DisplayableToDisplayCacheType;
  DisplayableToDisplayCacheType DisplayableToDisplayNodes;

  // Transforms
  void UpdateDisplayableTransforms(vtkMRMLDisplayableNode* node);
  void GetNodeTransformToWorld(vtkMRMLTransformableNode* node, vtkGeneralTransform* transformToWorld, vtkMatrix4x4* linearTransformToWorld, bool& isLinear);

  // Display Nodes
  void AddDisplayNode(vtkMRMLDisplayableNode*, vtkMRMLDisplayNode*);
  void UpdateDisplayNode(vtkMRMLDisplayNode* displayNode);
  void UpdateDisplayNodePipeline(vtkMRMLDisplayNode*, const Pipeline*);
  void UpdateScalarColoring(vtkMRMLVectorFieldDisplayNode* displayNode, vtkMapper* mapper);
  void UpdateActorProperties(vtkMRMLVectorFieldDisplayNode* displayNode, vtkMRMLDisplayNode* effectiveDisplayNode, const Pipeline* pipeline, double hierarchyOpacity);
  void RemoveDisplayNode(vtkMRMLDisplayNode* displayNode);

  // Observations
  void AddObservations(vtkMRMLDisplayableNode* node);
  void RemoveObservations(vtkMRMLDisplayableNode* node);

  // Helper functions
  bool IsVisible(vtkMRMLDisplayNode* displayNode);
  bool UseDisplayNode(vtkMRMLDisplayNode* displayNode);
  bool UseDisplayableNode(vtkMRMLDisplayableNode* displayNode);
  bool HasDisplayPipeline(vtkMRMLDisplayableNode* displayNode);
  void ClearDisplayableNodes();

  vtkInternal(vtkMRMLVectorFieldDisplayableManager* external);
  ~vtkInternal();

private:
  vtkMRMLVectorFieldDisplayableManager* External;
};

//---------------------------------------------------------------------------
// vtkInternal methods
vtkMRMLVectorFieldDisplayableManager::vtkInternal::vtkInternal(vtkMRMLVectorFieldDisplayableManager* external)
{
  this->External = external;
}

//---------------------------------------------------------------------------
vtkMRMLVectorFieldDisplayableManager::vtkInternal::~vtkInternal()
{
  this->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldDisplayableManager::vtkInternal::UseDisplayNode(vtkMRMLDisplayNode* displayNode)
{
  return displayNode && displayNode->IsA("vtkMRMLVectorFieldDisplayNode");
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldDisplayableManager::vtkInternal::UseDisplayableNode(vtkMRMLDisplayableNode* node)
{
  // Any displayable node can store a vector field (a model's point data, a vector volume,
  // a transform, ...), and a vector field display node can be added to it at any time, so
  // all of them are observed. Nodes that have no vector field display node simply have no
  // pipeline; see HasDisplayPipeline().
  return node != nullptr;
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldDisplayableManager::vtkInternal::HasDisplayPipeline(vtkMRMLDisplayableNode* node)
{
  auto displayableIt = this->DisplayableToDisplayNodes.find(node);
  return displayableIt != this->DisplayableToDisplayNodes.end() && !displayableIt->second.empty();
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldDisplayableManager::vtkInternal::IsVisible(vtkMRMLDisplayNode* displayNode)
{
  if (!displayNode)
  {
    return false;
  }
  vtkMRMLViewNode* viewNode = this->External->GetMRMLViewNode();
  bool visibleInView = (viewNode == nullptr) || displayNode->IsDisplayableInView(viewNode->GetID());
  return displayNode->GetVisibility() && displayNode->GetVisibility3D() && visibleInView;
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::GetNodeTransformToWorld(vtkMRMLTransformableNode* node,
                                                                               vtkGeneralTransform* transformToWorld,
                                                                               vtkMatrix4x4* linearTransformToWorld,
                                                                               bool& isLinear)
{
  isLinear = true;
  if (transformToWorld)
  {
    transformToWorld->Identity();
  }
  if (linearTransformToWorld)
  {
    linearTransformToWorld->Identity();
  }
  if (!node)
  {
    return;
  }
  vtkMRMLTransformNode* transformNode = node->GetParentTransformNode();
  if (!transformNode)
  {
    return;
  }
  if (transformNode->IsTransformToWorldLinear())
  {
    if (linearTransformToWorld)
    {
      transformNode->GetMatrixTransformToWorld(linearTransformToWorld);
    }
    return;
  }
  isLinear = false;
  if (transformToWorld)
  {
    transformNode->GetTransformToWorld(transformToWorld);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::UpdateDisplayableTransforms(vtkMRMLDisplayableNode* node)
{
  std::set<vtkMRMLDisplayNode*> displayNodes = this->DisplayableToDisplayNodes[node];
  for (auto displayNodeIt = displayNodes.begin(); displayNodeIt != displayNodes.end(); ++displayNodeIt)
  {
    PipelinesCacheType::iterator pipelineIt = this->DisplayPipelines.find(*displayNodeIt);
    if (pipelineIt != this->DisplayPipelines.end())
    {
      this->UpdateDisplayNodePipeline(pipelineIt->first, pipelineIt->second);
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::RemoveDisplayNode(vtkMRMLDisplayNode* displayNode)
{
  PipelinesCacheType::iterator pipelineIt = this->DisplayPipelines.find(displayNode);
  if (pipelineIt == this->DisplayPipelines.end())
  {
    return;
  }
  const Pipeline* pipeline = pipelineIt->second;
  this->External->GetRenderer()->RemoveViewProp(pipeline->Actor);
  delete pipeline;
  this->DisplayPipelines.erase(pipelineIt);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::AddDisplayNode(vtkMRMLDisplayableNode* displayableNode, vtkMRMLDisplayNode* displayNode)
{
  if (!displayableNode || !displayNode)
  {
    return;
  }

  // Do not add the display node if it is already associated with a pipeline object
  if (this->DisplayPipelines.find(displayNode) != this->DisplayPipelines.end())
  {
    return;
  }

  Pipeline* pipeline = new Pipeline();
  pipeline->NodeToWorld = vtkSmartPointer<vtkGeneralTransform>::New();
  pipeline->NodeWarper = vtkSmartPointer<vtkTransformFilter>::New();
  pipeline->SurfaceFilter = vtkSmartPointer<vtkGeometryFilter>::New();
  pipeline->ScalarThreshold = vtkSmartPointer<vtkThresholdPoints>::New();
  pipeline->MaskPoints = vtkSmartPointer<vtkMaskPoints>::New();
  pipeline->Glypher = vtkSmartPointer<vtkGlyph3DMapper>::New();
  pipeline->PolyDataMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  pipeline->Actor = vtkSmartPointer<vtkActor>::New();

  // Set up the static parts of the pipeline. The filters between the field source and the
  // glypher are rewired on each update, because most of them are optional
  // (see UpdateDisplayNodePipeline).
  pipeline->NodeWarper->SetTransform(pipeline->NodeToWorld);
  pipeline->MaskPoints->GenerateVerticesOff();
  pipeline->Glypher->SetInputConnection(pipeline->MaskPoints->GetOutputPort());
  pipeline->Actor->SetMapper(pipeline->Glypher);
  pipeline->Actor->SetVisibility(0);

  // Add actor to Renderer and local cache
  this->External->GetRenderer()->AddViewProp(pipeline->Actor);
  this->DisplayPipelines.insert(std::make_pair(displayNode, pipeline));

  this->UpdateDisplayNodePipeline(displayNode, pipeline);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::UpdateDisplayNode(vtkMRMLDisplayNode* displayNode)
{
  if (!displayNode)
  {
    return;
  }
  PipelinesCacheType::iterator pipelineIt = this->DisplayPipelines.find(displayNode);
  if (pipelineIt != this->DisplayPipelines.end())
  {
    this->UpdateDisplayNodePipeline(displayNode, pipelineIt->second);
  }
  else
  {
    this->External->AddDisplayableNode(displayNode->GetDisplayableNode());
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::UpdateDisplayNodePipeline(vtkMRMLDisplayNode* displayNode, const Pipeline* pipeline)
{
  if (!pipeline)
  {
    vtkErrorWithObjectMacro(this->External, "vtkMRMLVectorFieldDisplayableManager::UpdateDisplayNodePipeline failed: pipeline is invalid");
    return;
  }

  vtkMRMLVectorFieldDisplayNode* fieldDisplayNode = vtkMRMLVectorFieldDisplayNode::SafeDownCast(displayNode);
  if (!fieldDisplayNode)
  {
    pipeline->Actor->SetVisibility(false);
    return;
  }

  // Get display node from hierarchy that applies display properties on branch
  vtkMRMLDisplayableNode* displayableNode = displayNode->GetDisplayableNode();
  vtkMRMLDisplayNode* overrideHierarchyDisplayNode = vtkMRMLFolderDisplayNode::GetOverridingHierarchyDisplayNode(displayableNode);

  // Use hierarchy display node if any, and if overriding is allowed for the current display node.
  bool hierarchyVisibility = true;
  double hierarchyOpacity = 1.0;
  vtkMRMLDisplayNode* effectiveDisplayNode = fieldDisplayNode;
  if (fieldDisplayNode->GetFolderDisplayOverrideAllowed())
  {
    if (overrideHierarchyDisplayNode)
    {
      effectiveDisplayNode = overrideHierarchyDisplayNode;
    }
    hierarchyVisibility = vtkMRMLFolderDisplayNode::GetHierarchyVisibility(displayableNode);
    hierarchyOpacity = vtkMRMLFolderDisplayNode::GetHierarchyOpacity(displayableNode);
  }

  if (!hierarchyVisibility || !this->IsVisible(fieldDisplayNode))
  {
    pipeline->Actor->SetVisibility(false);
    return;
  }

  vtkAlgorithmOutput* fieldConnection = fieldDisplayNode->GetFieldConnection();
  if (!fieldConnection)
  {
    pipeline->Actor->SetVisibility(false);
    return;
  }

  // Parent transform: a non-linear transform has to be applied to the points, upstream of
  // masking. A linear transform is applied to the actor instead, which is much cheaper.
  bool isLinearTransform = true;
  vtkNew<vtkMatrix4x4> linearTransformToWorld;
  this->GetNodeTransformToWorld(displayableNode, pipeline->NodeToWorld, linearTransformToWorld, isLinearTransform);
  vtkAlgorithmOutput* glyphInputConnection = fieldConnection;
  if (!isLinearTransform)
  {
    pipeline->NodeWarper->SetInputConnection(fieldConnection);
    glyphInputConnection = pipeline->NodeWarper->GetOutputPort();
  }
  else
  {
    pipeline->NodeWarper->SetInputConnection(nullptr);
  }
  pipeline->Actor->SetUserMatrix(linearTransformToWorld);

  // The modes that draw poly data (grid, contour, streamline) replace the whole glyph
  // pipeline: the display node builds their geometry and the actor uses a plain mapper.
  if (fieldDisplayNode->IsPolyDataVisualizationMode())
  {
    vtkAlgorithmOutput* polyDataConnection = fieldDisplayNode->GetOutputPolyDataConnection(glyphInputConnection);
    if (!polyDataConnection)
    {
      pipeline->Actor->SetVisibility(false);
      return;
    }
    pipeline->PolyDataMapper->SetInputConnection(polyDataConnection);
    pipeline->Actor->SetMapper(pipeline->PolyDataMapper);
    this->UpdateScalarColoring(fieldDisplayNode, pipeline->PolyDataMapper);
    this->UpdateActorProperties(fieldDisplayNode, effectiveDisplayNode, pipeline, hierarchyOpacity);
    return;
  }
  pipeline->Actor->SetMapper(pipeline->Glypher);

  // Surface-sampling masking needs 2D (surface) cells to sample from; volumetric meshes (for
  // example tetrahedral solid meshes) typically only have 3D cells, so extract the boundary
  // surface first.
  if (fieldDisplayNode->GetMaskingMode() == vtkMRMLVectorFieldDisplayNode::MaskingModeUniformSurface)
  {
    pipeline->SurfaceFilter->SetInputConnection(glyphInputConnection);
    glyphInputConnection = pipeline->SurfaceFilter->GetOutputPort();
  }
  else
  {
    pipeline->SurfaceFilter->SetInputConnection(nullptr);
  }

  // Thresholding: only keep points where the active scalar is within the threshold range.
  // It is applied before masking, so that masking subsamples the points that are shown.
  const char* activeScalarName = fieldDisplayNode->GetActiveScalarName();
  bool hasActiveScalar = (activeScalarName && activeScalarName[0] != '\0');
  // Multi-component arrays are colored and thresholded by their magnitude
  vtkDataArray* activeScalarArray = fieldDisplayNode->GetActiveScalarArray();
  int numberOfScalarComponents = activeScalarArray ? activeScalarArray->GetNumberOfComponents() : 1;
  bool useScalarMagnitude = (numberOfScalarComponents > 1);
  if (fieldDisplayNode->GetThresholdEnabled() && hasActiveScalar)
  {
    double thresholdRange[2] = { 0.0, -1.0 };
    fieldDisplayNode->GetThresholdRange(thresholdRange);
    pipeline->ScalarThreshold->SetInputConnection(glyphInputConnection);
    pipeline->ScalarThreshold->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, activeScalarName);
    // vtkThresholdPoints uses the magnitude if the component index is not a valid component
    pipeline->ScalarThreshold->SetInputArrayComponent(useScalarMagnitude ? numberOfScalarComponents : 0);
    pipeline->ScalarThreshold->SetLowerThreshold(thresholdRange[0]);
    pipeline->ScalarThreshold->SetUpperThreshold(thresholdRange[1]);
    pipeline->ScalarThreshold->SetThresholdFunction(vtkThresholdPoints::THRESHOLD_BETWEEN);
    glyphInputConnection = pipeline->ScalarThreshold->GetOutputPort();
  }
  else
  {
    pipeline->ScalarThreshold->SetInputConnection(nullptr);
  }

  pipeline->MaskPoints->SetInputConnection(glyphInputConnection);

  // Masking: which of the points get a glyph.
  switch (fieldDisplayNode->GetMaskingMode())
  {
    case vtkMRMLVectorFieldDisplayNode::MaskingModeEveryNthPoint:
      pipeline->MaskPoints->RandomModeOff();
      pipeline->MaskPoints->SetOnRatio(fieldDisplayNode->GetMaskingNthPoint());
      break;
    case vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds:
      pipeline->MaskPoints->RandomModeOn();
      pipeline->MaskPoints->SetRandomModeType(vtkMaskPoints::UNIFORM_SPATIAL_BOUNDS);
      pipeline->MaskPoints->SetMaximumNumberOfPoints(fieldDisplayNode->GetMaskingPointsNumber());
      break;
    case vtkMRMLVectorFieldDisplayNode::MaskingModeUniformSurface:
      pipeline->MaskPoints->RandomModeOn();
      pipeline->MaskPoints->SetRandomModeType(vtkMaskPoints::UNIFORM_SPATIAL_SURFACE);
      pipeline->MaskPoints->SetMaximumNumberOfPoints(fieldDisplayNode->GetMaskingPointsNumber());
      break;
    case vtkMRMLVectorFieldDisplayNode::MaskingModeUniformVolume:
      pipeline->MaskPoints->RandomModeOn();
      pipeline->MaskPoints->SetRandomModeType(vtkMaskPoints::UNIFORM_SPATIAL_VOLUME);
      pipeline->MaskPoints->SetMaximumNumberOfPoints(fieldDisplayNode->GetMaskingPointsNumber());
      break;
    case vtkMRMLVectorFieldDisplayNode::MaskingModeAllPoints:
    default:
      pipeline->MaskPoints->RandomModeOff();
      pipeline->MaskPoints->SetOnRatio(1);
      break;
  }

  // Glyph geometry
  vtkSmartPointer<vtkPolyDataAlgorithm> glyphSource = vtkMRMLVectorFieldGlyphSource::Create3D(fieldDisplayNode);
  pipeline->Glypher->SetSourceConnection(glyphSource->GetOutputPort());

  // Orientation
  const char* orientationArrayName = fieldDisplayNode->GetOrientationArrayName();
  if (orientationArrayName && orientationArrayName[0] != '\0')
  {
    pipeline->Glypher->OrientOn();
    pipeline->Glypher->SetOrientationModeToDirection();
    pipeline->Glypher->SetOrientationArray(orientationArrayName);
  }
  else
  {
    pipeline->Glypher->OrientOff();
  }

  // Scale
  pipeline->Glypher->ScalingOn();
  pipeline->Glypher->SetScaleFactor(fieldDisplayNode->GetScaleFactor());
  const char* scaleArrayName = fieldDisplayNode->GetScaleArrayName();
  if (scaleArrayName && scaleArrayName[0] != '\0')
  {
    pipeline->Glypher->SetScaleArray(scaleArrayName);
    vtkDataArray* scaleArray = fieldDisplayNode->GetScaleArray();
    bool scaleByComponents = (scaleArray && scaleArray->GetNumberOfComponents() == 3 //
                              && fieldDisplayNode->GetVectorScaleMode() == vtkMRMLVectorFieldDisplayNode::VectorScaleModeByComponents);
    pipeline->Glypher->SetScaleMode(scaleByComponents ? vtkGlyph3DMapper::SCALE_BY_COMPONENTS : vtkGlyph3DMapper::SCALE_BY_MAGNITUDE);
  }
  else
  {
    pipeline->Glypher->SetScaleModeToNoDataScaling();
  }

  this->UpdateScalarColoring(fieldDisplayNode, pipeline->Glypher);
  this->UpdateActorProperties(fieldDisplayNode, effectiveDisplayNode, pipeline, hierarchyOpacity);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::UpdateScalarColoring(vtkMRMLVectorFieldDisplayNode* displayNode, vtkMapper* mapper)
{
  // Coloring: either by the active point data scalar array through the color node's
  // lookup table, or by the display node's solid color.
  const char* activeScalarName = displayNode->GetActiveScalarName();
  bool hasActiveScalar = (activeScalarName && activeScalarName[0] != 0);
  if (!displayNode->GetScalarVisibility() || !hasActiveScalar)
  {
    mapper->ScalarVisibilityOff();
    return;
  }
  // Multi-component arrays are colored by their magnitude
  vtkDataArray* activeScalarArray = displayNode->GetActiveScalarArray();
  bool useScalarMagnitude = (activeScalarArray && activeScalarArray->GetNumberOfComponents() > 1);

  mapper->ScalarVisibilityOn();
  mapper->SetScalarModeToUsePointFieldData();
  mapper->SelectColorArray(activeScalarName);
  mapper->SetColorModeToMapScalars();
  // Component -1 lets the lookup table turn the vector into a scalar (magnitude);
  // single-component arrays are mapped directly.
  mapper->SetArrayComponent(useScalarMagnitude ? -1 : 0);
  vtkMRMLColorNode* colorNode = displayNode->GetColorNode();
  if (useScalarMagnitude)
  {
    // The vector-to-scalar mode is a property of the lookup table, so a copy is used here:
    // the color node's lookup table may be shared with other mappers. If no color node is
    // set then a default table is used, so that the glyphs are still colored by the values.
    vtkSmartPointer<vtkLookupTable> lookupTable = vtkSmartPointer<vtkLookupTable>::Take(colorNode ? colorNode->CreateLookupTableCopy() : nullptr);
    if (!lookupTable)
    {
      lookupTable = vtkSmartPointer<vtkLookupTable>::New();
      lookupTable->Build();
    }
    lookupTable->SetVectorModeToMagnitude();
    mapper->SetLookupTable(lookupTable);
  }
  else if (colorNode && colorNode->GetLookupTable())
  {
    mapper->SetLookupTable(colorNode->GetLookupTable());
  }
  mapper->UseLookupTableScalarRangeOff();
  mapper->SetScalarRange(displayNode->GetScalarRange());
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::UpdateActorProperties(vtkMRMLVectorFieldDisplayNode* displayNode,
                                                                              vtkMRMLDisplayNode* effectiveDisplayNode,
                                                                              const Pipeline* pipeline,
                                                                              double hierarchyOpacity)
{
  vtkProperty* actorProperties = pipeline->Actor->GetProperty();
  actorProperties->SetColor(displayNode->GetColor());
  actorProperties->SetOpacity(hierarchyOpacity * effectiveDisplayNode->GetOpacity());
  actorProperties->SetAmbient(displayNode->GetAmbient());
  actorProperties->SetDiffuse(displayNode->GetDiffuse());
  actorProperties->SetSpecular(displayNode->GetSpecular());
  actorProperties->SetSpecularPower(displayNode->GetPower());
  actorProperties->SetRepresentation(displayNode->GetRepresentation());
  actorProperties->SetBackfaceCulling(displayNode->GetBackfaceCulling());
  actorProperties->SetFrontfaceCulling(displayNode->GetFrontfaceCulling());

  pipeline->Actor->SetVisibility(true);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::AddObservations(vtkMRMLDisplayableNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  const unsigned long events[] = {
    vtkMRMLDisplayableNode::TransformModifiedEvent,
    vtkMRMLDisplayableNode::DisplayModifiedEvent,
    vtkMRMLModelNode::PolyDataModifiedEvent,
    vtkMRMLVolumeNode::ImageDataModifiedEvent,
  };
  for (unsigned long event : events)
  {
    if (!broker->GetObservationExist(node, event, this->External, this->External->GetMRMLNodesCallbackCommand()))
    {
      broker->AddObservation(node, event, this->External, this->External->GetMRMLNodesCallbackCommand());
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::RemoveObservations(vtkMRMLDisplayableNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  const unsigned long events[] = {
    vtkMRMLDisplayableNode::TransformModifiedEvent,
    vtkMRMLDisplayableNode::DisplayModifiedEvent,
    vtkMRMLModelNode::PolyDataModifiedEvent,
    vtkMRMLVolumeNode::ImageDataModifiedEvent,
  };
  for (unsigned long event : events)
  {
    vtkEventBroker::ObservationVector observations = broker->GetObservations(node, event, this->External, this->External->GetMRMLNodesCallbackCommand());
    broker->RemoveObservations(observations);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::vtkInternal::ClearDisplayableNodes()
{
  while (this->DisplayableToDisplayNodes.size() > 0)
  {
    this->External->RemoveDisplayableNode(this->DisplayableToDisplayNodes.begin()->first);
  }
}

//---------------------------------------------------------------------------
// vtkMRMLVectorFieldDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLVectorFieldDisplayableManager::vtkMRMLVectorFieldDisplayableManager()
{
  this->Internal = new vtkInternal(this);
  this->AddingDisplayableNode = 0;
}

//---------------------------------------------------------------------------
vtkMRMLVectorFieldDisplayableManager::~vtkMRMLVectorFieldDisplayableManager()
{
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::AddDisplayableNode(vtkMRMLDisplayableNode* node)
{
  if (this->AddingDisplayableNode)
  {
    return;
  }
  if (!this->Internal->UseDisplayableNode(node))
  {
    return;
  }

  this->AddingDisplayableNode = 1;
  this->Internal->AddObservations(node);
  // Registers the node even if it has no vector field display node yet, so that one can be
  // added to it later.
  this->Internal->DisplayableToDisplayNodes[node];

  int numberOfDisplayNodes = node->GetNumberOfDisplayNodes();
  for (int i = 0; i < numberOfDisplayNodes; i++)
  {
    vtkMRMLDisplayNode* displayNode = node->GetNthDisplayNode(i);
    if (this->Internal->UseDisplayNode(displayNode))
    {
      this->Internal->DisplayableToDisplayNodes[node].insert(displayNode);
      this->Internal->AddDisplayNode(node, displayNode);
    }
  }
  this->AddingDisplayableNode = 0;
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::RemoveDisplayableNode(vtkMRMLDisplayableNode* node)
{
  if (!node)
  {
    return;
  }
  vtkInternal::DisplayableToDisplayCacheType::iterator displayableIt = this->Internal->DisplayableToDisplayNodes.find(node);
  if (displayableIt == this->Internal->DisplayableToDisplayNodes.end())
  {
    return;
  }

  std::set<vtkMRMLDisplayNode*> displayNodes = displayableIt->second;
  for (auto displayNodeIt = displayNodes.begin(); displayNodeIt != displayNodes.end(); ++displayNodeIt)
  {
    this->Internal->RemoveDisplayNode(*displayNodeIt);
  }
  this->Internal->RemoveObservations(node);
  this->Internal->DisplayableToDisplayNodes.erase(displayableIt);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  vtkMRMLDisplayableNode* displayableNode = vtkMRMLDisplayableNode::SafeDownCast(node);
  if (!displayableNode)
  {
    return;
  }

  // Escape if the scene is being closed, imported or connected
  if (this->GetMRMLScene()->IsBatchProcessing())
  {
    this->SetUpdateFromMRMLRequested(true);
    return;
  }

  this->AddDisplayableNode(displayableNode);
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  bool modified = false;
  vtkMRMLDisplayableNode* displayableNode = vtkMRMLDisplayableNode::SafeDownCast(node);
  if (displayableNode)
  {
    this->RemoveDisplayableNode(displayableNode);
    modified = true;
  }
  else
  {
    vtkMRMLDisplayNode* displayNode = vtkMRMLDisplayNode::SafeDownCast(node);
    if (displayNode)
    {
      this->Internal->RemoveDisplayNode(displayNode);
      modified = true;
    }
  }
  if (modified)
  {
    this->RequestRender();
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  if (scene == nullptr || scene->IsBatchProcessing())
  {
    return;
  }

  vtkMRMLDisplayableNode* displayableNode = vtkMRMLDisplayableNode::SafeDownCast(caller);
  if (!displayableNode)
  {
    this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
    return;
  }

  if (event == vtkMRMLDisplayableNode::DisplayModifiedEvent)
  {
    vtkMRMLDisplayNode* displayNode = vtkMRMLDisplayNode::SafeDownCast(reinterpret_cast<vtkMRMLNode*>(callData));
    if (displayNode && this->Internal->UseDisplayNode(displayNode))
    {
      // A vector field display node was added to the node or one of its properties changed
      this->Internal->DisplayableToDisplayNodes[displayableNode].insert(displayNode);
      this->Internal->UpdateDisplayNode(displayNode);
      this->RequestRender();
    }
    else if (this->Internal->HasDisplayPipeline(displayableNode))
    {
      this->Internal->UpdateDisplayableTransforms(displayableNode);
      this->RequestRender();
    }
  }
  else if (event == vtkMRMLDisplayableNode::TransformModifiedEvent //
           || event == vtkMRMLModelNode::PolyDataModifiedEvent     //
           || event == vtkMRMLVolumeNode::ImageDataModifiedEvent)
  {
    if (!this->Internal->HasDisplayPipeline(displayableNode))
    {
      // Nothing is drawn for this node, there is nothing to update or to re-render
      return;
    }
    // The pipeline decides for itself whether it has to re-execute; only the parent
    // transform has to be pushed into it, and a new render has to be requested.
    this->Internal->UpdateDisplayableTransforms(displayableNode);
    this->RequestRender();
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::UpdateFromMRML()
{
  this->SetUpdateFromMRMLRequested(false);

  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene)
  {
    vtkDebugMacro("vtkMRMLVectorFieldDisplayableManager->UpdateFromMRML: Scene is not set.");
    return;
  }

  // Find the display nodes that need to be removed
  // (not deleted immediately because it would invalidate the pipeline iteration)
  std::set<vtkMRMLDisplayableNode*> displayableNodesToRemove;
  std::deque<vtkMRMLDisplayNode*> displayNodesToRemove;
  for (auto displayNodeToPipeline : this->Internal->DisplayPipelines)
  {
    vtkMRMLDisplayNode* displayNode = displayNodeToPipeline.first;
    if (!displayNode)
    {
      continue;
    }
    if (!scene->IsNodePresent(displayNode))
    {
      displayNodesToRemove.push_back(displayNode);
      continue;
    }
    vtkMRMLDisplayableNode* displayableNode = displayNode->GetDisplayableNode();
    if (displayableNodesToRemove.find(displayableNode) != displayableNodesToRemove.end())
    {
      continue;
    }
    if (!this->Internal->UseDisplayableNode(displayableNode))
    {
      displayableNodesToRemove.insert(displayableNode);
      continue;
    }
    if (!this->Internal->UseDisplayNode(displayNode))
    {
      displayNodesToRemove.push_back(displayNode);
      continue;
    }
  }
  for (vtkMRMLDisplayableNode* displayableNodeToRemove : displayableNodesToRemove)
  {
    this->RemoveDisplayableNode(displayableNodeToRemove);
  }
  for (vtkMRMLDisplayNode* displayNodeToRemove : displayNodesToRemove)
  {
    this->Internal->RemoveDisplayNode(displayNodeToRemove);
  }

  std::vector<vtkMRMLNode*> displayableNodes;
  int numberOfDisplayableNodes = scene->GetNodesByClass("vtkMRMLDisplayableNode", displayableNodes);
  for (int i = 0; i < numberOfDisplayableNodes; i++)
  {
    vtkMRMLDisplayableNode* displayableNode = vtkMRMLDisplayableNode::SafeDownCast(displayableNodes[i]);
    if (displayableNode && this->Internal->UseDisplayableNode(displayableNode))
    {
      this->AddDisplayableNode(displayableNode);
    }
  }
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::UnobserveMRMLScene()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::OnMRMLSceneStartClose()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::OnMRMLSceneEndClose()
{
  this->SetUpdateFromMRMLRequested(true);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::OnMRMLSceneEndBatchProcess()
{
  this->SetUpdateFromMRMLRequested(true);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldDisplayableManager::Create()
{
  this->SetUpdateFromMRMLRequested(true);
}
