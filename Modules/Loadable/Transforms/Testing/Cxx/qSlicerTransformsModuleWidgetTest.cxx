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
#include <QApplication>
#include <QComboBox>
#include <QLayout>
#include <QPushButton>

// CTK includes
#include "ctkTest.h"
#include <ctkCollapsibleGroupBox.h>

// MRML includes
#include <qMRMLCoordinatesWidget.h>
#include <qMRMLMatrixWidget.h>
#include <qMRMLWidget.h>
#include "qMRMLLinearTransformOptionsWidget.h"
#include "qMRMLTransformAbstractOptionsWidget.h"
#include "qMRMLTransformOptionsWidgetsFactory.h"
#include "qSlicerTransformsModule.h"
#include "qSlicerTransformsModuleWidget.h"
#include <vtkMRMLScene.h>
#include <vtkMRMLTransformNode.h>

// SubjectHierarchy includes
#include <qSlicerSubjectHierarchyPluginHandler.h>
#include <qSlicerSubjectHierarchyPluginLogic.h>
#include <qSlicerSubjectHierarchyTransformsPlugin.h>
#include <vtkSlicerSubjectHierarchyModuleLogic.h>

// Slicer includes
#include <qSlicerApplication.h>

// VTK includes
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkThinPlateSplineTransform.h>

// ----------------------------------------------------------------------------
/// Options widget of thin-plate spline transforms, as a module could register for its transform type
class qMRMLTestTransformOptionsWidget : public qMRMLTransformAbstractOptionsWidget
{
  Q_OBJECT

public:
  qMRMLTestTransformOptionsWidget(QWidget* parent = nullptr)
    : qMRMLTransformAbstractOptionsWidget(parent)
  {
  }
  const QString className() const override { return "qMRMLTestTransformEditWidget"; }
  bool canManageMRMLTransformNode(vtkMRMLTransformNode* transformNode) const override
  {
    return transformNode && transformNode->GetTransformToParentAs("vtkThinPlateSplineTransform", false) != nullptr;
  }
  qMRMLTransformAbstractOptionsWidget* createInstance() const override { return new qMRMLTestTransformOptionsWidget(); }
  void updateWidgetFromMRML() override { this->NumberOfUpdates++; }
  void transformAboutToBeReplaced() override { this->NumberOfTransformReplacements++; }
  using qMRMLTransformAbstractOptionsWidget::setMRMLTransformNode;
  void setMRMLTransformNode(vtkMRMLTransformNode* transformNode) override { this->TransformNode = transformNode; }

  int NumberOfUpdates{ 0 };
  int NumberOfTransformReplacements{ 0 };
};

// ----------------------------------------------------------------------------
class qSlicerTransformsModuleWidgetTester : public QObject
{
  Q_OBJECT

public:
  QSharedPointer<qSlicerTransformsModule> transformsModule;
  QSharedPointer<qSlicerSubjectHierarchyPluginLogic> pluginLogic;
  vtkSmartPointer<vtkSlicerSubjectHierarchyModuleLogic> shModuleLogic;
  vtkSmartPointer<vtkMRMLScene> scene;

  bool interactive{ false };

private slots:

  void initTestCase();
  void cleanupTestCase();

  void testIdentity();
  void testInvert();
  void testLinearTransformOptionsWidget();
  void testOptionsWidgets();
};

// ----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetTester::initTestCase()
{
  // Transforms module must be initialized once and only once and we do it here
  // (multiple initializations would result in re-registering plugins,
  // which would lead to warnings that would make the test fail).

  scene = vtkSmartPointer<vtkMRMLScene>::New();

  // Set up Subject Hierarchy logic (needed for the subject hierarchy tree view)
  shModuleLogic = vtkSmartPointer<vtkSlicerSubjectHierarchyModuleLogic>::New();
  shModuleLogic->SetMRMLScene(scene);
  pluginLogic = QSharedPointer<qSlicerSubjectHierarchyPluginLogic>(new qSlicerSubjectHierarchyPluginLogic());
  pluginLogic->setMRMLScene(scene);
  qSlicerSubjectHierarchyPluginHandler::instance()->setPluginLogic(pluginLogic.get());
  qSlicerSubjectHierarchyPluginHandler::instance()->setMRMLScene(scene);

  transformsModule = QSharedPointer<qSlicerTransformsModule>(new qSlicerTransformsModule);
  // register qSlicerSubjectHierarchyTransformsPlugin
  transformsModule->initialize(qSlicerApplication::application()->applicationLogic());
  transformsModule->setMRMLScene(scene);
  transformsModule->logic();
}

