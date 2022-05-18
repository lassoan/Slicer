/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, and Cancer
  Care Ontario.

==============================================================================*/

#ifndef __qMRMLMarkupsControlPointsModel_p_h
#define __qMRMLMarkupsControlPointsModel_p_h

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Slicer API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// Qt includes
#include <QFlags>
#include <QMap>

// Markups includes
#include "qSlicerMarkupsModuleWidgetsExport.h"

#include "qMRMLMarkupsControlPointsModel.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLMarkupsNode.h>

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkSmartPointer.h>

class QStandardItemModel;

//------------------------------------------------------------------------------
// qMRMLMarkupsControlPointsModelPrivate
//------------------------------------------------------------------------------
class Q_SLICER_MODULE_MARKUPS_WIDGETS_EXPORT qMRMLMarkupsControlPointsModelPrivate
{
  Q_DECLARE_PUBLIC(qMRMLMarkupsControlPointsModel);

protected:
  qMRMLMarkupsControlPointsModel* const q_ptr;
public:
  qMRMLMarkupsControlPointsModelPrivate(qMRMLMarkupsControlPointsModel& object);
  virtual ~qMRMLMarkupsControlPointsModelPrivate();
  void init();

  QStandardItem* insertControlPoint(int controlPointIndex);
  void updateControlPoint(int controlPointIndex);

public:
  vtkSmartPointer<vtkCallbackCommand> CallBack;
  bool UpdatingItemFromControlPoint;

  QIcon VisibleIcon;
  QIcon HiddenIcon;

  QIcon LockIcon;
  QIcon UnlockIcon;

  QIcon PositionUndefinedIcon;
  QIcon PositionPreviewIcon;
  QIcon PositionDefinedIcon;
  QIcon PositionMissingIcon;

  bool UpdatingFromMRML = false;

  vtkSmartPointer<vtkMRMLMarkupsNode> MarkupsNode;
};

#endif
