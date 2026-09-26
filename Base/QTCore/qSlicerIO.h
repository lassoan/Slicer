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

#ifndef __qSlicerIO_h
#define __qSlicerIO_h

// Qt includes
#include <QMap>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QImage>
#include <QVariant>

// QtCore includes
#include "qSlicerBaseQTCoreExport.h"
#include "qSlicerObject.h"

class qSlicerIOOptions;
class qSlicerIOPrivate;
class vtkImageData;
class vtkMRMLMessageCollection;
class vtkMRMLFileIOHandler;
class vtkMRMLIOProperties;

/// Base class for qSlicerFileReader and qSlicerFileWriter
///
/// \deprecated Reading and writing is implemented in VTK-based classes (vtkMRMLFileReader, vtkMRMLFileWriter)
/// and managed by vtkMRMLFileIOManager. Qt-based readers and writers are only kept for backward compatibility
/// and to provide Qt-based options widgets.
/// If an IO handler is set (see ioHandler()) then all calls are delegated to the VTK-based IO handler.
class Q_SLICER_BASE_QTCORE_EXPORT qSlicerIO
  : public QObject
  , public qSlicerObject
{
  Q_OBJECT

public:
  typedef QObject Superclass;
  explicit qSlicerIO(QObject* parent = nullptr);
  ~qSlicerIO() override;

  typedef QString IOFileType;
  typedef QVariantMap IOProperties;

  /// Unique name of the reader/writer
  /// By default it returns the description of the VTK-based IO handler.
  Q_INVOKABLE virtual QString description() const;

  /// Multiple readers can share the same file type
  /// By default it returns the file type of the VTK-based IO handler.
  Q_INVOKABLE virtual qSlicerIO::IOFileType fileType() const;

  /// Returns a list of options for the reader. qSlicerIOOptions can be
  /// derived and have a UI associated to it (i.e. qSlicerIOOptionsWidget).
  /// Warning: you are responsible for freeing the memory of the returned
  /// options
  Q_INVOKABLE virtual qSlicerIOOptions* options() const;

  /// Additional warning or error messages occurred during IO operation.
  Q_INVOKABLE vtkMRMLMessageCollection* userMessages() const;

  /// VTK-based reader or writer that performs all the tasks of this class.
  /// Returns nullptr if this is a legacy Qt-based reader or writer that implements reading or writing itself.
  Q_INVOKABLE vtkMRMLFileIOHandler* ioHandler() const;

  /// Set the scene of this object and the VTK-based IO handler.
  void setMRMLScene(vtkMRMLScene* scene) override;

  /// Returns true if the VTK-based IO handler can be used directly instead of this object.
  /// It is true if this object delegates all tasks to the VTK-based IO handler, i.e., this object
  /// is a class that sets the IO handler and it is not a subclass that may override methods.
  bool isIOHandlerUsableDirectly() const;

  /// Convert Qt properties to VTK properties. QImage and QPixmap values are converted to vtkImageData.
  static void toVTKProperties(const IOProperties& properties, vtkMRMLIOProperties* vtkProperties);
  /// Convert VTK properties to Qt properties. vtkImageData values are converted to QImage.
  static IOProperties fromVTKProperties(vtkMRMLIOProperties* vtkProperties);

  /// Convert Qt image to VTK image (RGBA, unsigned char).
  static bool qImageToVTKImageData(const QImage& image, vtkImageData* imageData);
  /// Convert VTK image (unsigned char, 1, 3, or 4 components) to Qt image.
  static QImage vtkImageDataToQImage(vtkImageData* imageData);

protected:
  /// Set the VTK-based reader or writer that performs all the tasks of this class.
  /// Must be called from the constructor of the class that delegates all tasks to the VTK-based IO handler.
  void setIOHandler(vtkMRMLFileIOHandler* handler);

protected:
  QScopedPointer<qSlicerIOPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerIO);
  Q_DISABLE_COPY(qSlicerIO);
};

Q_DECLARE_METATYPE(qSlicerIO::IOFileType)
Q_DECLARE_METATYPE(qSlicerIO::IOProperties)

#endif
