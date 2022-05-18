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

==============================================================================*/

#ifndef __qMRMLMarkupsControlPointsTableViewPlugin_h
#define __qMRMLMarkupsControlPointsTableViewPlugin_h

#include "qSlicerMarkupsModuleWidgetsAbstractPlugin.h"

class Q_SLICER_MODULE_MARKUPS_WIDGETS_PLUGINS_EXPORT qMRMLMarkupsControlPointsTableViewPlugin
  : public QObject
  , public qSlicerMarkupsModuleWidgetsAbstractPlugin
{
  Q_OBJECT

public:
  qMRMLMarkupsControlPointsTableViewPlugin(QObject* parent = nullptr);

  QWidget *createWidget(QWidget* parent) override;
  QString  domXml() const override;
  QString  includeFile() const override;
  bool     isContainer() const override;
  QString  name() const override;

};

#endif
