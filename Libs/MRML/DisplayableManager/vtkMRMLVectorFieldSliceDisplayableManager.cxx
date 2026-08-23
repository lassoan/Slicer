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
#include "vtkMRMLVectorFieldSliceDisplayableManager.h"
#include "vtkMRMLVectorFieldGlyphSource.h"

// MRML includes
#include <vtkEventBroker.h>
#include <vtkMRMLColorNode.h>
#include <vtkMRMLDisplayableNode.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLFolderDisplayNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLVectorFieldDisplayNode.h>
#include <vtkMRMLVectorFieldModePipeline.h>
#include <vtkMRMLVectorFieldSampler.h>
#include <vtkMRMLVolumeNode.h>
#include <vtkProjectVectorsToPlane.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkArrayCalculator.h>
#include <vtkCollection.h>
#include <vtkDataArray.h>
#include <vtkGeneralTransform.h>
#include <vtkGlyph3D.h>
#include <vtkLookupTable.h>
#include <vtkMaskPoints.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPolyDataAlgorithm.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkSampleImplicitFunctionFilter.h>
#include <vtkSmartPointer.h>
#include <vtkThresholdPoints.h>
#include <vtkTransform.h>
#include <vtkTransformFilter.h>
#include <vtkTransformPolyDataFilter.h>

// STD includes
#include <deque>
#include <map>
#include <set>
#include <string>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLVectorFieldSliceDisplayableManager);

namespace
{
/// Name of the point data array that stores the signed distance of each mesh
/// point from the slice plane. Used for selecting the points that are within
/// the displayed slab.
const char* ColorMagnitudeArrayName = "GlyphColorMagnitude";
const char* SliceDistanceArrayName = "GlyphSliceDistance";
/// Appended to the name of the orientation array to name the array of vectors that were
/// projected onto the slice plane.
const char* ProjectedArrayNameSuffix = " projected";
} // namespace

//---------------------------------------------------------------------------
class vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal
{
public:
  struct Pipeline
  {
    /// Resamples the field in the plane of this slice view. Only used for sources that can
    /// do that (volumes, transforms); it is owned by the pipeline and not by the display
    /// node, because the same display node can be shown in several slice views at once.
    vtkSmartPointer<vtkMRMLVectorFieldSampler> SliceSampler;
    /// Geometry of the grid, contour and streamline modes in this slice view.
    vtkSmartPointer<vtkMRMLVectorFieldModePipeline> ModePipeline;
    vtkSmartPointer<vtkGeneralTransform> NodeToWorld;
    vtkSmartPointer<vtkTransformFilter> ModelWarper;
    vtkSmartPointer<vtkPlane> Plane;
    vtkSmartPointer<vtkSampleImplicitFunctionFilter> SliceDistance;
    vtkSmartPointer<vtkThresholdPoints> SlabThreshold;
    vtkSmartPointer<vtkProjectVectorsToPlane> Projector;
    vtkSmartPointer<vtkThresholdPoints> ScalarThreshold;
    vtkSmartPointer<vtkMaskPoints> MaskPoints;
    vtkSmartPointer<vtkGlyph3D> Glypher;
    vtkSmartPointer<vtkTransform> TransformToSlice;
    vtkSmartPointer<vtkArrayCalculator> ColorMagnitude;
    vtkSmartPointer<vtkTransformFilter> GlyphInputToSlice;
    vtkSmartPointer<vtkTransform> GlyphInputToSliceTransform;
    vtkSmartPointer<vtkTransformPolyDataFilter> Transformer;
    vtkSmartPointer<vtkPolyDataMapper2D> Mapper;
    vtkSmartPointer<vtkActor2D> Actor;
  };

  typedef std::map<vtkMRMLDisplayNode*, const Pipeline*> PipelinesCacheType;
  PipelinesCacheType DisplayPipelines;

  typedef std::map<vtkMRMLDisplayableNode*, std::set<vtkMRMLDisplayNode*>> ModelToDisplayCacheType;
  ModelToDisplayCacheType ModelToDisplayNodes;

  // Transforms
  void UpdateDisplayableTransforms(vtkMRMLDisplayableNode* node);
  void GetNodeTransformToWorld(vtkMRMLTransformableNode* node, vtkGeneralTransform* transformToWorld);
  // Slice Node
  void SetSliceNode(vtkMRMLSliceNode* sliceNode);
  void UpdateSliceNode();
  void GetSliceFieldOfViewSizeMm(double fieldOfViewSizeMm[2]);
  void SetSlicePlaneFromMatrix(vtkMatrix4x4* matrix, vtkPlane* plane);

  // Display Nodes
  void AddDisplayNode(vtkMRMLDisplayableNode*, vtkMRMLDisplayNode*);
  void UpdateDisplayNode(vtkMRMLDisplayNode* displayNode);
  void UpdateDisplayNodePipeline(vtkMRMLDisplayNode*, const Pipeline*);
  void UpdateSliceMapperColoring(vtkMRMLVectorFieldDisplayNode* fieldDisplayNode, vtkMRMLDisplayNode* effectiveDisplayNode, const Pipeline* pipeline, double hierarchyOpacity);
  void UpdateSliceActorProperties(vtkMRMLDisplayNode* effectiveDisplayNode, const Pipeline* pipeline, double hierarchyOpacity);
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

