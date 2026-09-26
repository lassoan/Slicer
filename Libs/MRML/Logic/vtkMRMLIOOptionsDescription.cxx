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

#include "vtkMRMLIOOptionsDescription.h"

#include "vtkMRMLIOProperties.h"

#include <vtkObjectFactory.h>
#include <vtkStringArray.h>

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

vtkStandardNewMacro(vtkMRMLIOOptionsDescription);

namespace
{
//----------------------------------------------------------------------------
struct Option
{
  std::string Property;
  std::string Type;
  std::string Label;
  std::string ToolTip;
  std::string Widget;
  std::string Separator;
  vtkVariant DefaultValue;
  std::vector<std::string> DefaultListValue;
  bool Enabled{ true };
  bool Visible{ true };
  bool NoneEnabled{ false };
  bool ShowHidden{ false };
  bool HasRange{ false };
  double Minimum{ 0.0 };
  double Maximum{ 0.0 };
  int Decimals{ -1 };
  std::vector<std::pair<vtkVariant, std::string>> Choices;
  std::vector<std::string> NodeClasses;
};

//----------------------------------------------------------------------------
std::string jsonString(const std::string& text)
{
  std::ostringstream out;
  out << '"';
  for (char c : text)
  {
    switch (c)
    {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20)
        {
          char buffer[8];
          snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
          out << buffer;
        }
        else
        {
          // UTF-8 bytes are written as is
          out << c;
        }
    }
  }
  out << '"';
  return out.str();
}

//----------------------------------------------------------------------------
std::string jsonNumber(double value)
{
  if (!std::isfinite(value))
  {
    return "null";
  }
  std::ostringstream out;
  out << std::setprecision(17) << value;
  return out.str();
}

//----------------------------------------------------------------------------
std::string jsonBool(bool value)
{
  return value ? "true" : "false";
}

//----------------------------------------------------------------------------
std::string jsonStringList(const std::vector<std::string>& values)
{
  std::string result = "[";
  for (size_t i = 0; i < values.size(); ++i)
  {
    result += (i > 0 ? ", " : "") + jsonString(values[i]);
  }
  return result + "]";
}

//----------------------------------------------------------------------------
std::string jsonVariant(const vtkVariant& value)
{
  if (!value.IsValid())
  {
    return "null";
  }
  if (value.IsString())
  {
    return jsonString(value.ToString());
  }
  if (value.IsFloat() || value.IsDouble())
  {
    return jsonNumber(value.ToDouble());
  }
  if (value.IsNumeric())
  {
    return std::to_string(value.ToTypeInt64());
  }
  return jsonString(value.ToString());
}

//----------------------------------------------------------------------------
std::string joinStrings(const std::vector<std::string>& values, const std::string& separator)
{
  std::string result;
  for (size_t i = 0; i < values.size(); ++i)
  {
    result += (i > 0 ? separator + " " : "") + values[i];
  }
  return result;
}

} // namespace

//----------------------------------------------------------------------------
class vtkMRMLIOOptionsDescription::vtkInternal
{
public:
  Option* FindOption(const std::string& property)
  {
    for (Option& option : this->Options)
    {
      if (option.Property == property)
      {
        return &option;
      }
    }
    return nullptr;
  }

  Option& AddOption(const char* property, const std::string& type, const std::string& label, const std::string& toolTip)
  {
    const std::string propertyStr = property ? property : "";
    Option* existing = this->FindOption(propertyStr);
    if (existing)
    {
      *existing = Option();
    }
    else
    {
      this->Options.emplace_back();
      existing = &this->Options.back();
    }
    existing->Property = propertyStr;
    existing->Type = type;
    existing->Label = label;
    existing->ToolTip = toolTip;
    return *existing;
  }

  /// Get value specified in properties or defaults. Returns nullptr if not specified.
  vtkMRMLIOProperties* GetSource(const std::string& property)
  {
    if (this->Properties && this->Properties->HasProperty(property.c_str()))
    {
      return this->Properties;
    }
    if (this->Defaults && this->Defaults->HasProperty(property.c_str()))
    {
      return this->Defaults;
    }
    return nullptr;
  }

