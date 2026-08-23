/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Andras Lasso and Franklin King at
  PerkLab, Queen's University and was supported through the Applied Cancer
  Research Unit program of Cancer Care Ontario with funds provided by the
  Ontario Ministry of Health and Long-Term Care.

==============================================================================*/

// qMRML includes
#include "qMRMLTransformDisplayNodeWidget.h"
#include "ui_qMRMLTransformDisplayNodeWidget.h"

// Qt includes
#include <QRegularExpression>
#include <QRegularExpressionValidator>

// MRML includes
#include <vtkMRMLColorNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLTransformDisplayNode.h>
#include <vtkMRMLProceduralColorNode.h>

// VTK includes
#include "vtkColorTransferFunction.h"

// CTK includes
#include "ctkVTKScalarsToColorsView.h"

// Qt includes
#include <QDebug>

//-----------------------------------------------------------------------------
class qMRMLTransformDisplayNodeWidgetPrivate : public Ui_qMRMLTransformDisplayNodeWidget
{
  Q_DECLARE_PUBLIC(qMRMLTransformDisplayNodeWidget);

protected:
  qMRMLTransformDisplayNodeWidget* const q_ptr;

public:
  qMRMLTransformDisplayNodeWidgetPrivate(qMRMLTransformDisplayNodeWidget& object);
  ~qMRMLTransformDisplayNodeWidgetPrivate();
  void init();

  vtkMRMLTransformDisplayNode* TransformDisplayNode;
  vtkColorTransferFunction* ColorTransferFunction;
};

//-----------------------------------------------------------------------------
// qMRMLTransformDisplayNodeWidgetPrivate methods

//-----------------------------------------------------------------------------
qMRMLTransformDisplayNodeWidgetPrivate::qMRMLTransformDisplayNodeWidgetPrivate(qMRMLTransformDisplayNodeWidget& object)
  : q_ptr(&object)
{
  this->TransformDisplayNode = nullptr;
  this->ColorTransferFunction = vtkColorTransferFunction::New();
}

