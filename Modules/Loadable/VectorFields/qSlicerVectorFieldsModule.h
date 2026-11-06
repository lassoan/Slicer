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

#ifndef __qSlicerVectorFieldsModule_h
#define __qSlicerVectorFieldsModule_h

// Slicer includes
#include "qSlicerLoadableModule.h"

#include "qSlicerVectorFieldsModuleExport.h"

/// \brief Shows the vector fields of models, vector volumes and transforms as glyphs, grids, contours or streamlines.
class Q_SLICER_QTMODULES_VECTORFIELDS_EXPORT qSlicerVectorFieldsModule : public qSlicerLoadableModule
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.slicer.modules.loadable.qSlicerLoadableModule/1.0");
  Q_INTERFACES(qSlicerLoadableModule);

public:
  typedef qSlicerLoadableModule Superclass;
  qSlicerVectorFieldsModule(QObject* parent = nullptr);
  ~qSlicerVectorFieldsModule() override;

  qSlicerGetTitleMacro(tr("Vector Fields"));

  QStringList categories() const override;
  QIcon icon() const override;
  QString helpText() const override;
  QString acknowledgementText() const override;
  QStringList contributors() const override;
  QStringList dependencies() const override;
  /// Vector field display nodes are edited here; their models, volumes and transforms in their own modules
  QStringList associatedNodeTypes() const override;

protected:
  /// Register the displayable managers and the subject hierarchy plugin
  void setup() override;
  qSlicerAbstractModuleRepresentation* createWidgetRepresentation() override;
  vtkMRMLAbstractLogic* createLogic() override;

private:
  Q_DISABLE_COPY(qSlicerVectorFieldsModule);
};

#endif
