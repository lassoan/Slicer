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

#include "qSlicerVectorFieldsModuleWidgetsAbstractPlugin.h"

//-----------------------------------------------------------------------------
qSlicerVectorFieldsModuleWidgetsAbstractPlugin::qSlicerVectorFieldsModuleWidgetsAbstractPlugin() = default;

//-----------------------------------------------------------------------------
QString qSlicerVectorFieldsModuleWidgetsAbstractPlugin::group() const
{
  return "Slicer [MRML Widgets]";
}

//-----------------------------------------------------------------------------
QIcon qSlicerVectorFieldsModuleWidgetsAbstractPlugin::icon() const
{
  return QIcon();
}

//-----------------------------------------------------------------------------
QString qSlicerVectorFieldsModuleWidgetsAbstractPlugin::toolTip() const
{
  return QString();
}

//-----------------------------------------------------------------------------
QString qSlicerVectorFieldsModuleWidgetsAbstractPlugin::whatsThis() const
{
  return QString();
}