// ----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetTester::cleanupTestCase()
{
  transformsModule.clear();
  pluginLogic.clear();
  shModuleLogic = nullptr;
  scene = nullptr;
}

// ----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetTester::testIdentity()
{
  qSlicerTransformsModuleWidget* transformsWidget = dynamic_cast<qSlicerTransformsModuleWidget*>(transformsModule->widgetRepresentation());

  scene->Clear();
  vtkNew<vtkMRMLTransformNode> transformNode;
  scene->AddNode(transformNode);

  transformsWidget->setEditedNode(transformNode);

  vtkNew<vtkMatrix4x4> matrix;
  transformNode->GetMatrixTransformToParent(matrix);
  matrix->SetElement(0, 0, 10.);
  matrix->SetElement(1, 0, 2.);
  transformNode->SetMatrixTransformToParent(matrix);
  // The Identity button is in the options widget of linear transforms
  qMRMLLinearTransformOptionsWidget* linearWidget = transformsWidget->findChild<qMRMLLinearTransformOptionsWidget*>();
  QVERIFY(linearWidget != nullptr);
  QPushButton* identityButton = linearWidget->findChild<QPushButton*>("IdentityPushButton");
  QVERIFY(identityButton != nullptr);
  QVERIFY(identityButton->isEnabled());
  identityButton->click();
  transformNode->GetMatrixTransformToParent(matrix);

  if (interactive)
  {
    transformsWidget->show();
    qApp->exec();
  }

  QCOMPARE(matrix->GetElement(0, 0), 1.);
  QCOMPARE(matrix->GetElement(1, 0), 0.);
}

// ----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetTester::testInvert()
{
  qSlicerTransformsModuleWidget* transformsWidget = dynamic_cast<qSlicerTransformsModuleWidget*>(transformsModule->widgetRepresentation());

  scene->Clear();
  vtkNew<vtkMRMLTransformNode> transformNode;
  scene->AddNode(transformNode);

  transformsWidget->setEditedNode(transformNode);

  vtkNew<vtkMatrix4x4> matrix;
  transformNode->GetMatrixTransformToParent(matrix);
  matrix->SetElement(0, 0, 10.);
  matrix->SetElement(1, 0, 2.);
  transformNode->SetMatrixTransformToParent(matrix);
  transformsWidget->invert();
  transformNode->GetMatrixTransformToParent(matrix);

  if (interactive)
  {
    transformsWidget->show();
    qApp->exec();
  }

  QCOMPARE(matrix->GetElement(0, 0), 0.1);
  QCOMPARE(matrix->GetElement(1, 0), -0.2);
}

