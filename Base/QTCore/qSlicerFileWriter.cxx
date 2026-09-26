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

#include "qSlicerFileWriter.h"

// Slicer includes
#include <vtkMRMLFileWriter.h>
#include <vtkMRMLIOProperties.h>

// VTK includes
#include <vtkNew.h>
#include <vtkStringArray.h>

//-----------------------------------------------------------------------------
class qSlicerFileWriterPrivate
{
public:
  QStringList WrittenNodes;
};

namespace
{
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
qSlicerFileWriter::qSlicerFileWriter(QObject* parentObject)
  : qSlicerIO(parentObject)
  , d_ptr(new qSlicerFileWriterPrivate)
{
}

//----------------------------------------------------------------------------
qSlicerFileWriter::~qSlicerFileWriter() = default;

//----------------------------------------------------------------------------
vtkMRMLFileWriter* qSlicerFileWriter::fileWriter() const
{
  return vtkMRMLFileWriter::SafeDownCast(this->ioHandler());
}

//----------------------------------------------------------------------------
bool qSlicerFileWriter::canWriteObject(vtkObject* object) const
{
  vtkMRMLFileWriter* writer = this->fileWriter();
  if (writer)
  {
    return writer->CanWriteObject(object);
  }
  return false;
}

//----------------------------------------------------------------------------
double qSlicerFileWriter::canWriteObjectConfidence(vtkObject* object) const
{
  vtkMRMLFileWriter* writer = this->fileWriter();
  if (writer && this->isIOHandlerUsableDirectly())
  {
    return writer->CanWriteObjectConfidence(object);
  }
  // A subclass may override canWriteObject(), therefore it must be used for computing the confidence
  if (!this->canWriteObject(object))
  {
    return 0.0;
  }
  return writer ? writer->GetConfidenceForMatchingClass() : 0.5;
}

//----------------------------------------------------------------------------
QStringList qSlicerFileWriter::extensions(vtkObject* object) const
{
  vtkMRMLFileWriter* writer = this->fileWriter();
  if (!writer)
  {
    return QStringList();
  }
  vtkNew<vtkStringArray> nameFilters;
  writer->GetNameFiltersForObject(object, nameFilters);
  return toQStringList(nameFilters);
}

//----------------------------------------------------------------------------
bool qSlicerFileWriter::write(const qSlicerIO::IOProperties& properties)
{
  Q_D(qSlicerFileWriter);
  d->WrittenNodes.clear();
  vtkMRMLFileWriter* writer = this->fileWriter();
  if (!writer)
  {
    return false;
  }
  writer->SetScene(this->mrmlScene());
  vtkNew<vtkMRMLIOProperties> vtkProperties;
  qSlicerIO::toVTKProperties(properties, vtkProperties);
  writer->ClearWrittenNodeIDs();
  bool success = writer->Write(vtkProperties);
  d->WrittenNodes = toQStringList(writer->GetWrittenNodeIDs());
  return success;
}

//----------------------------------------------------------------------------
void qSlicerFileWriter::setWrittenNodes(const QStringList& nodes)
{
  Q_D(qSlicerFileWriter);
  d->WrittenNodes = nodes;
  vtkMRMLFileWriter* writer = this->fileWriter();
  if (writer)
  {
    std::vector<std::string> nodeIDs;
    for (const QString& node : nodes)
    {
      nodeIDs.push_back(node.toStdString());
    }
    writer->SetWrittenNodeIDs(nodeIDs);
  }
}

//----------------------------------------------------------------------------
QStringList qSlicerFileWriter::writtenNodes() const
{
  Q_D(const qSlicerFileWriter);
  vtkMRMLFileWriter* writer = this->fileWriter();
  if (writer)
  {
    return toQStringList(writer->GetWrittenNodeIDs());
  }
  return d->WrittenNodes;
}
