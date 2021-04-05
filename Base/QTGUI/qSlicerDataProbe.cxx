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

// Qt includes
#include <QDebug>
#include <QScrollBar>

// CTK includes
#include <ctkWidgetsUtils.h>

// Slicer includes
#include "qSlicerCoreApplication.h"
#include "qSlicerDataProbe.h"
#include "ui_qSlicerDataProbe.h"
#include "qSlicerModuleManager.h"
#include "qSlicerAbstractModule.h"
#include "qSlicerAbstractModuleWidget.h"
#include "qSlicerUtils.h"

//---------------------------------------------------------------------------
class qSlicerDataProbePrivate: public Ui_qSlicerDataProbe
{
public:
  void setupUi(qSlicerWidget* widget);

  bool HelpAndAcknowledgmentVisible;
};

//---------------------------------------------------------------------------
qSlicerDataProbe::qSlicerDataProbe(QWidget* _parent, Qt::WindowFlags f)
  :qSlicerAbstractModulePanel(_parent, f)
  , d_ptr(new qSlicerDataProbePrivate)
{
  Q_D(qSlicerDataProbe);
  d->HelpAndAcknowledgmentVisible = true;
  d->setupUi(this);
}

//---------------------------------------------------------------------------
qSlicerDataProbe::~qSlicerDataProbe()
{
  this->setModule(QString());
}

//---------------------------------------------------------------------------
qSlicerAbstractCoreModule* qSlicerDataProbe::currentModule()const
{
  Q_D(const qSlicerDataProbe);
  QBoxLayout* scrollAreaLayout =
    qobject_cast<QBoxLayout*>(d->ScrollArea->widget()->layout());

  QLayoutItem* item = scrollAreaLayout->itemAt(1);

  qSlicerAbstractModuleWidget* currentModuleWidget =
    item ? qobject_cast<qSlicerAbstractModuleWidget*>(item->widget()) : 0;

  return currentModuleWidget ? currentModuleWidget->module() : nullptr;
}

//---------------------------------------------------------------------------
QString qSlicerDataProbe::currentModuleName()const
{
  qSlicerAbstractCoreModule* module = this->currentModule();
  return module ? module->name() : QString();
}

//---------------------------------------------------------------------------
void qSlicerDataProbe::setModule(const QString& moduleName)
{
  if (!this->moduleManager())
    {
    return;
    }

  // Log when the user switches between modules so that if the application crashed
  // we knew which module was active.
  qDebug() << "Switch to module: " << moduleName;

  qSlicerAbstractCoreModule * module = nullptr;
  if (!moduleName.isEmpty())
    {
    module = this->moduleManager()->module(moduleName);
    Q_ASSERT(module);
    }
  this->setModule(module);
}

//---------------------------------------------------------------------------
void qSlicerDataProbe::setModule(qSlicerAbstractCoreModule* module)
{
  // Retrieve current module associated with the module panel
  qSlicerAbstractCoreModule* oldModule = this->currentModule();
  if (module == oldModule)
    {
    return;
    }

  if (oldModule)
    {
    // Remove the current module
    this->removeModule(oldModule);
    }

  if (module)
    {
    // Add the new module
    this->addModule(module);
    }
  else
    {
    //d->HelpLabel->setHtml("");
    }
}

