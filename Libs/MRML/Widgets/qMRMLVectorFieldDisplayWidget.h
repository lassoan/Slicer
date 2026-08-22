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

public slots:
  /// Set the glyph display node shown/edited by this widget.
  void setMRMLVectorFieldDisplayNode(vtkMRMLVectorFieldDisplayNode* glyphDisplayNode);
  /// Utility function to be connected with generic signals.
  void setMRMLVectorFieldDisplayNode(vtkMRMLNode* node);

protected slots:
  void updateWidgetFromMRML();

  void onVisibilityToggled(bool visible);
  void onGlyphTypeChanged(int index);
  void onOrientationArrayChanged(int index);
  void onScaleArrayChanged(int index);
  void onVectorScaleModeChanged(int index);
  void onScaleFactorChanged(double value);
  void onMaskingModeChanged(int index);
  void onMaskingNthPointChanged(int value);
  void onMaskingPointsNumberChanged(int value);

  /// Convert the selected orientation array between the RAS and LPS coordinate
  /// systems by inverting the sign of the first two components of each vector.
  void swapOrientationArrayCoordinateSystem();

  void onVisibility3DToggled(bool visible);
  void onColorChanged(const QColor& color);
  void onOpacityChanged(double opacity);

  void onVisibility2DToggled(bool visible);
  void onSliceSlabThicknessChanged(double value);
  void onSliceGlyphScaleChanged(double value);
  void onSliceLineWidthChanged(int value);

protected:
  QScopedPointer<qMRMLVectorFieldDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLVectorFieldDisplayWidget);
  Q_DISABLE_COPY(qMRMLVectorFieldDisplayWidget);
};

#endif