//-----------------------------------------------------------------------------
qMRMLTransformDisplayNodeWidgetPrivate::~qMRMLTransformDisplayNodeWidgetPrivate()
{
  this->ColorTransferFunction->Delete();
  this->ColorTransferFunction = nullptr;
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidgetPrivate::init()
{
  Q_Q(qMRMLTransformDisplayNodeWidget);
  this->setupUi(q);

  // The color map of a transform is a coloring option, so it belongs in the one coloring
  // section of the shared widget rather than in a second section next to it
  this->VectorFieldDisplayWidget->addColoringWidget(this->ColorMapWidget);

  double validBounds[4] = { VTK_DOUBLE_MIN, VTK_DOUBLE_MAX, 0., 1. };

  this->ColorMapWidget->view()->setValidBounds(validBounds);
  this->ColorMapWidget->view()->addColorTransferFunction(nullptr);
  this->ColorMapWidget->view()->setColorTransferFunctionToPlots(this->ColorTransferFunction);

  double chartBounds[8] = { 0 };
  this->ColorMapWidget->view()->chartBounds(chartBounds);
  chartBounds[2] = 0;
  chartBounds[3] = 100;
  this->ColorMapWidget->view()->setChartUserBounds(chartBounds);
  chartBounds[2] = 1;
  chartBounds[3] = 50;
  this->ColorMapWidget->view()->setPlotsUserBounds(chartBounds);
  this->ColorMapWidget->view()->update();

  q->qvtkConnect(this->ColorTransferFunction, vtkCommand::EndInteractionEvent, q, SLOT(onColorInteractionEvent()), 0., Qt::QueuedConnection);
  q->qvtkConnect(this->ColorTransferFunction, vtkCommand::EndEvent, q, SLOT(onColorInteractionEvent()), 0., Qt::QueuedConnection);
  q->qvtkConnect(this->ColorTransferFunction, vtkCommand::ModifiedEvent, q, SLOT(onColorModifiedEvent()), 0., Qt::QueuedConnection);

  // Interaction panel
  QObject::connect(this->InteractionVisibleCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorVisibility(bool)));
  QObject::connect(this->InteractionVisible3dCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorVisibility3d(bool)));
  QObject::connect(this->InteractionVisible2dCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorVisibility2d(bool)));

  QObject::connect(this->InteractiveTranslation3DCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorTranslationEnabled(bool)));
  QObject::connect(this->InteractiveRotation3DCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorRotationEnabled(bool)));
  QObject::connect(this->InteractiveScaling3DCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorScalingEnabled(bool)));

  QObject::connect(this->InteractiveTranslationSliceCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorTranslationSliceEnabled(bool)));
  QObject::connect(this->EditorTranslationSliceAnywhereEnabledCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorTranslationSliceAnywhereEnabled(bool)));
  QObject::connect(this->editorTranslationSliceAnywhereSensitivitySlider, SIGNAL(valueChanged(double)), q, SLOT(setEditorTranslationSliceAnywhereSensitivity(double)));
  QObject::connect(this->InteractiveRotationSliceCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorRotationSliceEnabled(bool)));
  QObject::connect(this->InteractiveScalingSliceCheckBox, SIGNAL(toggled(bool)), q, SLOT(setEditorScalingSliceEnabled(bool)));

  QObject::connect(this->translateX3DCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));
  QObject::connect(this->translateY3DCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));
  QObject::connect(this->translateZ3DCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));
  QObject::connect(this->translateViewPlane3DCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));

  QObject::connect(this->rotateX3DCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));
  QObject::connect(this->rotateY3DCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));
  QObject::connect(this->rotateZ3DCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));
  QObject::connect(this->rotateViewPlane3DCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));

  QObject::connect(this->scaleX3DCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));
  QObject::connect(this->scaleY3DCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));
  QObject::connect(this->scaleZ3DCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));
  QObject::connect(this->scaleViewPlane3DCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));

  QObject::connect(this->translateXSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));
  QObject::connect(this->translateYSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));
  QObject::connect(this->translateZSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));
  QObject::connect(this->translateViewPlaneSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateTranslationComponentVisibility()));

  QObject::connect(this->rotateXSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));
  QObject::connect(this->rotateYSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));
  QObject::connect(this->rotateZSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));
  QObject::connect(this->rotateViewPlaneSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateRotationComponentVisibility()));

  QObject::connect(this->scaleXSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));
  QObject::connect(this->scaleYSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));
  QObject::connect(this->scaleZSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));
  QObject::connect(this->scaleViewPlaneSliceCheckBox, SIGNAL(clicked()), q, SLOT(updateScalingComponentVisibility()));

  QObject::connect(this->interactionHandleScaleSlider, SIGNAL(valueChanged(double)), q, SLOT(updateInteractionHandleScale()));
  QObject::connect(this->interactionHandleOpacitySlider, SIGNAL(valueChanged(double)), q, SLOT(updateInteractionHandleOpacity()));

  this->InteractiveAdvancedOptions3DFrame->hide();
  this->InteractiveAdvancedOptionsSliceFrame->hide();

  // Visualization panel




  // Common Parameters

  // Glyph Parameters
  // 3D Glyph Parameters

  // 2D Glyph Parameters

  // Grid Parameters

  // Contour Parameters
  QRegularExpression rx("^(([0-9]+(.[0-9]+)?)[ ]?)*([0-9]+(.[0-9]+)?)[ ]?$");


  q->updateWidgetFromDisplayNode();
}

//-----------------------------------------------------------------------------
// qMRMLTransformDisplayNodeWidget methods

//-----------------------------------------------------------------------------
qMRMLTransformDisplayNodeWidget::qMRMLTransformDisplayNodeWidget(QWidget* newParent)
  : Superclass(newParent)
  , d_ptr(new qMRMLTransformDisplayNodeWidgetPrivate(*this))
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  d->init();
}