//---------------------------------------------------------------------------
void qSlicerDataProbe::addModule(qSlicerAbstractCoreModule* module)
{
  Q_ASSERT(module);

  qSlicerAbstractModuleWidget* moduleWidget =
    dynamic_cast<qSlicerAbstractModuleWidget*>(module->widgetRepresentation());
  if (moduleWidget == nullptr)
    {
    qDebug() << "Warning, there is no UI for the module"<< module->name();
    emit moduleAdded(module->name());
    return;
    }

  Q_ASSERT(!moduleWidget->moduleName().isEmpty());

  Q_D(qSlicerDataProbe);

  // Update module layout
  if (moduleWidget->layout())
    {
    moduleWidget->layout()->setContentsMargins(0, 0, 0, 0);
    }
  else
    {
    qWarning() << moduleWidget->moduleName() << "widget doesn't have a top-level layout.";
    }

  QWidget* scrollAreaContents = d->ScrollArea->widget();
  // Insert module in the panel
  QBoxLayout* scrollAreaLayout =
    qobject_cast<QBoxLayout*>(scrollAreaContents->layout());
  Q_ASSERT(scrollAreaLayout);
  scrollAreaLayout->insertWidget(1, moduleWidget,1);

  moduleWidget->setSizePolicy(QSizePolicy::Minimum, moduleWidget->sizePolicy().verticalPolicy());
  moduleWidget->setVisible(true);

  QString help = module->helpText();

  qSlicerCoreApplication* app = qSlicerCoreApplication::application();
  if (app)
    {
    QString wikiVersion = "Nightly";
    if (app->releaseType() == "Stable")
      {
      wikiVersion = QString("%1.%2").arg(app->majorVersion()).arg(app->minorVersion());
      }
    help = qSlicerUtils::replaceWikiUrlVersion(module->helpText(), wikiVersion);
    }
  help.replace("\\n", "<br>");

  d->HelpCollapsibleButton->setVisible(this->isHelpAndAcknowledgmentVisible() && !help.isEmpty());
  d->HelpLabel->setHtml(help);
  //d->HelpLabel->load(QString("http://www.slicer.org/slicerWiki/index.php?title=Modules:Transforms-Documentation-3.4&useskin=chick"));
  d->AcknowledgementLabel->clear();
  qSlicerAbstractModule* guiModule = qobject_cast<qSlicerAbstractModule*>(module);
  if (guiModule && !guiModule->logo().isNull())
    {
    d->AcknowledgementLabel->document()->addResource(QTextDocument::ImageResource,
      QUrl("module://logo.png"), QVariant(guiModule->logo()));
    d->AcknowledgementLabel->append(
      QString("<center><img src=\"module://logo.png\"/></center><br>"));
    }
  QString acknowledgement = module->acknowledgementText();
  d->AcknowledgementLabel->insertHtml(acknowledgement);
  if (!module->contributors().isEmpty())
    {
    QString contributors = module->contributors().join(", ");
    QString contributorsText = QString("<br/><u>Contributors:</u> <i>") + contributors + "</i><br/>";
    d->AcknowledgementLabel->append(contributorsText);
    }

  moduleWidget->installEventFilter(this);
  this->updateGeometry();

  moduleWidget->enter();

  emit moduleAdded(module->name());
}

//---------------------------------------------------------------------------
void qSlicerDataProbe::removeModule(qSlicerAbstractCoreModule* module)
{
  Q_ASSERT(module);

  qSlicerAbstractModuleWidget * moduleWidget =
    dynamic_cast<qSlicerAbstractModuleWidget*>(module->widgetRepresentation());
  Q_ASSERT(moduleWidget);

  Q_D(qSlicerDataProbe);

  QBoxLayout* scrollAreaLayout =
    qobject_cast<QBoxLayout*>(d->ScrollArea->widget()->layout());
  Q_ASSERT(scrollAreaLayout);
  int index = scrollAreaLayout->indexOf(moduleWidget);
  if (index == -1)
    {
    return;
    }

  moduleWidget->exit();
  moduleWidget->removeEventFilter(this);

  //emit moduleAboutToBeRemoved(moduleWidget->module());

  // Remove widget from layout
  //d->Layout->removeWidget(module);
  scrollAreaLayout->takeAt(index);

  moduleWidget->setVisible(false);
  moduleWidget->setParent(nullptr);

  // if nobody took ownership of the module, make sure it both lost its parent and is hidden
//   if (moduleWidget->parent() == d->Layout->parentWidget())
//     {
//     moduleWidget->setVisible(false);
//     moduleWidget->setParent(0);
//     }

  emit moduleRemoved(module->name());
}

