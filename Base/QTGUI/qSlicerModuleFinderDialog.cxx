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
#include <algorithm>

// Qt includes
#include <QKeyEvent>
#include <QPushButton>

// QtGUI includes
#include "qSlicerAbstractCoreModule.h"
#include "qSlicerAbstractModule.h"
#include "qSlicerApplication.h"
#include "qSlicerModuleFactoryFilterModel.h"
#include "qSlicerModuleFactoryManager.h"
#include "qSlicerModuleManager.h"
//#include "qSlicerModulesMenu.h"
#include "qSlicerModuleFinderDialog.h"
#include "qSlicerUtils.h"
#include "ui_qSlicerModuleFinderDialog.h"

// --------------------------------------------------------------------------
// qSlicerModuleFinderDialogPrivate

//-----------------------------------------------------------------------------
class qSlicerModuleFinderDialogPrivate: public Ui_qSlicerModuleFinderDialog
{
  Q_DECLARE_PUBLIC(qSlicerModuleFinderDialog);
protected:
  qSlicerModuleFinderDialog* const q_ptr;

public:
  qSlicerModuleFinderDialogPrivate(qSlicerModuleFinderDialog& object);

  /// Convenient method regrouping all initialization code
  void init();

  QString CurrentModuleName;
};

// --------------------------------------------------------------------------
// qSlicerModuleFinderDialogPrivate methods

// --------------------------------------------------------------------------
qSlicerModuleFinderDialogPrivate::qSlicerModuleFinderDialogPrivate(qSlicerModuleFinderDialog& object)
  :q_ptr(&object)
{
  //this->ModulesMenu = nullptr;
}

// --------------------------------------------------------------------------
void qSlicerModuleFinderDialogPrivate::init()
{
  Q_Q(qSlicerModuleFinderDialog);

  this->setupUi(q);

  qSlicerCoreApplication * coreApp = qSlicerCoreApplication::application();
  qSlicerAbstractModuleFactoryManager* factoryManager = coreApp->moduleManager()->factoryManager();

  qSlicerModuleFactoryFilterModel* filterModel = this->SlicerModulesListView->filterModel();

  // Hide modules that do not have GUI (use cannot switch to them)
  filterModel->setShowHidden(false);
  // Hide testing modules by default
  filterModel->setShowTesting(false);

  // Hide module details by default
  this->ExpandModuleDetailsButton->setChecked(false);

  QObject::connect(this->ShowBuiltInCheckBox, SIGNAL(toggled(bool)),
    filterModel, SLOT(setShowBuiltIn(bool)));
  QObject::connect(this->ShowTestingCheckBox, SIGNAL(toggled(bool)),
    filterModel, SLOT(setShowTesting(bool)));
  QObject::connect(this->FilterTitleSearchBox, SIGNAL(textChanged(QString)),
    q, SLOT(setModuleTitleFilterText(QString)));

  QObject::connect(this->SlicerModulesListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
    q, SLOT(onSelectionChanged(QItemSelection, QItemSelection)));

  this->FilterTitleSearchBox->installEventFilter(q);

  QPushButton* okButton = this->ButtonBox->button(QDialogButtonBox::Ok);
  okButton->setText(q->tr("Switch to module"));

  if (filterModel->rowCount() > 0)
    {
    // select first item
    this->SlicerModulesListView->setCurrentIndex(filterModel->index(0, 0));
    }
}

// --------------------------------------------------------------------------
// qSlicerModuleFinderDialog methods

// --------------------------------------------------------------------------
qSlicerModuleFinderDialog::qSlicerModuleFinderDialog(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerModuleFinderDialogPrivate(*this))
{
  Q_D(qSlicerModuleFinderDialog);
  d->init();
}

// --------------------------------------------------------------------------
qSlicerModuleFinderDialog::~qSlicerModuleFinderDialog() = default;

void qSlicerModuleFinderDialog::setFactoryManager(qSlicerAbstractModuleFactoryManager* factoryManager)
{
  Q_D(qSlicerModuleFinderDialog);
  d->SlicerModulesListView->setFactoryManager(factoryManager);
}

