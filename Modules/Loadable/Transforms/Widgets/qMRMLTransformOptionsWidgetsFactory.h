/*==============================================================================

  Copyright (c) The Intervention Centre
  Oslo University Hospital, Oslo, Norway. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Rafael Palomar (The Intervention Centre,
  Oslo University Hospital), based on qMRMLTransformOptionsWidgetsFactory.h by
  Csaba Pinter (Perklab, Queen's University), and was supported by The Research
  Council of Norway through the ALive project (grant nr. 311393).

  ==============================================================================*/

#ifndef qMRMLTransformOptionsWidgetsFactory_h
#define qMRMLTransformOptionsWidgetsFactory_h

// Transforms widgets includes
#include "qMRMLTransformAbstractOptionsWidget.h"
#include "qSlicerTransformsModuleWidgetsExport.h"

// Qt includes
#include <QObject>
#include <QMap>
#include <QPointer>
#include <QWidget>

// -----------------------------------------------------------------------------------------
class qMRMLTransformAbstractOptionsWidget;
class qMRMLTransformOptionsWidgetsFactoryCleanup;

/// \class qMRMLTransformOptionsWidgetsFactory
/// \brief Singleton class managing the additional options widgets of the Transforms module
/// \sa qMRMLTransformAbstractOptionsWidget
class Q_SLICER_MODULE_TRANSFORMS_WIDGETS_EXPORT qMRMLTransformOptionsWidgetsFactory : public QObject
{
  Q_OBJECT

public:
  /// Instance getter for the singleton class
  /// \return Instance object
  Q_INVOKABLE static qMRMLTransformOptionsWidgetsFactory* instance();

public:
  /// Register an additional options widget.
  /// This factory object takes ownership of the widget (even if it fails to register it
  /// due to for example a widget of that class name already being registered), therefore the
  /// caller must NOT delete the widget.
  Q_INVOKABLE bool registerOptionsWidget(qMRMLTransformAbstractOptionsWidget* widget);

  /// Unregister an additional options widget.
  Q_INVOKABLE bool unregisterOptionsWidget(const QString& className);

  /// Unregister an additional options widget.
  Q_INVOKABLE bool unregisterOptionsWidget(qMRMLTransformAbstractOptionsWidget* widget);

  /// Unregister all widgets
  Q_INVOKABLE void unregisterAll();

  /// Returns the list of additional options widgets registered, in the order that they were registered in.
  /// The Transforms module shows the widgets of a section in this order.
  Q_INVOKABLE const QList<QString> registeredOptionsWidgetsClassNames() const { return this->RegisteredWidgetsClassNames; }

  /// Creates a transform options widget of a given class name. The transform options
  /// widget must have been registered previously (nullptr will return if not).
  Q_INVOKABLE qMRMLTransformAbstractOptionsWidget* createWidget(const QString& widgetClassName);

signals:
  void optionsWidgetRegistered();
  void optionsWidgetUnregistered();

protected:
  QMap<QString, QPointer<qMRMLTransformAbstractOptionsWidget>> RegisteredWidgets;
  /// Class names of the registered widgets, in the order of registration
  QList<QString> RegisteredWidgetsClassNames;

private:
  /// Allows cleanup of the singleton at application exit
  static void cleanup();

private:
  qMRMLTransformOptionsWidgetsFactory(QObject* parent = nullptr);
  ~qMRMLTransformOptionsWidgetsFactory() override;

  Q_DISABLE_COPY(qMRMLTransformOptionsWidgetsFactory);
  friend class qMRMLTransformOptionsWidgetsFactoryCleanup;
  friend class PythonQtWrapper_qMRMLTransformOptionsWidgetsFactory; // Allow Python wrapping without enabling direct instantiation

private:
  /// Instance of the singleton
  static qMRMLTransformOptionsWidgetsFactory* Instance;
};

#endif