// ----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetTester::testLinearTransformOptionsWidget()
{
  qSlicerTransformsModuleWidget* transformsWidget = dynamic_cast<qSlicerTransformsModuleWidget*>(transformsModule->widgetRepresentation());
  QWidget* editSection = transformsWidget->findChild<QWidget*>("DisplayEditCollapsibleWidget");
  QVERIFY(editSection != nullptr);

  // The module registers the options widget of linear transforms, and the module widget has an instance of it
  // at the top of the Edit section
  QVERIFY(qMRMLTransformOptionsWidgetsFactory::instance()->registeredOptionsWidgetsClassNames().contains("qMRMLLinearTransformOptionsWidget"));
  QList<qMRMLLinearTransformOptionsWidget*> linearWidgets = transformsWidget->findChildren<qMRMLLinearTransformOptionsWidget*>();
  QCOMPARE(linearWidgets.size(), 1);
  qMRMLLinearTransformOptionsWidget* linearWidget = linearWidgets[0];
  QVERIFY(editSection->isAncestorOf(linearWidget));
  QVERIFY(editSection->layout() != nullptr);
  QCOMPARE(editSection->layout()->indexOf(linearWidget), 0);
  QCOMPARE(linearWidget->mrmlScene(), scene.GetPointer());

  // The widgets of the linear transform editor are in the options widget, with the names that they had in the module widget
  qMRMLMatrixWidget* matrixWidget = linearWidget->findChild<qMRMLMatrixWidget*>("MatrixWidget");
  QVERIFY(matrixWidget != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("MatrixViewGroupBox") != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("TranslationSliders") != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("RotationSliders") != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("TranslateFirstToolButton") != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("CopyTransformToolButton") != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("PasteTransformToolButton") != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("IdentityPushButton") != nullptr);
  QVERIFY(linearWidget->findChild<QWidget*>("CenterOfTransformationGroupBox") != nullptr);
  // and they can still be found in the module widget
  QCOMPARE(transformsWidget->findChild<qMRMLMatrixWidget*>("MatrixWidget"), matrixWidget);

  // Shown for a linear transform
  scene->Clear();
  vtkNew<vtkMRMLTransformNode> transformNode;
  scene->AddNode(transformNode);
  transformsWidget->setEditedNode(transformNode);
  QCOMPARE(linearWidget->mrmlTransformNode(), transformNode.GetPointer());
  QVERIFY(linearWidget->canManageMRMLTransformNode(transformNode));
  QVERIFY(!linearWidget->isHidden());
  QVERIFY(matrixWidget->isEnabledTo(linearWidget));

  // Editing the matrix changes the node
  matrixWidget->setValue(0, 3, 12.5);
  vtkNew<vtkMatrix4x4> matrix;
  transformNode->GetMatrixTransformToParent(matrix);
  QCOMPARE(matrix->GetElement(0, 3), 12.5);
  // and changing the node updates the matrix widget
  matrix->SetElement(1, 3, -3.0);
  transformNode->SetMatrixTransformToParent(matrix);
  QCOMPARE(matrixWidget->value(1, 3), -3.0);

  // Identity resets the transform, and the matrix widget shows it
  linearWidget->identity();
  transformNode->GetMatrixTransformToParent(matrix);
  QCOMPARE(matrix->GetElement(0, 3), 0.0);
  QCOMPARE(matrixWidget->value(0, 3), 0.0);

  // Center of transformation, in local and in world coordinates
  QComboBox* centerCoordinatesComboBox = linearWidget->findChild<QComboBox*>("CenterOfTransformationCoordinatesComboBox");
  qMRMLCoordinatesWidget* centerCoordinatesWidget = linearWidget->findChild<qMRMLCoordinatesWidget*>("CenterOfTransformationCoordinatesWidget");
  QPushButton* resetCenterButton = linearWidget->findChild<QPushButton*>("ResetCenterOfTransformationButton");
  QVERIFY(centerCoordinatesComboBox && centerCoordinatesWidget && resetCenterButton);
  vtkNew<vtkMRMLTransformNode> parentTransformNode;
  scene->AddNode(parentTransformNode);
  vtkNew<vtkMatrix4x4> parentMatrix;
  parentMatrix->SetElement(0, 3, 10.0);
  parentTransformNode->SetMatrixTransformToParent(parentMatrix);
  transformNode->SetAndObserveTransformNodeID(parentTransformNode->GetID());
  transformNode->SetCenterOfTransformation(1.0, 2.0, 3.0);
  centerCoordinatesComboBox->setCurrentIndex(1); // local
  QCOMPARE(centerCoordinatesWidget->coordinates()[0], 1.0);
  centerCoordinatesComboBox->setCurrentIndex(0); // world
  QCOMPARE(centerCoordinatesWidget->coordinates()[0], 11.0);
  // Setting the center in world coordinates sets it in the local coordinate system of the transform
  double centerWorld[3] = { 20.0, 2.0, 3.0 };
  centerCoordinatesWidget->setCoordinates(centerWorld);
  double center[3] = { 0.0, 0.0, 0.0 };
  transformNode->GetCenterOfTransformation(center);
  QCOMPARE(center[0], 10.0);
  // The section is collapsed by default, its buttons work when it is expanded
  ctkCollapsibleGroupBox* centerGroupBox = linearWidget->findChild<ctkCollapsibleGroupBox*>("CenterOfTransformationGroupBox");
  QVERIFY(centerGroupBox != nullptr);
  centerGroupBox->setCollapsed(false);
  QVERIFY(resetCenterButton->isEnabled());
  resetCenterButton->click();
  transformNode->GetCenterOfTransformation(center);
  QCOMPARE(center[0], 0.0);
  QCOMPARE(center[1], 0.0);
  transformNode->SetAndObserveTransformNodeID(nullptr);

  // Hidden when the transform of the node becomes non-linear
  vtkNew<vtkThinPlateSplineTransform> thinPlateSplineTransform;
  transformNode->SetAndObserveTransformToParent(thinPlateSplineTransform);
  QVERIFY(!linearWidget->canManageMRMLTransformNode(transformNode));
  QVERIFY(linearWidget->isHidden());

  // Shown again when another node is selected, which has a linear transform
  vtkNew<vtkMRMLTransformNode> linearTransformNode;
  scene->AddNode(linearTransformNode);
  transformsWidget->setEditedNode(linearTransformNode);
  QCOMPARE(linearWidget->mrmlTransformNode(), linearTransformNode.GetPointer());
  QVERIFY(!linearWidget->isHidden());

  // Hidden when a node of a non-linear transform is selected
  transformsWidget->setEditedNode(transformNode);
  QVERIFY(linearWidget->isHidden());

  // Shown when the transform of the selected node becomes linear
  vtkNew<vtkMatrix4x4> identityMatrix;
  // A matrix can only be set in a node that has no non-linear transform
  transformNode->SetAndObserveTransformToParent(nullptr);
  transformNode->SetMatrixTransformToParent(identityMatrix);
  QVERIFY(!linearWidget->isHidden());

  // No transform is selected: the widget cannot manage it
  QVERIFY(!linearWidget->canManageMRMLTransformNode(nullptr));

  if (interactive)
  {
    transformsWidget->show();
    qApp->exec();
  }
}

