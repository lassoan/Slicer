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

==============================================================================*/

#ifndef __qSlicerVectorVolumeDisplayWidget_h
#define __qSlicerVectorVolumeDisplayWidget_h

// CTK includes
#include <ctkVTKObject.h>

// Slicer includes
#include <qSlicerWidget.h>

#include "qSlicerVolumesModuleWidgetsExport.h"

class qSlicerVectorVolumeDisplayWidgetPrivate;
class vtkMRMLNode;
class vtkMRMLVectorFieldDisplayNode;
class vtkMRMLVectorVolumeNode;

/// \brief Display properties of a vector volume.
///
/// Shows the image display properties (window/level, threshold) of the volume, and a
/// "Vector field" section that displays the voxel vectors as glyphs in the slice and 3D
/// views. The vector field display node is only created when that section is expanded, so
/// that loading a vector volume costs nothing.
class Q_SLICER_QTMODULES_VOLUMES_WIDGETS_EXPORT qSlicerVectorVolumeDisplayWidget : public qSlicerWidget
{
  Q_OBJECT
  QVTK_OBJECT
public:
  typedef qSlicerWidget Superclass;
  explicit qSlicerVectorVolumeDisplayWidget(QWidget* parent = nullptr);
  ~qSlicerVectorVolumeDisplayWidget() override;

  vtkMRMLVectorVolumeNode* volumeNode() const;

  /// Vector field display node of the volume, or nullptr if it does not have one yet.
  vtkMRMLVectorFieldDisplayNode* vectorFieldDisplayNode() const;

public slots:
  /// Set the MRML node of interest
  void setMRMLVolumeNode(vtkMRMLVectorVolumeNode* volumeNode);
  void setMRMLVolumeNode(vtkMRMLNode* node);

protected slots:
  void updateWidgetFromMRML();

  /// Create the vector field display node when the section is expanded for the first time.
  void onVectorFieldGroupBoxToggled(bool toggled);

protected:
  QScopedPointer<qSlicerVectorVolumeDisplayWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerVectorVolumeDisplayWidget);
  Q_DISABLE_COPY(qSlicerVectorVolumeDisplayWidget);
};

#endif
