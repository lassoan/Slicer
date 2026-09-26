/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// Python includes (must be first)
#include "vtkSlicerScriptedFileIOUtilities_p.h"

#include "vtkSlicerScriptedFileReader.h"
#include "vtkSlicerScriptedFileIOParent.h"

// MRML includes
#include <vtkMRMLIOOptionsDescription.h>

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

namespace utils = vtkSlicerScriptedFileIOUtilities;

//----------------------------------------------------------------------------
class vtkSlicerScriptedFileReader::vtkInternal
{
public:
  std::string PythonSource;
  std::string PythonClassName;
  PyObject* PythonSelf{ nullptr };
  vtkSmartPointer<vtkSlicerScriptedFileIOParent> Parent;
};

vtkStandardNewMacro(vtkSlicerScriptedFileReader);

//----------------------------------------------------------------------------
vtkSlicerScriptedFileReader::vtkSlicerScriptedFileReader()
{
  this->Internal = new vtkInternal;
  this->Internal->Parent = vtkSmartPointer<vtkSlicerScriptedFileIOParent>::New();
  this->Internal->Parent->SetFileIOHandler(this);
}

//----------------------------------------------------------------------------
vtkSlicerScriptedFileReader::~vtkSlicerScriptedFileReader()
{
  if (this->Internal->PythonSelf && Py_IsInitialized())
  {
    Py_DECREF(this->Internal->PythonSelf);
  }
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerScriptedFileReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "PythonSource: " << this->Internal->PythonSource << "\n";
  os << indent << "PythonClassName: " << this->Internal->PythonClassName << "\n";
}

//----------------------------------------------------------------------------
bool vtkSlicerScriptedFileReader::SetPythonSource(const std::string& filePath, const std::string& className, bool missingClassIsExpected)
{
  std::string pythonClassName = className;
  std::string errorMessage;
  PyObject* self = utils::InstantiateClass(filePath, "FileReader", pythonClassName, this->Internal->Parent, missingClassIsExpected, errorMessage);
  if (!self)
  {
    if (!errorMessage.empty())
    {
      vtkErrorMacro("SetPythonSource: " << errorMessage);
    }
    return false;
  }
  if (this->Internal->PythonSelf)
  {
    Py_DECREF(this->Internal->PythonSelf);
  }
  this->Internal->PythonSelf = self;
  this->Internal->PythonSource = filePath;
  this->Internal->PythonClassName = pythonClassName;

  // Get reader properties that do not change
  std::string value;
  PyObject* result = utils::CallMethod(self, "description");
  if (utils::ToString(result, value))
  {
    this->SetDescription(value.c_str());
  }
  else
  {
    vtkErrorMacro("SetPythonSource: in class " << pythonClassName << " (" << filePath << ") method 'description' is expected to return a string");
  }
  Py_XDECREF(result);

  result = utils::CallMethod(self, "fileType");
  if (utils::ToString(result, value))
  {
    this->SetFileType(value.c_str());
  }
  else
  {
    vtkErrorMacro("SetPythonSource: in class " << pythonClassName << " (" << filePath << ") method 'fileType' is expected to return a string");
  }
  Py_XDECREF(result);

  result = utils::CallMethod(self, "extensions");
  std::vector<std::string> nameFilters;
  if (utils::ToStringList(result, nameFilters))
  {
    this->SetNameFilters(nameFilters);
  }
  else
  {
    vtkErrorMacro("SetPythonSource: in class " << pythonClassName << " (" << filePath << ") method 'extensions' is expected to return a list of strings");
  }
  Py_XDECREF(result);

  return true;
}

//----------------------------------------------------------------------------
std::string vtkSlicerScriptedFileReader::GetPythonSource() const
{
  return this->Internal->PythonSource;
}

//----------------------------------------------------------------------------
std::string vtkSlicerScriptedFileReader::GetPythonClassName() const
{
  return this->Internal->PythonClassName;
}

//----------------------------------------------------------------------------
vtkSlicerScriptedFileIOParent* vtkSlicerScriptedFileReader::GetParent() const
{
  return this->Internal->Parent;
}

//----------------------------------------------------------------------------
bool vtkSlicerScriptedFileReader::CanLoadFile(const char* filePath)
{
  if (!filePath || !utils::HasMethod(this->Internal->PythonSelf, "canLoadFile"))
  {
    return this->Superclass::CanLoadFile(filePath);
  }
  PyObject* arguments = utils::MakeArguments(utils::NewString(filePath));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "canLoadFile", arguments);
  Py_DECREF(arguments);
  if (!result)
  {
    return false;
  }
  const bool canLoad = utils::IsTrue(result);
  Py_DECREF(result);
  return canLoad;
}

//----------------------------------------------------------------------------
double vtkSlicerScriptedFileReader::CanLoadFileConfidence(const char* filePath)
{
  if (!filePath || !utils::HasMethod(this->Internal->PythonSelf, "canLoadFileConfidence"))
  {
    // The default implementation uses CanLoadFile()
    return this->Superclass::CanLoadFileConfidence(filePath);
  }
  PyObject* arguments = utils::MakeArguments(utils::NewString(filePath));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "canLoadFileConfidence", arguments);
  Py_DECREF(arguments);
  double confidence = 0.0;
  if (result && !utils::ToDouble(result, confidence))
  {
    vtkErrorMacro("CanLoadFileConfidence: in class " << this->Internal->PythonClassName << " method 'canLoadFileConfidence' is expected to return a number");
    confidence = 0.0;
  }
  Py_XDECREF(result);
  return confidence;
}

//----------------------------------------------------------------------------
bool vtkSlicerScriptedFileReader::Load(vtkMRMLIOProperties* properties)
{
  this->ClearLoadedNodeIDs();
  if (!this->Internal->PythonSelf || !Py_IsInitialized())
  {
    vtkErrorMacro("Load: Python class is not set");
    return false;
  }
  // The Python class reports the node IDs by setting self.parent.loadedNodes
  utils::ResetStringListAttribute(this->Internal->Parent, "loadedNodes");

  PyObject* arguments = utils::MakeArguments(utils::ToPythonDict(properties));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "load", arguments);
  Py_DECREF(arguments);

  std::vector<std::string> nodeIDs = utils::GetStringListAttribute(this->Internal->Parent, "loadedNodes");
  if (!nodeIDs.empty())
  {
    // Keep node IDs that the Python class may have added directly to the file IO handler
    this->SetLoadedNodeIDs(nodeIDs);
  }

  if (!result)
  {
    return false;
  }
  const bool success = utils::IsTrue(result);
  Py_DECREF(result);
  return success;
}

//----------------------------------------------------------------------------
void vtkSlicerScriptedFileReader::GetOptionsDescription(vtkMRMLIOOptionsDescription* description)
{
  if (!description || !utils::HasMethod(this->Internal->PythonSelf, "getOptionsDescription"))
  {
    this->Superclass::GetOptionsDescription(description);
    return;
  }
  PyObject* arguments = utils::MakeArguments(vtkPythonUtil::GetObjectFromPointer(description));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "getOptionsDescription", arguments);
  Py_DECREF(arguments);
  Py_XDECREF(result);
}
