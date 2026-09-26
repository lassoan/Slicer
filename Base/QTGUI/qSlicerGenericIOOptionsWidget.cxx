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

// Qt includes
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSpinBox>

// STD includes
#include <cmath>
#include <limits>

// MRML widgets includes
#include <qMRMLColorTableComboBox.h>
#include <qMRMLNodeComboBox.h>

// Slicer includes
#include "qSlicerCoreApplication.h"
#include "qSlicerGenericIOOptionsWidget.h"
#include "qSlicerIOOptions_p.h"
#include <vtkSlicerApplicationLogic.h>
#include <vtkMRMLFileIOManager.h>
#include <vtkMRMLFileReader.h>
#include <vtkMRMLFileWriter.h>
#include <vtkMRMLIOProperties.h>

// MRML includes
#include <vtkMRMLNode.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkNew.h>

namespace
{
//-----------------------------------------------------------------------------
/// Convert JSON value to QVariant. Integer numbers are converted to int (JSON does not distinguish integer and floating-point numbers).
QVariant jsonToVariant(const QJsonValue& value)
{
  if (value.isDouble())
  {
    double doubleValue = value.toDouble();
    if (doubleValue >= static_cast<double>(std::numeric_limits<int>::min()) //
        && doubleValue <= static_cast<double>(std::numeric_limits<int>::max()) //
        && doubleValue == std::floor(doubleValue))
    {
      return QVariant(static_cast<int>(doubleValue));
    }
    return QVariant(doubleValue);
  }
  return value.toVariant();
}
} // namespace

//-----------------------------------------------------------------------------
class qSlicerGenericIOOptionsWidgetPrivate : public qSlicerIOOptionsPrivate
{
public:
  /// Widgets of an option
  struct OptionWidgets
  {
    QString Type;
    QString WidgetHint;
    QString Separator;
    QPointer<QLabel> Label;
    QPointer<QWidget> Control;
  };

  vtkSmartPointer<vtkMRMLFileIOHandler> IOHandler;
  /// Values that the user set
  QVariantMap EditedValues;
  /// Values set by updateGUI
  QVariantMap SpecifiedValues;
  /// ID of the node to write (for writers)
  QString NodeID;
  QMap<QString, OptionWidgets> Options;
  QStringList OptionOrder;
  bool UpdatingOptions{ false };
};

//-----------------------------------------------------------------------------
qSlicerGenericIOOptionsWidget::qSlicerGenericIOOptionsWidget(QWidget* parentWidget)
  : Superclass(new qSlicerGenericIOOptionsWidgetPrivate, parentWidget)
{
  // All options are displayed in a single row (the options widget is displayed
  // in a single row of a table in the data dialogs)
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  this->setLayout(layout);
}

//-----------------------------------------------------------------------------
qSlicerGenericIOOptionsWidget::qSlicerGenericIOOptionsWidget(vtkMRMLFileIOHandler* ioHandler, QWidget* parentWidget)
  : qSlicerGenericIOOptionsWidget(parentWidget)
{
  this->setIOHandler(ioHandler);
}

//-----------------------------------------------------------------------------
qSlicerGenericIOOptionsWidget::~qSlicerGenericIOOptionsWidget() = default;

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::setIOHandler(vtkMRMLFileIOHandler* ioHandler)
{
  Q_D(qSlicerGenericIOOptionsWidget);
  d->IOHandler = ioHandler;
  if (ioHandler && !this->mrmlScene() && ioHandler->GetScene())
  {
    // Node selectors need a scene
    this->setMRMLScene(ioHandler->GetScene());
  }
  this->updateOptions();
}

//-----------------------------------------------------------------------------
vtkMRMLFileIOHandler* qSlicerGenericIOOptionsWidget::ioHandler() const
{
  Q_D(const qSlicerGenericIOOptionsWidget);
  return d->IOHandler;
}

