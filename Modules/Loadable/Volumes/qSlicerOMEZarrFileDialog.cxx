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

// Qt includes
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

// Slicer includes
#include "qSlicerCoreApplication.h"
#include "qSlicerCoreIOManager.h"
#include "qSlicerOMEZarrFileDialog.h"

// MRML includes
#include <vtkMRMLNode.h>

// VTK includes
#include <vtkCollection.h>
#include <vtkNew.h>

//-----------------------------------------------------------------------------
qSlicerOMEZarrFileDialog::qSlicerOMEZarrFileDialog(QObject* parent)
  : qSlicerFileDialog(parent)
{
}

//-----------------------------------------------------------------------------
qSlicerOMEZarrFileDialog::~qSlicerOMEZarrFileDialog() = default;

//-----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerOMEZarrFileDialog::fileType() const
{
  return QString("VolumeFile");
}

//-----------------------------------------------------------------------------
QString qSlicerOMEZarrFileDialog::description() const
{
  return tr("OME-Zarr image");
}

//-----------------------------------------------------------------------------
qSlicerFileDialog::IOAction qSlicerOMEZarrFileDialog::action() const
{
  return qSlicerFileDialog::Read;
}

//-----------------------------------------------------------------------------
bool qSlicerOMEZarrFileDialog::isOMEZarrDirectory(const QString& path)
{
  QFileInfo pathInfo(path);
  if (!pathInfo.isDir())
  {
    return false;
  }
  // zarr v2 stores contain a .zgroup/.zattrs at the top level.
  // (zarr v3 stores contain zarr.json instead, but those cannot be read by
  // the current OME-Zarr image reader, therefore they are not claimed here.)
  return QFileInfo::exists(path + "/.zattrs");
}

//-----------------------------------------------------------------------------
bool qSlicerOMEZarrFileDialog::isMimeDataAccepted(const QMimeData* mimeData) const
{
  if (!mimeData || !mimeData->hasUrls())
  {
    return false;
  }
  for (const QUrl& url : mimeData->urls())
  {
    if (url.isValid() && qSlicerOMEZarrFileDialog::isOMEZarrDirectory(url.toLocalFile()))
    {
      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------
void qSlicerOMEZarrFileDialog::dropEvent(QDropEvent* event)
{
  this->DroppedPaths.clear();
  if (!event || !event->mimeData())
  {
    return;
  }
  for (const QUrl& url : event->mimeData()->urls())
  {
    if (!url.isValid())
    {
      continue;
    }
    QString localPath = url.toLocalFile();
    if (qSlicerOMEZarrFileDialog::isOMEZarrDirectory(localPath))
    {
      this->DroppedPaths << localPath;
    }
  }
  if (!this->DroppedPaths.isEmpty())
  {
    event->acceptProposedAction();
  }
}

//-----------------------------------------------------------------------------
bool qSlicerOMEZarrFileDialog::exec(const qSlicerIO::IOProperties& ioProperties)
{
  this->LoadedNodeIDs.clear();
  QStringList paths = this->DroppedPaths;
  this->DroppedPaths.clear();
  if (paths.isEmpty())
  {
    // Not invoked through drag-and-drop: let the user pick a directory.
    QString directory = QFileDialog::getExistingDirectory(nullptr, tr("Select OME-Zarr image directory"));
    if (directory.isEmpty())
    {
      return false;
    }
    paths << directory;
  }
  qSlicerCoreIOManager* coreIOManager = qSlicerCoreApplication::application()->coreIOManager();
  bool success = false;
  for (const QString& path : paths)
  {
    if (!qSlicerOMEZarrFileDialog::isOMEZarrDirectory(path))
    {
      continue;
    }
    qSlicerIO::IOProperties properties = ioProperties;
    properties["fileName"] = path;
    vtkNew<vtkCollection> loadedNodes;
    if (coreIOManager->loadNodes(this->fileType(), properties, loadedNodes.GetPointer()))
    {
      success = true;
      for (int i = 0; i < loadedNodes->GetNumberOfItems(); ++i)
      {
        vtkMRMLNode* node = vtkMRMLNode::SafeDownCast(loadedNodes->GetItemAsObject(i));
        if (node && node->GetID())
        {
          this->LoadedNodeIDs << QString::fromUtf8(node->GetID());
        }
      }
    }
  }
  return success;
}

//-----------------------------------------------------------------------------
QStringList qSlicerOMEZarrFileDialog::loadedNodes() const
{
  return this->LoadedNodeIDs;
}