//------------------------------------------------------------------------------
void qSlicerModuleFinderDialog::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  Q_UNUSED(deselected);
  Q_D(qSlicerModuleFinderDialog);

  qSlicerAbstractCoreModule* module = nullptr;
  if (!selected.indexes().empty())
    {
    QString moduleName = selected.indexes().first().data(Qt::UserRole).toString();
    qSlicerCoreApplication* coreApp = qSlicerCoreApplication::application();
    qSlicerModuleManager* moduleManager = coreApp->moduleManager();
    module = moduleManager->module(moduleName);
    }

  if (module)
    {
    d->CurrentModuleName = module->name();

    d->ModuleNameLineEdit->setText(module->name());

    QString type = QLatin1String("Core");
    if (module->inherits("qSlicerScriptedLoadableModule"))
      {
      type = QLatin1String("Python Scripted Loadable");
      }
    else if (module->inherits("qSlicerLoadableModule"))
      {
      type = QLatin1String("C++ Loadable");
      }
    else if (module->inherits("qSlicerCLIModule"))
      {
      type = QLatin1String("Command-Line Interface (CLI)");
      }
    if (module->isBuiltIn())
      {
      type += QLatin1String(", built-in");
      }

    d->ModuleTypeLineEdit->setText(type);

    QStringList categories = module->categories();
    QStringList filteredCategories;
    foreach(QString category, categories)
      {
      if (category.isEmpty())
        {
        category = QLatin1String("[main]");
        }
      else
        {
        category.replace(".", "->");
        }
      filteredCategories << category;
      }
    d->ModuleCategoriesLineEdit->setText(filteredCategories.join(", "));

    d->ModuleDependenciesLineEdit->setText(module->dependencies().join(", "));

    d->ModuleLocationLineEdit->setText(module->path());

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
    d->ModuleHelpBrowser->setHtml(help);

    d->ModuleAcknowledgementBrowser->clear();
    qSlicerAbstractModule* guiModule = qobject_cast<qSlicerAbstractModule*>(module);
    if (guiModule && !guiModule->logo().isNull())
      {
      d->ModuleAcknowledgementBrowser->document()->addResource(QTextDocument::ImageResource,
        QUrl("module://logo.png"), QVariant(guiModule->logo()));
      d->ModuleAcknowledgementBrowser->append(
        QString("<center><img src=\"module://logo.png\"/></center><br>"));
      }
    QString acknowledgement = module->acknowledgementText();
    d->ModuleAcknowledgementBrowser->insertHtml(acknowledgement);
    if (!module->contributors().isEmpty())
      {
      if (!acknowledgement.isEmpty())
        {
        // separate from acknowledgment text
        d->ModuleAcknowledgementBrowser->append("<br/>");
        }
      QString contributors = module->contributors().join(", ");
      QString contributorsText = QString("<u>Contributors:</u> <i>") + contributors + "</i><br/>";
      d->ModuleAcknowledgementBrowser->append(contributorsText);
      }
    }
  else
    {
    d->CurrentModuleName.clear();
    d->ModuleNameLineEdit->clear();
    d->ModuleTypeLineEdit->clear();
    d->ModuleCategoriesLineEdit->clear();
    d->ModuleLocationLineEdit->clear();
    d->ModuleDependenciesLineEdit->clear();
    d->ModuleHelpBrowser->clear();
    }
}

//---------------------------------------------------------------------------
bool qSlicerModuleFinderDialog::eventFilter(QObject* target, QEvent* event)
{
  Q_D(qSlicerModuleFinderDialog);
  if (target == d->FilterTitleSearchBox)
    {
    // Prevent giving the focus to the previous/next widget if arrow keys are used
    // at the edge of the table (without this: if the current cell is in the top
    // row and user press the Up key, the focus goes from the table to the previous
    // widget in the tab order)
    if (event->type() == QEvent::KeyPress)
      {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      qSlicerModuleFactoryFilterModel* filterModel = d->SlicerModulesListView->filterModel();
      if (keyEvent != nullptr && filterModel->rowCount() > 0)
        {
        int currentRow = d->SlicerModulesListView->currentIndex().row();
        int stepSize = 1;
        if (keyEvent->key() == Qt::Key_PageUp || keyEvent->key() == Qt::Key_PageDown)
          {
          stepSize = 5;
          }
        else if (keyEvent->key() == Qt::Key_Home || keyEvent->key() == Qt::Key_End)
          {
          stepSize = 10000;
          }
        if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_PageUp || keyEvent->key() == Qt::Key_Home)
          {
          if (currentRow > 0)
            {
            d->SlicerModulesListView->setCurrentIndex(filterModel->index(std::max(0, currentRow - stepSize), 0));
            d->SlicerModulesListView->scrollTo(d->SlicerModulesListView->currentIndex());
            }
          return true;
          }
        else if (keyEvent->key() == Qt::Key_Down || keyEvent->key() == Qt::Key_PageDown || keyEvent->key() == Qt::Key_End)
          {
          if (currentRow + 1 < filterModel->rowCount())
            {
            d->SlicerModulesListView->setCurrentIndex(filterModel->index(std::min(currentRow + stepSize, filterModel->rowCount()-1), 0));
            d->SlicerModulesListView->scrollTo(d->SlicerModulesListView->currentIndex());
            }
          return true;
          }
        }
      }
    }
  return this->Superclass::eventFilter(target, event);
}

//---------------------------------------------------------------------------
QString qSlicerModuleFinderDialog::currentModuleName()const
{
  Q_D(const qSlicerModuleFinderDialog);
  return d->CurrentModuleName;
}

//---------------------------------------------------------------------------
void qSlicerModuleFinderDialog::setFocusToModuleTitleFilter()
{
  Q_D(const qSlicerModuleFinderDialog);
  d->FilterTitleSearchBox->setFocus();
  if (d->SlicerModulesListView->currentIndex().isValid())
    {
    d->SlicerModulesListView->scrollTo(d->SlicerModulesListView->currentIndex());
    }
}

//---------------------------------------------------------------------------
void qSlicerModuleFinderDialog::setModuleTitleFilterText(const QString& text)
{
  Q_D(const qSlicerModuleFinderDialog);
  qSlicerModuleFactoryFilterModel* filterModel = d->SlicerModulesListView->filterModel();
  filterModel->setFilterFixedString(d->FilterTitleSearchBox->text());
  // Make sure that an item is selected
  if (!d->SlicerModulesListView->currentIndex().isValid())
    {
    if (filterModel->rowCount() > 0)
      {
      // select first item
      d->SlicerModulesListView->setCurrentIndex(filterModel->index(0, 0));
      }
    }
  // Make sure that the selected item is visible
  if (d->SlicerModulesListView->currentIndex().isValid())
    {
    d->SlicerModulesListView->scrollTo(d->SlicerModulesListView->currentIndex());
    }
}
