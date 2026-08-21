/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under this License.

==============================================================================*/

// qMRML includes
#include "qMRMLGlyphDisplayWidget.h"
#include "ui_qMRMLGlyphDisplayWidget.h"

// MRML includes
#include <vtkMRMLGlyphDisplayNode.h>
#include <vtkMRMLModelNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkWeakPointer.h>

//------------------------------------------------------------------------------
class qMRMLGlyphDisplayWidgetPrivate
  : public QWidget
  , public Ui_qMRMLGlyphDisplayWidget
{
  Q_DECLARE_PUBLIC(qMRMLGlyphDisplayWidget);

protected:
  qMRMLGlyphDisplayWidget* const q_ptr;
  typedef QWidget Superclass;

public:
  qMRMLGlyphDisplayWidgetPrivate(qMRMLGlyphDisplayWidget& object);
  void init();

  bool IsUpdatingWidgetFromMRML{ false };

  vtkWeakPointer<vtkMRMLGlyphDisplayNode> GlyphDisplayNode;
  vtkWeakPointer<vtkMRMLModelNode> ModelNode;
};

//------------------------------------------------------------------------------
qMRMLGlyphDisplayWidgetPrivate::qMRMLGlyphDisplayWidgetPrivate(qMRMLGlyphDisplayWidget& object)
  : q_ptr(&object)
{
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidgetPrivate::init()
{
  Q_Q(qMRMLGlyphDisplayWidget);
  this->setupUi(q);

  this->GlyphTypeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Arrow"), vtkMRMLGlyphDisplayNode::GlyphTypeArrow);
  this->GlyphTypeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Cone"), vtkMRMLGlyphDisplayNode::GlyphTypeCone);
  this->GlyphTypeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Box"), vtkMRMLGlyphDisplayNode::GlyphTypeBox);
  this->GlyphTypeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Cylinder"), vtkMRMLGlyphDisplayNode::GlyphTypeCylinder);
  this->GlyphTypeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Line"), vtkMRMLGlyphDisplayNode::GlyphTypeLine);
  this->GlyphTypeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Sphere"), vtkMRMLGlyphDisplayNode::GlyphTypeSphere);

  this->VectorScaleModeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Scale by magnitude"), vtkMRMLGlyphDisplayNode::VectorScaleModeByMagnitude);
  this->VectorScaleModeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("Scale by components"), vtkMRMLGlyphDisplayNode::VectorScaleModeByComponents);

  this->MaskingModeComboBox->addItem(qMRMLGlyphDisplayWidget::tr("All points"), vtkMRMLGlyphDisplayNode::MaskingModeAllPoints);
  this->MaskingModeComboBox->addItem(              //
    qMRMLGlyphDisplayWidget::tr("Every Nth point"), //
    vtkMRMLGlyphDisplayNode::MaskingModeEveryNthPoint);
  this->MaskingModeComboBox->addItem(                                             //
    qMRMLGlyphDisplayWidget::tr("Uniform spatial distribution - bounds based"), //
    vtkMRMLGlyphDisplayNode::MaskingModeUniformBounds);
  this->MaskingModeComboBox->addItem(                                               //
    qMRMLGlyphDisplayWidget::tr("Uniform spatial distribution - surface sampling"), //
    vtkMRMLGlyphDisplayNode::MaskingModeUniformSurface);
  this->MaskingModeComboBox->addItem(                                              //
    qMRMLGlyphDisplayWidget::tr("Uniform spatial distribution - volume sampling"), //
    vtkMRMLGlyphDisplayNode::MaskingModeUniformVolume);

  q->connect(this->GlyphTypeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onGlyphTypeChanged(int)));
  q->connect(this->OrientationArrayComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onOrientationArrayChanged(int)));
  q->connect(this->ScaleArrayComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onScaleArrayChanged(int)));
  q->connect(this->VectorScaleModeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onVectorScaleModeChanged(int)));
  q->connect(this->ScaleFactorSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onScaleFactorChanged(double)));
  q->connect(this->MaskingModeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onMaskingModeChanged(int)));
  q->connect(this->MaskingNthPointSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onMaskingNthPointChanged(int)));
  q->connect(this->MaskingPointsNumberSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onMaskingPointsNumberChanged(int)));

  q->setEnabled(false);
}

