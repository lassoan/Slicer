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

#include "vtkMRMLIOProperties.h"

#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkStringArray.h>

#include <algorithm>
#include <cctype>

vtkStandardNewMacro(vtkMRMLIOProperties);

//----------------------------------------------------------------------------
vtkMRMLIOProperties::vtkMRMLIOProperties() = default;

//----------------------------------------------------------------------------
vtkMRMLIOProperties::~vtkMRMLIOProperties() = default;

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  for (const std::string& name : this->Order)
  {
    os << indent << name << ": ";
    if (this->IsStringListProperty(name.c_str()))
    {
      for (const std::string& value : this->GetStringListProperty(name.c_str()))
      {
        os << "[" << value << "]";
      }
    }
    else if (this->IsObjectProperty(name.c_str()))
    {
      vtkObject* object = this->GetObjectProperty(name.c_str());
      os << (object ? object->GetClassName() : "(none)");
    }
    else
    {
      os << this->Properties.at(name).ToString();
    }
    os << "\n";
  }
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetProperty(const char* name, const vtkVariant& value)
{
  if (!name)
  {
    vtkErrorMacro("SetProperty: a property needs a name");
    return;
  }
  if (this->Properties.find(name) == this->Properties.end())
  {
    this->Order.emplace_back(name);
  }
  this->Properties[name] = value;
  this->BoolProperties.erase(std::remove(this->BoolProperties.begin(), this->BoolProperties.end(), std::string(name)), this->BoolProperties.end());
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetStringProperty(const char* name, const char* value)
{
  this->SetProperty(name, vtkVariant(value ? value : ""));
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetIntProperty(const char* name, int value)
{
  this->SetProperty(name, vtkVariant(value));
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetDoubleProperty(const char* name, double value)
{
  this->SetProperty(name, vtkVariant(value));
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetBoolProperty(const char* name, bool value)
{
  this->SetProperty(name, vtkVariant(value ? 1 : 0));
  if (name)
  {
    this->BoolProperties.emplace_back(name);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetStringListProperty(const char* name, vtkStringArray* values)
{
  vtkNew<vtkStringArray> copiedValues;
  if (values)
  {
    copiedValues->DeepCopy(values);
  }
  this->SetProperty(name, vtkVariant(copiedValues.GetPointer()));
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetStringListProperty(const char* name, const std::vector<std::string>& values)
{
  vtkNew<vtkStringArray> valuesArray;
  for (const std::string& value : values)
  {
    valuesArray->InsertNextValue(value);
  }
  this->SetProperty(name, vtkVariant(valuesArray.GetPointer()));
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::SetObjectProperty(const char* name, vtkObject* value)
{
  this->SetProperty(name, vtkVariant(value));
}

//----------------------------------------------------------------------------
vtkVariant vtkMRMLIOProperties::GetProperty(const char* name) const
{
  if (!name)
  {
    return vtkVariant();
  }
  std::map<std::string, vtkVariant>::const_iterator found = this->Properties.find(name);
  return found == this->Properties.end() ? vtkVariant() : found->second;
}

//----------------------------------------------------------------------------
std::string vtkMRMLIOProperties::GetStringProperty(const char* name, const std::string& defaultValue) const
{
  vtkVariant value = this->GetProperty(name);
  if (!value.IsValid())
  {
    return defaultValue;
  }
  if (value.IsVTKObject())
  {
    // A list of one string is the same as that string
    vtkStringArray* values = vtkStringArray::SafeDownCast(value.ToVTKObject());
    return (values && values->GetNumberOfValues() == 1) ? values->GetValue(0) : std::string();
  }
  return value.ToString();
}

//----------------------------------------------------------------------------
int vtkMRMLIOProperties::GetIntProperty(const char* name, int defaultValue) const
{
  vtkVariant value = this->GetProperty(name);
  if (!value.IsValid() || value.IsVTKObject())
  {
    return defaultValue;
  }
  bool valid = false;
  int intValue = value.ToInt(&valid);
  if (!valid)
  {
    // floating-point value given as string
    double doubleValue = value.ToDouble(&valid);
    intValue = static_cast<int>(doubleValue);
  }
  return valid ? intValue : defaultValue;
}

//----------------------------------------------------------------------------
double vtkMRMLIOProperties::GetDoubleProperty(const char* name, double defaultValue) const
{
  vtkVariant value = this->GetProperty(name);
  if (!value.IsValid() || value.IsVTKObject())
  {
    return defaultValue;
  }
  bool valid = false;
  double doubleValue = value.ToDouble(&valid);
  return valid ? doubleValue : defaultValue;
}

//----------------------------------------------------------------------------
bool vtkMRMLIOProperties::GetBoolProperty(const char* name, bool defaultValue) const
{
  vtkVariant value = this->GetProperty(name);
  if (!value.IsValid() || value.IsVTKObject())
  {
    return defaultValue;
  }
  if (value.IsString())
  {
    std::string text = value.ToString();
    std::transform(text.begin(), text.end(), text.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return !(text == "" || text == "0" || text == "false" || text == "no");
  }
  return value.ToDouble() != 0.0;
}

//----------------------------------------------------------------------------
bool vtkMRMLIOProperties::GetStringListProperty(const char* name, vtkStringArray* values) const
{
  if (!values)
  {
    return false;
  }
  values->Reset();
  if (!this->HasProperty(name))
  {
    return false;
  }
  for (const std::string& value : this->GetStringListProperty(name))
  {
    values->InsertNextValue(value);
  }
  return true;
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkMRMLIOProperties::GetStringListProperty(const char* name) const
{
  std::vector<std::string> result;
  vtkVariant value = this->GetProperty(name);
  if (!value.IsValid())
  {
    return result;
  }
  if (value.IsVTKObject())
  {
    vtkStringArray* values = vtkStringArray::SafeDownCast(value.ToVTKObject());
    for (vtkIdType i = 0; values && i < values->GetNumberOfValues(); ++i)
    {
      result.push_back(values->GetValue(i));
    }
    return result;
  }
  result.push_back(value.ToString());
  return result;
}

//----------------------------------------------------------------------------
vtkObject* vtkMRMLIOProperties::GetObjectProperty(const char* name) const
{
  vtkVariant value = this->GetProperty(name);
  if (!value.IsVTKObject())
  {
    return nullptr;
  }
  return vtkObject::SafeDownCast(value.ToVTKObject());
}

//----------------------------------------------------------------------------
bool vtkMRMLIOProperties::IsBoolProperty(const char* name) const
{
  return name && std::find(this->BoolProperties.begin(), this->BoolProperties.end(), std::string(name)) != this->BoolProperties.end();
}

//----------------------------------------------------------------------------
bool vtkMRMLIOProperties::IsStringListProperty(const char* name) const
{
  vtkVariant value = this->GetProperty(name);
  return value.IsVTKObject() && vtkStringArray::SafeDownCast(value.ToVTKObject()) != nullptr;
}

//----------------------------------------------------------------------------
bool vtkMRMLIOProperties::IsObjectProperty(const char* name) const
{
  vtkVariant value = this->GetProperty(name);
  return value.IsVTKObject() && vtkStringArray::SafeDownCast(value.ToVTKObject()) == nullptr;
}

//----------------------------------------------------------------------------
bool vtkMRMLIOProperties::HasProperty(const char* name) const
{
  return name && this->Properties.find(name) != this->Properties.end();
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::RemoveProperty(const char* name)
{
  if (!name || !this->HasProperty(name))
  {
    return;
  }
  this->Properties.erase(name);
  this->Order.erase(std::remove(this->Order.begin(), this->Order.end(), std::string(name)), this->Order.end());
  this->BoolProperties.erase(std::remove(this->BoolProperties.begin(), this->BoolProperties.end(), std::string(name)), this->BoolProperties.end());
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::RemoveAllProperties()
{
  if (this->Properties.empty())
  {
    return;
  }
  this->Properties.clear();
  this->Order.clear();
  this->BoolProperties.clear();
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::GetPropertyNames(vtkStringArray* names) const
{
  if (!names)
  {
    return;
  }
  names->Reset();
  for (const std::string& name : this->Order)
  {
    names->InsertNextValue(name);
  }
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkMRMLIOProperties::GetPropertyNames() const
{
  return this->Order;
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::Copy(vtkMRMLIOProperties* source)
{
  if (!source || source == this)
  {
    return;
  }
  this->RemoveAllProperties();
  this->Update(source);
}

//----------------------------------------------------------------------------
void vtkMRMLIOProperties::Update(vtkMRMLIOProperties* source)
{
  if (!source || source == this)
  {
    return;
  }
  for (const std::string& name : source->Order)
  {
    if (source->IsStringListProperty(name.c_str()))
    {
      this->SetStringListProperty(name.c_str(), source->GetStringListProperty(name.c_str()));
    }
    else if (source->IsBoolProperty(name.c_str()))
    {
      this->SetBoolProperty(name.c_str(), source->GetBoolProperty(name.c_str()));
    }
    else
    {
      this->SetProperty(name.c_str(), source->Properties.at(name));
    }
  }
}
