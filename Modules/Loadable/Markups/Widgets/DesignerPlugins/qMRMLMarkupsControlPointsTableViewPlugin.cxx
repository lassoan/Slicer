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

#include "qMRMLMarkupsControlPointsTableViewPlugin.h"
#include "qMRMLMarkupsControlPointsTableView.h"

//-----------------------------------------------------------------------------
qMRMLMarkupsControlPointsTableViewPlugin::qMRMLMarkupsControlPointsTableViewPlugin(QObject* pluginParent)
  : QObject(pluginParent)
{
}

//-----------------------------------------------------------------------------
QWidget *qMRMLMarkupsControlPointsTableViewPlugin::createWidget(QWidget* parentWidget)
{
  qMRMLMarkupsControlPointsTableView* pluginWidget =
    new qMRMLMarkupsControlPointsTableView(parentWidget);
  return pluginWidget;
}

//-----------------------------------------------------------------------------
QString qMRMLMarkupsControlPointsTableViewPlugin::domXml() const
{
  return "<widget class=\"qMRMLMarkupsControlPointsTableView\" \
          name=\"MarkupsControlPointsTableView\">\n"
          "</widget>\n";
}

//-----------------------------------------------------------------------------
QString qMRMLMarkupsControlPointsTableViewPlugin::includeFile() const
{
  return "qMRMLMarkupsControlPointsTableView.h";
}

//-----------------------------------------------------------------------------
bool qMRMLMarkupsControlPointsTableViewPlugin::isContainer() const
{
  return false;
}

//-----------------------------------------------------------------------------
QString qMRMLMarkupsControlPointsTableViewPlugin::name() const
{
  return "qMRMLMarkupsControlPointsTableView";
}
