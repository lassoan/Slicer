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

// STD includes
#include <typeinfo>

// Qt includes
#include <QDebug>
#include <QPixmap>

#include "qSlicerIO.h"

// Slicer includes
#include <vtkMRMLFileIOHandler.h>
#include <vtkMRMLIOProperties.h>

// MRML includes
#include <vtkMRMLMessageCollection.h>

// VTK includes
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>

// CTK includes
#include <ctkPimpl.h>

// STD includes
#include <cstring>

//-----------------------------------------------------------------------------
class qSlicerIOPrivate
{
  Q_DECLARE_PUBLIC(qSlicerIO);

protected:
  qSlicerIO* q_ptr;

public:
  qSlicerIOPrivate(qSlicerIO& object);
  virtual ~qSlicerIOPrivate();

  vtkSmartPointer<vtkMRMLMessageCollection> UserMessages;
  vtkSmartPointer<vtkMRMLFileIOHandler> IOHandler;
  /// Class that set the IO handler. If the actual class of the object is a subclass of this
  /// then methods may be overridden and so the IO handler cannot be used directly.
  const QMetaObject* IOHandlerOwnerMetaObject{ nullptr };
  const std::type_info* IOHandlerOwnerType{ nullptr };
};

//-----------------------------------------------------------------------------
// qSlicerIOPrivate methods

//-----------------------------------------------------------------------------
qSlicerIOPrivate::qSlicerIOPrivate(qSlicerIO& object)
  : q_ptr(&object)
{
  this->UserMessages = vtkSmartPointer<vtkMRMLMessageCollection>::New();
}

//-----------------------------------------------------------------------------
qSlicerIOPrivate::~qSlicerIOPrivate() = default;

//-----------------------------------------------------------------------------
// qSlicerIO methods

//----------------------------------------------------------------------------
qSlicerIO::qSlicerIO(QObject* parentObject)
  : Superclass(parentObject)
  , d_ptr(new qSlicerIOPrivate(*this))
{
  qRegisterMetaType<qSlicerIO::IOFileType>("qSlicerIO::IOFileType");
  qRegisterMetaType<qSlicerIO::IOProperties>("qSlicerIO::IOProperties");
}

//----------------------------------------------------------------------------
qSlicerIO::~qSlicerIO() = default;

//----------------------------------------------------------------------------
QString qSlicerIO::description() const
{
  Q_D(const qSlicerIO);
  if (!d->IOHandler || !d->IOHandler->GetDescription())
  {
    return QString();
  }
  return QString::fromUtf8(d->IOHandler->GetDescription());
}

//----------------------------------------------------------------------------
qSlicerIO::IOFileType qSlicerIO::fileType() const
{
  Q_D(const qSlicerIO);
  if (!d->IOHandler || !d->IOHandler->GetFileType())
  {
    return IOFileType();
  }
  return IOFileType(d->IOHandler->GetFileType());
}

//----------------------------------------------------------------------------
qSlicerIOOptions* qSlicerIO::options() const
{
  return nullptr;
}

//----------------------------------------------------------------------------
vtkMRMLMessageCollection* qSlicerIO::userMessages() const
{
  Q_D(const qSlicerIO);
  if (d->IOHandler)
  {
    return d->IOHandler->GetUserMessages();
  }
  return d->UserMessages;
}

//----------------------------------------------------------------------------
vtkMRMLFileIOHandler* qSlicerIO::ioHandler() const
{
  Q_D(const qSlicerIO);
  return d->IOHandler;
}

//----------------------------------------------------------------------------
void qSlicerIO::setIOHandler(vtkMRMLFileIOHandler* handler)
{
  Q_D(qSlicerIO);
  d->IOHandler = handler;
  // This method is called from the constructor of the class that delegates all tasks
  // to the IO handler, therefore metaObject() and typeid() return the type of that class.
  // The C++ type is checked as well, because subclasses may not use the Q_OBJECT macro.
  d->IOHandlerOwnerMetaObject = this->metaObject();
  d->IOHandlerOwnerType = &typeid(*this);
  if (handler && this->mrmlScene())
  {
    handler->SetScene(this->mrmlScene());
  }
}

