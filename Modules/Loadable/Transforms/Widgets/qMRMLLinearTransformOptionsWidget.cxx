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

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Transforms Widgets includes
#include "qMRMLLinearTransformOptionsWidget.h"
#include "ui_qMRMLLinearTransformOptionsWidget.h"

// Qt includes
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QTableWidgetItem>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAddonMathUtilities.h>
#include <vtkGeneralTransform.h>
#include <vtkLinearTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>

static const int COORDINATE_COMBOBOX_INDEX_WORLD = 0;
static const int COORDINATE_COMBOBOX_INDEX_LOCAL = 1;

//-----------------------------------------------------------------------------
class qMRMLLinearTransformOptionsWidgetPrivate : public Ui_qMRMLLinearTransformOptionsWidget
{
  Q_DECLARE_PUBLIC(qMRMLLinearTransformOptionsWidget);

protected:
  qMRMLLinearTransformOptionsWidget* const q_ptr;

public:
  qMRMLLinearTransformOptionsWidgetPrivate(qMRMLLinearTransformOptionsWidget& object);
  void init();

  QAction* CopyAction{ nullptr };
  QAction* PasteAction{ nullptr };
};

//-----------------------------------------------------------------------------
qMRMLLinearTransformOptionsWidgetPrivate::qMRMLLinearTransformOptionsWidgetPrivate(qMRMLLinearTransformOptionsWidget& object)
  : q_ptr(&object)
{
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidgetPrivate::init()
{
  Q_Q(qMRMLLinearTransformOptionsWidget);
  this->setupUi(q);

  // Copy and paste actions.
  // The Transforms module adds the actions of its options widgets to the module widget, too,
  // so the shortcuts work anywhere in the module.
  this->CopyAction = new QAction(q);
  this->CopyAction->setIcon(QIcon(":Icons/Medium/SlicerEditCopy.png"));
  this->CopyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  this->CopyAction->setShortcuts(QKeySequence::Copy);
  this->CopyAction->setToolTip(qMRMLLinearTransformOptionsWidget::tr("Copy"));
  q->addAction(this->CopyAction);
  this->PasteAction = new QAction(q);
  this->PasteAction->setIcon(QIcon(":Icons/Medium/SlicerEditPaste.png"));
  this->PasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  this->PasteAction->setShortcuts(QKeySequence::Paste);
  this->PasteAction->setToolTip(qMRMLLinearTransformOptionsWidget::tr("Paste"));
  q->addAction(this->PasteAction);

  this->CopyTransformToolButton->setDefaultAction(this->CopyAction);
  QObject::connect(this->CopyAction, SIGNAL(triggered()), q, SLOT(copyTransform()));

  this->PasteTransformToolButton->setDefaultAction(this->PasteAction);
  QObject::connect(this->PasteAction, SIGNAL(triggered()), q, SLOT(pasteTransform()));

  // Coordinate reference button
  QObject::connect(this->TranslateFirstToolButton, SIGNAL(toggled(bool)), q, SLOT(onTranslateFirstButtonPressed(bool)));

  QObject::connect(this->IdentityPushButton, SIGNAL(clicked()), q, SLOT(identity()));

  // Center of transformation
  QObject::connect(this->CenterOfTransformationCoordinatesComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(updateCenterOfTransformationWidgets()));
  QObject::connect(this->CenterOfTransformationCoordinatesWidget, SIGNAL(coordinatesChanged(double*)), q, SLOT(onCenterOfTransformationChanged()));
  QObject::connect(this->ResetCenterOfTransformationButton, SIGNAL(clicked()), q, SLOT(resetCenterOfTransformation()));

  // Set a static min/max range to let users freely enter values
  this->MatrixWidget->setRange(-1e10, 1e10);

  // Homogeneous transformation matrix is expected to have (0,0,0,1)
  // in its last row, so do not allow users to edit the last row.
  for (int col = 0; col < 4; col++)
  {
    QTableWidgetItem* item = this->MatrixWidget->widgetItem(3, col);
    if (!item)
    {
      continue;
    }
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
  }

  this->RotationSliders->setSingleStep(0.1);
  this->RotationSliders->setDecimals(1);
}

//-----------------------------------------------------------------------------
qMRMLLinearTransformOptionsWidget::qMRMLLinearTransformOptionsWidget(QWidget* parentWidget)
  : Superclass(parentWidget)
  , d_ptr(new qMRMLLinearTransformOptionsWidgetPrivate(*this))
{
  Q_D(qMRMLLinearTransformOptionsWidget);
  d->init();
  this->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
qMRMLLinearTransformOptionsWidget::~qMRMLLinearTransformOptionsWidget() = default;

//-----------------------------------------------------------------------------
bool qMRMLLinearTransformOptionsWidget::canManageMRMLTransformNode(vtkMRMLTransformNode* transformNode) const
{
  return transformNode && transformNode->IsLinear();
}

//-----------------------------------------------------------------------------
qMRMLTransformAbstractOptionsWidget* qMRMLLinearTransformOptionsWidget::createInstance() const
{
  return new qMRMLLinearTransformOptionsWidget();
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qMRMLLinearTransformOptionsWidget);
  this->Superclass::setMRMLScene(scene);
  d->TranslationSliders->setMRMLScene(scene);
  d->CenterOfTransformationCoordinatesWidget->setMRMLScene(scene);
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::setMRMLTransformNode(vtkMRMLTransformNode* transformNode)
{
  Q_D(qMRMLLinearTransformOptionsWidget);
  this->TransformNode = transformNode;

  // The node is set even if its transform is not linear, because it may become linear later.
  // The matrix widget and the sliders observe the node and update themselves when it is modified.
  d->TranslationSliders->setMRMLTransformNode(transformNode);
  d->RotationSliders->setMRMLTransformNode(transformNode);
  d->MatrixWidget->setMRMLTransformNode(transformNode);

  this->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::updateWidgetFromMRML()
{
  Q_D(qMRMLLinearTransformOptionsWidget);
  // This method may be called very frequently (when the transform is changing in real time),
  // so only the enabled state is updated here (which returns immediately if the state does not change).
  bool isLinearTransform = this->canManageMRMLTransformNode(this->TransformNode);
  d->MatrixViewGroupBox->setEnabled(isLinearTransform);
  d->TranslateFirstToolButton->setEnabled(isLinearTransform);
  d->CopyAction->setEnabled(isLinearTransform);
  d->PasteAction->setEnabled(isLinearTransform);
  d->IdentityPushButton->setEnabled(isLinearTransform);
  d->CenterOfTransformationGroupBox->setEnabled(isLinearTransform);
  this->updateCenterOfTransformationWidgets();
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::updateCenterOfTransformationWidgets()
{
  Q_D(qMRMLLinearTransformOptionsWidget);
  if (!this->canManageMRMLTransformNode(this->TransformNode))
  {
    return;
  }

  double centerOfTransformation[3] = { 0.0, 0.0, 0.0 };
  this->TransformNode->GetCenterOfTransformation(centerOfTransformation);
  if (d->CenterOfTransformationCoordinatesComboBox->currentIndex() == COORDINATE_COMBOBOX_INDEX_WORLD)
  {
    vtkNew<vtkGeneralTransform> transform;
    this->TransformNode->GetTransformToWorld(transform);
    transform->TransformPoint(centerOfTransformation, centerOfTransformation);
  }

  bool wasBlocked = d->CenterOfTransformationCoordinatesWidget->blockSignals(true);
  d->CenterOfTransformationCoordinatesWidget->setCoordinates(centerOfTransformation);
  d->CenterOfTransformationCoordinatesWidget->blockSignals(wasBlocked);
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::onCenterOfTransformationChanged()
{
  Q_D(qMRMLLinearTransformOptionsWidget);
  if (!this->canManageMRMLTransformNode(this->TransformNode))
  {
    return;
  }

  const double* coordinates = d->CenterOfTransformationCoordinatesWidget->coordinates();
  if (d->CenterOfTransformationCoordinatesComboBox->currentIndex() == COORDINATE_COMBOBOX_INDEX_LOCAL)
  {
    this->TransformNode->SetCenterOfTransformation(coordinates);
  }
  else
  {
    // World coordinates
    vtkNew<vtkGeneralTransform> transform;
    this->TransformNode->GetTransformFromWorld(transform);
    double coordinates_Local[3] = { 0.0, 0.0, 0.0 };
    transform->TransformPoint(coordinates, coordinates_Local);
    this->TransformNode->SetCenterOfTransformation(coordinates_Local);
  }
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::resetCenterOfTransformation()
{
  if (!this->TransformNode)
  {
    return;
  }
  this->TransformNode->SetCenterOfTransformation(0.0, 0.0, 0.0);
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::identity()
{
  if (!this->canManageMRMLTransformNode(this->TransformNode))
  {
    return;
  }
  this->transformAboutToBeReplaced();
  vtkNew<vtkMatrix4x4> matrix; // initialized to identity by default
  this->TransformNode->SetMatrixTransformToParent(matrix.GetPointer());
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::onTranslateFirstButtonPressed(bool checked)
{
  Q_D(qMRMLLinearTransformOptionsWidget);

  qMRMLTransformSliders::CoordinateReferenceType ref = checked ? qMRMLTransformSliders::LOCAL : qMRMLTransformSliders::GLOBAL;
  d->TranslationSliders->setCoordinateReference(ref);
  d->RotationSliders->setCoordinateReference(ref);
}

//-----------------------------------------------------------------------------
int qMRMLLinearTransformOptionsWidget::coordinateReference() const
{
  Q_D(const qMRMLLinearTransformOptionsWidget);
  return (d->TranslateFirstToolButton->isChecked() ? qMRMLTransformSliders::LOCAL : qMRMLTransformSliders::GLOBAL);
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::transformAboutToBeReplaced()
{
  this->resetUnactiveSliders();
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::resetUnactiveSliders()
{
  Q_D(qMRMLLinearTransformOptionsWidget);
  d->TranslationSliders->resetUnactiveSliders();
  d->RotationSliders->resetUnactiveSliders();
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::copyTransform()
{
  if (!this->TransformNode)
  {
    return;
  }

  vtkLinearTransform* linearTransform = vtkLinearTransform::SafeDownCast(this->TransformNode->GetTransformToParent());
  if (!linearTransform)
  {
    // Silent fail, no worries!
    qWarning() << "Unable to cast parent transform as a vtkLinearTransform";
    return;
  }

  vtkMatrix4x4* internalMatrix = linearTransform->GetMatrix();
  std::string delimiter = " ";
  std::string rowDelimiter = "\n";
  std::string output = vtkAddonMathUtilities::ToString(internalMatrix, delimiter, rowDelimiter);
  QApplication::clipboard()->setText(QString::fromStdString(output));
}

//-----------------------------------------------------------------------------
void qMRMLLinearTransformOptionsWidget::pasteTransform()
{
  if (!this->canManageMRMLTransformNode(this->TransformNode))
  {
    return;
  }

  vtkNew<vtkMatrix4x4> tempMatrix;

  std::string text = QApplication::clipboard()->text().toStdString();
  bool success = vtkAddonMathUtilities::FromString(tempMatrix.GetPointer(), text);
  if (!success)
  {
    qWarning() << "Cannot convert pasted string to matrix.";
    return;
  }
  // Homogeneous transformation matrix is expected to have (0,0,0,1)
  // in its last row.
  tempMatrix->SetElement(3, 0, 0.0);
  tempMatrix->SetElement(3, 1, 0.0);
  tempMatrix->SetElement(3, 2, 0.0);
  tempMatrix->SetElement(3, 3, 1.0);
  this->TransformNode->SetMatrixTransformToParent(tempMatrix.GetPointer());
}
