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

#ifndef __qMRMLVectorFieldDisplayWidget_h
#define __qMRMLVectorFieldDisplayWidget_h

// MRMLWidgets includes
#include "qMRMLWidget.h"

// CTK includes
#include <ctkVTKObject.h>

// qMRML includes
#include "qMRMLWidgetsExport.h"

class qMRMLVectorFieldDisplayWidgetPrivate;
class vtkMRMLVectorFieldDisplayNode;
class vtkMRMLNode;

/// \brief Widget to edit the properties of a vtkMRMLVectorFieldDisplayNode.
class QMRML_WIDGETS_EXPORT qMRMLVectorFieldDisplayWidget : public qMRMLWidget
{
  Q_OBJECT
  QVTK_OBJECT

public:
  typedef qMRMLWidget Superclass;
  qMRMLVectorFieldDisplayWidget(QWidget* parent = nullptr);
  ~qMRMLVectorFieldDisplayWidget() override;

  /// Get the glyph display node currently shown/edited by this widget.
  vtkMRMLVectorFieldDisplayNode* mrmlVectorFieldDisplayNode() const;

  /// Add a widget below the shared coloring controls, inside the Coloring section, so that
  /// a source can offer its own coloring options without a second section of its own (the
  /// color map editor of a transform). The widget is reparented.
  void addColoringWidget(QWidget* widget);

public slots:
  /// Set the glyph display node shown/edited by this widget.
  void setMRMLVectorFieldDisplayNode(vtkMRMLVectorFieldDisplayNode* glyphDisplayNode);
  /// Utility function to be connected with generic signals.
  void setMRMLVectorFieldDisplayNode(vtkMRMLNode* node);

protected slots:
  void updateWidgetFromMRML();

  void onGlyphTypeChanged(int index);
  void onOrientationArrayChanged(int index);
  void onScaleArrayChanged(int index);
  void onVectorScaleModeChanged(int index);
  void onScaleFactorChanged(double value);
  void onMaskingModeChanged(int index);
  void onMaskingPointsNumberChanged(int value);
  void onVisibilityToggled(bool visible);
  void onColorByChanged(int index);
  void onGlyphSpacingChanged(double value);
  void onThresholdEnabledToggled(bool enabled);
  void onThresholdRangeChanged(double minimum, double maximum);
  void onVisualizationModeToggled(bool checked);
  void onSamplingSpacingChanged(double value);
  void onRegionNodeChanged(vtkMRMLNode* node);
  void onSamplePointsNodeChanged(vtkMRMLNode* node);
  void onGridSpacingChanged(double value);
  void onGridShowNonWarpedToggled(bool enabled);
  void onGridScaleChanged(double value);
  void onGridLineDiameterChanged(double value);
  void onContourLevelsChanged();
  void onMaximumPropagationChanged(double value);
  void onStreamlineTubeDiameterChanged(double value);

  /// Convert the selected orientation array between the RAS and LPS coordinate
  /// systems by inverting the sign of the first two components of each vector.
  void swapOrientationArrayCoordinateSystem();

  void onVisibility3DToggled(bool visible);
  void onColorChanged(const QColor& color);
  void onOpacityChanged(double opacity);

  void onVisibility2DToggled(bool visible);
  void onSliceSlabThicknessChanged(double value);
  void onSliceLineWidthChanged(int value);

protected:
  QScopedPointer<qMRMLVectorFieldDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLVectorFieldDisplayWidget);
  Q_DISABLE_COPY(qMRMLVectorFieldDisplayWidget);
};

#endif
