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

// Qt includes
#include <QColor>

// qMRML includes
#include "qMRMLVectorFieldDisplayWidget.h"
#include "qMRMLScalarsDisplayWidget.h"
#include "ui_qMRMLVectorFieldDisplayWidget.h"

// MRML includes
#include <vtkMRMLDisplayableNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformableNode.h>
#include <vtkMRMLVectorFieldDisplayNode.h>
#include <vtkMRMLVolumeNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkWeakPointer.h>

//------------------------------------------------------------------------------
class qMRMLVectorFieldDisplayWidgetPrivate
  : public QWidget
  , public Ui_qMRMLVectorFieldDisplayWidget
{
  Q_DECLARE_PUBLIC(qMRMLVectorFieldDisplayWidget);

protected:
  qMRMLVectorFieldDisplayWidget* const q_ptr;
  typedef QWidget Superclass;

public:
  qMRMLVectorFieldDisplayWidgetPrivate(qMRMLVectorFieldDisplayWidget& object);
  void init();

  bool IsUpdatingWidgetFromMRML{ false };

  vtkWeakPointer<vtkMRMLVectorFieldDisplayNode> GlyphDisplayNode;
  vtkWeakPointer<vtkMRMLDisplayableNode> DisplayableNode;
};

//------------------------------------------------------------------------------
qMRMLVectorFieldDisplayWidgetPrivate::qMRMLVectorFieldDisplayWidgetPrivate(qMRMLVectorFieldDisplayWidget& object)
  : q_ptr(&object)
{
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidgetPrivate::init()
{
  Q_Q(qMRMLVectorFieldDisplayWidget);
  this->setupUi(q);

  this->GlyphTypeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Arrow"), vtkMRMLVectorFieldDisplayNode::GlyphTypeArrow);
  this->GlyphTypeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Cone"), vtkMRMLVectorFieldDisplayNode::GlyphTypeCone);
  this->GlyphTypeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Box"), vtkMRMLVectorFieldDisplayNode::GlyphTypeBox);
  this->GlyphTypeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Cylinder"), vtkMRMLVectorFieldDisplayNode::GlyphTypeCylinder);
  this->GlyphTypeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Line"), vtkMRMLVectorFieldDisplayNode::GlyphTypeLine);
  this->GlyphTypeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Sphere"), vtkMRMLVectorFieldDisplayNode::GlyphTypeSphere);

  this->VectorScaleModeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Scale by magnitude"), vtkMRMLVectorFieldDisplayNode::VectorScaleModeByMagnitude);
  this->VectorScaleModeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Scale by components"), vtkMRMLVectorFieldDisplayNode::VectorScaleModeByComponents);

  this->MaskingModeComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("All points"), vtkMRMLVectorFieldDisplayNode::MaskingModeAllPoints);
  this->MaskingModeComboBox->addItem(              //
    qMRMLVectorFieldDisplayWidget::tr("Every Nth point"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeEveryNthPoint);
  this->MaskingModeComboBox->addItem(                                             //
    qMRMLVectorFieldDisplayWidget::tr("Uniform spatial distribution - bounds based"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds);
  this->MaskingModeComboBox->addItem(                                               //
    qMRMLVectorFieldDisplayWidget::tr("Uniform spatial distribution - surface sampling"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeUniformSurface);
  this->MaskingModeComboBox->addItem(                                              //
    qMRMLVectorFieldDisplayWidget::tr("Uniform spatial distribution - volume sampling"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeUniformVolume);

  q->connect(this->VisibilityCheckBox, SIGNAL(toggled(bool)), q, SLOT(onVisibilityToggled(bool)));
  q->connect(this->GlyphTypeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onGlyphTypeChanged(int)));
  q->connect(this->OrientationArrayComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onOrientationArrayChanged(int)));
  q->connect(this->ScaleArrayComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onScaleArrayChanged(int)));
  q->connect(this->SwapOrientationArrayCoordinateSystemButton, SIGNAL(clicked()), q, SLOT(swapOrientationArrayCoordinateSystem()));
  q->connect(this->VectorScaleModeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onVectorScaleModeChanged(int)));
  q->connect(this->ScaleFactorSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onScaleFactorChanged(double)));
  q->connect(this->MaskingModeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onMaskingModeChanged(int)));
  q->connect(this->MaskingNthPointSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onMaskingNthPointChanged(int)));
  q->connect(this->MaskingPointsNumberSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onMaskingPointsNumberChanged(int)));

  // 3D display
  q->connect(this->Visibility3DCheckBox, SIGNAL(toggled(bool)), q, SLOT(onVisibility3DToggled(bool)));
  q->connect(this->ColorPickerButton, SIGNAL(colorChanged(QColor)), q, SLOT(onColorChanged(QColor)));
  q->connect(this->OpacitySliderWidget, SIGNAL(valueChanged(double)), q, SLOT(onOpacityChanged(double)));

  // Slice display
  q->connect(this->Visibility2DCheckBox, SIGNAL(toggled(bool)), q, SLOT(onVisibility2DToggled(bool)));
  q->connect(this->SliceSlabThicknessSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onSliceSlabThicknessChanged(double)));
  q->connect(this->SliceGlyphScaleSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onSliceGlyphScaleChanged(double)));
  q->connect(this->SliceLineWidthSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onSliceLineWidthChanged(int)));

  q->setEnabled(false);
}