//------------------------------------------------------------------------------
qMRMLGlyphDisplayWidget::qMRMLGlyphDisplayWidget(QWidget* _parent)
  : qMRMLWidget(_parent)
  , d_ptr(new qMRMLGlyphDisplayWidgetPrivate(*this))
{
  Q_D(qMRMLGlyphDisplayWidget);
  d->init();
}

//------------------------------------------------------------------------------
qMRMLGlyphDisplayWidget::~qMRMLGlyphDisplayWidget() = default;

//------------------------------------------------------------------------------
vtkMRMLGlyphDisplayNode* qMRMLGlyphDisplayWidget::mrmlGlyphDisplayNode() const
{
  Q_D(const qMRMLGlyphDisplayWidget);
  return d->GlyphDisplayNode;
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::setMRMLGlyphDisplayNode(vtkMRMLNode* node)
{
  this->setMRMLGlyphDisplayNode(vtkMRMLGlyphDisplayNode::SafeDownCast(node));
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::setMRMLGlyphDisplayNode(vtkMRMLGlyphDisplayNode* glyphDisplayNode)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (d->GlyphDisplayNode == glyphDisplayNode)
  {
    return;
  }

  qvtkReconnect(d->GlyphDisplayNode, glyphDisplayNode, vtkCommand::ModifiedEvent, this, SLOT(updateWidgetFromMRML()));
  d->GlyphDisplayNode = glyphDisplayNode;

  // Also observe mesh changes on the associated model node, so that the
  // array combo boxes are refreshed when the model's point data changes.
  vtkMRMLModelNode* modelNode = glyphDisplayNode ? vtkMRMLModelNode::SafeDownCast(glyphDisplayNode->GetDisplayableNode()) : nullptr;
  qvtkReconnect(d->ModelNode, modelNode, vtkMRMLModelNode::MeshModifiedEvent, this, SLOT(updateWidgetFromMRML()));
  d->ModelNode = modelNode;

  this->updateWidgetFromMRML();
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::updateWidgetFromMRML()
{
  Q_D(qMRMLGlyphDisplayWidget);
  this->setEnabled(d->GlyphDisplayNode.GetPointer() != nullptr);
  if (!d->GlyphDisplayNode.GetPointer())
  {
    return;
  }
  if (d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->IsUpdatingWidgetFromMRML = true;

  int glyphTypeIndex = d->GlyphTypeComboBox->findData(d->GlyphDisplayNode->GetGlyphType());
  d->GlyphTypeComboBox->setCurrentIndex(glyphTypeIndex >= 0 ? glyphTypeIndex : 0);

  // Populate array combo boxes from the associated model node's point data.
  vtkDataSet* mesh = d->ModelNode.GetPointer() ? d->ModelNode->GetMesh() : nullptr;
  vtkPointData* pointData = mesh ? mesh->GetPointData() : nullptr;

  QString currentOrientationArrayName =
    d->GlyphDisplayNode->GetOrientationArrayName() ? QString::fromUtf8(d->GlyphDisplayNode->GetOrientationArrayName()) : QString();
  QString currentScaleArrayName = d->GlyphDisplayNode->GetScaleArrayName() ? QString::fromUtf8(d->GlyphDisplayNode->GetScaleArrayName()) : QString();

  bool wasBlockedOrientation = d->OrientationArrayComboBox->blockSignals(true);
  bool wasBlockedScale = d->ScaleArrayComboBox->blockSignals(true);

  d->OrientationArrayComboBox->clear();
  d->OrientationArrayComboBox->addItem(qMRMLGlyphDisplayWidget::tr("None"), QString());
  d->ScaleArrayComboBox->clear();
  d->ScaleArrayComboBox->addItem(qMRMLGlyphDisplayWidget::tr("None"), QString());

  vtkDataArray* currentScaleArray = nullptr;
  if (pointData)
  {
    for (int arrayIndex = 0; arrayIndex < pointData->GetNumberOfArrays(); ++arrayIndex)
    {
      vtkDataArray* array = pointData->GetArray(arrayIndex);
      if (!array || !array->GetName())
      {
        continue;
      }
      QString arrayName = QString::fromUtf8(array->GetName());
      if (array->GetNumberOfComponents() == 3)
      {
        d->OrientationArrayComboBox->addItem(arrayName, arrayName);
      }
      d->ScaleArrayComboBox->addItem(arrayName, arrayName);
      if (arrayName == currentScaleArrayName)
      {
        currentScaleArray = array;
      }
    }
  }

  int orientationIndex = d->OrientationArrayComboBox->findData(currentOrientationArrayName);
  d->OrientationArrayComboBox->setCurrentIndex(orientationIndex >= 0 ? orientationIndex : 0);
  int scaleIndex = d->ScaleArrayComboBox->findData(currentScaleArrayName);
  d->ScaleArrayComboBox->setCurrentIndex(scaleIndex >= 0 ? scaleIndex : 0);

  d->OrientationArrayComboBox->blockSignals(wasBlockedOrientation);
  d->ScaleArrayComboBox->blockSignals(wasBlockedScale);

  int vectorScaleModeIndex = d->VectorScaleModeComboBox->findData(d->GlyphDisplayNode->GetVectorScaleMode());
  d->VectorScaleModeComboBox->setCurrentIndex(vectorScaleModeIndex >= 0 ? vectorScaleModeIndex : 0);
  bool scaleArrayIsVector = (currentScaleArray && currentScaleArray->GetNumberOfComponents() == 3);
  d->VectorScaleModeLabel->setEnabled(scaleArrayIsVector);
  d->VectorScaleModeComboBox->setEnabled(scaleArrayIsVector);

  d->ScaleFactorSpinBox->setValue(d->GlyphDisplayNode->GetScaleFactor());

  int maskingModeIndex = d->MaskingModeComboBox->findData(d->GlyphDisplayNode->GetMaskingMode());
  d->MaskingModeComboBox->setCurrentIndex(maskingModeIndex >= 0 ? maskingModeIndex : 0);
  d->MaskingNthPointSpinBox->setValue(d->GlyphDisplayNode->GetMaskingNthPoint());
  d->MaskingPointsNumberSpinBox->setValue(d->GlyphDisplayNode->GetMaskingPointsNumber());

  bool showNthPoint = (d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLGlyphDisplayNode::MaskingModeEveryNthPoint);
  d->MaskingNthPointLabel->setVisible(showNthPoint);
  d->MaskingNthPointSpinBox->setVisible(showNthPoint);

  bool showPointsNumber = (d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLGlyphDisplayNode::MaskingModeUniformBounds ||
                           d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLGlyphDisplayNode::MaskingModeUniformSurface ||
                           d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLGlyphDisplayNode::MaskingModeUniformVolume);
  d->MaskingPointsNumberLabel->setVisible(showPointsNumber);
  d->MaskingPointsNumberSpinBox->setVisible(showPointsNumber);

  d->IsUpdatingWidgetFromMRML = false;
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onGlyphTypeChanged(int index)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetGlyphType(d->GlyphTypeComboBox->itemData(index).toInt());
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onOrientationArrayChanged(int index)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  QString arrayName = d->OrientationArrayComboBox->itemData(index).toString();
  d->GlyphDisplayNode->SetOrientationArrayName(arrayName.isEmpty() ? nullptr : arrayName.toUtf8().constData());
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onScaleArrayChanged(int index)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  QString arrayName = d->ScaleArrayComboBox->itemData(index).toString();
  d->GlyphDisplayNode->SetScaleArrayName(arrayName.isEmpty() ? nullptr : arrayName.toUtf8().constData());
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onVectorScaleModeChanged(int index)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetVectorScaleMode(d->VectorScaleModeComboBox->itemData(index).toInt());
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onScaleFactorChanged(double value)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetScaleFactor(value);
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onMaskingModeChanged(int index)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetMaskingMode(d->MaskingModeComboBox->itemData(index).toInt());
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onMaskingNthPointChanged(int value)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetMaskingNthPoint(value);
}

//------------------------------------------------------------------------------
void qMRMLGlyphDisplayWidget::onMaskingPointsNumberChanged(int value)
{
  Q_D(qMRMLGlyphDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetMaskingPointsNumber(value);
}
