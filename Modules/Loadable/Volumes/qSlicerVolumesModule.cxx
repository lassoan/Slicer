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

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/
#include <QDebug>
#include <QTimer>

// Slicer includes
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerIOManager.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerNodeWriter.h>

// qMRML includes
#include <qMRMLSliceView.h>
#include <qMRMLSliceWidget.h>

// MRML Logic includes
#include <vtkMRMLSliceLayerLogic.h>
#include <vtkMRMLSliceLogic.h>

// VTK includes
#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty2D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <map>

// Volumes Logic includes
#include <vtkSlicerVolumesLogic.h>

// Volumes QTModule includes
#include "qSlicerOMEZarrFileDialog.h"
#include "qSlicerVolumesReader.h"
#include "qSlicerVolumesModule.h"
#include "qSlicerVolumesModuleWidget.h"

// MRML Logic includes
#include <vtkMRMLColorLogic.h>
#include <vtkMRMLScalarVolumeNode.h>
#include <vtkMRMLVoxelDataProvider.h>

// MRML includes
#include <vtkMRMLScene.h>

// SubjectHierarchy Plugins includes
#include "qSlicerSubjectHierarchyPluginHandler.h"
#include "qSlicerSubjectHierarchyVolumesPlugin.h"
#include "qSlicerSubjectHierarchyLabelMapsPlugin.h"
#include "qSlicerSubjectHierarchyDiffusionTensorVolumesPlugin.h"

//-----------------------------------------------------------------------------
namespace
{

//-----------------------------------------------------------------------------
/// Small pie-sector loading indicator shown in the top-right corner of a
/// slice view while a voxel data provider of a displayed volume is fetching
/// data in the background. The circle outline appears when a request starts,
/// the sector fills with the request progress, and everything disappears
/// when the request completes.
struct qSlicerVolumesLoadingIndicator
{
  vtkSmartPointer<vtkPolyData> OutlinePolyData;
  vtkSmartPointer<vtkPolyData> SectorPolyData;
  vtkSmartPointer<vtkActor2D> OutlineActor;
  vtkSmartPointer<vtkActor2D> SectorActor;
  vtkWeakPointer<vtkRenderer> Renderer;
  bool Visible{ false };
};

//-----------------------------------------------------------------------------
void qSlicerVolumesBuildCircle(vtkPolyData* polyData, double centerX, double centerY, double radius)
{
  const int numberOfSegments = 32;
  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> lines;
  lines->InsertNextCell(numberOfSegments + 1);
  for (int i = 0; i <= numberOfSegments; ++i)
  {
    double angle = 2.0 * vtkMath::Pi() * i / numberOfSegments;
    points->InsertNextPoint(centerX + radius * sin(angle), centerY + radius * cos(angle), 0.0);
    lines->InsertCellPoint(i);
  }
  polyData->SetPoints(points);
  polyData->SetLines(lines);
}

//-----------------------------------------------------------------------------
void qSlicerVolumesBuildSector(vtkPolyData* polyData, double centerX, double centerY, double radius, double fraction)
{
  // Filled pie sector from 12 o'clock, clockwise, covering the fraction
  int numberOfSegments = std::max(2, static_cast<int>(32 * fraction) + 1);
  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> polys;
  points->InsertNextPoint(centerX, centerY, 0.0);
  polys->InsertNextCell(numberOfSegments + 2);
  polys->InsertCellPoint(0);
  for (int i = 0; i <= numberOfSegments; ++i)
  {
    double angle = 2.0 * vtkMath::Pi() * fraction * i / numberOfSegments;
    points->InsertNextPoint(centerX + radius * sin(angle), centerY + radius * cos(angle), 0.0);
    polys->InsertCellPoint(i + 1);
  }
  polyData->SetPoints(points);
  polyData->SetPolys(polys);
}

} // namespace

class qSlicerVolumesModulePrivate
{
public:
  std::map<QString, qSlicerVolumesLoadingIndicator> LoadingIndicators;

  void updateLoadingIndicators();
};