//------------------------------------------------------------------------------
qMRMLVectorFieldDisplayWidget::qMRMLVectorFieldDisplayWidget(QWidget* _parent)
  : qMRMLWidget(_parent)
  , d_ptr(new qMRMLVectorFieldDisplayWidgetPrivate(*this))
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  d->init();
}

//------------------------------------------------------------------------------
qMRMLVectorFieldDisplayWidget::~qMRMLVectorFieldDisplayWidget() = default;

//------------------------------------------------------------------------------
vtkMRMLVectorFieldDisplayNode* qMRMLVectorFieldDisplayWidget::mrmlVectorFieldDisplayNode() const
{
  Q_D(const qMRMLVectorFieldDisplayWidget);
  return d->GlyphDisplayNode;
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::setMRMLVectorFieldDisplayNode(vtkMRMLNode* node)
{
  this->setMRMLVectorFieldDisplayNode(vtkMRMLVectorFieldDisplayNode::SafeDownCast(node));
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::setMRMLVectorFieldDisplayNode(vtkMRMLVectorFieldDisplayNode* glyphDisplayNode)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (d->GlyphDisplayNode == glyphDisplayNode)
  {
    return;
  }

  qvtkReconnect(d->GlyphDisplayNode, glyphDisplayNode, vtkCommand::ModifiedEvent, this, SLOT(updateWidgetFromMRML()));
  d->GlyphDisplayNode = glyphDisplayNode;

  // Also observe data changes on the displayable node that stores the field, so that the
  // array combo boxes are refreshed when the available arrays change. The events are
  // source specific (a mesh, an image, a transform), so all of them are observed.
  vtkMRMLDisplayableNode* displayableNode = glyphDisplayNode ? glyphDisplayNode->GetDisplayableNode() : nullptr;
  qvtkReconnect(d->DisplayableNode, displayableNode, vtkMRMLModelNode::MeshModifiedEvent, this, SLOT(updateWidgetFromMRML()));
  qvtkReconnect(d->DisplayableNode, displayableNode, vtkMRMLVolumeNode::ImageDataModifiedEvent, this, SLOT(updateWidgetFromMRML()));
  qvtkReconnect(d->DisplayableNode, displayableNode, vtkMRMLTransformableNode::TransformModifiedEvent, this, SLOT(updateWidgetFromMRML()));
  d->DisplayableNode = displayableNode;

  // The scalars widget edits the coloring properties (active scalar array, color
  // table, displayed range, threshold) of the same glyph display node.
  d->ScalarsDisplayWidget->setMRMLDisplayNode(glyphDisplayNode);

  this->updateWidgetFromMRML();
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::updateWidgetFromMRML()
{
  Q_D(qMRMLVectorFieldDisplayWidget);
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

  d->VisibilityCheckBox->setChecked(d->GlyphDisplayNode->GetVisibility() != 0);

  int glyphTypeIndex = d->GlyphTypeComboBox->findData(d->GlyphDisplayNode->GetGlyphType());
  d->GlyphTypeComboBox->setCurrentIndex(glyphTypeIndex >= 0 ? glyphTypeIndex : 0);

  // Populate array combo boxes with the arrays that the field source provides. This must
  // not update the pipeline, so the names are asked from the display node instead of read
  // from the sampled point set.
  std::vector<std::string> arrayNames;
  std::vector<int> arrayNumberOfComponents;
  d->GlyphDisplayNode->GetFieldArrayInfo(arrayNames, arrayNumberOfComponents);

  QString currentOrientationArrayName =
    d->GlyphDisplayNode->GetOrientationArrayName() ? QString::fromUtf8(d->GlyphDisplayNode->GetOrientationArrayName()) : QString();
  QString currentScaleArrayName = d->GlyphDisplayNode->GetScaleArrayName() ? QString::fromUtf8(d->GlyphDisplayNode->GetScaleArrayName()) : QString();

  bool wasBlockedOrientation = d->OrientationArrayComboBox->blockSignals(true);
  bool wasBlockedScale = d->ScaleArrayComboBox->blockSignals(true);

  d->OrientationArrayComboBox->clear();
  d->OrientationArrayComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("None"), QString());
  d->ScaleArrayComboBox->clear();
  d->ScaleArrayComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("None"), QString());

  int currentScaleArrayComponents = 0;
  bool currentOrientationArrayFound = false;
  for (size_t arrayIndex = 0; arrayIndex < arrayNames.size(); ++arrayIndex)
  {
    QString arrayName = QString::fromStdString(arrayNames[arrayIndex]);
    int numberOfComponents = arrayNumberOfComponents[arrayIndex];
    if (numberOfComponents == 3)
    {
      d->OrientationArrayComboBox->addItem(arrayName, arrayName);
    }
    d->ScaleArrayComboBox->addItem(arrayName, arrayName);
    if (arrayName == currentScaleArrayName)
    {
      currentScaleArrayComponents = numberOfComponents;
    }
    if (arrayName == currentOrientationArrayName)
    {
      currentOrientationArrayFound = true;
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
  bool scaleArrayIsVector = (currentScaleArrayComponents == 3);
  d->VectorScaleModeLabel->setEnabled(scaleArrayIsVector);
  d->VectorScaleModeComboBox->setEnabled(scaleArrayIsVector);

  // Converting between RAS and LPS is only meaningful for the 3-component arrays that can
  // orient a glyph, and only for sources whose arrays can be edited in place (a mesh);
  // arrays that a sampler computes are overwritten on the next update.
  d->SwapOrientationArrayCoordinateSystemButton->setEnabled(currentOrientationArrayFound //
                                                            && vtkMRMLModelNode::SafeDownCast(d->DisplayableNode) != nullptr);

  d->ScaleFactorSpinBox->setValue(d->GlyphDisplayNode->GetScaleFactor());

  int maskingModeIndex = d->MaskingModeComboBox->findData(d->GlyphDisplayNode->GetMaskingMode());
  d->MaskingModeComboBox->setCurrentIndex(maskingModeIndex >= 0 ? maskingModeIndex : 0);
  d->MaskingNthPointSpinBox->setValue(d->GlyphDisplayNode->GetMaskingNthPoint());
  d->MaskingPointsNumberSpinBox->setValue(d->GlyphDisplayNode->GetMaskingPointsNumber());

  bool showNthPoint = (d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLVectorFieldDisplayNode::MaskingModeEveryNthPoint);
  d->MaskingNthPointLabel->setVisible(showNthPoint);
  d->MaskingNthPointSpinBox->setVisible(showNthPoint);

  bool showPointsNumber = (d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds ||
                           d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLVectorFieldDisplayNode::MaskingModeUniformSurface ||
                           d->GlyphDisplayNode->GetMaskingMode() == vtkMRMLVectorFieldDisplayNode::MaskingModeUniformVolume);
  d->MaskingPointsNumberLabel->setVisible(showPointsNumber);
  d->MaskingPointsNumberSpinBox->setVisible(showPointsNumber);

  // 3D display
  d->Visibility3DCheckBox->setChecked(d->GlyphDisplayNode->GetVisibility3D() != 0);
  double* color = d->GlyphDisplayNode->GetColor();
  if (color)
  {
    d->ColorPickerButton->setColor(QColor::fromRgbF(color[0], color[1], color[2]));
  }
  d->OpacitySliderWidget->setValue(d->GlyphDisplayNode->GetOpacity());

  // Slice display
  d->Visibility2DCheckBox->setChecked(d->GlyphDisplayNode->GetVisibility2D() != 0);
  d->SliceSlabThicknessSpinBox->setValue(d->GlyphDisplayNode->GetSliceSlabThicknessMm());
  d->SliceGlyphScaleSpinBox->setValue(d->GlyphDisplayNode->GetSliceGlyphScalePercent());
  d->SliceLineWidthSpinBox->setValue(d->GlyphDisplayNode->GetSliceIntersectionThickness());

  d->IsUpdatingWidgetFromMRML = false;
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onVisibilityToggled(bool visible)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetVisibility(visible);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onGlyphTypeChanged(int index)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetGlyphType(d->GlyphTypeComboBox->itemData(index).toInt());
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onOrientationArrayChanged(int index)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  QString arrayName = d->OrientationArrayComboBox->itemData(index).toString();
  d->GlyphDisplayNode->SetOrientationArrayName(arrayName.isEmpty() ? nullptr : arrayName.toUtf8().constData());
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onScaleArrayChanged(int index)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  QString arrayName = d->ScaleArrayComboBox->itemData(index).toString();
  d->GlyphDisplayNode->SetScaleArrayName(arrayName.isEmpty() ? nullptr : arrayName.toUtf8().constData());
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onVectorScaleModeChanged(int index)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetVectorScaleMode(d->VectorScaleModeComboBox->itemData(index).toInt());
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onScaleFactorChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetScaleFactor(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onMaskingModeChanged(int index)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetMaskingMode(d->MaskingModeComboBox->itemData(index).toInt());
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onMaskingNthPointChanged(int value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetMaskingNthPoint(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onMaskingPointsNumberChanged(int value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetMaskingPointsNumber(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::swapOrientationArrayCoordinateSystem()
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  vtkDataArray* orientationArray = d->GlyphDisplayNode->GetOrientationArray();
  if (!orientationArray || orientationArray->GetNumberOfComponents() != 3)
  {
    return;
  }

  // RAS and LPS differ in the sign of the first two axes
  vtkIdType numberOfTuples = orientationArray->GetNumberOfTuples();
  for (vtkIdType tupleIndex = 0; tupleIndex < numberOfTuples; ++tupleIndex)
  {
    double vector[3] = { 0.0, 0.0, 0.0 };
    orientationArray->GetTuple(tupleIndex, vector);
    vector[0] = -vector[0];
    vector[1] = -vector[1];
    orientationArray->SetTuple(tupleIndex, vector);
  }
  orientationArray->Modified();

  // The array belongs to the model node's mesh: notify its observers (display
  // pipelines) that the mesh data changed.
  vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(d->DisplayableNode);
  vtkDataSet* mesh = modelNode ? modelNode->GetMesh() : nullptr;
  if (mesh)
  {
    mesh->GetPointData()->Modified();
    mesh->Modified();
  }
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onVisibility3DToggled(bool visible)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetVisibility3D(visible);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onColorChanged(const QColor& color)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetColor(color.redF(), color.greenF(), color.blueF());
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onOpacityChanged(double opacity)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetOpacity(opacity);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onVisibility2DToggled(bool visible)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetVisibility2D(visible);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onSliceSlabThicknessChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetSliceSlabThicknessMm(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onSliceGlyphScaleChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetSliceGlyphScalePercent(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onSliceLineWidthChanged(int value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetSliceIntersectionThickness(value);
}