//-----------------------------------------------------------------------------
qMRMLTransformDisplayNodeWidget::~qMRMLTransformDisplayNodeWidget() = default;

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::addVisualizationWidget(QWidget* widget)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!widget)
  {
    return;
  }
  d->visualizationLayout->addWidget(widget);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::addColoringWidget(QWidget* widget)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  d->VectorFieldDisplayWidget->addColoringWidget(widget);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setMRMLTransformNode(vtkMRMLNode* transformNode)
{
  setMRMLTransformNode(vtkMRMLTransformNode::SafeDownCast(transformNode));
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setMRMLTransformNode(vtkMRMLTransformNode* transformNode)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  vtkMRMLTransformDisplayNode* displayNode = nullptr;
  if (transformNode != nullptr)
  {
    displayNode = vtkMRMLTransformDisplayNode::SafeDownCast(transformNode->GetDisplayNode());
  }

  qvtkReconnect(d->TransformDisplayNode, displayNode, vtkCommand::ModifiedEvent, this, SLOT(updateWidgetFromDisplayNode()));

  d->TransformDisplayNode = displayNode;
  // How the field is drawn is edited by the shared vector field widget; this widget only
  // adds what is specific to a transform: the displacement color map and the editor handles.
  d->VectorFieldDisplayWidget->setMRMLVectorFieldDisplayNode(displayNode);
  this->updateWidgetFromDisplayNode();
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateWidgetFromDisplayNode()
{
  Q_D(qMRMLTransformDisplayNodeWidget);

  this->setEnabled(d->TransformDisplayNode != nullptr);

  if (!d->TransformDisplayNode)
  {
    return;
  }

  // Update widget if different from MRML node

  // Display


  bool glyphMode = (d->TransformDisplayNode->GetVisualizationMode() == vtkMRMLTransformDisplayNode::VIS_MODE_GLYPH);
  bool gridMode = (d->TransformDisplayNode->GetVisualizationMode() == vtkMRMLTransformDisplayNode::VIS_MODE_GRID);
  bool contourMode = (d->TransformDisplayNode->GetVisualizationMode() == vtkMRMLTransformDisplayNode::VIS_MODE_CONTOUR);



  // Update Visualization Parameters

  // Common Parameters

  // Glyph Parameters

  bool arrowGlyph = (d->TransformDisplayNode->GetGlyphType() == vtkMRMLTransformDisplayNode::GLYPH_TYPE_ARROW);
  bool coneGlyph = (d->TransformDisplayNode->GetGlyphType() == vtkMRMLTransformDisplayNode::GLYPH_TYPE_CONE);
  bool sphereGlyph = (d->TransformDisplayNode->GetGlyphType() == vtkMRMLTransformDisplayNode::GLYPH_TYPE_SPHERE);


  // Update ColorMap
  vtkColorTransferFunction* colorTransferFunctionInNode = d->TransformDisplayNode->GetColorMap();
  if (colorTransferFunctionInNode)
  {
    if (!vtkMRMLProceduralColorNode::IsColorMapEqual(d->ColorTransferFunction, colorTransferFunctionInNode))
    {
      // only update the range if the colormap is changed to avoid immediate update,
      // because we don't want to change the colormap plot range while dragging the control point
      d->ColorTransferFunction->DeepCopy(colorTransferFunctionInNode);
      this->colorUpdateRange();
    }
  }

  // Interaction
  bool wasBlocking = false;

  wasBlocking = d->InteractionVisibleCheckBox->blockSignals(true);
  d->InteractionVisibleCheckBox->setChecked(d->TransformDisplayNode->GetEditorVisibility());
  d->InteractionVisibleCheckBox->blockSignals(wasBlocking);

  wasBlocking = d->InteractionVisible3dCheckBox->blockSignals(true);
  d->InteractionVisible3dCheckBox->setChecked(d->TransformDisplayNode->GetEditorVisibility3D());
  d->InteractionVisible3dCheckBox->blockSignals(wasBlocking);

  wasBlocking = d->InteractionVisible2dCheckBox->blockSignals(true);
  d->InteractionVisible2dCheckBox->setChecked(d->TransformDisplayNode->GetEditorSliceIntersectionVisibility());
  d->InteractionVisible2dCheckBox->blockSignals(wasBlocking);

  wasBlocking = d->interactionHandleScaleSlider->blockSignals(true);
  d->interactionHandleScaleSlider->setValue(d->TransformDisplayNode->GetInteractionScalePercent());
  d->interactionHandleScaleSlider->blockSignals(wasBlocking);

  wasBlocking = d->interactionHandleOpacitySlider->blockSignals(true);
  d->interactionHandleOpacitySlider->setValue(d->TransformDisplayNode->GetInteractionHandleOpacity());
  d->interactionHandleOpacitySlider->blockSignals(wasBlocking);

  this->updateInteraction3DWidgetsFromDisplayNode();
  this->updateInteractionSliceWidgetsFromDisplayNode();
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateInteraction3DWidgetsFromDisplayNode()
{
  Q_D(qMRMLTransformDisplayNodeWidget);

  if (!d->TransformDisplayNode)
  {
    return;
  }

  bool wasBlocking = false;

  bool enabled3D = d->TransformDisplayNode->GetEditorVisibility3D();

  //////////////
  // Translation
  wasBlocking = d->InteractiveTranslation3DCheckBox->blockSignals(true);
  bool translationEnabled = d->TransformDisplayNode->GetEditorTranslationEnabled();
  d->InteractiveTranslation3DCheckBox->setChecked(translationEnabled);
  d->InteractiveTranslation3DCheckBox->blockSignals(wasBlocking);
  d->InteractiveTranslation3DCheckBox->setEnabled(enabled3D);

  bool translationComponentVisibility[4] = { false, false, false, false };
  d->TransformDisplayNode->GetTranslationHandleComponentVisibility3D(translationComponentVisibility);
  wasBlocking = d->translateX3DCheckBox->blockSignals(true);
  d->translateX3DCheckBox->setChecked(translationComponentVisibility[0]);
  d->translateX3DCheckBox->blockSignals(wasBlocking);
  d->translateX3DCheckBox->setEnabled(translationEnabled && enabled3D);

  wasBlocking = d->translateY3DCheckBox->blockSignals(true);
  d->translateY3DCheckBox->setChecked(translationComponentVisibility[1]);
  d->translateY3DCheckBox->blockSignals(wasBlocking);
  d->translateY3DCheckBox->setEnabled(translationEnabled && enabled3D);

  wasBlocking = d->translateZ3DCheckBox->blockSignals(true);
  d->translateZ3DCheckBox->setChecked(translationComponentVisibility[2]);
  d->translateZ3DCheckBox->blockSignals(wasBlocking);
  d->translateZ3DCheckBox->setEnabled(translationEnabled && enabled3D);

  wasBlocking = d->translateViewPlane3DCheckBox->blockSignals(true);
  d->translateViewPlane3DCheckBox->setChecked(translationComponentVisibility[3]);
  d->translateViewPlane3DCheckBox->blockSignals(wasBlocking);
  d->translateViewPlane3DCheckBox->setEnabled(translationEnabled && enabled3D);

  //////////////
  // Rotation
  wasBlocking = d->InteractiveRotation3DCheckBox->blockSignals(true);
  bool rotationEnabled = d->TransformDisplayNode->GetEditorRotationEnabled();
  d->InteractiveRotation3DCheckBox->setChecked(rotationEnabled);
  d->InteractiveRotation3DCheckBox->blockSignals(wasBlocking);
  d->InteractiveRotation3DCheckBox->setEnabled(enabled3D);

  bool rotationComponentVisibility[4] = { false, false, false, false };
  d->TransformDisplayNode->GetRotationHandleComponentVisibility3D(rotationComponentVisibility);
  wasBlocking = d->rotateX3DCheckBox->blockSignals(true);
  d->rotateX3DCheckBox->setChecked(rotationComponentVisibility[0]);
  d->rotateX3DCheckBox->blockSignals(wasBlocking);
  d->rotateX3DCheckBox->setEnabled(rotationEnabled && enabled3D);

  wasBlocking = d->rotateY3DCheckBox->blockSignals(true);
  d->rotateY3DCheckBox->setChecked(rotationComponentVisibility[1]);
  d->rotateY3DCheckBox->blockSignals(wasBlocking);
  d->rotateY3DCheckBox->setEnabled(rotationEnabled && enabled3D);

  wasBlocking = d->rotateZ3DCheckBox->blockSignals(true);
  d->rotateZ3DCheckBox->setChecked(rotationComponentVisibility[2]);
  d->rotateZ3DCheckBox->blockSignals(wasBlocking);
  d->rotateZ3DCheckBox->setEnabled(rotationEnabled && enabled3D);

  wasBlocking = d->rotateViewPlane3DCheckBox->blockSignals(true);
  d->rotateViewPlane3DCheckBox->setChecked(rotationComponentVisibility[3]);
  d->rotateViewPlane3DCheckBox->blockSignals(wasBlocking);
  d->rotateViewPlane3DCheckBox->setEnabled(rotationEnabled && enabled3D);

  //////////////
  // Scaling
  wasBlocking = d->InteractiveScaling3DCheckBox->blockSignals(true);
  bool scalingEnabled = d->TransformDisplayNode->GetEditorScalingEnabled();
  d->InteractiveScaling3DCheckBox->setChecked(scalingEnabled);
  d->InteractiveScaling3DCheckBox->blockSignals(wasBlocking);
  d->InteractiveScaling3DCheckBox->setEnabled(enabled3D);

  bool scalingComponentVisibility[4] = { false, false, false, false };
  d->TransformDisplayNode->GetScaleHandleComponentVisibility3D(scalingComponentVisibility);
  wasBlocking = d->scaleX3DCheckBox->blockSignals(true);
  d->scaleX3DCheckBox->setChecked(scalingComponentVisibility[0]);
  d->scaleX3DCheckBox->blockSignals(wasBlocking);
  d->scaleX3DCheckBox->setEnabled(scalingEnabled && enabled3D);

  wasBlocking = d->scaleY3DCheckBox->blockSignals(true);
  d->scaleY3DCheckBox->setChecked(scalingComponentVisibility[1]);
  d->scaleY3DCheckBox->blockSignals(wasBlocking);
  d->scaleY3DCheckBox->setEnabled(scalingEnabled && enabled3D);

  wasBlocking = d->scaleZ3DCheckBox->blockSignals(true);
  d->scaleZ3DCheckBox->setChecked(scalingComponentVisibility[2]);
  d->scaleZ3DCheckBox->blockSignals(wasBlocking);
  d->scaleZ3DCheckBox->setEnabled(scalingEnabled && enabled3D);

  wasBlocking = d->scaleViewPlane3DCheckBox->blockSignals(true);
  d->scaleViewPlane3DCheckBox->setChecked(scalingComponentVisibility[3]);
  d->scaleViewPlane3DCheckBox->blockSignals(wasBlocking);
  d->scaleViewPlane3DCheckBox->setEnabled(scalingEnabled && enabled3D);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateInteractionSliceWidgetsFromDisplayNode()
{
  Q_D(qMRMLTransformDisplayNodeWidget);

  if (!d->TransformDisplayNode)
  {
    return;
  }

  bool wasBlocking = false;

  bool enabledSlice = d->TransformDisplayNode->GetEditorSliceIntersectionVisibility();

  //////////////
  // Translation
  wasBlocking = d->InteractiveTranslationSliceCheckBox->blockSignals(true);
  bool translationEnabled = d->TransformDisplayNode->GetEditorTranslationSliceEnabled();
  d->InteractiveTranslationSliceCheckBox->setChecked(translationEnabled);
  d->InteractiveTranslationSliceCheckBox->blockSignals(wasBlocking);
  d->InteractiveTranslationSliceCheckBox->setEnabled(enabledSlice);

  bool translationComponentVisibility[4] = { false, false, false, false };
  d->TransformDisplayNode->GetTranslationHandleComponentVisibilitySlice(translationComponentVisibility);
  wasBlocking = d->translateXSliceCheckBox->blockSignals(true);
  d->translateXSliceCheckBox->setChecked(translationComponentVisibility[0]);
  d->translateXSliceCheckBox->blockSignals(wasBlocking);
  d->translateXSliceCheckBox->setEnabled(translationEnabled && enabledSlice);

  wasBlocking = d->translateYSliceCheckBox->blockSignals(true);
  d->translateYSliceCheckBox->setChecked(translationComponentVisibility[1]);
  d->translateYSliceCheckBox->blockSignals(wasBlocking);
  d->translateYSliceCheckBox->setEnabled(translationEnabled && enabledSlice);

  wasBlocking = d->translateZSliceCheckBox->blockSignals(true);
  d->translateZSliceCheckBox->setChecked(translationComponentVisibility[2]);
  d->translateZSliceCheckBox->blockSignals(wasBlocking);
  d->translateZSliceCheckBox->setEnabled(translationEnabled && enabledSlice);

  wasBlocking = d->translateViewPlaneSliceCheckBox->blockSignals(true);
  d->translateViewPlaneSliceCheckBox->setChecked(translationComponentVisibility[3]);
  d->translateViewPlaneSliceCheckBox->blockSignals(wasBlocking);
  d->translateViewPlaneSliceCheckBox->setEnabled(translationEnabled && enabledSlice);

  wasBlocking = d->EditorTranslationSliceAnywhereEnabledCheckBox->blockSignals(true);
  bool translationSliceAnywhereEnabled = d->TransformDisplayNode->GetEditorTranslationSliceAnywhereEnabled();
  d->EditorTranslationSliceAnywhereEnabledCheckBox->setChecked(translationSliceAnywhereEnabled);
  d->EditorTranslationSliceAnywhereEnabledCheckBox->blockSignals(wasBlocking);
  d->EditorTranslationSliceAnywhereEnabledCheckBox->setEnabled(translationEnabled && enabledSlice);

  wasBlocking = d->editorTranslationSliceAnywhereSensitivitySlider->blockSignals(true);
  d->editorTranslationSliceAnywhereSensitivitySlider->setValue(d->TransformDisplayNode->GetEditorTranslationSliceAnywhereSensitivity());
  d->editorTranslationSliceAnywhereSensitivitySlider->blockSignals(wasBlocking);
  d->editorTranslationSliceAnywhereSensitivitySlider->setEnabled(translationSliceAnywhereEnabled && translationEnabled && enabledSlice);

  //////////////
  // Rotation
  wasBlocking = d->InteractiveRotationSliceCheckBox->blockSignals(true);
  bool rotationEnabled = d->TransformDisplayNode->GetEditorRotationSliceEnabled();
  d->InteractiveRotationSliceCheckBox->setChecked(rotationEnabled);
  d->InteractiveRotationSliceCheckBox->blockSignals(wasBlocking);
  d->InteractiveRotationSliceCheckBox->setEnabled(enabledSlice);

  bool rotationComponentVisibility[4] = { false, false, false, false };
  d->TransformDisplayNode->GetRotationHandleComponentVisibilitySlice(rotationComponentVisibility);
  wasBlocking = d->rotateXSliceCheckBox->blockSignals(true);
  d->rotateXSliceCheckBox->setChecked(rotationComponentVisibility[0]);
  d->rotateXSliceCheckBox->blockSignals(wasBlocking);
  d->rotateXSliceCheckBox->setEnabled(rotationEnabled && enabledSlice);

  wasBlocking = d->rotateYSliceCheckBox->blockSignals(true);
  d->rotateYSliceCheckBox->setChecked(rotationComponentVisibility[1]);
  d->rotateYSliceCheckBox->blockSignals(wasBlocking);
  d->rotateYSliceCheckBox->setEnabled(rotationEnabled && enabledSlice);

  wasBlocking = d->rotateZSliceCheckBox->blockSignals(true);
  d->rotateZSliceCheckBox->setChecked(rotationComponentVisibility[2]);
  d->rotateZSliceCheckBox->blockSignals(wasBlocking);
  d->rotateZSliceCheckBox->setEnabled(rotationEnabled && enabledSlice);

  wasBlocking = d->rotateViewPlaneSliceCheckBox->blockSignals(true);
  d->rotateViewPlaneSliceCheckBox->setChecked(rotationComponentVisibility[3]);
  d->rotateViewPlaneSliceCheckBox->blockSignals(wasBlocking);
  d->rotateViewPlaneSliceCheckBox->setEnabled(rotationEnabled && enabledSlice);

  //////////////
  // Scaling
  wasBlocking = d->InteractiveScalingSliceCheckBox->blockSignals(true);
  bool scalingEnabled = d->TransformDisplayNode->GetEditorScalingSliceEnabled();
  d->InteractiveScalingSliceCheckBox->setChecked(scalingEnabled);
  d->InteractiveScalingSliceCheckBox->blockSignals(wasBlocking);
  d->InteractiveScalingSliceCheckBox->setEnabled(enabledSlice);

  bool scalingComponentVisibility[4] = { false, false, false, false };
  d->TransformDisplayNode->GetScaleHandleComponentVisibilitySlice(scalingComponentVisibility);
  wasBlocking = d->scaleXSliceCheckBox->blockSignals(true);
  d->scaleXSliceCheckBox->setChecked(scalingComponentVisibility[0]);
  d->scaleXSliceCheckBox->blockSignals(wasBlocking);
  d->scaleXSliceCheckBox->setEnabled(scalingEnabled && enabledSlice);

  wasBlocking = d->scaleYSliceCheckBox->blockSignals(true);
  d->scaleYSliceCheckBox->setChecked(scalingComponentVisibility[1]);
  d->scaleYSliceCheckBox->blockSignals(wasBlocking);
  d->scaleYSliceCheckBox->setEnabled(scalingEnabled && enabledSlice);

  wasBlocking = d->scaleZSliceCheckBox->blockSignals(true);
  d->scaleZSliceCheckBox->setChecked(scalingComponentVisibility[2]);
  d->scaleZSliceCheckBox->blockSignals(wasBlocking);
  d->scaleZSliceCheckBox->setEnabled(scalingEnabled && enabledSlice);

  wasBlocking = d->scaleViewPlaneSliceCheckBox->blockSignals(true);
  d->scaleViewPlaneSliceCheckBox->setChecked(scalingComponentVisibility[3]);
  d->scaleViewPlaneSliceCheckBox->blockSignals(wasBlocking);
  d->scaleViewPlaneSliceCheckBox->setEnabled(scalingEnabled && enabledSlice);
}
//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorVisibility(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorVisibility(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorVisibility3d(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorVisibility3D(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorVisibility2d(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorSliceIntersectionVisibility(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorTranslationEnabled(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorTranslationEnabled(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorTranslationSliceEnabled(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorTranslationSliceEnabled(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorTranslationSliceAnywhereEnabled(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorTranslationSliceAnywhereEnabled(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorTranslationSliceAnywhereSensitivity(double sensitivity)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorTranslationSliceAnywhereSensitivity(sensitivity);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorRotationEnabled(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorRotationEnabled(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorRotationSliceEnabled(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorRotationSliceEnabled(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorScalingEnabled(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorScalingEnabled(enabled);
}

//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setEditorScalingSliceEnabled(bool enabled)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetEditorScalingSliceEnabled(enabled);
}
//-----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::setColorTableNode(vtkMRMLNode* colorTableNode)
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetAndObserveColorNodeID(colorTableNode ? colorTableNode->GetID() : nullptr);
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::colorUpdateRange()
{
  Q_D(qMRMLTransformDisplayNodeWidget);

  if (!d->TransformDisplayNode)
  {
    return;
  }
  // Rescale the chart so that all the points are visible
  vtkColorTransferFunction* colorMap = d->TransformDisplayNode->GetColorMap();
  if (colorMap == nullptr)
  {
    return;
  }
  double range[2] = { 0.0, 10.0 };
  colorMap->GetRange(range);
  double chartBounds[8] = { 0 };
  d->ColorMapWidget->view()->chartBounds(chartBounds);
  chartBounds[2] = 0;
  chartBounds[3] = range[1] * 1.1;
  d->ColorMapWidget->view()->setChartUserBounds(chartBounds);
  d->ColorMapWidget->view()->update();
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::onColorInteractionEvent()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  colorUpdateRange();
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::onColorModifiedEvent()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }
  d->TransformDisplayNode->SetColorMap(d->ColorTransferFunction);
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateEditorBounds()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }

  qWarning() << "qMRMLTransformDisplayNodeWidget::updateEditorBounds() is not implemented yet";
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateTranslationComponentVisibility()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }

  bool componentVisibility[4] = {
    d->translateX3DCheckBox->isChecked(), d->translateY3DCheckBox->isChecked(), d->translateZ3DCheckBox->isChecked(), d->translateViewPlane3DCheckBox->isChecked()
  };
  d->TransformDisplayNode->SetTranslationHandleComponentVisibility3D(componentVisibility);

  componentVisibility[0] = d->translateXSliceCheckBox->isChecked();
  componentVisibility[1] = d->translateYSliceCheckBox->isChecked();
  componentVisibility[2] = d->translateZSliceCheckBox->isChecked();
  componentVisibility[3] = d->translateViewPlaneSliceCheckBox->isChecked();
  d->TransformDisplayNode->SetTranslationHandleComponentVisibilitySlice(componentVisibility);
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateRotationComponentVisibility()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }

  bool componentVisibility[4] = {
    d->rotateX3DCheckBox->isChecked(), d->rotateY3DCheckBox->isChecked(), d->rotateZ3DCheckBox->isChecked(), d->rotateViewPlane3DCheckBox->isChecked()
  };
  d->TransformDisplayNode->SetRotationHandleComponentVisibility3D(componentVisibility);

  componentVisibility[0] = d->rotateXSliceCheckBox->isChecked();
  componentVisibility[1] = d->rotateYSliceCheckBox->isChecked();
  componentVisibility[2] = d->rotateZSliceCheckBox->isChecked();
  componentVisibility[3] = d->rotateViewPlaneSliceCheckBox->isChecked();
  d->TransformDisplayNode->SetRotationHandleComponentVisibilitySlice(componentVisibility);
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateScalingComponentVisibility()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }

  bool componentVisibility[4] = { d->scaleX3DCheckBox->isChecked(), d->scaleY3DCheckBox->isChecked(), d->scaleZ3DCheckBox->isChecked(), d->scaleViewPlane3DCheckBox->isChecked() };
  d->TransformDisplayNode->SetScaleHandleComponentVisibility3D(componentVisibility);

  componentVisibility[0] = d->scaleXSliceCheckBox->isChecked();
  componentVisibility[1] = d->scaleYSliceCheckBox->isChecked();
  componentVisibility[2] = d->scaleZSliceCheckBox->isChecked();
  componentVisibility[3] = d->scaleViewPlaneSliceCheckBox->isChecked();
  d->TransformDisplayNode->SetScaleHandleComponentVisibilitySlice(componentVisibility);
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateInteractionHandleScale()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }

  double scale = d->interactionHandleScaleSlider->value();
  d->TransformDisplayNode->SetInteractionScalePercent(scale);
}

// ----------------------------------------------------------------------------
void qMRMLTransformDisplayNodeWidget::updateInteractionHandleOpacity()
{
  Q_D(qMRMLTransformDisplayNodeWidget);
  if (!d->TransformDisplayNode)
  {
    return;
  }

  d->TransformDisplayNode->SetInteractionHandleOpacity(d->interactionHandleOpacitySlider->value());
}
