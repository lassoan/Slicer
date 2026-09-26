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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Slicer API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifndef __vtkSlicerScriptedFileIOUtilities_p_h
#define __vtkSlicerScriptedFileIOUtilities_p_h

// Python includes
#include <vtkPython.h>

// MRML includes
#include <vtkMRMLIOProperties.h>

// VTK includes
#include <vtkObject.h>
#include <vtkPythonUtil.h>
#include <vtkVariant.h>
#include <vtksys/SystemTools.hxx>

// STD includes
#include <string>
#include <vector>

namespace vtkSlicerScriptedFileIOUtilities
{

//----------------------------------------------------------------------------
/// Print the current Python error (if any) and clear it.
/// SystemExit is not allowed to terminate the application.
inline void ReportError()
{
  if (!PyErr_Occurred())
  {
    return;
  }
  if (PyErr_ExceptionMatches(PyExc_SystemExit))
  {
    PyErr_Clear();
    vtkGenericWarningMacro("SystemExit raised in a scripted file reader or writer was ignored");
    return;
  }
  PyErr_Print();
}

//----------------------------------------------------------------------------
/// Create a Python string (new reference). Invalid UTF-8 characters are replaced,
/// so that the returned value is never nullptr.
inline PyObject* NewString(const std::string& text)
{
  PyObject* result = PyUnicode_DecodeUTF8(text.c_str(), static_cast<Py_ssize_t>(text.size()), "replace");
  if (!result)
  {
    PyErr_Clear();
    result = PyUnicode_FromString("");
  }
  return result;
}

//----------------------------------------------------------------------------
/// Instantiate the Python class that implements a scripted reader or writer.
/// The class is looked up in the Python module that was loaded from \a filePath
/// (the module must be already imported, which is done when the scripted module is loaded).
/// If \a className is empty then it is set to <ModuleName><classNameSuffix>.
/// Returns a new reference to the instance, nullptr if the class is not found or instantiation failed.
inline PyObject* InstantiateClass(const std::string& filePath,
                                  const std::string& classNameSuffix,
                                  std::string& className,
                                  vtkObjectBase* parent,
                                  bool missingClassIsExpected,
                                  std::string& errorMessage)
{
  if (!Py_IsInitialized())
  {
    errorMessage = "Python is not initialized";
    return nullptr;
  }
  if (!vtksys::SystemTools::StringEndsWith(filePath, ".py") && !vtksys::SystemTools::StringEndsWith(filePath, ".pyc"))
  {
    errorMessage = "Python source file name must end with .py or .pyc: " + filePath;
    return nullptr;
  }
  const std::string moduleName = vtksys::SystemTools::GetFilenameWithoutExtension(filePath);
  if (className.empty())
  {
    className = moduleName;
    if (!vtksys::SystemTools::StringEndsWith(moduleName, classNameSuffix.c_str()))
    {
      className += classNameSuffix;
    }
  }

  // Get the module from sys.modules
  PyObject* sysModules = PyImport_GetModuleDict();                         // borrowed reference
  PyObject* module = PyDict_GetItemString(sysModules, moduleName.c_str()); // borrowed reference
  if (!module || !PyObject_HasAttrString(module, className.c_str()))
  {
    if (!missingClassIsExpected)
    {
      errorMessage = "Class " + className + " was not found in file " + filePath;
    }
    return nullptr;
  }

  PyObject* classToInstantiate = PyObject_GetAttrString(module, className.c_str());
  PyObject* arguments = PyTuple_New(1);
  PyTuple_SET_ITEM(arguments, 0, vtkPythonUtil::GetObjectFromPointer(parent));
  PyObject* self = PyObject_CallObject(classToInstantiate, arguments);
  Py_DECREF(arguments);
  Py_DECREF(classToInstantiate);
  if (!self)
  {
    ReportError();
    errorMessage = "Failed to instantiate class " + className + " from file " + filePath;
    return nullptr;
  }
  return self;
}

//----------------------------------------------------------------------------
/// Returns true if the Python object has a callable attribute with the specified name.
inline bool HasMethod(PyObject* self, const char* methodName)
{
  if (!self || !Py_IsInitialized() || !PyObject_HasAttrString(self, methodName))
  {
    return false;
  }
  PyObject* method = PyObject_GetAttrString(self, methodName);
  const bool callable = method && PyCallable_Check(method);
  Py_XDECREF(method);
  return callable;
}

//----------------------------------------------------------------------------
/// Call a method of the Python object. \a arguments may be nullptr (no arguments).
/// Returns a new reference to the result, nullptr if the call failed (the Python error is printed).
inline PyObject* CallMethod(PyObject* self, const char* methodName, PyObject* arguments = nullptr)
{
  if (!self || !Py_IsInitialized())
  {
    return nullptr;
  }
  // Clear any error that was left set by other code, it must not make this call fail
  PyErr_Clear();
  PyObject* method = PyObject_GetAttrString(self, methodName);
  if (!method)
  {
    ReportError();
    return nullptr;
  }
  PyObject* result = PyObject_CallObject(method, arguments);
  Py_DECREF(method);
  if (!result)
  {
    ReportError();
  }
  return result;
}

//----------------------------------------------------------------------------
/// Returns a new reference to a Python tuple that contains a single object.
inline PyObject* MakeArguments(PyObject* argument)
{
  if (!argument)
  {
    // Creating the argument failed
    ReportError();
    Py_INCREF(Py_None);
    argument = Py_None;
  }
  PyObject* arguments = PyTuple_New(1);
  PyTuple_SET_ITEM(arguments, 0, argument); // steals reference
  return arguments;
}

//----------------------------------------------------------------------------
/// Get string from a Python object. Returns false if the object is not a string.
inline bool ToString(PyObject* object, std::string& value)
{
  if (!object || !PyUnicode_Check(object))
  {
    return false;
  }
  const char* text = PyUnicode_AsUTF8(object);
  if (!text)
  {
    PyErr_Clear();
    return false;
  }
  value = text;
  return true;
}

//----------------------------------------------------------------------------
/// Get list of strings from a Python list or tuple. Returns false if the object is not a list or tuple of strings.
inline bool ToStringList(PyObject* object, std::vector<std::string>& values)
{
  values.clear();
  if (!object || (!PyList_Check(object) && !PyTuple_Check(object)))
  {
    return false;
  }
  PyObject* sequence = PySequence_Fast(object, "expected a sequence");
  if (!sequence)
  {
    PyErr_Clear();
    return false;
  }
  const Py_ssize_t size = PySequence_Fast_GET_SIZE(sequence);
  bool valid = true;
  for (Py_ssize_t i = 0; i < size; ++i)
  {
    std::string value;
    if (!ToString(PySequence_Fast_GET_ITEM(sequence, i), value))
    {
      valid = false;
      break;
    }
    values.push_back(value);
  }
  Py_DECREF(sequence);
  return valid;
}

//----------------------------------------------------------------------------
/// Get a number from a Python object (float, int or bool). Returns false if the object is not a number.
inline bool ToDouble(PyObject* object, double& value)
{
  if (!object || !PyNumber_Check(object))
  {
    return false;
  }
  value = PyFloat_AsDouble(object);
  if (PyErr_Occurred())
  {
    PyErr_Clear();
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
/// Returns true if the Python object is true. If the truth value cannot be determined
/// (e.g., for a numpy array) then the error is reported and false is returned.
inline bool IsTrue(PyObject* object)
{
  if (!object)
  {
    return false;
  }
  const int result = PyObject_IsTrue(object);
  if (result < 0)
  {
    ReportError();
    return false;
  }
  return result == 1;
}

//----------------------------------------------------------------------------
/// Convert IO properties to a Python dictionary (new reference).
inline PyObject* ToPythonDict(vtkMRMLIOProperties* properties)
{
  PyObject* dict = PyDict_New();
  if (!properties)
  {
    return dict;
  }
  for (const std::string& name : properties->GetPropertyNames())
  {
    PyObject* value = nullptr;
    if (properties->IsObjectProperty(name.c_str()))
    {
      value = vtkPythonUtil::GetObjectFromPointer(properties->GetObjectProperty(name.c_str()));
    }
    else if (properties->IsStringListProperty(name.c_str()))
    {
      std::vector<std::string> strings = properties->GetStringListProperty(name.c_str());
      value = PyList_New(static_cast<Py_ssize_t>(strings.size()));
      for (size_t i = 0; i < strings.size(); ++i)
      {
        PyList_SET_ITEM(value, static_cast<Py_ssize_t>(i), NewString(strings[i]));
      }
    }
    else if (properties->IsBoolProperty(name.c_str()))
    {
      value = PyBool_FromLong(properties->GetBoolProperty(name.c_str()) ? 1 : 0);
    }
    else
    {
      vtkVariant variant = properties->GetProperty(name.c_str());
      if (variant.IsFloat() || variant.IsDouble())
      {
        value = PyFloat_FromDouble(variant.ToDouble());
      }
      else if (variant.IsNumeric())
      {
        value = PyLong_FromLongLong(variant.ToLongLong());
      }
      else
      {
        value = NewString(variant.ToString());
      }
    }
    if (!value)
    {
      PyErr_Clear();
      continue;
    }
    PyDict_SetItemString(dict, name.c_str(), value);
    Py_DECREF(value);
  }
  return dict;
}

//----------------------------------------------------------------------------
/// Set an attribute of a VTK object's Python wrapper to an empty list
/// (such as "loadedNodes" of the parent object before calling load()).
inline void ResetStringListAttribute(vtkObjectBase* object, const char* attributeName)
{
  if (!Py_IsInitialized())
  {
    return;
  }
  PyObject* pythonObject = vtkPythonUtil::GetObjectFromPointer(object);
  if (!pythonObject)
  {
    PyErr_Clear();
    return;
  }
  PyObject* emptyList = PyList_New(0);
  if (PyObject_SetAttrString(pythonObject, attributeName, emptyList) < 0)
  {
    PyErr_Clear();
  }
  Py_DECREF(emptyList);
  Py_DECREF(pythonObject);
}

//----------------------------------------------------------------------------
/// Get a list of strings from an attribute of a VTK object's Python wrapper
/// (such as "loadedNodes" of the parent object). A single string is accepted, too.
inline std::vector<std::string> GetStringListAttribute(vtkObjectBase* object, const char* attributeName)
{
  std::vector<std::string> values;
  if (!Py_IsInitialized())
  {
    return values;
  }
  PyObject* pythonObject = vtkPythonUtil::GetObjectFromPointer(object);
  if (!pythonObject)
  {
    PyErr_Clear();
    return values;
  }
  if (PyObject_HasAttrString(pythonObject, attributeName))
  {
    PyObject* attribute = PyObject_GetAttrString(pythonObject, attributeName);
    std::string singleValue;
    if (ToString(attribute, singleValue))
    {
      values.push_back(singleValue);
    }
    else
    {
      ToStringList(attribute, values);
    }
    Py_XDECREF(attribute);
  }
  PyErr_Clear();
  Py_DECREF(pythonObject);
  return values;
}

} // namespace vtkSlicerScriptedFileIOUtilities

#endif