//---------------------------------------------------------------------------
void qSlicerDataProbe::removeAllModules()
{
  this->setModule("");
}

//---------------------------------------------------------------------------
void qSlicerDataProbe::setHelpAndAcknowledgmentVisible(bool value)
{
  Q_D(qSlicerDataProbe);
  d->HelpAndAcknowledgmentVisible = value;
  if (value)
    {
    if (!d->HelpLabel->toHtml().isEmpty())
      {
      d->HelpCollapsibleButton->setVisible(true);
      }
    }
  else
    {
    d->HelpCollapsibleButton->setVisible(false);
    }
}

//---------------------------------------------------------------------------
bool qSlicerDataProbe::isHelpAndAcknowledgmentVisible()const
{
  Q_D(const qSlicerDataProbe);
  return d->HelpAndAcknowledgmentVisible;
}

//---------------------------------------------------------------------------
bool qSlicerDataProbe::eventFilter(QObject* watchedModule, QEvent* event)
{
  Q_UNUSED(watchedModule);
  if (event->type() == QEvent::Resize)
    {
    // The module has a new size, that means the module panel should probably
    // be resized as well.
    this->updateGeometry();
    }
  return false;
}

//---------------------------------------------------------------------------
QSize qSlicerDataProbe::minimumSizeHint()const
{
  Q_D(const qSlicerDataProbe);
  // QScrollArea::minumumSizeHint is wrong. QScrollArea are not meant to
  // be resized. The minimumSizeHint is actually the width of the module
  // representation.
  QSize size = QSize(d->ScrollArea->widget()->minimumSizeHint().width() +
                  d->ScrollArea->horizontalScrollBar()->sizeHint().width(),
                  this->Superclass::minimumSizeHint().height());
  return size;
}

//---------------------------------------------------------------------------
// qSlicerDataProbePrivate methods

//---------------------------------------------------------------------------
void qSlicerDataProbePrivate::setupUi(qSlicerWidget * widget)
{
  this->Ui_qSlicerDataProbe::setupUi(widget);
  this->HelpLabel->setOpenExternalLinks(true);
  this->AcknowledgementLabel->setOpenExternalLinks(true);
}

/*
  QWidget* panel = new QWidget;
  this->HelpCollapsibleButton = new ctkCollapsibleButton("Help");
  this->HelpCollapsibleButton->setCollapsed(true);
  this->HelpCollapsibleButton->setSizePolicy(
    QSizePolicy::Ignored, QSizePolicy::Minimum);

  // Note: QWebView could be used as an alternative to ctkFittedTextBrowser ?
  //this->HelpLabel = new QWebView;
  this->HelpLabel = static_cast<QTextBrowser*>(new ctkFittedTextBrowser);
  this->HelpLabel->setOpenExternalLinks(true);
  this->HelpLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->HelpLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->HelpLabel->setFrameShape(QFrame::NoFrame);

  QPalette p = this->HelpLabel->palette();
  p.setBrush(QPalette::Window, QBrush ());
  this->HelpLabel->setPalette(p);

  QGridLayout* helpLayout = new QGridLayout(this->HelpCollapsibleButton);
  helpLayout->addWidget(this->HelpLabel);

  this->Layout = new QVBoxLayout(panel);
  this->Layout->addWidget(this->HelpCollapsibleButton);
  this->Layout->addItem(
    new QSpacerItem(0, 0, QSizePolicy::Minimum,
                    QSizePolicy::MinimumExpanding));

  this->ScrollArea = new QScrollArea;
  this->ScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  this->ScrollArea->setWidget(panel);
  this->ScrollArea->setWidgetResizable(true);

  QGridLayout* gridLayout = new QGridLayout;
  gridLayout->addWidget(this->ScrollArea);
  gridLayout->setContentsMargins(0,0,0,0);
  widget->setLayout(gridLayout);
}
*/
