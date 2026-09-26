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

#ifndef __qSlicerNodeWriter_h
#define __qSlicerNodeWriter_h

// QtGUI includes
#include "qSlicerBaseQTGUIExport.h"
#include "qSlicerFileWriter.h"
#include "qSlicerNodeWriterOptionsWidget.h"

// Slicer includes
#include <vtkMRMLNodeWriter.h>

// VTK includes
#include <vtkNew.h>

class vtkMRMLNode;

/// Utility class that is ready to use for most of the nodes.
///
/// \deprecated All tasks are delegated to vtkMRMLNodeWriter.
class Q_SLICER_BASE_QTGUI_EXPORT qSlicerNodeWriter : public qSlicerFileWriter
{
  Q_OBJECT
  /// Some storage nodes don't support the compression option
  Q_PROPERTY(bool supportUseCompression READ supportUseCompression WRITE setSupportUseCompression);

public:
  typedef qSlicerFileWriter Superclass;
  qSlicerNodeWriter(const QString& description,
                    const qSlicerIO::IOFileType& fileType,
                    const QStringList& nodeClassNames,
                    bool supportUseCompression,
                    QObject* parent)
    : Superclass(parent)
  {
    vtkNew<vtkMRMLNodeWriter> writer;
    writer->SetDescription(description.toUtf8().constData());
    writer->SetFileType(fileType.toUtf8().constData());
    writer->SetSupportUseCompression(supportUseCompression);
    this->setIOHandler(writer);
    this->setNodeClassNames(nodeClassNames);
  }

  /// VTK-based writer that performs all tasks
  vtkMRMLNodeWriter* nodeWriter() const { return vtkMRMLNodeWriter::SafeDownCast(this->ioHandler()); }

  void setSupportUseCompression(bool support) { this->nodeWriter()->SetSupportUseCompression(support); }
  bool supportUseCompression() const { return this->nodeWriter()->GetSupportUseCompression(); }

  virtual vtkMRMLNode* getNodeByID(const char* id) const { return this->nodeWriter()->GetNodeByID(id); }

  /// Options widget (for writers that call this method from their options() implementation).
  qSlicerIOOptions* options() const override
  {
    qSlicerNodeWriterOptionsWidget* options = new qSlicerNodeWriterOptionsWidget;
    options->setShowUseCompression(this->supportUseCompression());
    return options;
  }

  /// Constructor for subclasses. A default vtkMRMLNodeWriter is created,
  /// which may be replaced by a vtkMRMLNodeWriter subclass by calling setIOHandler.
  explicit qSlicerNodeWriter(QObject* parent)
    : Superclass(parent)
  {
    vtkNew<vtkMRMLNodeWriter> writer;
    this->setIOHandler(writer);
  }

protected:
  void setNodeClassNames(const QStringList& nodeClassNames)
  {
    std::vector<std::string> classNames;
    for (const QString& className : nodeClassNames)
    {
      classNames.push_back(className.toStdString());
    }
    this->nodeWriter()->SetNodeClassNames(classNames);
  }
  QStringList nodeClassNames() const
  {
    QStringList classNames;
    for (const std::string& className : this->nodeWriter()->GetNodeClassNames())
    {
      classNames << QString::fromStdString(className);
    }
    return classNames;
  }
};

#endif