  /// Effective value of an option
  vtkVariant GetValue(const Option& option)
  {
    vtkMRMLIOProperties* source = this->GetSource(option.Property);
    if (option.Type == "stringList")
    {
      std::vector<std::string> values = source ? source->GetStringListProperty(option.Property.c_str()) : option.DefaultListValue;
      return vtkVariant(joinStrings(values, option.Separator));
    }
    if (!source)
    {
      return option.DefaultValue;
    }
    if (option.Type == "bool")
    {
      return vtkVariant(source->GetBoolProperty(option.Property.c_str()) ? 1 : 0);
    }
    if (option.Type == "int")
    {
      return vtkVariant(source->GetIntProperty(option.Property.c_str()));
    }
    if (option.Type == "double")
    {
      return vtkVariant(source->GetDoubleProperty(option.Property.c_str()));
    }
    if (option.Type == "enum" && option.DefaultValue.IsNumeric())
    {
      // keep the value type of the choices
      if (option.DefaultValue.IsFloat() || option.DefaultValue.IsDouble())
      {
        return vtkVariant(source->GetDoubleProperty(option.Property.c_str()));
      }
      return vtkVariant(source->GetIntProperty(option.Property.c_str()));
    }
    return vtkVariant(source->GetStringProperty(option.Property.c_str()));
  }

  std::vector<std::string> GetListValue(const Option& option)
  {
    vtkMRMLIOProperties* source = this->GetSource(option.Property);
    return source ? source->GetStringListProperty(option.Property.c_str()) : option.DefaultListValue;
  }

  std::vector<Option> Options;
  vtkSmartPointer<vtkMRMLIOProperties> Properties;
  vtkSmartPointer<vtkMRMLIOProperties> Defaults;
};