//-----------------------------------------------------------------------------
void qSlicerVolumesModulePrivate::updateLoadingIndicators()
{
  qSlicerApplication* app = qSlicerApplication::application();
  qSlicerLayoutManager* layoutManager = app ? app->layoutManager() : nullptr;
  if (!layoutManager)
  {
    return;
  }
  foreach (const QString& viewName, layoutManager->sliceViewNames())
  {
    qMRMLSliceWidget* sliceWidget = layoutManager->sliceWidget(viewName);
    qMRMLSliceView* sliceView = sliceWidget ? sliceWidget->sliceView() : nullptr;
    if (!sliceView)
    {
      continue;
    }
    // Loading progress of the region that THIS view is displaying (other
    // views of the same volume may display different regions/levels and are
    // not affected by this request)
    double progress = -1.0;
    vtkMRMLSliceLogic* sliceLogic = sliceWidget->sliceLogic();
    if (sliceLogic)
    {
      vtkMRMLSliceLayerLogic* layers[2] = { sliceLogic->GetBackgroundLayer(), sliceLogic->GetForegroundLayer() };
      for (vtkMRMLSliceLayerLogic* layer : layers)
      {
        vtkMRMLScalarVolumeNode* volumeNode = layer ? vtkMRMLScalarVolumeNode::SafeDownCast(layer->GetVolumeNode()) : nullptr;
        vtkMRMLVoxelDataProvider* provider = volumeNode ? volumeNode->GetVoxelDataProvider() : nullptr;
        if (provider && layer->GetTargetResolutionLevel() >= 0)
        {
          progress = std::max(progress, provider->GetRegionRequestProgress(layer->GetTargetRegionExtent(), layer->GetTargetResolutionLevel()));
        }
      }
    }
    vtkRenderWindow* renderWindow = sliceView->renderWindow();
    vtkRenderer* renderer = renderWindow ? vtkRenderer::SafeDownCast(renderWindow->GetRenderers()->GetItemAsObject(0)) : nullptr;
    if (!renderer)
    {
      continue;
    }
    qSlicerVolumesLoadingIndicator& indicator = this->LoadingIndicators[viewName];
    if (indicator.Renderer != renderer)
    {
      if (indicator.Renderer)
      {
        indicator.Renderer->RemoveActor2D(indicator.OutlineActor);
        indicator.Renderer->RemoveActor2D(indicator.SectorActor);
      }
      indicator.OutlinePolyData = vtkSmartPointer<vtkPolyData>::New();
      indicator.SectorPolyData = vtkSmartPointer<vtkPolyData>::New();
      vtkNew<vtkPolyDataMapper2D> outlineMapper;
      outlineMapper->SetInputData(indicator.OutlinePolyData);
      indicator.OutlineActor = vtkSmartPointer<vtkActor2D>::New();
      indicator.OutlineActor->SetMapper(outlineMapper);
      indicator.OutlineActor->GetProperty()->SetColor(1.0, 1.0, 1.0);
      indicator.OutlineActor->GetProperty()->SetOpacity(0.8);
      indicator.OutlineActor->GetProperty()->SetLineWidth(1.5);
      indicator.OutlineActor->SetVisibility(0);
      vtkNew<vtkPolyDataMapper2D> sectorMapper;
      sectorMapper->SetInputData(indicator.SectorPolyData);
      indicator.SectorActor = vtkSmartPointer<vtkActor2D>::New();
      indicator.SectorActor->SetMapper(sectorMapper);
      indicator.SectorActor->GetProperty()->SetColor(1.0, 1.0, 1.0);
      indicator.SectorActor->GetProperty()->SetOpacity(0.55);
      indicator.SectorActor->SetVisibility(0);
      renderer->AddActor2D(indicator.SectorActor);
      renderer->AddActor2D(indicator.OutlineActor);
      indicator.Renderer = renderer;
      indicator.Visible = false;
    }
    if (progress < 0.0)
    {
      if (indicator.Visible)
      {
        indicator.OutlineActor->SetVisibility(0);
        indicator.SectorActor->SetVisibility(0);
        indicator.Visible = false;
        sliceView->scheduleRender();
      }
      continue;
    }
    const int* size = renderer->GetSize();
    const int* origin = renderer->GetOrigin();
    const double radius = 9.0;
    const double margin = 8.0;
    double centerX = origin[0] + size[0] - radius - margin;
    double centerY = origin[1] + size[1] - radius - margin;
    qSlicerVolumesBuildCircle(indicator.OutlinePolyData, centerX, centerY, radius);
    qSlicerVolumesBuildSector(indicator.SectorPolyData, centerX, centerY, radius - 1.5, std::max(0.01, progress));
    indicator.OutlineActor->SetVisibility(1);
    indicator.SectorActor->SetVisibility(1);
    indicator.Visible = true;
    sliceView->scheduleRender();
  }
}

//-----------------------------------------------------------------------------
qSlicerVolumesModule::qSlicerVolumesModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerVolumesModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerVolumesModule::~qSlicerVolumesModule() = default;

//-----------------------------------------------------------------------------
QString qSlicerVolumesModule::title() const
{
  return tr("Volumes");
}

//-----------------------------------------------------------------------------
QString qSlicerVolumesModule::helpText() const
{
  QString help = tr("The Volumes Module is the interface for adjusting Window, Level, Threshold, "
                    "Color LUT and other parameters that control the display of volume image data "
                    "in the scene.")
                 + "<br>";
  help += this->defaultDocumentationLink();
  return help;
}