  vtkInternal(vtkMRMLVectorFieldSliceDisplayableManager* external);
  ~vtkInternal();

private:
  vtkSmartPointer<vtkMatrix4x4> SliceXYToRAS;
  vtkSmartPointer<vtkMRMLSliceNode> SliceNode;
  vtkMRMLVectorFieldSliceDisplayableManager* External;
};

//---------------------------------------------------------------------------
// vtkInternal methods
vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::vtkInternal(vtkMRMLVectorFieldSliceDisplayableManager* external)
{
  this->External = external;
  this->SliceXYToRAS = vtkSmartPointer<vtkMatrix4x4>::New();
  this->SliceXYToRAS->Identity();
}

//---------------------------------------------------------------------------
vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::~vtkInternal()
{
  this->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UseDisplayNode(vtkMRMLDisplayNode* displayNode)
{
  return displayNode && displayNode->IsA("vtkMRMLVectorFieldDisplayNode");
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::IsVisible(vtkMRMLDisplayNode* displayNode)
{
  if (!displayNode)
  {
    return false;
  }
  bool visibleOnNode = true;
  if (this->SliceNode)
  {
    visibleOnNode = displayNode->GetVisibility() && displayNode->IsDisplayableInView(this->SliceNode->GetID());
  }
  else
  {
    visibleOnNode = (displayNode->GetVisibility() == 1);
  }
  return visibleOnNode && (displayNode->GetVisibility2D() != 0);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::SetSliceNode(vtkMRMLSliceNode* sliceNode)
{
  if (!sliceNode || this->SliceNode == sliceNode)
  {
    return;
  }
  this->SliceNode = sliceNode;
  this->UpdateSliceNode();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UpdateSliceNode()
{
  // Update the slice node transform, then update the pipelines to account for the new plane location
  this->SliceXYToRAS->DeepCopy(this->SliceNode->GetXYToRAS());
  for (auto it = this->DisplayPipelines.begin(); it != this->DisplayPipelines.end(); ++it)
  {
    this->UpdateDisplayNodePipeline(it->first, it->second);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::GetSliceFieldOfViewSizeMm(double fieldOfViewSizeMm[2])
{
  fieldOfViewSizeMm[0] = 0.0;
  fieldOfViewSizeMm[1] = 0.0;
  if (!this->SliceNode)
  {
    return;
  }
  double fieldOfView[3] = { 0.0, 0.0, 0.0 };
  this->SliceNode->GetFieldOfView(fieldOfView);
  fieldOfViewSizeMm[0] = fieldOfView[0];
  fieldOfViewSizeMm[1] = fieldOfView[1];
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::SetSlicePlaneFromMatrix(vtkMatrix4x4* sliceMatrix, vtkPlane* plane)
{
  double normal[3];
  double origin[3];

  // +/-1: orientation of the normal
  const int planeOrientation = 1;
  for (int i = 0; i < 3; i++)
  {
    normal[i] = planeOrientation * sliceMatrix->GetElement(i, 2);
    origin[i] = sliceMatrix->GetElement(i, 3);
  }

  vtkMath::Normalize(normal);
  plane->SetNormal(normal);
  plane->SetOrigin(origin);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::GetNodeTransformToWorld(vtkMRMLTransformableNode* node, vtkGeneralTransform* transformToWorld)
{
  if (!node || !transformToWorld)
  {
    return;
  }
  vtkMRMLTransformNode* tnode = node->GetParentTransformNode();
  transformToWorld->Identity();
  if (tnode)
  {
    tnode->GetTransformToWorld(transformToWorld);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UpdateDisplayableTransforms(vtkMRMLDisplayableNode* mNode)
{
  PipelinesCacheType::iterator pipelinesIter;
  std::set<vtkMRMLDisplayNode*> displayNodes = this->ModelToDisplayNodes[mNode];
  for (auto dnodesIter = displayNodes.begin(); dnodesIter != displayNodes.end(); dnodesIter++)
  {
    if (((pipelinesIter = this->DisplayPipelines.find(*dnodesIter)) != this->DisplayPipelines.end()))
    {
      this->GetNodeTransformToWorld(mNode, pipelinesIter->second->NodeToWorld);
      this->UpdateDisplayNodePipeline(pipelinesIter->first, pipelinesIter->second);
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::RemoveDisplayNode(vtkMRMLDisplayNode* displayNode)
{
  PipelinesCacheType::iterator actorsIt = this->DisplayPipelines.find(displayNode);
  if (actorsIt == this->DisplayPipelines.end())
  {
    return;
  }
  const Pipeline* pipeline = actorsIt->second;
  this->External->GetRenderer()->RemoveActor(pipeline->Actor);
  delete pipeline;
  this->DisplayPipelines.erase(actorsIt);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::AddDisplayNode(vtkMRMLDisplayableNode* mNode, vtkMRMLDisplayNode* displayNode)
{
  if (!mNode || !displayNode)
  {
    return;
  }

  // Do not add the display node if it is already associated with a pipeline object
  if (this->DisplayPipelines.find(displayNode) != this->DisplayPipelines.end())
  {
    return;
  }

  Pipeline* pipeline = new Pipeline();
  // Sources that can be resampled in an arbitrary plane get their own sampler for this
  // slice view. It is created once, here, and only reconfigured afterwards.
  vtkMRMLVectorFieldDisplayNode* fieldDisplayNode = vtkMRMLVectorFieldDisplayNode::SafeDownCast(displayNode);
  if (fieldDisplayNode)
  {
    pipeline->SliceSampler = fieldDisplayNode->CreateSliceFieldSampler();
  }
  pipeline->NodeToWorld = vtkSmartPointer<vtkGeneralTransform>::New();
  pipeline->ModelWarper = vtkSmartPointer<vtkTransformFilter>::New();
  pipeline->Plane = vtkSmartPointer<vtkPlane>::New();
  pipeline->SliceDistance = vtkSmartPointer<vtkSampleImplicitFunctionFilter>::New();
  pipeline->SlabThreshold = vtkSmartPointer<vtkThresholdPoints>::New();
  pipeline->ScalarThreshold = vtkSmartPointer<vtkThresholdPoints>::New();
  pipeline->MaskPoints = vtkSmartPointer<vtkMaskPoints>::New();
  pipeline->Projector = vtkSmartPointer<vtkProjectVectorsToPlane>::New();
  pipeline->ModePipeline = vtkSmartPointer<vtkMRMLVectorFieldModePipeline>::New();
  pipeline->Glypher = vtkSmartPointer<vtkGlyph3D>::New();
  pipeline->ColorMagnitude = vtkSmartPointer<vtkArrayCalculator>::New();
  pipeline->ColorMagnitude->SetAttributeTypeToPointData();
  pipeline->GlyphInputToSliceTransform = vtkSmartPointer<vtkTransform>::New();
  pipeline->GlyphInputToSlice = vtkSmartPointer<vtkTransformFilter>::New();
  pipeline->TransformToSlice = vtkSmartPointer<vtkTransform>::New();
  pipeline->Transformer = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  pipeline->Mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
  pipeline->Actor = vtkSmartPointer<vtkActor2D>::New();

  // Set up the static parts of the pipeline. The point selection filters between
  // the slab selection and the glypher are rewired on each update, because
  // thresholding by the active scalar is optional (see UpdateDisplayNodePipeline).
  pipeline->ModelWarper->SetTransform(pipeline->NodeToWorld);
  pipeline->SliceDistance->SetImplicitFunction(pipeline->Plane);
  pipeline->SliceDistance->SetScalarArrayName(SliceDistanceArrayName);
  pipeline->SliceDistance->ComputeGradientsOff();
  pipeline->SliceDistance->SetInputConnection(pipeline->ModelWarper->GetOutputPort());
  pipeline->SlabThreshold->SetInputConnection(pipeline->SliceDistance->GetOutputPort());
  pipeline->SlabThreshold->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, SliceDistanceArrayName);
  pipeline->MaskPoints->GenerateVerticesOff();
  // The glyphs are built in the slice view's own coordinate system, not in RAS. A flat 2D
  // glyph placed in RAS would be turned out of the slice plane by the rotation that
  // vtkGlyph3D applies to point it along the vector, and would be seen edge-on in every
  // view whose plane is not perpendicular to the rotation axis - a sagittal view showed
  // bare lines instead of arrows. In slice coordinates the glyph source lies in the XY
  // plane and the vectors are in that plane too, so the rotation is about the view normal
  // and the glyph stays flat.
  pipeline->GlyphInputToSlice->SetTransform(pipeline->GlyphInputToSliceTransform);
  pipeline->GlyphInputToSlice->TransformAllInputVectorsOn();
  pipeline->Transformer->SetTransform(pipeline->TransformToSlice);
  pipeline->Mapper->SetInputConnection(pipeline->Transformer->GetOutputPort());
  pipeline->Actor->SetMapper(pipeline->Mapper);
  pipeline->Actor->SetVisibility(0);

  // Add actor to Renderer and local cache
  this->External->GetRenderer()->AddActor(pipeline->Actor);
  this->DisplayPipelines.insert(std::make_pair(displayNode, pipeline));

  // Update cached matrices. Calls UpdateDisplayNodePipeline
  this->UpdateDisplayableTransforms(mNode);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UpdateDisplayNode(vtkMRMLDisplayNode* displayNode)
{
  if (!displayNode)
  {
    return;
  }
  PipelinesCacheType::iterator it = this->DisplayPipelines.find(displayNode);
  if (it != this->DisplayPipelines.end())
  {
    this->UpdateDisplayNodePipeline(displayNode, it->second);
  }
  else
  {
    this->External->AddDisplayableNode(displayNode->GetDisplayableNode());
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UpdateDisplayNodePipeline(vtkMRMLDisplayNode* displayNode, const Pipeline* pipeline)
{
  if (!pipeline)
  {
    vtkErrorWithObjectMacro(this->External, "vtkMRMLVectorFieldSliceDisplayableManager::UpdateDisplayNodePipeline failed: pipeline is invalid");
    return;
  }

  vtkMRMLVectorFieldDisplayNode* glyphDisplayNode = vtkMRMLVectorFieldDisplayNode::SafeDownCast(displayNode);
  if (!glyphDisplayNode)
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
  if (displayNode->GetFolderDisplayOverrideAllowed())
  {
    if (overrideHierarchyDisplayNode)
    {
      displayNode = overrideHierarchyDisplayNode;
    }
    hierarchyVisibility = vtkMRMLFolderDisplayNode::GetHierarchyVisibility(displayableNode);
    hierarchyOpacity = vtkMRMLFolderDisplayNode::GetHierarchyOpacity(displayableNode);
  }

  if (!hierarchyVisibility || !this->IsVisible(glyphDisplayNode))
  {
    pipeline->Actor->SetVisibility(false);
    return;
  }

  // Where the points that get a glyph come from. Sources that can be resampled in an
  // arbitrary plane (vector volumes, transforms) are sampled directly in the slice plane,
  // which gives an evenly spaced lattice of glyphs. For the other sources (the point data
  // of a mesh) the points that are within a slab around the slice plane are selected.
  vtkAlgorithmOutput* glyphInputConnection = nullptr;
  if (pipeline->SliceSampler)
  {
    double fieldOfViewSizeMm[2] = { 0.0, 0.0 };
    this->GetSliceFieldOfViewSizeMm(fieldOfViewSizeMm);
    pipeline->SliceSampler->SetSlicePlane(this->SliceXYToRAS, fieldOfViewSizeMm);
    // The spacing the field is really sampled at, not SamplingSpacingMm: nodes that keep a
    // separate spacing per visualization mode (transform display) return the one that applies
    // to the current mode, refined so that the grid lines land a whole grid cell apart.
    pipeline->SliceSampler->SetSamplingSpacingMm(glyphDisplayNode->GetSamplingSpacingForFieldMm());
    pipeline->SliceSampler->SetLatticeGroupSize(glyphDisplayNode->GetGridSubdivision());
    // Glyphs at the points of another node are shown in slice views too, where the ones the
    // slice does not cross are left out.
    vtkNew<vtkPoints> samplePositions_RAS;
    if (glyphDisplayNode->GetEffectiveMaskingMode() == vtkMRMLVectorFieldDisplayNode::MaskingModeNodePoints //
        && glyphDisplayNode->GetSamplePositions(samplePositions_RAS))
    {
      pipeline->SliceSampler->SetSamplePositions(samplePositions_RAS);
    }
    else
    {
      pipeline->SliceSampler->SetSamplePositions(nullptr);
    }
    double sliceFieldOfView[3] = { 0.0, 0.0, 0.0 };
    this->SliceNode->GetFieldOfView(sliceFieldOfView);
    pipeline->SliceSampler->SetSliceThicknessMm(sliceFieldOfView[2]);
    glyphInputConnection = pipeline->SliceSampler->GetOutputPort();
  }
  else
  {
    vtkAlgorithmOutput* fieldConnection = glyphDisplayNode->GetFieldConnection();
    if (!fieldConnection)
    {
      pipeline->Actor->SetVisibility(false);
      return;
    }
    pipeline->ModelWarper->SetInputConnection(fieldConnection);

    // Select the points that are within the displayed slab around the slice plane
    this->SetSlicePlaneFromMatrix(this->SliceXYToRAS, pipeline->Plane);
    pipeline->Plane->Modified();
    // The slab is as thick as the slice itself, so that the points that are drawn are the
    // ones the slice shows.
    double fieldOfView[3] = { 0.0, 0.0, 0.0 };
    this->SliceNode->GetFieldOfView(fieldOfView);
    double slabHalfThickness = 0.5 * (fieldOfView[2] > 0.0 ? fieldOfView[2] : 1.0);
    if (slabHalfThickness <= 0.0)
    {
      pipeline->Actor->SetVisibility(false);
      return;
    }
    pipeline->SlabThreshold->SetLowerThreshold(-slabHalfThickness);
    pipeline->SlabThreshold->SetUpperThreshold(slabHalfThickness);
    pipeline->SlabThreshold->SetThresholdFunction(vtkThresholdPoints::THRESHOLD_BETWEEN);
    glyphInputConnection = pipeline->SlabThreshold->GetOutputPort();
  }

  // The modes that draw poly data (grid, contour, streamline) replace the glyph pipeline:
  // the geometry is built from the field that was sampled in this slice plane, and drawn
  // with a plain 2D mapper.
  if (glyphDisplayNode->IsPolyDataVisualizationMode())
  {
    if (pipeline->SliceSampler)
    {
      pipeline->SliceSampler->GenerateCellsOn();
    }
    vtkAlgorithmOutput* polyDataConnection = pipeline->ModePipeline->Update(glyphDisplayNode, glyphInputConnection, true);
    if (!polyDataConnection)
    {
      pipeline->Actor->SetVisibility(false);
      return;
    }
    pipeline->Transformer->SetInputConnection(polyDataConnection);
    pipeline->Mapper->SetInputConnection(pipeline->Transformer->GetOutputPort());
    this->UpdateSliceMapperColoring(glyphDisplayNode, displayNode, pipeline, hierarchyOpacity);
    this->UpdateSliceActorProperties(displayNode, pipeline, hierarchyOpacity);
    return;
  }
  // Glyphs are drawn in slice coordinates, so they go to the mapper as they are
  pipeline->Transformer->SetInputConnection(nullptr);
  pipeline->Mapper->SetInputConnection(pipeline->Glypher->GetOutputPort());

  // Optional thresholding by the active scalar array
  const char* activeScalarName = glyphDisplayNode->GetActiveScalarName();
  bool hasActiveScalar = (activeScalarName && activeScalarName[0] != '\0');
  // Multi-component arrays are colored and thresholded by their magnitude
  vtkDataArray* activeScalarArray = glyphDisplayNode->GetActiveScalarArray();
  int numberOfScalarComponents = activeScalarArray ? activeScalarArray->GetNumberOfComponents() : 1;
  bool useScalarMagnitude = (numberOfScalarComponents > 1);
  if (glyphDisplayNode->GetThresholdEnabled() && hasActiveScalar)
  {
    double thresholdRange[2] = { 0.0, -1.0 };
    glyphDisplayNode->GetThresholdRange(thresholdRange);
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

  // Masking: which of the points in the slab get a glyph. Only the point-based
  // modes are applicable here, because the slab selection leaves points without
  // cells; the surface and volume sampling modes therefore sample the bounds
  // of the selected points.
  switch (glyphDisplayNode->GetMaskingMode())
  {
    case vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds:
    case vtkMRMLVectorFieldDisplayNode::MaskingModeUniformSurface:
    case vtkMRMLVectorFieldDisplayNode::MaskingModeUniformVolume:
      pipeline->MaskPoints->RandomModeOn();
      pipeline->MaskPoints->SetRandomModeType(vtkMaskPoints::UNIFORM_SPATIAL_BOUNDS);
      pipeline->MaskPoints->SetMaximumNumberOfPoints(glyphDisplayNode->GetMaskingPointsNumber());
      break;
    case vtkMRMLVectorFieldDisplayNode::MaskingModeAllPoints:
    default:
      pipeline->MaskPoints->RandomModeOff();
      pipeline->MaskPoints->SetOnRatio(1);
      break;
  }

  const char* orientationArrayName = glyphDisplayNode->GetOrientationArrayName();
  bool hasOrientationArray = (orientationArrayName && orientationArrayName[0] != '\0');

  // Project the vectors onto the slice plane, so that the length of a glyph shows the
  // in-plane component instead of a foreshortened 3D vector. The projected vectors go into
  // an array of their own: the original vectors and the magnitude scalars stay untouched, so
  // the glyphs are still colored by the true magnitude of the vector and not by the part of
  // it that happens to lie in the plane.
  std::string glyphOrientationArrayName = (hasOrientationArray ? orientationArrayName : "");
  // What feeds the glyphs, before they are taken into slice coordinates
  vtkAlgorithmOutput* glyphInputConnectionForColor = nullptr;
  if (glyphDisplayNode->GetSliceProjectionEnabled() && hasOrientationArray)
  {
    double sliceNormal_RAS[3] = { this->SliceXYToRAS->GetElement(0, 2), //
                                  this->SliceXYToRAS->GetElement(1, 2),
                                  this->SliceXYToRAS->GetElement(2, 2) };
    glyphOrientationArrayName = std::string(orientationArrayName) + ProjectedArrayNameSuffix;
    pipeline->Projector->SetPlaneNormal(sliceNormal_RAS);
    pipeline->Projector->SetVectorArrayName(orientationArrayName);
    pipeline->Projector->SetOutputVectorArrayName(glyphOrientationArrayName.c_str());
    pipeline->Projector->SetInputConnection(pipeline->MaskPoints->GetOutputPort());
    glyphInputConnectionForColor = pipeline->Projector->GetOutputPort();
  }
  else
  {
    pipeline->Projector->SetInputConnection(nullptr);
    glyphInputConnectionForColor = pipeline->MaskPoints->GetOutputPort();
  }
  pipeline->GlyphInputToSlice->SetInputConnection(glyphInputConnectionForColor);
  // Coloring by a vector array means coloring by its magnitude, and that magnitude has to
  // be the one the vector has in RAS: the transform into slice coordinates below scales the
  // vectors, so a magnitude taken after it would color the same vector differently in a
  // slice view than in a 3D view. It is computed here, into a scalar array that the
  // transform leaves alone.
  const char* activeScalarNameForColor = glyphDisplayNode->GetActiveScalarName();
  vtkDataArray* activeScalarArrayForColor = glyphDisplayNode->GetActiveScalarArray();
  bool colorByMagnitude = (activeScalarNameForColor && activeScalarNameForColor[0] != '\0' //
                           && activeScalarArrayForColor && activeScalarArrayForColor->GetNumberOfComponents() > 1);
  if (colorByMagnitude)
  {
    pipeline->ColorMagnitude->SetInputConnection(glyphInputConnectionForColor);
    pipeline->ColorMagnitude->RemoveAllVariables();
    pipeline->ColorMagnitude->AddVectorArrayName(activeScalarNameForColor);
    pipeline->ColorMagnitude->SetResultArrayName(ColorMagnitudeArrayName);
    pipeline->ColorMagnitude->SetFunction((std::string("mag(") + activeScalarNameForColor + ")").c_str());
    pipeline->GlyphInputToSlice->SetInputConnection(pipeline->ColorMagnitude->GetOutputPort());
  }
  else
  {
    pipeline->ColorMagnitude->SetInputConnection(nullptr);
  }

  // RAS to slice XY, for the points and for the vectors that orient and scale the glyphs.
  // The vectors are scaled by it as well, which is what makes a vector of a given length in
  // mm come out that long on the slice.
  vtkNew<vtkMatrix4x4> rasToXY;
  vtkMatrix4x4::Invert(this->SliceXYToRAS, rasToXY.GetPointer());
  pipeline->GlyphInputToSliceTransform->SetMatrix(rasToXY.GetPointer());
  pipeline->Glypher->SetInputConnection(pipeline->GlyphInputToSlice->GetOutputPort());

  // Glyph geometry. Slice views draw flat 2D glyphs (as the transform display does):
  // their outlines stay crisp at any zoom level and they do not hide the image underneath.
  vtkNew<vtkTransform> glyphSourceTransform;
  vtkSmartPointer<vtkPolyDataAlgorithm> glyphSource = vtkMRMLVectorFieldGlyphSource::Create2D(glyphDisplayNode, glyphSourceTransform);
  // The glyph source is generated in the XY plane, which is the plane the glyphs are drawn
  // in, so only the source's own transform is needed.
  pipeline->Glypher->SetSourceTransform(glyphSourceTransform);
  pipeline->Glypher->SetSourceConnection(glyphSource->GetOutputPort());

  // Orientation
  if (hasOrientationArray)
  {
    pipeline->Glypher->OrientOn();
    pipeline->Glypher->SetVectorModeToUseVector();
    pipeline->Glypher->SetInputArrayToProcess(1, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, glyphOrientationArrayName.c_str());
  }
  else
  {
    pipeline->Glypher->OrientOff();
  }

  // Scale. vtkGlyph3D scales by the orientation vectors, so an independent
  // 3-component scale array can only be used when it is also the array that
  // orients the glyphs; otherwise glyphs are scaled uniformly.
  pipeline->Glypher->SetScaleFactor(glyphDisplayNode->GetScaleFactor());
  const char* scaleArrayName = glyphDisplayNode->GetEffectiveScaleArrayName();
  bool hasScaleArray = (scaleArrayName && scaleArrayName[0] != '\0');
  vtkDataArray* scaleArray = glyphDisplayNode->GetEffectiveScaleArray();
  int scaleArrayComponents = scaleArray ? scaleArray->GetNumberOfComponents() : 0;
  if (hasScaleArray && scaleArrayComponents == 1)
  {
    pipeline->Glypher->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, scaleArrayName);
    pipeline->Glypher->SetScaleModeToScaleByScalar();
  }
  else if (hasScaleArray && hasOrientationArray && strcmp(scaleArrayName, orientationArrayName) == 0 //
           && (scaleArrayComponents == 3 || scaleArrayComponents == 0))
  {
    if (glyphDisplayNode->GetVectorScaleMode() == vtkMRMLVectorFieldDisplayNode::VectorScaleModeByComponents)
    {
      pipeline->Glypher->SetScaleModeToScaleByVectorComponents();
    }
    else
    {
      pipeline->Glypher->SetScaleModeToScaleByVector();
    }
  }
  else
  {
    pipeline->Glypher->SetScaleModeToDataScalingOff();
  }

  this->UpdateSliceMapperColoring(glyphDisplayNode, displayNode, pipeline, hierarchyOpacity);
  this->UpdateSliceActorProperties(displayNode, pipeline, hierarchyOpacity);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UpdateSliceMapperColoring(vtkMRMLVectorFieldDisplayNode* fieldDisplayNode,
                                                                                      vtkMRMLDisplayNode* effectiveDisplayNode,
                                                                                      const Pipeline* pipeline,
                                                                                      double hierarchyOpacity)
{
  // Coloring: either by the active point data scalar array through the color node's
  // lookup table, or by the display node's solid color.
  const char* activeScalarName = fieldDisplayNode->GetActiveScalarName();
  bool hasActiveScalar = (activeScalarName && activeScalarName[0] != 0);
  if (!fieldDisplayNode->GetScalarVisibility() || !hasActiveScalar)
  {
    pipeline->Glypher->SetColorModeToColorByScale();
    pipeline->Mapper->SetScalarVisibility(false);
    return;
  }
  // Multi-component arrays are colored by their magnitude, which the pipeline computed
  // into a scalar array of its own before the vectors were taken into slice coordinates
  vtkDataArray* activeScalarArray = fieldDisplayNode->GetActiveScalarArray();
  bool useScalarMagnitude = (activeScalarArray && activeScalarArray->GetNumberOfComponents() > 1);
  std::string colorArrayName = (useScalarMagnitude ? ColorMagnitudeArrayName : activeScalarName);

  pipeline->Glypher->SetColorModeToColorByScalar();
  pipeline->Glypher->SetInputArrayToProcess(3, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, colorArrayName.c_str());
  // A copy of the color node's lookup table is used, because the alpha, the scalar
  // range and the vector mode are set on it and it must not be shared between mappers.
  // If no color node is set then a default table is used, so that the glyphs are
  // still colored by the scalar values.
  vtkSmartPointer<vtkLookupTable> lookupTable =
    vtkSmartPointer<vtkLookupTable>::Take(fieldDisplayNode->GetColorNode() ? fieldDisplayNode->GetColorNode()->CreateLookupTableCopy() : nullptr);
  if (!lookupTable)
  {
    lookupTable = vtkSmartPointer<vtkLookupTable>::New();
    lookupTable->Build();
  }
  lookupTable->SetAlpha(hierarchyOpacity * effectiveDisplayNode->GetSliceIntersectionOpacity());

  pipeline->Mapper->SetLookupTable(lookupTable);
  pipeline->Mapper->SetColorModeToMapScalars();
  pipeline->Mapper->SetScalarModeToUsePointFieldData();
  pipeline->Mapper->ColorByArrayComponent(const_cast<char*>(colorArrayName.c_str()), 0);
  pipeline->Mapper->UseLookupTableScalarRangeOff();
  pipeline->Mapper->SetScalarRange(fieldDisplayNode->GetScalarRange());
  pipeline->Mapper->SetScalarVisibility(true);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UpdateSliceActorProperties(vtkMRMLDisplayNode* effectiveDisplayNode,
                                                                                       const Pipeline* pipeline,
                                                                                       double hierarchyOpacity)
{
  // Map the geometry from RAS into the slice view's XY coordinate system
  vtkNew<vtkMatrix4x4> rasToXY;
  vtkMatrix4x4::Invert(this->SliceXYToRAS, rasToXY.GetPointer());
  pipeline->TransformToSlice->SetMatrix(rasToXY.GetPointer());

  vtkProperty2D* actorProperties = pipeline->Actor->GetProperty();
  actorProperties->SetColor(effectiveDisplayNode->GetColor());
  actorProperties->SetLineWidth(effectiveDisplayNode->GetSliceIntersectionThickness());
  actorProperties->SetOpacity(hierarchyOpacity * effectiveDisplayNode->GetSliceIntersectionOpacity());

  pipeline->Actor->SetPosition(0, 0);
  pipeline->Actor->SetVisibility(true);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::AddObservations(vtkMRMLDisplayableNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();

  if (!broker->GetObservationExist(node, vtkMRMLDisplayableNode::TransformModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(node, vtkMRMLDisplayableNode::TransformModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  }
  if (!broker->GetObservationExist(node, vtkMRMLDisplayableNode::DisplayModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(node, vtkMRMLDisplayableNode::DisplayModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  }
  if (!broker->GetObservationExist(node, vtkMRMLModelNode::PolyDataModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(node, vtkMRMLModelNode::PolyDataModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  }
  if (!broker->GetObservationExist(node, vtkMRMLVolumeNode::ImageDataModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand()))
  {
    broker->AddObservation(node, vtkMRMLVolumeNode::ImageDataModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::RemoveObservations(vtkMRMLDisplayableNode* node)
{
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  vtkEventBroker::ObservationVector observations;
  observations = broker->GetObservations(node, vtkMRMLVolumeNode::ImageDataModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  broker->RemoveObservations(observations);
  observations = broker->GetObservations(node, vtkMRMLModelNode::PolyDataModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  broker->RemoveObservations(observations);
  observations = broker->GetObservations(node, vtkMRMLDisplayableNode::DisplayModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  broker->RemoveObservations(observations);
  observations = broker->GetObservations(node, vtkMRMLDisplayableNode::TransformModifiedEvent, this->External, this->External->GetMRMLNodesCallbackCommand());
  broker->RemoveObservations(observations);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::ClearDisplayableNodes()
{
  while (this->ModelToDisplayNodes.size() > 0)
  {
    this->External->RemoveDisplayableNode(this->ModelToDisplayNodes.begin()->first);
  }
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::UseDisplayableNode(vtkMRMLDisplayableNode* node)
{
  // Any displayable node can store a vector field (a model's point data, a vector volume,
  // a transform, ...), and a vector field display node can be added to it at any time, so
  // all of them are observed. Nodes that have no vector field display node simply have no
  // pipeline; see HasDisplayPipeline().
  return node != nullptr;
}

//---------------------------------------------------------------------------
bool vtkMRMLVectorFieldSliceDisplayableManager::vtkInternal::HasDisplayPipeline(vtkMRMLDisplayableNode* node)
{
  auto displayableIt = this->ModelToDisplayNodes.find(node);
  return displayableIt != this->ModelToDisplayNodes.end() && !displayableIt->second.empty();
}

//---------------------------------------------------------------------------
// vtkMRMLVectorFieldSliceDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLVectorFieldSliceDisplayableManager::vtkMRMLVectorFieldSliceDisplayableManager()
{
  this->Internal = new vtkInternal(this);
  this->AddingDisplayableNode = 0;
}

//---------------------------------------------------------------------------
vtkMRMLVectorFieldSliceDisplayableManager::~vtkMRMLVectorFieldSliceDisplayableManager()
{
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::AddDisplayableNode(vtkMRMLDisplayableNode* node)
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
  int nnodes = node->GetNumberOfDisplayNodes();

  this->Internal->AddObservations(node);
  // Registers the node even if it has no vector field display node yet, so that one can be
  // added to it later.
  this->Internal->ModelToDisplayNodes[node];

  for (int i = 0; i < nnodes; i++)
  {
    vtkMRMLDisplayNode* dnode = node->GetNthDisplayNode(i);
    if (this->Internal->UseDisplayNode(dnode))
    {
      this->Internal->ModelToDisplayNodes[node].insert(dnode);
      this->Internal->AddDisplayNode(node, dnode);
    }
  }
  this->AddingDisplayableNode = 0;
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::RemoveDisplayableNode(vtkMRMLDisplayableNode* node)
{
  if (!node)
  {
    return;
  }
  vtkInternal::ModelToDisplayCacheType::iterator displayableIt = this->Internal->ModelToDisplayNodes.find(node);
  if (displayableIt == this->Internal->ModelToDisplayNodes.end())
  {
    return;
  }

  std::set<vtkMRMLDisplayNode*> dnodes = displayableIt->second;
  for (auto diter = dnodes.begin(); diter != dnodes.end(); ++diter)
  {
    this->Internal->RemoveDisplayNode(*diter);
  }
  this->Internal->RemoveObservations(node);
  this->Internal->ModelToDisplayNodes.erase(displayableIt);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  if (!vtkMRMLDisplayableNode::SafeDownCast(node))
  {
    return;
  }

  // Escape if the scene is being closed, imported or connected
  if (this->GetMRMLScene()->IsBatchProcessing())
  {
    this->SetUpdateFromMRMLRequested(true);
    return;
  }

  this->AddDisplayableNode(vtkMRMLDisplayableNode::SafeDownCast(node));
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  if (node && !vtkMRMLDisplayableNode::SafeDownCast(node) //
      && !node->IsA("vtkMRMLVectorFieldDisplayNode"))
  {
    return;
  }

  vtkMRMLDisplayableNode* modelNode = nullptr;
  vtkMRMLDisplayNode* displayNode = nullptr;

  bool modified = false;
  if ((modelNode = vtkMRMLDisplayableNode::SafeDownCast(node)))
  {
    this->RemoveDisplayableNode(modelNode);
    modified = true;
  }
  else if ((displayNode = vtkMRMLDisplayNode::SafeDownCast(node)))
  {
    this->Internal->RemoveDisplayNode(displayNode);
    modified = true;
  }
  if (modified)
  {
    this->RequestRender();
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  vtkMRMLScene* scene = this->GetMRMLScene();

  if (scene == nullptr || scene->IsBatchProcessing())
  {
    return;
  }

  vtkMRMLDisplayableNode* displayableNode = vtkMRMLDisplayableNode::SafeDownCast(caller);

  if (displayableNode)
  {
    vtkMRMLNode* callDataNode = reinterpret_cast<vtkMRMLDisplayNode*>(callData);
    vtkMRMLDisplayNode* displayNode = vtkMRMLDisplayNode::SafeDownCast(callDataNode);

    if (event == vtkMRMLDisplayableNode::DisplayModifiedEvent)
    {
      if (displayNode && this->Internal->UseDisplayNode(displayNode))
      {
        // A vector field display node was added to the node, or one of its properties changed
        this->Internal->ModelToDisplayNodes[displayableNode].insert(displayNode);
        this->Internal->UpdateDisplayNode(displayNode);
        this->RequestRender();
      }
      else if (this->Internal->HasDisplayPipeline(displayableNode))
      {
        this->Internal->UpdateDisplayableTransforms(displayableNode);
        this->RequestRender();
      }
    }
    else if ((event == vtkMRMLDisplayableNode::TransformModifiedEvent) //
             || (event == vtkMRMLModelNode::PolyDataModifiedEvent)     //
             || (event == vtkMRMLVolumeNode::ImageDataModifiedEvent))
    {
      if (!this->Internal->HasDisplayPipeline(displayableNode))
      {
        // Nothing is drawn for this node, there is nothing to update or to re-render
        return;
      }
      this->Internal->UpdateDisplayableTransforms(displayableNode);
      this->RequestRender();
    }
  }
  else if (vtkMRMLSliceNode::SafeDownCast(caller))
  {
    this->Internal->UpdateSliceNode();
    this->RequestRender();
  }
  else
  {
    this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::UpdateFromMRML()
{
  this->SetUpdateFromMRMLRequested(false);

  vtkMRMLScene* scene = this->GetMRMLScene();
  if (!scene)
  {
    vtkDebugMacro("vtkMRMLVectorFieldSliceDisplayableManager->UpdateFromMRML: Scene is not set.");
    return;
  }

  // Find the display nodes that need to be removed
  // (not deleted immediately because it would invalidate the pipeline iteration)
  std::set<vtkMRMLDisplayableNode*> displayableNodesToRemove;
  std::deque<vtkMRMLDisplayNode*> displayNodesToRemove;
  for (auto modelToDisplayNode : this->Internal->DisplayPipelines)
  {
    vtkMRMLDisplayNode* displayNode = modelToDisplayNode.first;
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

  std::vector<vtkMRMLNode*> mNodes;
  int nnodes = scene->GetNodesByClass("vtkMRMLDisplayableNode", mNodes);
  for (int i = 0; i < nnodes; i++)
  {
    vtkMRMLDisplayableNode* mNode = vtkMRMLDisplayableNode::SafeDownCast(mNodes[i]);
    if (mNode && this->Internal->UseDisplayableNode(mNode))
    {
      this->AddDisplayableNode(mNode);
    }
  }
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::UnobserveMRMLScene()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::OnMRMLSceneStartClose()
{
  this->Internal->ClearDisplayableNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::OnMRMLSceneEndClose()
{
  this->SetUpdateFromMRMLRequested(true);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::OnMRMLSceneEndBatchProcess()
{
  this->SetUpdateFromMRMLRequested(true);
}

//---------------------------------------------------------------------------
void vtkMRMLVectorFieldSliceDisplayableManager::Create()
{
  this->Internal->SetSliceNode(this->GetMRMLSliceNode());
  this->SetUpdateFromMRMLRequested(true);
}
