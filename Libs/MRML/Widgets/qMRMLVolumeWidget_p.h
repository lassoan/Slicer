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

#ifndef __qMRMLVolumeWidget_p_h
#define __qMRMLVolumeWidget_p_h

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

// qMRML includes
#include "qMRMLVolumeWidget.h"

// VTK includes
#include <vtkWeakPointer.h>

class QMenu;
class ctkLinearValueProxy;
class qMRMLSpinBox;

// -----------------------------------------------------------------------------
class qMRMLVolumeWidgetPrivate : public QObject
{
  Q_OBJECT
  Q_DECLARE_PUBLIC(qMRMLVolumeWidget);

protected:
  qMRMLVolumeWidget* const q_ptr;

public:
  qMRMLVolumeWidgetPrivate(qMRMLVolumeWidget& object);
  ~qMRMLVolumeWidgetPrivate() override;

  virtual void init();

  /// Block all the signals emitted by the widgets that are impacted by the
  /// range.
  /// To be reimplemented in subclasses
  virtual bool blockSignals(bool block);
  /// Compute the ideal singleStep based on a range.
  /// Set the single step to the widgets such as sliders, spinboxes...
  void updateSingleStep(double min, double max);

  /// Update the value proxies from the display node's voxel value scaling
  /// (physical = VoxelValueScale * value + VoxelValueOffset), so that widgets
  /// display physical values while window/level/threshold values are kept in
  /// (stored) input units in the display node.
  void updateValueProxy();

public slots:
  virtual void setRange(double min, double max);
  virtual void setDecimals(int decimals);
  virtual void setSingleStep(double singleStep);
  virtual void updateRangeFromSpinBox();

protected:
  vtkWeakPointer<vtkMRMLScalarVolumeNode> VolumeNode;
  vtkWeakPointer<vtkMRMLScalarVolumeDisplayNode> VolumeDisplayNode;
  QMenu* OptionsMenu;
  qMRMLSpinBox* MinRangeSpinBox;
  qMRMLSpinBox* MaxRangeSpinBox;
  double DisplayScalarRange[2];
  /// Displays physical values (scale and offset applied) for value-type
  /// widgets (level, thresholds, range boundaries).
  ctkLinearValueProxy* ValueProxy;
  /// Displays physical values for range-width-type widgets (window):
  /// only the scale is applied (offsets cancel out in differences).
  ctkLinearValueProxy* ScaleOnlyValueProxy;
};

#endif
