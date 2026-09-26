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

/// QtCore includes
#include "qSlicerFileReader.h"

// Slicer includes
#include <vtkMRMLFileReader.h>
#include <vtkMRMLIOProperties.h>

// VTK includes
#include <vtkNew.h>
#include <vtkStringArray.h>

//-----------------------------------------------------------------------------
class qSlicerFileReaderPrivate
{
public:
  QStringList LoadedNodes;
};

namespace
{
//----------------------------------------------------------------------------
std::vector<std::string> toStdStringVector(const QStringList& list)
{
  std::vector<std::string> result;
  for (const QString& item : list)
  {
    result.push_back(item.toStdString());
  }
  return result;
}

//----------------------------------------------------------------------------
QStringList toQStringList(vtkStringArray* array)
{
  QStringList result;
  for (vtkIdType i = 0; array && i < array->GetNumberOfValues(); ++i)
  {
    result << QString::fromStdString(array->GetValue(i));
  }
  return result;
}
} // namespace

//----------------------------------------------------------------------------
qSlicerFileReader::qSlicerFileReader(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerFileReaderPrivate)
{
}

//----------------------------------------------------------------------------
qSlicerFileReader::~qSlicerFileReader() = default;

//----------------------------------------------------------------------------
vtkMRMLFileReader* qSlicerFileReader::fileReader() const
{
  return vtkMRMLFileReader::SafeDownCast(this->ioHandler());
}

//----------------------------------------------------------------------------
QStringList qSlicerFileReader::extensions() const
{
  vtkMRMLFileReader* reader = this->fileReader();
  if (reader)
  {
    QStringList nameFilters;
    for (const std::string& nameFilter : reader->GetNameFilters())
    {
      nameFilters << QString::fromStdString(nameFilter);
    }
    return nameFilters;
  }
  return QStringList() << "*.*";
}

//----------------------------------------------------------------------------
bool qSlicerFileReader::canLoadFile(const QString& fileName) const
{
  vtkMRMLFileReader* reader = this->fileReader();
  if (reader)
  {
    return reader->CanLoadFile(fileName.toUtf8().constData());
  }
  return this->supportedNameFilters(fileName).count() > 0;
}

//----------------------------------------------------------------------------
double qSlicerFileReader::canLoadFileConfidence(const QString& fileName) const
{
  vtkMRMLFileReader* reader = this->fileReader();
  if (reader)
  {
    return reader->CanLoadFileConfidence(fileName.toUtf8().constData());
  }
  if (!this->canLoadFile(fileName))
  {
    return 0.0;
  }
  int longestExtensionMatch = 0;
  this->supportedNameFilters(fileName, &longestExtensionMatch);
  // If longer extension is matched then the confidence that this is a good reader is
  // slightly higher. For example, for "somefile.seg.nrrd", a reader that is specifically
  // for ".seg.nrrd" files get slightly higher confidence than readers that of generic ".nrrd" files.
  return 0.5 + 0.01 * longestExtensionMatch;
}

//----------------------------------------------------------------------------
QStringList qSlicerFileReader::supportedNameFilters(const QString& fileName, int* longestExtensionMatchPtr /* =nullptr */) const
{
  QStringList matchingNameFilters;
  for (const std::string& nameFilter : vtkMRMLFileIOHandler::GetMatchingNameFilters(
         fileName.toUtf8().constData(), toStdStringVector(this->extensions()), /*requireReadableFile=*/true, longestExtensionMatchPtr))
  {
    matchingNameFilters << QString::fromStdString(nameFilter);
  }
  return matchingNameFilters;
}

//----------------------------------------------------------------------------
bool qSlicerFileReader::load(const IOProperties& properties)
{
  Q_D(qSlicerFileReader);
  d->LoadedNodes.clear();
  vtkMRMLFileReader* reader = this->fileReader();
  if (!reader)
  {
    return false;
  }
  reader->SetScene(this->mrmlScene());
  vtkNew<vtkMRMLIOProperties> vtkProperties;
  qSlicerIO::toVTKProperties(properties, vtkProperties);
  reader->ClearLoadedNodeIDs();
  bool success = reader->Load(vtkProperties);
  d->LoadedNodes = toQStringList(reader->GetLoadedNodeIDs());
  return success;
}

//----------------------------------------------------------------------------
void qSlicerFileReader::setLoadedNodes(const QStringList& nodes)
{
  Q_D(qSlicerFileReader);
  d->LoadedNodes = nodes;
  vtkMRMLFileReader* reader = this->fileReader();
  if (reader)
  {
    reader->SetLoadedNodeIDs(toStdStringVector(nodes));
  }
}

//----------------------------------------------------------------------------
QStringList qSlicerFileReader::loadedNodes() const
{
  Q_D(const qSlicerFileReader);
  vtkMRMLFileReader* reader = this->fileReader();
  if (reader)
  {
    return toQStringList(reader->GetLoadedNodeIDs());
  }
  return d->LoadedNodes;
}

//----------------------------------------------------------------------------
bool qSlicerFileReader::examineFileInfoList(QFileInfoList& fileInfoList, QFileInfo& archetypeFileInfo, qSlicerIO::IOProperties& ioProperties) const
{
  vtkMRMLFileReader* reader = this->fileReader();
  if (!reader)
  {
    return false;
  }
  vtkNew<vtkStringArray> fileList;
  for (const QFileInfo& fileInfo : fileInfoList)
  {
    fileList->InsertNextValue(fileInfo.absoluteFilePath().toStdString());
  }
  vtkNew<vtkMRMLIOProperties> vtkProperties;
  qSlicerIO::toVTKProperties(ioProperties, vtkProperties);
  std::string archetypeFile = reader->ExamineFileList(fileList, vtkProperties);
  if (archetypeFile.empty())
  {
    return false;
  }
  archetypeFileInfo = QFileInfo(QString::fromStdString(archetypeFile));
  // Remove files from the list that the reader removed
  QStringList remainingFiles = toQStringList(fileList);
  QMutableListIterator<QFileInfo> fileInfoIterator(fileInfoList);
  while (fileInfoIterator.hasNext())
  {
    if (!remainingFiles.contains(fileInfoIterator.next().absoluteFilePath()))
    {
      fileInfoIterator.remove();
    }
  }
  ioProperties = qSlicerIO::fromVTKProperties(vtkProperties);
  return true;
}
