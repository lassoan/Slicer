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

// Slicer includes
#include "qSlicerApplication.h"

#include "vtkSlicerTransformLogic.h"
#include "vtkMRMLSliceViewDisplayableManagerFactory.h"
#include "vtkMRMLThreeDViewDisplayableManagerFactory.h"

// Transforms includes
#include "qMRMLLinearTransformOptionsWidget.h"
#include "qMRMLTransformOptionsWidgetsFactory.h"
#include "qSlicerTransformsModule.h"
#include "qSlicerTransformsModuleWidget.h"

// Qt includes
#include <QPointer>

// VTK includes
#include "vtkSmartPointer.h"

// SubjectHierarchy Plugins includes
#include "qSlicerSubjectHierarchyPluginHandler.h"
#include "qSlicerSubjectHierarchyTransformsPlugin.h"

// DisplayableManager initialization
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkSlicerTransformsModuleMRMLDisplayableManager)

//-----------------------------------------------------------------------------
class qSlicerTransformsModulePrivate
{
public:
  /// Options widget of linear transforms that the module registered. It is owned by the options widgets factory.
  QPointer<qMRMLTransformAbstractOptionsWidget> LinearTransformOptionsWidget;
};

//-----------------------------------------------------------------------------
qSlicerTransformsModule::qSlicerTransformsModule(QObject* _parentObject)
  : Superclass(_parentObject)
  , d_ptr(new qSlicerTransformsModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerTransformsModule::~qSlicerTransformsModule()
{
  Q_D(qSlicerTransformsModule);
  // The registered widget holds VTK objects, so it must not be left for the cleanup of the factory at exit
  if (d->LinearTransformOptionsWidget)
  {
    qMRMLTransformOptionsWidgetsFactory::instance()->unregisterOptionsWidget(d->LinearTransformOptionsWidget.data());
  }
}

//-----------------------------------------------------------------------------
QIcon qSlicerTransformsModule::icon() const
{
  return QIcon(":/Icons/Transforms.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerTransformsModule::categories() const
{
  return QStringList() << "" << qSlicerAbstractCoreModule::tr("Registration");
}

//-----------------------------------------------------------------------------
QStringList qSlicerTransformsModule::dependencies() const
{
  QStringList moduleDependencies;
  moduleDependencies << "Units";
  return moduleDependencies;
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerTransformsModule::createWidgetRepresentation()
{
  return new qSlicerTransformsModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerTransformsModule::createLogic()
{
  return vtkSlicerTransformLogic::New();
}

//-----------------------------------------------------------------------------
QString qSlicerTransformsModule::helpText() const
{
  QString help = tr("The Transforms Module creates and edits transforms.") + QLatin1String("<br>");
  help += this->defaultDocumentationLink();
  return help;
}

//-----------------------------------------------------------------------------
QString qSlicerTransformsModule::acknowledgementText() const
{
  return tr("This work was supported by NA-MIC, NAC, BIRN, NCIGT, and the Slicer Community.");
}

//-----------------------------------------------------------------------------
QStringList qSlicerTransformsModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Alex Yarmarkovich (Isomics)");
  moduleContributors << QString("Jean-Christophe Fillion-Robin (Kitware)");
  moduleContributors << QString("Julien Finet (Kitware)");
  moduleContributors << QString("Andras Lasso (PerkLab, Queen's)");
  moduleContributors << QString("Franklin King (PerkLab, Queen's)");
  moduleContributors << QString("Ron Kikinis (SPL, BWH)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
void qSlicerTransformsModule::setup()
{
  Q_D(qSlicerTransformsModule);
  qSlicerApplication* app = qSlicerApplication::application();
  if (!app)
  {
    return;
  }

  // Register displayable managers
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLTransformsDisplayableManager2D");
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLLinearTransformsDisplayableManager");
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLTransformsDisplayableManager3D");
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLLinearTransformsDisplayableManager");

  // Register Subject Hierarchy core plugins
  qSlicerSubjectHierarchyPluginHandler::instance()->registerPlugin(new qSlicerSubjectHierarchyTransformsPlugin());

  // Register the options widget of linear transforms (matrix and sliders of the Edit section).
  // Options widgets are shown in the order of registration, and modules that register options widgets
  // depend on this module, so this one is at the top of the section.
  // The factory takes ownership of the widget (it deletes it if it cannot register it).
  d->LinearTransformOptionsWidget = new qMRMLLinearTransformOptionsWidget();
  qMRMLTransformOptionsWidgetsFactory::instance()->registerOptionsWidget(d->LinearTransformOptionsWidget.data());
}

//-----------------------------------------------------------------------------
QStringList qSlicerTransformsModule::associatedNodeTypes() const
{
  return QStringList() //
         << "vtkMRMLTransformNode"
         << "vtkMRMLTransformDisplayNode";
}
