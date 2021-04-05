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

  This file was originally developed by Julien Finet, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

#ifndef __qSlicerDataProbePlugin_h
#define __qSlicerDataProbePlugin_h

#include "qSlicerQTGUIAbstractPlugin.h"

class Q_SLICER_DESIGNER_PLUGINS_EXPORT qSlicerDataProbePlugin
  : public QObject,
    public qSlicerQTGUIAbstractPlugin
{
  Q_OBJECT
public:
  qSlicerDataProbePlugin(QObject* parent = nullptr);

  QWidget *createWidget(QWidget *_parent) override;
  QString domXml() const override;
  QString includeFile() const override;
  bool isContainer() const override;
  QString name() const override;
};

#endif
