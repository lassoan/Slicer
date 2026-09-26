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

#ifndef __vtkMRMLIOOptionsDescription_h
#define __vtkMRMLIOOptionsDescription_h

#include "vtkMRMLLogicExport.h"

#include <vtkObject.h>
#include <vtkSmartPointer.h>
#include <vtkVariant.h>

#include <string>
#include <vector>

class vtkMRMLIOProperties;
class vtkStringArray;

/// Description of the options of a file reader or writer, which user interfaces can use to
/// display widgets for setting the options (in desktop Slicer: qSlicerGenericIOOptionsWidget).
///
/// Each option sets one property of the reader or writer (such as "labelmap" or "colorNodeID").
/// Readers and writers describe their options in vtkMRMLFileIOHandler::GetOptionsDescription(), by
/// calling the Add...Option() methods. Labels and tool tips must be translated using vtkMRMLTr()
/// with string literals, so that the translatable text can be extracted from the source code.
///
/// The value of each option is determined in this order:
/// 1. the value in Properties (set by the caller: for example, values that the user edited),
/// 2. the value in Defaults (set by the application: for example, from user settings),
/// 3. the default value specified when the option is added.
/// Readers and writers can use GetOptionValue() to compute defaults of other options (for example,
/// the default color node depends on whether the volume is loaded as a labelmap).
///
/// Properties also contain the context: "fileName" (string or list of strings) for readers,
/// "nodeID" for writers (empty if writing the scene).
///
/// The description can be serialized to JSON (ToJSON()):
/// \code{.json}
/// { "options": [
///   { "property": "labelmap", "type": "bool", "label": "LabelMap", "toolTip": "...", "value": false,
///     "enabled": true, "visible": true },
///   { "property": "colorNodeID", "type": "node", "label": "", "toolTip": "...", "value": "vtkMRMLColorTableNodeGrey",
///     "nodeClasses": ["vtkMRMLColorTableNode"], "noneEnabled": false, "showHidden": false, "widget": "colorTable",
///     "enabled": true, "visible": true },
///   { "property": "coordinateSystem", "type": "enum", "label": "Coordinate system:", "value": -1,
///     "choices": [ {"value": -1, "label": "Default"}, {"value": 0, "label": "RAS"} ], ... }
/// ]}
/// \endcode
/// Option types: "bool", "int", "double" (optional "minimum", "maximum", "decimals"), "string",
/// "stringList" (list of strings edited as text, items separated by "separator"), "enum" (one of "choices"),
/// "node" (ID of a MRML node of "nodeClasses"). "widget" is an optional hint for choosing a specific widget
/// (for example, "colorTable" for color nodes).
class VTK_MRML_LOGIC_EXPORT vtkMRMLIOOptionsDescription : public vtkObject
{
public:
  static vtkMRMLIOOptionsDescription* New();
  vtkTypeMacro(vtkMRMLIOOptionsDescription, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Context and current values of options (for example, edited by the user). These values take precedence over defaults.
  void SetProperties(vtkMRMLIOProperties* properties);
  vtkMRMLIOProperties* GetProperties();

  /// Default values specified by the application (for example, from user settings).
  void SetDefaults(vtkMRMLIOProperties* defaults);
  vtkMRMLIOProperties* GetDefaults();

  /// Get the file name from the properties. If multiple file names are specified then the first one is returned.
  std::string GetFileName();
  /// Get all file names from the properties.
  std::vector<std::string> GetFileNames();

  //@{
  /// Add an option. If an option already exists for the property then it is replaced.
  void AddBoolOption(const char* property, const std::string& label, const std::string& toolTip, bool defaultValue);
  void AddIntOption(const char* property, const std::string& label, const std::string& toolTip, int defaultValue, int minimum, int maximum);
  void AddDoubleOption(const char* property,
                       const std::string& label,
                       const std::string& toolTip,
                       double defaultValue,
                       double minimum,
                       double maximum,
                       int decimals);
  void AddStringOption(const char* property, const std::string& label, const std::string& toolTip, const std::string& defaultValue);
  void AddStringListOption(const char* property,
                           const std::string& label,
                           const std::string& toolTip,
                           const std::vector<std::string>& defaultValue,
                           const std::string& separator = ";");
  /// Add an option that can have one of the values added by AddEnumChoice().
  void AddEnumOption(const char* property, const std::string& label, const std::string& toolTip, const vtkVariant& defaultValue);
  void AddEnumChoice(const char* property, const vtkVariant& value, const std::string& label);
  /// Add an option for selecting a MRML node. The value is the node ID.
  void AddNodeOption(const char* property,
                     const std::string& label,
                     const std::string& toolTip,
                     const std::string& defaultNodeID,
                     const std::vector<std::string>& nodeClasses,
                     bool noneEnabled);
  //@}

  //@{
  /// Set additional attributes of an option.
  void SetOptionEnabled(const char* property, bool enabled);
  void SetOptionVisible(const char* property, bool visible);
  /// Hint for choosing a specific widget (for example "colorTable").
  void SetOptionWidget(const char* property, const std::string& widget);
  /// Show nodes that are hidden from editors in node selectors.
  void SetOptionShowHidden(const char* property, bool showHidden);
  //@}

  /// Number of options.
  int GetNumberOfOptions();
  bool HasOption(const char* property);
  /// Remove all options.
  void RemoveAllOptions();

  /// Effective value of an option (from properties, defaults, or the default specified for the option).
  /// Returns invalid variant if the option is not found. For string list options, items are joined using the separator.
  vtkVariant GetOptionValue(const char* property);
  /// Effective value of a boolean option. Returns defaultValue if the option is not found.
  bool GetBoolOptionValue(const char* property, bool defaultValue = false);

  /// Get the effective values of all options. Options that have empty string or string list values are not included.
  void GetOptionValues(vtkMRMLIOProperties* values);

  /// Get the description of all options as JSON.
  std::string ToJSON();

protected:
  vtkMRMLIOOptionsDescription();
  ~vtkMRMLIOOptionsDescription() override;

private:
  vtkMRMLIOOptionsDescription(const vtkMRMLIOOptionsDescription&) = delete;
  void operator=(const vtkMRMLIOOptionsDescription&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif
