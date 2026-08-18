/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright 2026 Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

#ifndef __qSlicerOMEZarrFileDialog_h
#define __qSlicerOMEZarrFileDialog_h

// Slicer includes
#include "qSlicerFileDialog.h"
#include "qSlicerVolumesModuleExport.h"

/// \brief File dialog that loads OME-Zarr images (directories) directly.
///
/// OME-Zarr images are stored as directories, therefore they cannot be
/// selected in regular file dialogs. This dialog is offered when an OME-Zarr
/// directory is dropped on the application window (next to the DICOM and
/// generic "Any data" choices) and loads the image without further prompts.
class Q_SLICER_QTMODULES_VOLUMES_EXPORT qSlicerOMEZarrFileDialog : public qSlicerFileDialog
{
  Q_OBJECT
public:
  typedef qSlicerFileDialog Superclass;
  qSlicerOMEZarrFileDialog(QObject* parent = nullptr);
  ~qSlicerOMEZarrFileDialog() override;

  qSlicerIO::IOFileType fileType() const override;
  QString description() const override;
  qSlicerFileDialog::IOAction action() const override;
  bool exec(const qSlicerIO::IOProperties& ioProperties = qSlicerIO::IOProperties()) override;
  bool isMimeDataAccepted(const QMimeData* mimeData) const override;
  void dropEvent(QDropEvent* event) override;
  QStringList loadedNodes() const override;

  /// Returns true if the path is a readable OME-Zarr image directory
  /// (a directory containing zarr v2 metadata).
  static bool isOMEZarrDirectory(const QString& path);

private:
  QStringList DroppedPaths;
  QStringList LoadedNodeIDs;

  Q_DISABLE_COPY(qSlicerOMEZarrFileDialog);
};

#endif
