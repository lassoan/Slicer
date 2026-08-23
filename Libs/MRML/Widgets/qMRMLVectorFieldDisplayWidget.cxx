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
#include <QToolButton>

// STD includes
#include <algorithm>
#include <map>

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

  /// The button that selects each visualization mode, keyed by the mode.
  std::map<int, QToolButton*> ModeToolButtons;

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
  this->MaskingModeComboBox->addItem(                                             //
    qMRMLVectorFieldDisplayWidget::tr("Uniform spatial distribution - bounds based"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds);
  this->MaskingModeComboBox->addItem(                                               //
    qMRMLVectorFieldDisplayWidget::tr("Uniform spatial distribution - surface sampling"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeUniformSurface);
  this->MaskingModeComboBox->addItem(                                              //
    qMRMLVectorFieldDisplayWidget::tr("Uniform spatial distribution - volume sampling"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeUniformVolume);
  this->MaskingModeComboBox->addItem(                  //
    qMRMLVectorFieldDisplayWidget::tr("Fixed spacing"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeFixedSpacing);
  this->MaskingModeComboBox->addItem(                //
    qMRMLVectorFieldDisplayWidget::tr("Node points"), //
    vtkMRMLVectorFieldDisplayNode::MaskingModeNodePoints);

  this->ColorByComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Solid color"), 0);
  this->ColorByComboBox->addItem(qMRMLVectorFieldDisplayWidget::tr("Magnitude"), 1);

  // One checkable icon button per visualization mode, as the transform display had
  this->ModeToolButtons[vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph] = this->GlyphModeToolButton;
  this->ModeToolButtons[vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid] = this->GridModeToolButton;
  this->ModeToolButtons[vtkMRMLVectorFieldDisplayNode::VisualizationModeContour] = this->ContourModeToolButton;
  this->ModeToolButtons[vtkMRMLVectorFieldDisplayNode::VisualizationModeStreamline] = this->StreamlineModeToolButton;

  q->connect(this->VisibilityCheckBox, SIGNAL(toggled(bool)), q, SLOT(onVisibilityToggled(bool)));
  q->connect(this->GlyphTypeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onGlyphTypeChanged(int)));
  q->connect(this->OrientationArrayComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onOrientationArrayChanged(int)));
  q->connect(this->ScaleArrayComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onScaleArrayChanged(int)));
  q->connect(this->SwapOrientationArrayCoordinateSystemButton, SIGNAL(clicked()), q, SLOT(swapOrientationArrayCoordinateSystem()));
  q->connect(this->VectorScaleModeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onVectorScaleModeChanged(int)));
  q->connect(this->ScaleFactorSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onScaleFactorChanged(double)));
  q->connect(this->MaskingModeComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onMaskingModeChanged(int)));
  q->connect(this->ColorByComboBox, SIGNAL(currentIndexChanged(int)), q, SLOT(onColorByChanged(int)));
  q->connect(this->GlyphSpacingSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onGlyphSpacingChanged(double)));
  q->connect(this->FieldThresholdCheckBox, SIGNAL(toggled(bool)), q, SLOT(onThresholdEnabledToggled(bool)));
  q->connect(this->FieldThresholdRangeWidget, SIGNAL(valuesChanged(double, double)), q, SLOT(onThresholdRangeChanged(double, double)));
  // Thresholding is offered here, with the other parameters that apply whatever the field
  // is colored by, so the coloring widget does not repeat it.
  this->ScalarsDisplayWidget->setThresholdVisible(false);
  q->connect(this->MaskingPointsNumberSpinBox, SIGNAL(valueChanged(int)), q, SLOT(onMaskingPointsNumberChanged(int)));
  for (auto modeToButton : this->ModeToolButtons)
  {
    q->connect(modeToButton.second, SIGNAL(toggled(bool)), q, SLOT(onVisualizationModeToggled(bool)));
  }
  q->connect(this->SamplingSpacingSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onSamplingSpacingChanged(double)));
  q->connect(this->RegionNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)), q, SLOT(onRegionNodeChanged(vtkMRMLNode*)));
  q->connect(this->SamplePointsNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)), q, SLOT(onSamplePointsNodeChanged(vtkMRMLNode*)));
  q->connect(this->GridSpacingSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onGridSpacingChanged(double)));
  q->connect(this->GridShowNonWarpedCheckBox, SIGNAL(toggled(bool)), q, SLOT(onGridShowNonWarpedToggled(bool)));
  q->connect(this->GridLineDiameterSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onGridLineDiameterChanged(double)));
  q->connect(this->ContourLevelsLineEdit, SIGNAL(editingFinished()), q, SLOT(onContourLevelsChanged()));
  q->connect(this->MaximumPropagationSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onMaximumPropagationChanged(double)));
  q->connect(this->StreamlineTubeDiameterSpinBox, SIGNAL(valueChanged(double)), q, SLOT(onStreamlineTubeDiameterChanged(double)));

  // 3D display
  q->connect(this->Visibility3DCheckBox, SIGNAL(toggled(bool)), q, SLOT(onVisibility3DToggled(bool)));
  q->connect(this->ColorPickerButton, SIGNAL(colorChanged(QColor)), q, SLOT(onColorChanged(QColor)));
  q->connect(this->OpacitySliderWidget, SIGNAL(valueChanged(double)), q, SLOT(onOpacityChanged(double)));

  // Slice display
  q->connect(this->Visibility2DCheckBox, SIGNAL(toggled(bool)), q, SLOT(onVisibility2DToggled(bool)));
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

  d->ScaleFactorSpinBox->setValue(d->GlyphDisplayNode->GetEffectiveScaleFactor());

  // Only the ways of placing glyphs that this source can honor are offered
  for (int itemIndex = 0; itemIndex < d->MaskingModeComboBox->count(); ++itemIndex)
  {
    bool supported = d->GlyphDisplayNode->IsMaskingModeSupported(d->MaskingModeComboBox->itemData(itemIndex).toInt());
    d->MaskingModeComboBox->setItemData(itemIndex, supported ? QVariant() : QVariant(0), Qt::UserRole - 1);
  }
  int effectiveMaskingMode = d->GlyphDisplayNode->GetEffectiveMaskingMode();
  int maskingModeIndex = d->MaskingModeComboBox->findData(effectiveMaskingMode);
  d->MaskingModeComboBox->setCurrentIndex(maskingModeIndex >= 0 ? maskingModeIndex : 0);
  d->MaskingPointsNumberSpinBox->setValue(d->GlyphDisplayNode->GetMaskingPointsNumber());

  bool showPointsNumber = (effectiveMaskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeUniformBounds ||
                           effectiveMaskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeUniformSurface ||
                           effectiveMaskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeUniformVolume);
  d->MaskingPointsNumberLabel->setVisible(showPointsNumber);
  d->MaskingPointsNumberSpinBox->setVisible(showPointsNumber);

  // Visualization mode, and the parameters that only apply to one of the modes
  int visualizationMode = d->GlyphDisplayNode->GetVisualizationMode();
  d->VisibilityCheckBox->setChecked(d->GlyphDisplayNode->GetVisibility() != 0);
  for (auto modeToButton : d->ModeToolButtons)
  {
    modeToButton.second->setVisible(d->GlyphDisplayNode->IsVisualizationModeSupported(modeToButton.first));
    bool wasBlocked = modeToButton.second->blockSignals(true);
    modeToButton.second->setChecked(modeToButton.first == visualizationMode);
    modeToButton.second->blockSignals(wasBlocked);
  }

  d->GridSpacingSpinBox->setValue(d->GlyphDisplayNode->GetGridSpacingMm());
  d->GridShowNonWarpedCheckBox->setChecked(d->GlyphDisplayNode->GetGridShowNonWarped());
  d->GridLineDiameterSpinBox->setValue(d->GlyphDisplayNode->GetGridLineDiameterMm());
  d->ContourLevelsLineEdit->setText(QString::fromStdString(d->GlyphDisplayNode->GetContourLevelsMmAsString()));
  d->MaximumPropagationSpinBox->setValue(d->GlyphDisplayNode->GetMaximumPropagationMm());
  d->StreamlineTubeDiameterSpinBox->setValue(d->GlyphDisplayNode->GetStreamlineTubeDiameterMm());

  bool glyphMode = (visualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGlyph);
  bool gridMode = (visualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeGrid);
  bool contourMode = (visualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeContour);
  bool streamlineMode = (visualizationMode == vtkMRMLVectorFieldDisplayNode::VisualizationModeStreamline);

  for (QWidget* glyphWidget : { static_cast<QWidget*>(d->GlyphTypeLabel),
                                static_cast<QWidget*>(d->GlyphTypeComboBox),
                                static_cast<QWidget*>(d->ScaleArrayLabel),
                                static_cast<QWidget*>(d->ScaleArrayComboBox),
                                static_cast<QWidget*>(d->MaskingModeLabel),
                                static_cast<QWidget*>(d->MaskingModeComboBox) })
  {
    glyphWidget->setVisible(glyphMode);
  }
  if (!glyphMode)
  {
    d->MaskingPointsNumberLabel->setVisible(false);
    d->MaskingPointsNumberSpinBox->setVisible(false);
    d->VectorScaleModeLabel->setVisible(false);
    d->VectorScaleModeComboBox->setVisible(false);
  }
  else
  {
    d->VectorScaleModeLabel->setVisible(true);
    d->VectorScaleModeComboBox->setVisible(true);
  }
  // The scale factor means something in every mode that scales the field, and it is named
  // the same in all of them.
  bool scaleFactorUsed = d->GlyphDisplayNode->IsScaleFactorUsed();
  d->ScaleFactorLabel->setVisible(scaleFactorUsed);
  d->ScaleFactorSpinBox->setVisible(scaleFactorUsed);

  d->GridSpacingLabel->setVisible(gridMode);
  d->GridSpacingSpinBox->setVisible(gridMode);
  d->GridShowNonWarpedCheckBox->setVisible(gridMode);
  d->GridLineDiameterLabel->setVisible(gridMode);
  d->GridLineDiameterSpinBox->setVisible(gridMode);
  d->ContourLevelsLabel->setVisible(contourMode);
  d->ContourLevelsLineEdit->setVisible(contourMode);
  d->MaximumPropagationLabel->setVisible(streamlineMode);
  d->MaximumPropagationSpinBox->setVisible(streamlineMode);
  d->StreamlineTubeDiameterLabel->setVisible(streamlineMode);
  d->StreamlineTubeDiameterSpinBox->setVisible(streamlineMode);

  // Sources whose arrays are fixed (a sampled field always gives one vector array and its
  // magnitude) leave the user nothing to pick, so the array selectors are hidden.
  bool hasFixedArrays = d->GlyphDisplayNode->HasFixedFieldArrays();
  for (QWidget* arrayWidget : { static_cast<QWidget*>(d->OrientationArrayLabel),
                                static_cast<QWidget*>(d->OrientationArrayComboBox),
                                static_cast<QWidget*>(d->SwapOrientationArrayCoordinateSystemButton),
                                static_cast<QWidget*>(d->ScaleArrayLabel),
                                static_cast<QWidget*>(d->ScaleArrayComboBox) })
  {
    arrayWidget->setVisible(!hasFixedArrays && glyphMode);
  }
  if (hasFixedArrays)
  {
    d->VectorScaleModeLabel->setVisible(false);
    d->VectorScaleModeComboBox->setVisible(false);
  }

  // Sampling: only sources that can be sampled at arbitrary positions (a vector volume, a
  // transform) use these; the point data of a model is taken at the points of the mesh.
  bool isSampledSource = d->GlyphDisplayNode->CanSampleAtArbitraryPositions();
  // The lattice spacing is what a non-glyph mode is sampled on, and in glyph mode it only
  // applies when the glyphs sit on a lattice.
  bool usesLattice = (isSampledSource //
                      && (!glyphMode  //
                          || effectiveMaskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeFixedSpacing));
  bool usesNodePoints = (effectiveMaskingMode == vtkMRMLVectorFieldDisplayNode::MaskingModeNodePoints);
  bool showGlyphSpacing = (glyphMode && usesLattice);
  d->GlyphSpacingLabel->setVisible(showGlyphSpacing);
  d->GlyphSpacingSpinBox->setVisible(showGlyphSpacing);
  // The same spacing is offered among the advanced parameters for the modes that sample the
  // field without drawing glyphs, where it reads as a resolution rather than a spacing.
  d->SamplingSpacingLabel->setVisible(usesLattice && !showGlyphSpacing);
  d->SamplingSpacingSpinBox->setVisible(usesLattice && !showGlyphSpacing);
  d->RegionNodeComboBox->setVisible(isSampledSource);
  d->SamplePointsLabel->setVisible(usesNodePoints);
  d->SamplePointsNodeComboBox->setVisible(usesNodePoints);
  if (isSampledSource)
  {
    d->SamplingSpacingSpinBox->setValue(d->GlyphDisplayNode->GetEffectiveSamplingSpacingMm());
    d->GlyphSpacingSpinBox->setValue(d->GlyphDisplayNode->GetEffectiveSamplingSpacingMm());
    d->RegionNodeComboBox->setCurrentNode(d->GlyphDisplayNode->GetRegionNode());
  }
  d->SamplePointsNodeComboBox->setCurrentNode(d->GlyphDisplayNode->GetSamplePointsNode());

  // Coloring: either one color, or the values of the active array through the color node
  bool colorByScalar = (d->GlyphDisplayNode->GetScalarVisibility() != 0);
  d->ColorByComboBox->setCurrentIndex(colorByScalar ? 1 : 0);
  d->ColorLabel->setVisible(!colorByScalar);
  d->ColorPickerButton->setVisible(!colorByScalar);
  d->ScalarsDisplayWidget->setVisible(colorByScalar);
  d->ScalarsDisplayWidget->setThresholdVisible(false);
  // The scalars widget has a visibility check box of its own, which this selector replaces,
  // and an array selector that means nothing for a source whose arrays are fixed.
  for (const char* hiddenChild : { "ScalarsVisibilityLabel", "ScalarsVisibilityCheckBox" })
  {
    QWidget* child = d->ScalarsDisplayWidget->findChild<QWidget*>(hiddenChild);
    if (child)
    {
      child->setVisible(false);
    }
  }
  for (const char* arrayChild : { "ActiveScalarLabel", "ActiveScalarComboBox" })
  {
    QWidget* child = d->ScalarsDisplayWidget->findChild<QWidget*>(arrayChild);
    if (child)
    {
      child->setVisible(!hasFixedArrays);
    }
  }

  // Thresholding, which hides the parts of the field whose magnitude is outside the range
  bool thresholdUsed = d->GlyphDisplayNode->IsThresholdUsed();
  d->FieldThresholdLabel->setVisible(thresholdUsed);
  d->FieldThresholdCheckBox->setVisible(thresholdUsed);
  d->FieldThresholdRangeWidget->setVisible(thresholdUsed);
  bool thresholdEnabled = (d->GlyphDisplayNode->GetThresholdEnabled() != 0);
  d->FieldThresholdCheckBox->setChecked(thresholdEnabled);
  d->FieldThresholdRangeWidget->setEnabled(thresholdEnabled);
  double scalarRange[2] = { 0.0, 0.0 };
  d->GlyphDisplayNode->GetScalarRange(scalarRange);
  double thresholdRange[2] = { 0.0, 0.0 };
  d->GlyphDisplayNode->GetThresholdRange(thresholdRange);
  // The slider covers the values that are there to choose from and the ones that are chosen,
  // so that a threshold set outside the scalar range is shown as it is instead of being
  // clamped to the ends of the slider.
  if (thresholdRange[1] >= thresholdRange[0])
  {
    double sliderRange[2] = { std::min(scalarRange[0], thresholdRange[0]), std::max(scalarRange[1], thresholdRange[1]) };
    if (sliderRange[1] > sliderRange[0])
    {
      d->FieldThresholdRangeWidget->setRange(sliderRange[0], sliderRange[1]);
    }
    d->FieldThresholdRangeWidget->setValues(thresholdRange[0], thresholdRange[1]);
  }
  else if (scalarRange[1] > scalarRange[0])
  {
    d->FieldThresholdRangeWidget->setRange(scalarRange[0], scalarRange[1]);
  }

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
  d->SliceLineWidthSpinBox->setValue(d->GlyphDisplayNode->GetSliceIntersectionThickness());

  d->IsUpdatingWidgetFromMRML = false;
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
  d->GlyphDisplayNode->SetEffectiveScaleFactor(value);
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
void qMRMLVectorFieldDisplayWidget::onVisualizationModeToggled(bool checked)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!checked || !d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  // The buttons are auto-exclusive, so exactly one of them is checked
  for (auto modeToButton : d->ModeToolButtons)
  {
    if (modeToButton.second == this->sender())
    {
      d->GlyphDisplayNode->SetVisualizationMode(modeToButton.first);
      return;
    }
  }
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onGlyphSpacingChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetEffectiveSamplingSpacingMm(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onThresholdEnabledToggled(bool enabled)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetThresholdEnabled(enabled);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onThresholdRangeChanged(double minimum, double maximum)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetThresholdRange(minimum, maximum);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onColorByChanged(int index)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetScalarVisibility(d->ColorByComboBox->itemData(index).toInt() != 0);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::addColoringWidget(QWidget* widget)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!widget)
  {
    return;
  }
  d->coloringLayout->addWidget(widget);
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
void qMRMLVectorFieldDisplayWidget::onSamplingSpacingChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetEffectiveSamplingSpacingMm(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onRegionNodeChanged(vtkMRMLNode* node)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetAndObserveRegionNode(node);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onSamplePointsNodeChanged(vtkMRMLNode* node)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetAndObserveSamplePointsNode(node);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onGridSpacingChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetGridSpacingMm(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onGridShowNonWarpedToggled(bool enabled)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetGridShowNonWarped(enabled);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onGridLineDiameterChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetGridLineDiameterMm(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onContourLevelsChanged()
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetContourLevelsMmFromString(d->ContourLevelsLineEdit->text().toUtf8().constData());
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onMaximumPropagationChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetMaximumPropagationMm(value);
}

//------------------------------------------------------------------------------
void qMRMLVectorFieldDisplayWidget::onStreamlineTubeDiameterChanged(double value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetStreamlineTubeDiameterMm(value);
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
void qMRMLVectorFieldDisplayWidget::onSliceLineWidthChanged(int value)
{
  Q_D(qMRMLVectorFieldDisplayWidget);
  if (!d->GlyphDisplayNode.GetPointer() || d->IsUpdatingWidgetFromMRML)
  {
    return;
  }
  d->GlyphDisplayNode->SetSliceIntersectionThickness(value);
}
