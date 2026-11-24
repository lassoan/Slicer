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
  Oslo University Hospital) and was supported by The Research Council of Norway
  through the ALive project (grant nr. 311393).

==============================================================================*/

#ifndef __qMRMLTransformAbstractOptionsWidget_h_
#define __qMRMLTransformAbstractOptionsWidget_h_

// Transforms Widgets includes
#include "qSlicerTransformsModuleWidgetsExport.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLTransformNode.h>

// Qt includes
#include <QWidget>

//-------------------------------------------------------------------------------
class vtkMRMLTransformNode;
class vtkMRMLNode;

//-------------------------------------------------------------------------------
/// \name qMRMLTransformAbstractOptionsWidget
/// \brief qMRMLTransformAbstractOptionsWidget is a base class for the
/// additional options widgets that the Transforms module shows for some transforms.
///
/// A module (for example, in an extension) that adds a transform type can register a widget in
/// qMRMLTransformOptionsWidgetsFactory for editing the properties of its transforms. The Transforms
/// module creates an instance of each registered widget and shows it in its Edit section, while the
/// selected transform node is one that the widget can manage.
class Q_SLICER_MODULE_TRANSFORMS_WIDGETS_EXPORT qMRMLTransformAbstractOptionsWidget : public QWidget
{
  Q_OBJECT

  Q_PROPERTY(QString className READ className CONSTANT);

public:
  typedef QWidget Superclass;

  qMRMLTransformAbstractOptionsWidget(QWidget* parent = nullptr);
  ~qMRMLTransformAbstractOptionsWidget() = default;

  /// Updates the widget based on information from MRML.
  virtual void updateWidgetFromMRML() = 0;

  /// Gets the name of the additional options widget type
  virtual const QString className() const = 0;

  // Returns the associated transform node
  vtkMRMLTransformNode* mrmlTransformNode() const { return this->TransformNode.GetPointer(); }

  // Returns the associated transform node
  vtkMRMLScene* mrmlScene() const { return this->MRMLScene.GetPointer(); }

  /// Checks whether a given node can be handled by the widget. This allows
  /// using complex logics to determine whether the widget can manage a given
  /// transform node or not. The node may be nullptr (no transform node is selected).
  virtual bool canManageMRMLTransformNode(vtkMRMLTransformNode* transformNode) const = 0;

  /// Clone options widget . Override to return a new instance of the options widget
  virtual qMRMLTransformAbstractOptionsWidget* createInstance() const = 0;

  /// Called before the Transforms module replaces the transform of the node (for example, sets it to identity or
  /// inverts it). A widget that keeps a state that refers to the current transform (for example, sliders that show
  /// the translation and rotation relative to the transform when editing started) resets that state.
  /// The default implementation does nothing.
  virtual void transformAboutToBeReplaced() {}

public slots:
  /// Sets the vtkMRMLNode to operate on.
  void setMRMLTransformNode(vtkMRMLNode* transformNode);
  /// Sets the vtkMRMLTransformNode to operate on.
  virtual void setMRMLTransformNode(vtkMRMLTransformNode* transformNode) = 0;
  /// Sets the vtkMRMLNode to operate on.
  virtual void setMRMLScene(vtkMRMLScene* mrmlScene) { this->MRMLScene = mrmlScene; }

protected:
  vtkWeakPointer<vtkMRMLTransformNode> TransformNode;
  vtkWeakPointer<vtkMRMLScene> MRMLScene;

private:
  Q_DISABLE_COPY(qMRMLTransformAbstractOptionsWidget);
};

#endif
