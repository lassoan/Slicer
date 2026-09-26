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

==============================================================================*/

#ifndef __vtkMRMLIOProperties_h
#define __vtkMRMLIOProperties_h

#include "vtkMRMLLogicExport.h"

#include <vtkObject.h>
#include <vtkVariant.h>

#include <map>
#include <string>
#include <vector>

class vtkStringArray;

/// What a file reader or writer is asked to do: the file name, the node name, and whatever else
/// the caller wants to say.
///
/// This is qSlicerIO::IOProperties (a QVariantMap) without Qt. The names are the ones Slicer
/// already uses - "fileName", "name", "nodeID", "singleFile", "show" - so that the readers and
/// writers of modules, which are given these as a Python dictionary, are unaffected.
class VTK_MRML_LOGIC_EXPORT vtkMRMLIOProperties : public vtkObject
{
public:
  static vtkMRMLIOProperties* New();
  vtkTypeMacro(vtkMRMLIOProperties, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Set a property. A value that is already there is replaced.
  void SetProperty(const char* name, const vtkVariant& value);
  void SetStringProperty(const char* name, const char* value);
  void SetIntProperty(const char* name, int value);
  void SetDoubleProperty(const char* name, double value);
  void SetBoolProperty(const char* name, bool value);

  /// Set a property to a list of strings (such as "fileNames"). The strings are copied.
  void SetStringListProperty(const char* name, vtkStringArray* values);
  void SetStringListProperty(const char* name, const std::vector<std::string>& values);

  /// Set a property to an object (such as the "screenShot" image of a scene). The object is referenced.
  void SetObjectProperty(const char* name, vtkObject* value);

  /// The value of a property, or an invalid variant if it is not there.
  vtkVariant GetProperty(const char* name) const;
  /// The value of a property as a string; *defaultValue* if it is not there.
  std::string GetStringProperty(const char* name, const std::string& defaultValue = "") const;
  int GetIntProperty(const char* name, int defaultValue = 0) const;
  double GetDoubleProperty(const char* name, double defaultValue = 0.0) const;
  bool GetBoolProperty(const char* name, bool defaultValue = false) const;
  /// The value of a property as a list of strings. A single string value is returned as a list of one item.
  /// Returns false if the property is not there.
  bool GetStringListProperty(const char* name, vtkStringArray* values) const;
  std::vector<std::string> GetStringListProperty(const char* name) const;
  /// The value of an object property, or nullptr if it is not there or not an object.
  vtkObject* GetObjectProperty(const char* name) const;

  /// Whether the property was set as a boolean (SetBoolProperty), a list of strings
  /// (SetStringListProperty), or an object (SetObjectProperty).
  /// This allows converting the properties to other representations (such as a Python dictionary) without losing the type.
  bool IsBoolProperty(const char* name) const;
  bool IsStringListProperty(const char* name) const;
  bool IsObjectProperty(const char* name) const;

  bool HasProperty(const char* name) const;
  void RemoveProperty(const char* name);
  void RemoveAllProperties();

  /// The names of the properties that are set, in the order they were first set.
  void GetPropertyNames(vtkStringArray* names) const;
  std::vector<std::string> GetPropertyNames() const;

  /// Replace these properties with the properties of another set.
  void Copy(vtkMRMLIOProperties* source);

  /// Add the properties of another set to these, replacing values that are already set.
  void Update(vtkMRMLIOProperties* source);

protected:
  vtkMRMLIOProperties();
  ~vtkMRMLIOProperties() override;

private:
  vtkMRMLIOProperties(const vtkMRMLIOProperties&) = delete;
  void operator=(const vtkMRMLIOProperties&) = delete;

  std::map<std::string, vtkVariant> Properties;
  std::vector<std::string> Order;
  /// Names of properties that were set as boolean values
  std::vector<std::string> BoolProperties;
};

#endif
