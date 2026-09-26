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

#include "vtkSlicerScriptedFileWriter.h"
#include "vtkSlicerScriptedFileIOParent.h"

// MRML includes
#include <vtkMRMLIOOptionsDescription.h>

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>

namespace utils = vtkSlicerScriptedFileIOUtilities;

//----------------------------------------------------------------------------
class vtkSlicerScriptedFileWriter::vtkInternal
{
public:
  std::string PythonSource;
  std::string PythonClassName;
  PyObject* PythonSelf{ nullptr };
  vtkSmartPointer<vtkSlicerScriptedFileIOParent> Parent;
};

vtkStandardNewMacro(vtkSlicerScriptedFileWriter);

//----------------------------------------------------------------------------
vtkSlicerScriptedFileWriter::vtkSlicerScriptedFileWriter()
{
  this->Internal = new vtkInternal;
  this->Internal->Parent = vtkSmartPointer<vtkSlicerScriptedFileIOParent>::New();
  this->Internal->Parent->SetFileIOHandler(this);
}

//----------------------------------------------------------------------------
vtkSlicerScriptedFileWriter::~vtkSlicerScriptedFileWriter()
{
  if (this->Internal->PythonSelf && Py_IsInitialized())
  {
    Py_DECREF(this->Internal->PythonSelf);
  }
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkSlicerScriptedFileWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "PythonSource: " << this->Internal->PythonSource << "\n";
  os << indent << "PythonClassName: " << this->Internal->PythonClassName << "\n";
}

//----------------------------------------------------------------------------
bool vtkSlicerScriptedFileWriter::SetPythonSource(const std::string& filePath, const std::string& className, bool missingClassIsExpected)
{
  std::string pythonClassName = className;
  std::string errorMessage;
  PyObject* self = utils::InstantiateClass(filePath, "FileWriter", pythonClassName, this->Internal->Parent, missingClassIsExpected, errorMessage);
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

  // Get writer properties that do not change
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

  return true;
}

//----------------------------------------------------------------------------
std::string vtkSlicerScriptedFileWriter::GetPythonSource() const
{
  return this->Internal->PythonSource;
}

//----------------------------------------------------------------------------
std::string vtkSlicerScriptedFileWriter::GetPythonClassName() const
{
  return this->Internal->PythonClassName;
}

//----------------------------------------------------------------------------
vtkSlicerScriptedFileIOParent* vtkSlicerScriptedFileWriter::GetParent() const
{
  return this->Internal->Parent;
}

//----------------------------------------------------------------------------
bool vtkSlicerScriptedFileWriter::CanWriteObject(vtkObject* object)
{
  if (!utils::HasMethod(this->Internal->PythonSelf, "canWriteObject"))
  {
    return this->Superclass::CanWriteObject(object);
  }
  PyObject* arguments = utils::MakeArguments(vtkPythonUtil::GetObjectFromPointer(object));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "canWriteObject", arguments);
  Py_DECREF(arguments);
  if (!result)
  {
    return false;
  }
  const bool canWrite = utils::IsTrue(result);
  Py_DECREF(result);
  return canWrite;
}

//----------------------------------------------------------------------------
double vtkSlicerScriptedFileWriter::CanWriteObjectConfidence(vtkObject* object)
{
  if (!utils::HasMethod(this->Internal->PythonSelf, "canWriteObjectConfidence"))
  {
    // The default implementation uses CanWriteObject()
    return this->Superclass::CanWriteObjectConfidence(object);
  }
  PyObject* arguments = utils::MakeArguments(vtkPythonUtil::GetObjectFromPointer(object));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "canWriteObjectConfidence", arguments);
  Py_DECREF(arguments);
  double confidence = 0.0;
  if (result && !utils::ToDouble(result, confidence))
  {
    vtkErrorMacro("CanWriteObjectConfidence: in class " << this->Internal->PythonClassName
                                                        << " method 'canWriteObjectConfidence' is expected to return a number");
    confidence = 0.0;
  }
  Py_XDECREF(result);
  return confidence;
}

//----------------------------------------------------------------------------
void vtkSlicerScriptedFileWriter::GetNameFiltersForObject(vtkObject* object, vtkStringArray* nameFilters)
{
  if (!nameFilters)
  {
    return;
  }
  nameFilters->Reset();
  if (!this->Internal->PythonSelf || !Py_IsInitialized())
  {
    return;
  }
  PyObject* arguments = utils::MakeArguments(vtkPythonUtil::GetObjectFromPointer(object));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "extensions", arguments);
  Py_DECREF(arguments);
  std::vector<std::string> filters;
  if (result && !utils::ToStringList(result, filters))
  {
    vtkErrorMacro("GetNameFiltersForObject: in class " << this->Internal->PythonClassName << " method 'extensions' is expected to return a list of strings");
  }
  Py_XDECREF(result);
  for (const std::string& filter : filters)
  {
    nameFilters->InsertNextValue(filter);
  }
}

//----------------------------------------------------------------------------
bool vtkSlicerScriptedFileWriter::Write(vtkMRMLIOProperties* properties)
{
  this->ClearWrittenNodeIDs();
  if (!this->Internal->PythonSelf || !Py_IsInitialized())
  {
    vtkErrorMacro("Write: Python class is not set");
    return false;
  }
  // The Python class reports the node IDs by setting self.parent.writtenNodes
  utils::ResetStringListAttribute(this->Internal->Parent, "writtenNodes");

  PyObject* arguments = utils::MakeArguments(utils::ToPythonDict(properties));
  PyObject* result = utils::CallMethod(this->Internal->PythonSelf, "write", arguments);
  Py_DECREF(arguments);

  std::vector<std::string> nodeIDs = utils::GetStringListAttribute(this->Internal->Parent, "writtenNodes");
  if (!nodeIDs.empty())
  {
    // Keep node IDs that the Python class may have added directly to the file IO handler
    this->SetWrittenNodeIDs(nodeIDs);
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
void vtkSlicerScriptedFileWriter::GetOptionsDescription(vtkMRMLIOOptionsDescription* description)
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
