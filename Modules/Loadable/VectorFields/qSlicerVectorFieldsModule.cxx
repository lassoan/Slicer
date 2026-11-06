/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was developed with funding from the Children's Hospital of
  Philadelphia Frontier Program.

==============================================================================*/

// VectorFields includes
#include "qSlicerVectorFieldsModule.h"
#include "qSlicerVectorFieldsModuleWidget.h"
#include "vtkSlicerVectorFieldsLogic.h"

// SubjectHierarchy Plugins includes
#include "qSlicerSubjectHierarchyPluginHandler.h"
#include "qSlicerSubjectHierarchyVectorFieldsPlugin.h"

// DisplayableManager initialization
#include <vtkAutoInit.h>
#include <vtkMRMLSliceViewDisplayableManagerFactory.h>
#include <vtkMRMLThreeDViewDisplayableManagerFactory.h>
VTK_MODULE_INIT(vtkSlicerVectorFieldsModuleMRMLDisplayableManager)

//-----------------------------------------------------------------------------
qSlicerVectorFieldsModule::qSlicerVectorFieldsModule(QObject* _parent)
  : Superclass(_parent)
{
}

//-----------------------------------------------------------------------------
qSlicerVectorFieldsModule::~qSlicerVectorFieldsModule() = default;

//-----------------------------------------------------------------------------
QStringList qSlicerVectorFieldsModule::categories() const
{
  return QStringList() << qSlicerAbstractCoreModule::tr("Informatics");
}

//-----------------------------------------------------------------------------
QIcon qSlicerVectorFieldsModule::icon() const
{
  return QIcon(":/Icons/VectorFields.png");
}

//-----------------------------------------------------------------------------
QString qSlicerVectorFieldsModule::helpText() const
{
  QString help = tr("Shows the vector field of a model (a 3-component point or cell data array), a vector volume, or a transform (its displacement) as glyphs, a "
                    "deformed grid, contours of the magnitude, or streamlines. A node can have any number of vector field displays.");
  help += this->defaultDocumentationLink();
  return help;
}

//-----------------------------------------------------------------------------
QString qSlicerVectorFieldsModule::acknowledgementText() const
{
  return tr("This module was developed with funding from the Children's Hospital of Philadelphia Frontier Program.");
}

//-----------------------------------------------------------------------------
QStringList qSlicerVectorFieldsModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Andras Lasso (PerkLab, Queen's University)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QStringList qSlicerVectorFieldsModule::dependencies() const
{
  return QStringList() << "Colors" << "SubjectHierarchy";
}

//-----------------------------------------------------------------------------
QStringList qSlicerVectorFieldsModule::associatedNodeTypes() const
{
  return QStringList() << "vtkMRMLVectorFieldDisplayNode";
}

//-----------------------------------------------------------------------------
void qSlicerVectorFieldsModule::setup()
{
  this->Superclass::setup();
  vtkMRMLThreeDViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLVectorFieldDisplayableManager");
  vtkMRMLSliceViewDisplayableManagerFactory::GetInstance()->RegisterDisplayableManager("vtkMRMLVectorFieldSliceDisplayableManager");
  qSlicerSubjectHierarchyPluginHandler::instance()->registerPlugin(new qSlicerSubjectHierarchyVectorFieldsPlugin());
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerVectorFieldsModule::createWidgetRepresentation()
{
  return new qSlicerVectorFieldsModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerVectorFieldsModule::createLogic()
{
  return vtkSlicerVectorFieldsLogic::New();
}
