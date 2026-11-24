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

#ifndef __qMRMLLinearTransformOptionsWidget_h
#define __qMRMLLinearTransformOptionsWidget_h

// Transforms Widgets includes
#include "qMRMLTransformAbstractOptionsWidget.h"
#include "qSlicerTransformsModuleWidgetsExport.h"

// Qt includes
#include <QScopedPointer>

class qMRMLLinearTransformOptionsWidgetPrivate;

/// \brief Options widget of the Transforms module for editing linear transforms:
/// transform matrix, translation and rotation sliders, the reference frame of the translation,
/// copy/paste of the matrix, reset to identity, and the center of transformation.
///
/// The Transforms module registers it in qMRMLTransformOptionsWidgetsFactory, the same way as
/// modules register the options widgets of their transform types.
/// \sa qMRMLTransformAbstractOptionsWidget, qMRMLTransformOptionsWidgetsFactory
class Q_SLICER_MODULE_TRANSFORMS_WIDGETS_EXPORT qMRMLLinearTransformOptionsWidget : public qMRMLTransformAbstractOptionsWidget
{
  Q_OBJECT

public:
  typedef qMRMLTransformAbstractOptionsWidget Superclass;
  qMRMLLinearTransformOptionsWidget(QWidget* parent = nullptr);
  ~qMRMLLinearTransformOptionsWidget() override;

  const QString className() const override { return "qMRMLLinearTransformOptionsWidget"; }

  /// The widget manages the transform nodes that have a linear transform.
  bool canManageMRMLTransformNode(vtkMRMLTransformNode* transformNode) const override;

  qMRMLTransformAbstractOptionsWidget* createInstance() const override;

  void updateWidgetFromMRML() override;

  /// Coordinate system that is currently selected for the translation and rotation sliders
  /// (qMRMLTransformSliders::LOCAL or qMRMLTransformSliders::GLOBAL).
  Q_INVOKABLE int coordinateReference() const;

  // These are slots of the base class (moc does not allow a using declaration in a slots section)
  using qMRMLTransformAbstractOptionsWidget::setMRMLTransformNode;
  void setMRMLTransformNode(vtkMRMLTransformNode* transformNode) override;
  void setMRMLScene(vtkMRMLScene* mrmlScene) override;

  /// Reset the translation and rotation sliders
  void transformAboutToBeReplaced() override;

public slots:
  /// Set the transform to identity
  void identity();

  /// Set the center of transformation to the origin of the local coordinate system
  void resetCenterOfTransformation();

  /// Reset the sliders to the position 0. To be called when the transform is changed
  /// by something else than the sliders (for example, it is set to identity or inverted).
  void resetUnactiveSliders();

  /// Copy the matrix of the transform to the clipboard
  void copyTransform();
  /// Set the matrix of the transform from the clipboard
  void pasteTransform();

protected slots:
  void onTranslateFirstButtonPressed(bool checked);
  void updateCenterOfTransformationWidgets();
  void onCenterOfTransformationChanged();

protected:
  QScopedPointer<qMRMLLinearTransformOptionsWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLLinearTransformOptionsWidget);
  Q_DISABLE_COPY(qMRMLLinearTransformOptionsWidget);
};

#endif