//----------------------------------------------------------------------------
bool qSlicerIO::isIOHandlerUsableDirectly() const
{
  Q_D(const qSlicerIO);
  return d->IOHandler && d->IOHandlerOwnerMetaObject == this->metaObject() //
         && d->IOHandlerOwnerType && *d->IOHandlerOwnerType == typeid(*this);
}

//----------------------------------------------------------------------------
void qSlicerIO::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerIO);
  this->qSlicerObject::setMRMLScene(scene);
  if (d->IOHandler)
  {
    d->IOHandler->SetScene(scene);
  }
}

//----------------------------------------------------------------------------
void qSlicerIO::toVTKProperties(const IOProperties& properties, vtkMRMLIOProperties* vtkProperties)
{
  if (!vtkProperties)
  {
    return;
  }
  vtkProperties->RemoveAllProperties();
  for (IOProperties::const_iterator it = properties.constBegin(); it != properties.constEnd(); ++it)
  {
    const std::string name = it.key().toStdString();
    const QVariant& value = it.value();
    if (!value.isValid())
    {
      continue;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int type = value.typeId();
#else
    const int type = value.userType();
#endif
    switch (type)
    {
      case QMetaType::Bool: vtkProperties->SetBoolProperty(name.c_str(), value.toBool()); break;
      case QMetaType::Int:
      case QMetaType::UInt:
      case QMetaType::Short:
      case QMetaType::UShort:
      case QMetaType::Char:
      case QMetaType::UChar: vtkProperties->SetIntProperty(name.c_str(), value.toInt()); break;
      case QMetaType::LongLong:
      case QMetaType::ULongLong:
      case QMetaType::Long:
      case QMetaType::ULong:
        vtkProperties->SetProperty(name.c_str(), vtkVariant(value.toLongLong()));
        break;
      case QMetaType::Float:
      case QMetaType::Double: vtkProperties->SetDoubleProperty(name.c_str(), value.toDouble()); break;
      case QMetaType::QStringList:
      case QMetaType::QVariantList:
      {
        vtkNew<vtkStringArray> values;
        for (const QString& item : value.toStringList())
        {
          values->InsertNextValue(item.toStdString());
        }
        vtkProperties->SetStringListProperty(name.c_str(), values);
        break;
      }
      case QMetaType::QImage:
      case QMetaType::QPixmap:
      {
        QImage image = value.value<QImage>();
        if (type == QMetaType::QPixmap)
        {
          image = value.value<QPixmap>().toImage();
        }
        vtkNew<vtkImageData> imageData;
        if (qSlicerIO::qImageToVTKImageData(image, imageData))
        {
          vtkProperties->SetObjectProperty(name.c_str(), imageData);
        }
        break;
      }
      default:
        if (value.canConvert<QString>())
        {
          vtkProperties->SetStringProperty(name.c_str(), value.toString().toStdString().c_str());
        }
        else if (value.canConvert<QStringList>())
        {
          vtkNew<vtkStringArray> values;
          for (const QString& item : value.toStringList())
          {
            values->InsertNextValue(item.toStdString());
          }
          vtkProperties->SetStringListProperty(name.c_str(), values);
        }
        else
        {
          qWarning() << Q_FUNC_INFO << "Property" << it.key() << "is ignored, its type is not supported:" << value.typeName();
        }
    }
  }
}

//----------------------------------------------------------------------------
qSlicerIO::IOProperties qSlicerIO::fromVTKProperties(vtkMRMLIOProperties* vtkProperties)
{
  IOProperties properties;
  if (!vtkProperties)
  {
    return properties;
  }
  for (const std::string& name : vtkProperties->GetPropertyNames())
  {
    const QString key = QString::fromStdString(name);
    if (vtkProperties->IsBoolProperty(name.c_str()))
    {
      properties[key] = vtkProperties->GetBoolProperty(name.c_str());
    }
    else if (vtkProperties->IsStringListProperty(name.c_str()))
    {
      QStringList values;
      for (const std::string& value : vtkProperties->GetStringListProperty(name.c_str()))
      {
        values << QString::fromStdString(value);
      }
      properties[key] = values;
    }
    else if (vtkProperties->IsObjectProperty(name.c_str()))
    {
      vtkImageData* imageData = vtkImageData::SafeDownCast(vtkProperties->GetObjectProperty(name.c_str()));
      if (imageData)
      {
        properties[key] = qSlicerIO::vtkImageDataToQImage(imageData);
      }
      else
      {
        qWarning() << Q_FUNC_INFO << "Property" << key << "is ignored, it cannot be converted to Qt type";
      }
    }
    else
    {
      vtkVariant value = vtkProperties->GetProperty(name.c_str());
      if (value.IsString())
      {
        properties[key] = QString::fromStdString(value.ToString());
      }
      else if (value.IsFloat() || value.IsDouble())
      {
        properties[key] = value.ToDouble();
      }
      else if (value.IsLongLong() || value.IsUnsignedLongLong() || value.IsLong() || value.IsUnsignedLong())
      {
        properties[key] = value.ToTypeInt64();
      }
      else if (value.IsNumeric())
      {
        properties[key] = value.ToInt();
      }
      else
      {
        properties[key] = QString::fromStdString(value.ToString());
      }
    }
  }
  return properties;
}

//----------------------------------------------------------------------------
bool qSlicerIO::qImageToVTKImageData(const QImage& image, vtkImageData* imageData)
{
  if (!imageData || image.isNull())
  {
    return false;
  }
  const int width = image.width();
  const int height = image.height();
  // Qt image is upside-down compared to VTK
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  QImage normalizedImage = image.convertToFormat(QImage::Format_RGBA8888).flipped();
#else
  QImage normalizedImage = image.convertToFormat(QImage::Format_RGBA8888).mirrored();
#endif
  const int numberOfScalarComponents = 4;
  imageData->SetExtent(0, width - 1, 0, height - 1, 0, 0);
  imageData->AllocateScalars(VTK_UNSIGNED_CHAR, numberOfScalarComponents);
  unsigned char* imageBuffer = static_cast<unsigned char*>(imageData->GetScalarPointer());
  const size_t lineSize = static_cast<size_t>(numberOfScalarComponents) * width;
  for (int y = 0; y < height; ++y)
  {
    memcpy(imageBuffer + y * lineSize, normalizedImage.constScanLine(y), lineSize);
  }
  imageData->GetPointData()->GetScalars()->Modified();
  return true;
}

//----------------------------------------------------------------------------
QImage qSlicerIO::vtkImageDataToQImage(vtkImageData* imageData)
{
  if (!imageData || !imageData->GetPointData() || !imageData->GetPointData()->GetScalars() || imageData->GetScalarType() != VTK_UNSIGNED_CHAR)
  {
    return QImage();
  }
  const int width = imageData->GetDimensions()[0];
  const int height = imageData->GetDimensions()[1];
  const int numberOfScalarComponents = imageData->GetNumberOfScalarComponents();
  QImage::Format pixelFormat;
  switch (numberOfScalarComponents)
  {
    case 1: pixelFormat = QImage::Format_Grayscale8; break;
    case 3: pixelFormat = QImage::Format_RGB888; break;
    case 4: pixelFormat = QImage::Format_RGBA8888; break;
    default: return QImage(); // unsupported pixel format
  }
  const int bytesPerLine = static_cast<int>(imageData->GetIncrements()[1]);
  // The QImage object does not take ownership of the voxel buffer.
  QImage image(static_cast<const uchar*>(imageData->GetScalarPointer()), width, height, bytesPerLine, pixelFormat);
  // Qt image is upside-down compared to VTK. Mirroring deep-copies the pixel buffer.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  return image.flipped();
#else
  return image.mirrored();
#endif
}