// ----------------------------------------------------------------------------
void qSlicerTransformsModuleWidgetTester::testOptionsWidgets()
{
  qSlicerTransformsModuleWidget* transformsWidget = dynamic_cast<qSlicerTransformsModuleWidget*>(transformsModule->widgetRepresentation());
  QWidget* editSection = transformsWidget->findChild<QWidget*>("DisplayEditCollapsibleWidget");
  QVERIFY(editSection != nullptr);
  QCOMPARE(transformsWidget->findChildren<qMRMLTestTransformOptionsWidget*>().size(), 0);

  scene->Clear();
  vtkNew<vtkMRMLTransformNode> transformNode;
  scene->AddNode(transformNode);
  transformsWidget->setEditedNode(transformNode);

  // A widget that is registered after the module widget is created is added to the Edit section
  qMRMLTransformOptionsWidgetsFactory* factory = qMRMLTransformOptionsWidgetsFactory::instance();
  QPointer<qMRMLTestTransformOptionsWidget> editPrototype = new qMRMLTestTransformOptionsWidget();
  QVERIFY(factory->registerOptionsWidget(editPrototype));
  QVERIFY(factory->registeredOptionsWidgetsClassNames().contains("qMRMLTestTransformEditWidget"));
  QList<qMRMLTestTransformOptionsWidget*> optionsWidgets = transformsWidget->findChildren<qMRMLTestTransformOptionsWidget*>();
  QCOMPARE(optionsWidgets.size(), 1);
  qMRMLTestTransformOptionsWidget* editWidget = optionsWidgets[0];
  // It is an instance, not the registered widget
  QVERIFY(editWidget != editPrototype.data());
  QVERIFY(editSection->isAncestorOf(editWidget));
  // Widgets are shown in the order of registration: the linear transform widget of the module remains the first one
  // of the Edit section, the widgets that are registered later are after it
  qMRMLLinearTransformOptionsWidget* linearWidget = transformsWidget->findChild<qMRMLLinearTransformOptionsWidget*>();
  QVERIFY(linearWidget != nullptr);
  QCOMPARE(editSection->layout()->indexOf(linearWidget), 0);
  QCOMPARE(editSection->layout()->indexOf(editWidget), 1);
  QCOMPARE(editWidget->mrmlScene(), scene.GetPointer());

  // It gets the selected node, and is hidden while it cannot manage it
  QCOMPARE(editWidget->mrmlTransformNode(), transformNode.GetPointer());
  QVERIFY(editWidget->isHidden());
  QCOMPARE(editWidget->NumberOfUpdates, 0);

  // The node gets a transform that it can manage: it is shown and updated
  vtkNew<vtkThinPlateSplineTransform> thinPlateSplineTransform;
  transformNode->SetAndObserveTransformToParent(thinPlateSplineTransform);
  QVERIFY(!editWidget->isHidden());
  QVERIFY(editWidget->NumberOfUpdates > 0);
  // Updated when the transform is modified
  int numberOfUpdates = editWidget->NumberOfUpdates;
  thinPlateSplineTransform->SetSigma(2.0);
  QVERIFY(editWidget->NumberOfUpdates > numberOfUpdates);
  // Notified before the module replaces the transform (inverting it twice gives back the original transform)
  QCOMPARE(editWidget->NumberOfTransformReplacements, 0);
  transformsWidget->invert();
  transformsWidget->invert();
  QCOMPARE(editWidget->NumberOfTransformReplacements, 2);

  if (interactive)
  {
    transformsWidget->show();
    qApp->exec();
  }

  // Another node, which it cannot manage
  vtkNew<vtkMRMLTransformNode> linearTransformNode;
  scene->AddNode(linearTransformNode);
  transformsWidget->setEditedNode(linearTransformNode);
  QCOMPARE(editWidget->mrmlTransformNode(), linearTransformNode.GetPointer());
  QVERIFY(editWidget->isHidden());
  transformsWidget->setEditedNode(transformNode);
  QVERIFY(!editWidget->isHidden());

  // A widget of the same class cannot be registered again (the factory deletes the rejected widget)
  QPointer<qMRMLTestTransformOptionsWidget> duplicatePrototype = new qMRMLTestTransformOptionsWidget();
  QVERIFY(!factory->registerOptionsWidget(duplicatePrototype));
  QVERIFY(duplicatePrototype.isNull());

  // An unregistered widget is removed from the module
  QVERIFY(factory->unregisterOptionsWidget(editPrototype));
  QVERIFY(editPrototype.isNull());
  QVERIFY(!factory->registeredOptionsWidgetsClassNames().contains("qMRMLTestTransformEditWidget"));
  QCOMPARE(transformsWidget->findChildren<qMRMLTestTransformOptionsWidget*>().size(), 0);
}

// ----------------------------------------------------------------------------
// CTK_TEST_MAIN(qSlicerTransformsModuleWidgetTest)
int qSlicerTransformsModuleWidgetTest(int argc, char* argv[])
{
  qMRMLWidget::preInitializeApplication();
  qSlicerApplication app(argc, argv);
  qMRMLWidget::postInitializeApplication();

  qSlicerTransformsModuleWidgetTester tc;
  tc.interactive = false; // set it to true for interactive testing

  return QTest::qExec(&tc, argc, argv);
}

#include "qSlicerTransformsModuleWidgetTest.moc"