//----------------------------------------------------------------------------
vtkMRMLIOOptionsDescription::vtkMRMLIOOptionsDescription()
{
  this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkMRMLIOOptionsDescription::~vtkMRMLIOOptionsDescription()
{
  delete this->Internal;
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Options: " << this->ToJSON() << "\n";
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::SetProperties(vtkMRMLIOProperties* properties)
{
  this->Internal->Properties = properties;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLIOProperties* vtkMRMLIOOptionsDescription::GetProperties()
{
  return this->Internal->Properties;
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::SetDefaults(vtkMRMLIOProperties* defaults)
{
  this->Internal->Defaults = defaults;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkMRMLIOProperties* vtkMRMLIOOptionsDescription::GetDefaults()
{
  return this->Internal->Defaults;
}

//----------------------------------------------------------------------------
std::string vtkMRMLIOOptionsDescription::GetFileName()
{
  std::vector<std::string> fileNames = this->GetFileNames();
  return fileNames.empty() ? std::string() : fileNames[0];
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkMRMLIOOptionsDescription::GetFileNames()
{
  if (!this->Internal->Properties)
  {
    return std::vector<std::string>();
  }
  std::vector<std::string> fileNames = this->Internal->Properties->GetStringListProperty("fileName");
  if (fileNames.empty() || (fileNames.size() == 1 && fileNames[0].empty()))
  {
    fileNames = this->Internal->Properties->GetStringListProperty("fileNames");
  }
  return fileNames;
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddBoolOption(const char* property, const std::string& label, const std::string& toolTip, bool defaultValue)
{
  Option& option = this->Internal->AddOption(property, "bool", label, toolTip);
  option.DefaultValue = vtkVariant(defaultValue ? 1 : 0);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddIntOption(const char* property,
                                               const std::string& label,
                                               const std::string& toolTip,
                                               int defaultValue,
                                               int minimum,
                                               int maximum)
{
  Option& option = this->Internal->AddOption(property, "int", label, toolTip);
  option.DefaultValue = vtkVariant(defaultValue);
  option.HasRange = true;
  option.Minimum = minimum;
  option.Maximum = maximum;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddDoubleOption(const char* property,
                                                  const std::string& label,
                                                  const std::string& toolTip,
                                                  double defaultValue,
                                                  double minimum,
                                                  double maximum,
                                                  int decimals)
{
  Option& option = this->Internal->AddOption(property, "double", label, toolTip);
  option.DefaultValue = vtkVariant(defaultValue);
  option.HasRange = true;
  option.Minimum = minimum;
  option.Maximum = maximum;
  option.Decimals = decimals;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddStringOption(const char* property, const std::string& label, const std::string& toolTip, const std::string& defaultValue)
{
  Option& option = this->Internal->AddOption(property, "string", label, toolTip);
  option.DefaultValue = vtkVariant(defaultValue);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddStringListOption(const char* property,
                                                      const std::string& label,
                                                      const std::string& toolTip,
                                                      const std::vector<std::string>& defaultValue,
                                                      const std::string& separator /*=";"*/)
{
  Option& option = this->Internal->AddOption(property, "stringList", label, toolTip);
  option.DefaultListValue = defaultValue;
  option.Separator = separator;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddEnumOption(const char* property, const std::string& label, const std::string& toolTip, const vtkVariant& defaultValue)
{
  Option& option = this->Internal->AddOption(property, "enum", label, toolTip);
  option.DefaultValue = defaultValue;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddEnumChoice(const char* property, const vtkVariant& value, const std::string& label)
{
  Option* option = this->Internal->FindOption(property ? property : "");
  if (!option || option->Type != "enum")
  {
    vtkErrorMacro("AddEnumChoice failed: enum option " << (property ? property : "(null)") << " is not found");
    return;
  }
  option->Choices.emplace_back(value, label);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::AddNodeOption(const char* property,
                                                const std::string& label,
                                                const std::string& toolTip,
                                                const std::string& defaultNodeID,
                                                const std::vector<std::string>& nodeClasses,
                                                bool noneEnabled)
{
  Option& option = this->Internal->AddOption(property, "node", label, toolTip);
  option.DefaultValue = vtkVariant(defaultNodeID);
  option.NodeClasses = nodeClasses;
  option.NoneEnabled = noneEnabled;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::SetOptionEnabled(const char* property, bool enabled)
{
  Option* option = this->Internal->FindOption(property ? property : "");
  if (!option)
  {
    vtkErrorMacro("SetOptionEnabled failed: option " << (property ? property : "(null)") << " is not found");
    return;
  }
  option->Enabled = enabled;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::SetOptionVisible(const char* property, bool visible)
{
  Option* option = this->Internal->FindOption(property ? property : "");
  if (!option)
  {
    vtkErrorMacro("SetOptionVisible failed: option " << (property ? property : "(null)") << " is not found");
    return;
  }
  option->Visible = visible;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::SetOptionWidget(const char* property, const std::string& widget)
{
  Option* option = this->Internal->FindOption(property ? property : "");
  if (!option)
  {
    vtkErrorMacro("SetOptionWidget failed: option " << (property ? property : "(null)") << " is not found");
    return;
  }
  option->Widget = widget;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::SetOptionShowHidden(const char* property, bool showHidden)
{
  Option* option = this->Internal->FindOption(property ? property : "");
  if (!option)
  {
    vtkErrorMacro("SetOptionShowHidden failed: option " << (property ? property : "(null)") << " is not found");
    return;
  }
  option->ShowHidden = showHidden;
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkMRMLIOOptionsDescription::GetNumberOfOptions()
{
  return static_cast<int>(this->Internal->Options.size());
}

//----------------------------------------------------------------------------
bool vtkMRMLIOOptionsDescription::HasOption(const char* property)
{
  return this->Internal->FindOption(property ? property : "") != nullptr;
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::RemoveAllOptions()
{
  this->Internal->Options.clear();
  this->Modified();
}

//----------------------------------------------------------------------------
vtkVariant vtkMRMLIOOptionsDescription::GetOptionValue(const char* property)
{
  Option* option = this->Internal->FindOption(property ? property : "");
  if (!option)
  {
    return vtkVariant();
  }
  return this->Internal->GetValue(*option);
}

//----------------------------------------------------------------------------
bool vtkMRMLIOOptionsDescription::GetBoolOptionValue(const char* property, bool defaultValue /*=false*/)
{
  vtkVariant value = this->GetOptionValue(property);
  if (!value.IsValid())
  {
    return defaultValue;
  }
  return value.ToInt() != 0;
}

//----------------------------------------------------------------------------
void vtkMRMLIOOptionsDescription::GetOptionValues(vtkMRMLIOProperties* values)
{
  if (!values)
  {
    return;
  }
  for (const Option& option : this->Internal->Options)
  {
    const char* name = option.Property.c_str();
    if (option.Type == "stringList")
    {
      std::vector<std::string> listValue = this->Internal->GetListValue(option);
      if (listValue.empty() || (listValue.size() == 1 && listValue[0].empty()))
      {
        continue;
      }
      values->SetStringListProperty(name, listValue);
      continue;
    }
    vtkVariant value = this->Internal->GetValue(option);
    if (option.Type == "bool")
    {
      values->SetBoolProperty(name, value.ToInt() != 0);
    }
    else if ((option.Type == "string" || option.Type == "node") && value.ToString().empty())
    {
      continue;
    }
    else
    {
      values->SetProperty(name, value);
    }
  }
}

//----------------------------------------------------------------------------
std::string vtkMRMLIOOptionsDescription::ToJSON()
{
  std::ostringstream json;
  json << "{\"options\": [";
  bool firstOption = true;
  for (const Option& option : this->Internal->Options)
  {
    json << (firstOption ? "\n" : ",\n") << "  {";
    firstOption = false;
    json << "\"property\": " << jsonString(option.Property);
    json << ", \"type\": " << jsonString(option.Type);
    json << ", \"label\": " << jsonString(option.Label);
    json << ", \"toolTip\": " << jsonString(option.ToolTip);
    if (option.Type == "stringList")
    {
      json << ", \"value\": " << jsonStringList(this->Internal->GetListValue(option));
      json << ", \"separator\": " << jsonString(option.Separator);
    }
    else if (option.Type == "bool")
    {
      json << ", \"value\": " << jsonBool(this->Internal->GetValue(option).ToInt() != 0);
    }
    else
    {
      json << ", \"value\": " << jsonVariant(this->Internal->GetValue(option));
    }
    json << ", \"enabled\": " << jsonBool(option.Enabled);
    json << ", \"visible\": " << jsonBool(option.Visible);
    if (!option.Widget.empty())
    {
      json << ", \"widget\": " << jsonString(option.Widget);
    }
    if (option.HasRange)
    {
      json << ", \"minimum\": " << jsonNumber(option.Minimum);
      json << ", \"maximum\": " << jsonNumber(option.Maximum);
      if (option.Decimals >= 0)
      {
        json << ", \"decimals\": " << option.Decimals;
      }
    }
    if (option.Type == "enum")
    {
      json << ", \"choices\": [";
      for (size_t i = 0; i < option.Choices.size(); ++i)
      {
        json << (i > 0 ? ", " : "") << "{\"value\": " << jsonVariant(option.Choices[i].first) << ", \"label\": " << jsonString(option.Choices[i].second) << "}";
      }
      json << "]";
    }
    if (option.Type == "node")
    {
      json << ", \"nodeClasses\": " << jsonStringList(option.NodeClasses);
      json << ", \"noneEnabled\": " << jsonBool(option.NoneEnabled);
      json << ", \"showHidden\": " << jsonBool(option.ShowHidden);
    }
    json << "}";
  }
  json << (firstOption ? "" : "\n") << "]}";
  return json.str();
}
