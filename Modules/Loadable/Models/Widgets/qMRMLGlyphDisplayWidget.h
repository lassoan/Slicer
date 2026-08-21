/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under this License.

==============================================================================*/

#ifndef __qMRMLGlyphDisplayWidget_h
#define __qMRMLGlyphDisplayWidget_h

// MRMLWidgets includes
#include "qMRMLWidget.h"

// CTK includes
#include <ctkVTKObject.h>

// qMRML includes
#include "qSlicerModelsModuleWidgetsExport.h"

class qMRMLGlyphDisplayWidgetPrivate;
class vtkMRMLGlyphDisplayNode;
class vtkMRMLNode;

/// \brief Widget to edit the properties of a vtkMRMLGlyphDisplayNode.
class Q_SLICER_QTMODULES_MODELS_WIDGETS_EXPORT qMRMLGlyphDisplayWidget : public qMRMLWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  typedef qMRMLWidget Superclass;
  qMRMLGlyphDisplayWidget(QWidget* parent = nullptr);
  ~qMRMLGlyphDisplayWidget() override;

  /// Get the glyph display node currently shown/edited by this widget.
  vtkMRMLGlyphDisplayNode* mrmlGlyphDisplayNode() const;

public slots:
  /// Set the glyph display node shown/edited by this widget.
  void setMRMLGlyphDisplayNode(vtkMRMLGlyphDisplayNode* glyphDisplayNode);
  /// Utility function to be connected with generic signals.
  void setMRMLGlyphDisplayNode(vtkMRMLNode* node);

protected slots:
  void updateWidgetFromMRML();

  void onGlyphTypeChanged(int index);
  void onOrientationArrayChanged(int index);
  void onScaleArrayChanged(int index);
  void onVectorScaleModeChanged(int index);
  void onScaleFactorChanged(double value);
  void onMaskingModeChanged(int index);
  void onMaskingNthPointChanged(int value);
  void onMaskingPointsNumberChanged(int value);

protected:
  QScopedPointer<qMRMLGlyphDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLGlyphDisplayWidget);
  Q_DISABLE_COPY(qMRMLGlyphDisplayWidget);
};

#endif
