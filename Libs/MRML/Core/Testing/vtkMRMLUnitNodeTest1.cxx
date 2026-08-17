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

  This file was originally developed by Johan Andruejol, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// MRML includes
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLUnitNode.h"

// VTK includes
#include <vtkNew.h>

// STD includes
#include <vector>
#include <iostream>

const size_t NUMBER_OF_UNITS = 5;
const char* UNITS[NUMBER_OF_UNITS][2] = { { "length", "m" }, { "length", "km" }, { "energy", "J" }, { "luminous_intensity", "cd" }, { "energy", "eV" } };

//---------------------------------------------------------------------------
vtkMRMLScene* CreatePopulatedScene();
bool TestScenesUnitNodeID(vtkMRMLScene* scene);
bool TestUnitNodeAttribute(vtkMRMLScene* scene);
int TestValueConversion();

//---------------------------------------------------------------------------
int vtkMRMLUnitNodeTest1(int, char*[])
{
  vtkNew<vtkMRMLUnitNode> node1;
  EXERCISE_ALL_BASIC_MRML_METHODS(node1.GetPointer());

  CHECK_EXIT_SUCCESS(TestValueConversion());

  bool res = true;
  vtkMRMLScene* scene = CreatePopulatedScene();
  res = res && TestScenesUnitNodeID(scene);
  res = res && TestUnitNodeAttribute(scene);

  scene->Delete();
  return res ? EXIT_SUCCESS : EXIT_FAILURE;
}

//---------------------------------------------------------------------------
int TestValueConversion()
{
  TESTING_OUTPUT_INIT();
  TESTING_OUTPUT_RESET();

  vtkNew<vtkMRMLUnitNode> unit;
  unit->SetDisplayCoefficient(0.01);
  unit->SetDisplayOffset(2.0);

  CHECK_DOUBLE_TOLERANCE(unit->GetDisplayValueFromValue(300.0), 5.0, 1e-9);
  // GetValueFromDisplayValue must be the inverse of GetDisplayValueFromValue
  CHECK_DOUBLE_TOLERANCE(unit->GetValueFromDisplayValue(5.0), 300.0, 1e-9);
  CHECK_DOUBLE_TOLERANCE(unit->GetValueFromDisplayValue(unit->GetDisplayValueFromValue(123.4)), 123.4, 1e-9);

  // Invalid (zero) display coefficient: must warn and return 0
  unit->SetDisplayCoefficient(0.0);
  TESTING_OUTPUT_ASSERT_WARNINGS_BEGIN();
  CHECK_DOUBLE(unit->GetValueFromDisplayValue(5.0), 0.0);
  TESTING_OUTPUT_ASSERT_WARNINGS_END();

  return EXIT_SUCCESS;
}

//---------------------------------------------------------------------------
vtkMRMLScene* CreatePopulatedScene()
{
  vtkMRMLScene* scene = vtkMRMLScene::New();
  for (size_t i = 0; i < NUMBER_OF_UNITS; ++i)
  {
    vtkNew<vtkMRMLUnitNode> unit;
    unit->SetQuantity(UNITS[i][0]);
    unit->SetName(UNITS[i][1]);

    scene->AddNode(unit.GetPointer());
  }
  return scene;
}

//---------------------------------------------------------------------------
bool TestScenesUnitNodeID(vtkMRMLScene* scene)
{
  assert(scene);

  std::vector<vtkMRMLNode*> units;
  scene->GetNodesByClass("vtkMRMLUnitNode", units);

  if (units.size() != NUMBER_OF_UNITS)
  {
    std::cerr << "Did not find the right number of nodes in the scene!" << std::endl;
    return false;
  }

  for (size_t i = 0; i < units.size(); ++i)
  {
    std::string id = "vtkMRMLUnitNode";
    id += UNITS[i][1];
    if (id.compare(units[i]->GetID()) != 0)
    {
      std::cerr << "Did not find the right ID !\n"
                   "expected: "
                << id << " got: " << units[i]->GetID() << std::endl;
      return false;
    }
  }

  return true;
}

//---------------------------------------------------------------------------
bool TestUnitNodeAttribute(vtkMRMLScene* scene)
{
  assert(scene);
  int testedNode = 3;
  vtkMRMLNode* unit = scene->GetNthNodeByClass(testedNode, "vtkMRMLUnitNode");
  if (!unit)
  {
    std::cerr << "Did not find correct node in the scene!" << std::endl;
    return false;
  }

  bool foundQuantityAttribute = false;
  std::vector<std::string> attributes = unit->GetAttributeNames();
  for (size_t i = 0; i < attributes.size(); ++i)
  {
    if (attributes[i].compare("Quantity") == 0)
    {
      foundQuantityAttribute = true;
      break;
    }
  }

  if (!foundQuantityAttribute)
  {
    std::cerr << "Did not find attribute Quantity in the node!" << std::endl;
    return false;
  }

  return foundQuantityAttribute;
}