//-----------------------------------------------------------------------------
QString qSlicerVolumesModule::acknowledgementText() const
{
  QString acknowledgement = QString("<center><table border=\"0\"><tr>"
                                    "<td><img src=\":Logos/NAMIC.png\" alt\"NA-MIC\"></td>"
                                    "<td><img src=\":Logos/NAC.png\" alt\"NAC\"></td>"
                                    "</tr><tr>"
                                    "<td><img src=\":Logos/BIRN-NoText.png\" alt\"BIRN\"></td>"
                                    "<td><img src=\":Logos/NCIGT.png\" alt\"NCIGT\"></td>"
                                    "</tr></table></center>"
                                    + tr("This work was supported by NA-MIC, NAC, BIRN, NCIGT, and the Slicer Community."));
  return acknowledgement;
}

//-----------------------------------------------------------------------------
QStringList qSlicerVolumesModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Steve Pieper (Isomics)");
  moduleContributors << QString("Julien Finet (Kitware)");
  moduleContributors << QString("Alex Yarmarkovich (Isomics)");
  moduleContributors << QString("Nicole Aucoin (SPL, BWH)");
  moduleContributors << QString("Ron Kikinis (SPL, BWH)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerVolumesModule::icon() const
{
  return QIcon(":/Icons/Medium/SlicerVolumes.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerVolumesModule::categories() const
{
  return QStringList() << "";
}

//-----------------------------------------------------------------------------
QStringList qSlicerVolumesModule::dependencies() const
{
  QStringList moduleDependencies;
  moduleDependencies << "Colors" << "Units";
  return moduleDependencies;
}

//-----------------------------------------------------------------------------
void qSlicerVolumesModule::setup()
{
  this->Superclass::setup();

  vtkSlicerVolumesLogic* volumesLogic = vtkSlicerVolumesLogic::SafeDownCast(this->logic());

  qSlicerCoreIOManager* ioManager = qSlicerCoreApplication::application()->coreIOManager();
  ioManager->registerIO(new qSlicerVolumesReader(volumesLogic, this));
  ioManager->registerIO(new qSlicerNodeWriter("Volumes", QString("VolumeFile"), QStringList() << "vtkMRMLVolumeNode", true, this));

  // Offer direct loading of OME-Zarr image directories on drag-and-drop
  // (next to the DICOM and generic "Any data" choices).
  qSlicerIOManager* guiIOManager = qSlicerApplication::application() ? qSlicerApplication::application()->ioManager() : nullptr;
  if (guiIOManager)
  {
    guiIOManager->registerDialog(new qSlicerOMEZarrFileDialog());
  }

  // Register Subject Hierarchy core plugins
  qSlicerSubjectHierarchyPluginHandler::instance()->registerPlugin(new qSlicerSubjectHierarchyVolumesPlugin());
  qSlicerSubjectHierarchyPluginHandler::instance()->registerPlugin(new qSlicerSubjectHierarchyLabelMapsPlugin());
  qSlicerSubjectHierarchyPluginHandler::instance()->registerPlugin(new qSlicerSubjectHierarchyDiffusionTensorVolumesPlugin());

  // Pump for background voxel data requests: voxel data providers fetch
  // resolution levels of multi-resolution volumes in worker threads (for
  // progressive refinement in slice views), and completed requests must be
  // finalized on the main thread (RegionReadyEvent is invoked from
  // ProcessPendingRegionRequests). MRML is Qt-free, so the periodic
  // main-thread callback is provided here.
  QTimer* voxelDataRequestTimer = new QTimer(this);
  voxelDataRequestTimer->setInterval(100);
  QObject::connect(voxelDataRequestTimer,
                   &QTimer::timeout,
                   this,
                   [this]()
                   {
                     vtkMRMLScene* scene = this->mrmlScene();
                     if (!scene)
                     {
                       return;
                     }
                     std::vector<vtkMRMLNode*> volumeNodes;
                     scene->GetNodesByClass("vtkMRMLScalarVolumeNode", volumeNodes);
                     for (vtkMRMLNode* node : volumeNodes)
                     {
                       vtkMRMLScalarVolumeNode* volumeNode = vtkMRMLScalarVolumeNode::SafeDownCast(node);
                       vtkMRMLVoxelDataProvider* provider = volumeNode ? volumeNode->GetVoxelDataProvider() : nullptr;
                       if (provider && provider->HasPendingRegionRequests())
                       {
                         provider->ProcessPendingRegionRequests();
                       }
                     }
                     // Show loading progress indicators in the slice views
                     // while providers are fetching data in the background
                     Q_D(qSlicerVolumesModule);
                     d->updateLoadingIndicators();
                   });
  voxelDataRequestTimer->start();
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerVolumesModule::createWidgetRepresentation()
{
  return new qSlicerVolumesModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerVolumesModule::createLogic()
{
  return vtkSlicerVolumesLogic::New();
}

//-----------------------------------------------------------------------------
QStringList qSlicerVolumesModule::associatedNodeTypes() const
{
  return QStringList() //
         << "vtkMRMLVolumeNode"
         << "vtkMRMLVolumeDisplayNode";
}