//-----------------------------------------------------------------------------
vtkMRMLFileIOHandler* qSlicerGenericIOOptionsWidget::findIOHandler(const char* className)
{
  qSlicerCoreApplication* app = qSlicerCoreApplication::application();
  vtkMRMLFileIOManager* fileIOManager = (app && app->applicationLogic()) ? app->applicationLogic()->GetFileIOManager() : nullptr;
  if (!fileIOManager || !className)
  {
    return nullptr;
  }
  for (int i = 0; i < fileIOManager->GetNumberOfReaders(); ++i)
  {
    vtkMRMLFileIOHandler* handler = fileIOManager->GetNthReader(i);
    if (strcmp(handler->GetClassName(), className) == 0)
    {
      return handler;
    }
  }
  for (int i = 0; i < fileIOManager->GetNumberOfWriters(); ++i)
  {
    vtkMRMLFileIOHandler* handler = fileIOManager->GetNthWriter(i);
    if (strcmp(handler->GetClassName(), className) == 0)
    {
      return handler;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::initializeUnregisteredIOHandler(vtkMRMLFileIOHandler* handler)
{
  qSlicerCoreApplication* app = qSlicerCoreApplication::application();
  if (!handler || !app)
  {
    return;
  }
  handler->SetScene(app->mrmlScene());
  if (app->applicationLogic())
  {
    // The handler is not registered, but the manager gives access to the application logic
    handler->SetFileIOManager(app->applicationLogic()->GetFileIOManager());
  }
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::setFileName(const QString& fileName)
{
  Q_D(qSlicerGenericIOOptionsWidget);
  this->Superclass::setFileName(fileName);
  // Default values depend on the file name, so previous edits are discarded
  d->EditedValues.clear();
  this->updateOptions();
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::setFileNames(const QStringList& fileNames)
{
  Q_D(qSlicerGenericIOOptionsWidget);
  this->Superclass::setFileNames(fileNames);
  // Default values depend on the file names, so previous edits are discarded
  d->EditedValues.clear();
  this->updateOptions();
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::setObject(vtkObject* object)
{
  Q_D(qSlicerGenericIOOptionsWidget);
  vtkMRMLNode* node = vtkMRMLNode::SafeDownCast(object);
  d->NodeID = (node && node->GetID()) ? QString::fromUtf8(node->GetID()) : QString();
  if (d->NodeID.isEmpty())
  {
    d->Properties.remove("nodeID");
  }
  else
  {
    d->Properties["nodeID"] = d->NodeID;
  }
  // Default values depend on the object, so previous edits are discarded
  d->EditedValues.clear();
  this->updateOptions();
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qSlicerGenericIOOptionsWidget);
  this->Superclass::setMRMLScene(scene);
  // Node selectors may select a node automatically when the scene is set.
  // That is not a change made by the user, so the selected node is restored.
  const bool wasUpdatingOptions = d->UpdatingOptions;
  d->UpdatingOptions = true;
  for (const QString& property : d->OptionOrder)
  {
    qMRMLNodeComboBox* nodeComboBox = qobject_cast<qMRMLNodeComboBox*>(d->Options[property].Control);
    if (nodeComboBox)
    {
      nodeComboBox->setMRMLScene(scene);
      nodeComboBox->setCurrentNodeID(d->Properties.value(property).toString());
    }
  }
  d->UpdatingOptions = wasUpdatingOptions;
  // Options are not updated here: the description does not depend on the scene of the widget
  // (and the scene is set to nullptr when the application is shutting down, when the reader
  // or writer may not be able to provide a description anymore).
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::updateGUI(const qSlicerIO::IOProperties& ioProperties)
{
  Q_D(qSlicerGenericIOOptionsWidget);
  this->Superclass::updateGUI(ioProperties);
  for (qSlicerIO::IOProperties::const_iterator it = ioProperties.constBegin(); it != ioProperties.constEnd(); ++it)
  {
    if (it.key() == "fileName" || it.key() == "fileNames" || it.key() == "nodeID" || it.key() == "fileType")
    {
      continue;
    }
    d->SpecifiedValues[it.key()] = it.value();
    d->EditedValues.remove(it.key());
  }
  this->updateOptions();
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::onOptionChanged()
{
  Q_D(qSlicerGenericIOOptionsWidget);
  if (d->UpdatingOptions)
  {
    return;
  }
  QWidget* control = qobject_cast<QWidget*>(this->sender());
  QString property = control ? control->property("ioOptionProperty").toString() : QString();
  if (property.isEmpty() || !d->Options.contains(property))
  {
    return;
  }
  const qSlicerGenericIOOptionsWidgetPrivate::OptionWidgets& option = d->Options[property];
  QVariant value;
  if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(control))
  {
    value = checkBox->isChecked();
  }
  else if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(control))
  {
    value = spinBox->value();
  }
  else if (QDoubleSpinBox* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(control))
  {
    value = doubleSpinBox->value();
  }
  else if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(control))
  {
    if (option.Type == "stringList")
    {
      QStringList items;
      for (const QString& item : lineEdit->text().split(option.Separator))
      {
        if (!item.trimmed().isEmpty())
        {
          items << item.trimmed();
        }
      }
      value = items;
    }
    else
    {
      value = lineEdit->text();
    }
  }
  else if (qMRMLNodeComboBox* nodeComboBox = qobject_cast<qMRMLNodeComboBox*>(control))
  {
    value = nodeComboBox->currentNodeID();
  }
  else if (QComboBox* comboBox = qobject_cast<QComboBox*>(control))
  {
    value = comboBox->currentData();
  }
  d->EditedValues[property] = value;
  this->updateOptions();
}

//-----------------------------------------------------------------------------
void qSlicerGenericIOOptionsWidget::updateOptions()
{
  Q_D(qSlicerGenericIOOptionsWidget);
  if (!d->IOHandler || d->UpdatingOptions)
  {
    return;
  }

  // Request the description with the current context and values
  qSlicerIO::IOProperties context;
  if (d->Properties.contains("fileName"))
  {
    // setFileNames() stores the list of file names in the fileName property
    QVariant fileNameValue = d->Properties["fileName"];
    if (fileNameValue.userType() == QMetaType::QStringList)
    {
      QStringList fileNames = fileNameValue.toStringList();
      context["fileNames"] = fileNames;
      if (!fileNames.isEmpty())
      {
        context["fileName"] = fileNames.first();
      }
    }
    else
    {
      context["fileName"] = fileNameValue;
    }
  }
  if (!d->NodeID.isEmpty())
  {
    context["nodeID"] = d->NodeID;
  }
  for (QVariantMap::const_iterator it = d->SpecifiedValues.constBegin(); it != d->SpecifiedValues.constEnd(); ++it)
  {
    context[it.key()] = it.value();
  }
  for (QVariantMap::const_iterator it = d->EditedValues.constBegin(); it != d->EditedValues.constEnd(); ++it)
  {
    context[it.key()] = it.value();
  }
  vtkNew<vtkMRMLIOProperties> properties;
  qSlicerIO::toVTKProperties(context, properties);
  QByteArray descriptionJson = QByteArray::fromStdString(d->IOHandler->GetOptionsDescriptionJSON(properties));
  QJsonArray options;
  if (!descriptionJson.isEmpty())
  {
    QJsonParseError parseError;
    QJsonDocument description = QJsonDocument::fromJson(descriptionJson, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
      qWarning() << Q_FUNC_INFO << "failed to parse options description:" << parseError.errorString();
    }
    options = description.object().value("options").toArray();
  }

  d->UpdatingOptions = true;
  QStringList optionOrder;
  for (const QJsonValue& optionValue : options)
  {
    QJsonObject option = optionValue.toObject();
    const QString property = option.value("property").toString();
    const QString type = option.value("type").toString();
    const QString widgetHint = option.value("widget").toString();
    if (property.isEmpty())
    {
      continue;
    }
    optionOrder << property;

    // Create widgets (or re-create if the type changed)
    qSlicerGenericIOOptionsWidgetPrivate::OptionWidgets& widgets = d->Options[property];
    if (widgets.Control.isNull() || widgets.Type != type || widgets.WidgetHint != widgetHint)
    {
      delete widgets.Label;
      delete widgets.Control;
      widgets.Type = type;
      widgets.WidgetHint = widgetHint;
      QWidget* control = nullptr;
      if (type == "bool")
      {
        QCheckBox* checkBox = new QCheckBox(this);
        QObject::connect(checkBox, SIGNAL(toggled(bool)), this, SLOT(onOptionChanged()));
        control = checkBox;
      }
      else if (type == "int")
      {
        QSpinBox* spinBox = new QSpinBox(this);
        QObject::connect(spinBox, SIGNAL(valueChanged(int)), this, SLOT(onOptionChanged()));
        control = spinBox;
      }
      else if (type == "double")
      {
        QDoubleSpinBox* spinBox = new QDoubleSpinBox(this);
        QObject::connect(spinBox, SIGNAL(valueChanged(double)), this, SLOT(onOptionChanged()));
        control = spinBox;
      }
      else if (type == "string" || type == "stringList")
      {
        QLineEdit* lineEdit = new QLineEdit(this);
        QObject::connect(lineEdit, SIGNAL(textEdited(QString)), this, SLOT(onOptionChanged()));
        control = lineEdit;
      }
      else if (type == "enum")
      {
        QComboBox* comboBox = new QComboBox(this);
        QObject::connect(comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onOptionChanged()));
        control = comboBox;
      }
      else if (type == "node")
      {
        qMRMLNodeComboBox* nodeComboBox = (widgetHint == "colorTable") ? new qMRMLColorTableComboBox(this) : new qMRMLNodeComboBox(this);
        nodeComboBox->setAddEnabled(false);
        nodeComboBox->setRemoveEnabled(false);
        nodeComboBox->setRenameEnabled(false);
        QObject::connect(nodeComboBox, SIGNAL(currentNodeIDChanged(QString)), this, SLOT(onOptionChanged()));
        control = nodeComboBox;
      }
      else
      {
        qCritical() << Q_FUNC_INFO << "Unsupported option type" << type << "for property" << property;
        continue;
      }
      control->setObjectName(property + "OptionWidget");
      control->setProperty("ioOptionProperty", property);
      if (type != "bool")
      {
        widgets.Label = new QLabel(this);
        this->layout()->addWidget(widgets.Label);
      }
      this->layout()->addWidget(control);
      widgets.Control = control;
    }

    // Update attributes
    QWidget* control = widgets.Control;
    const QString label = option.value("label").toString();
    const QString toolTip = option.value("toolTip").toString();
    const bool enabled = option.value("enabled").toBool(true);
    const bool visible = option.value("visible").toBool(true);
    control->setToolTip(toolTip);
    control->setEnabled(enabled);
    control->setVisible(visible);
    if (widgets.Label)
    {
      widgets.Label->setText(label);
      widgets.Label->setToolTip(toolTip);
      widgets.Label->setEnabled(enabled);
      widgets.Label->setVisible(visible && !label.isEmpty());
    }

    // Update value (values that the user set are kept)
    const QJsonValue value = option.value("value");
    const bool updateValue = !d->EditedValues.contains(property);
    if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(control))
    {
      checkBox->setText(label);
      if (updateValue)
      {
        checkBox->setChecked(value.toBool());
      }
      d->Properties[property] = checkBox->isChecked();
    }
    else if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(control))
    {
      if (option.contains("minimum") && option.contains("maximum"))
      {
        spinBox->setRange(option.value("minimum").toInt(), option.value("maximum").toInt());
      }
      if (updateValue)
      {
        spinBox->setValue(value.toInt());
      }
      d->Properties[property] = spinBox->value();
    }
    else if (QDoubleSpinBox* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(control))
    {
      if (option.contains("decimals"))
      {
        doubleSpinBox->setDecimals(option.value("decimals").toInt());
      }
      if (option.contains("minimum") && option.contains("maximum"))
      {
        doubleSpinBox->setRange(option.value("minimum").toDouble(), option.value("maximum").toDouble());
      }
      if (updateValue)
      {
        doubleSpinBox->setValue(value.toDouble());
      }
      d->Properties[property] = doubleSpinBox->value();
    }
    else if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(control))
    {
      if (type == "stringList")
      {
        widgets.Separator = option.value("separator").toString(";");
        QStringList items;
        for (const QJsonValue& item : value.toArray())
        {
          items << item.toString();
        }
        if (updateValue)
        {
          lineEdit->setText(items.join(widgets.Separator + " "));
        }
        else
        {
          items = d->EditedValues[property].toStringList();
        }
        if (items.isEmpty())
        {
          d->Properties.remove(property);
        }
        else
        {
          d->Properties[property] = items;
        }
      }
      else
      {
        if (updateValue)
        {
          lineEdit->setText(value.toString());
        }
        if (lineEdit->text().isEmpty())
        {
          d->Properties.remove(property);
        }
        else
        {
          d->Properties[property] = lineEdit->text();
        }
      }
    }
    else if (qMRMLNodeComboBox* nodeComboBox = qobject_cast<qMRMLNodeComboBox*>(control))
    {
      QStringList nodeClasses;
      for (const QJsonValue& nodeClass : option.value("nodeClasses").toArray())
      {
        nodeClasses << nodeClass.toString();
      }
      if (nodeComboBox->nodeTypes() != nodeClasses)
      {
        nodeComboBox->setNodeTypes(nodeClasses);
      }
      nodeComboBox->setNoneEnabled(option.value("noneEnabled").toBool());
      nodeComboBox->setShowHidden(option.value("showHidden").toBool());
      if (nodeComboBox->mrmlScene() != this->mrmlScene())
      {
        nodeComboBox->setMRMLScene(this->mrmlScene());
      }
      if (updateValue)
      {
        nodeComboBox->setCurrentNodeID(value.toString());
      }
      d->Properties[property] = nodeComboBox->currentNodeID();
    }
    else if (QComboBox* comboBox = qobject_cast<QComboBox*>(control))
    {
      // Update choices if changed
      QJsonArray choices = option.value("choices").toArray();
      bool choicesChanged = (comboBox->count() != choices.size());
      for (int i = 0; !choicesChanged && i < choices.size(); ++i)
      {
        QJsonObject choice = choices[i].toObject();
        choicesChanged = (comboBox->itemText(i) != choice.value("label").toString() || comboBox->itemData(i) != jsonToVariant(choice.value("value")));
      }
      if (choicesChanged)
      {
        QVariant currentValue = comboBox->currentData();
        comboBox->clear();
        for (const QJsonValue& choiceValue : choices)
        {
          QJsonObject choice = choiceValue.toObject();
          comboBox->addItem(choice.value("label").toString(), jsonToVariant(choice.value("value")));
        }
        comboBox->setCurrentIndex(comboBox->findData(currentValue));
      }
      if (updateValue)
      {
        comboBox->setCurrentIndex(comboBox->findData(jsonToVariant(value)));
      }
      if (comboBox->currentIndex() >= 0)
      {
        d->Properties[property] = comboBox->currentData();
      }
      else
      {
        d->Properties.remove(property);
      }
    }
  }

  // Remove widgets of options that are not described anymore
  for (const QString& property : d->OptionOrder)
  {
    if (!optionOrder.contains(property))
    {
      delete d->Options[property].Label;
      delete d->Options[property].Control;
      d->Options.remove(property);
      d->Properties.remove(property);
    }
  }
  d->OptionOrder = optionOrder;

  // Make sure widgets are in the same order as options in the description
  // (widgets of options that changed type are re-created and added at the end)
  QBoxLayout* boxLayout = qobject_cast<QBoxLayout*>(this->layout());
  if (boxLayout)
  {
    int layoutIndex = 0;
    for (const QString& property : optionOrder)
    {
      const qSlicerGenericIOOptionsWidgetPrivate::OptionWidgets& widgets = d->Options[property];
      for (QWidget* widget : { static_cast<QWidget*>(widgets.Label), static_cast<QWidget*>(widgets.Control) })
      {
        if (widget && boxLayout->indexOf(widget) != layoutIndex)
        {
          boxLayout->removeWidget(widget);
          boxLayout->insertWidget(layoutIndex, widget);
        }
        if (widget)
        {
          ++layoutIndex;
        }
      }
    }
  }

  d->UpdatingOptions = false;
  this->updateValid();
}
