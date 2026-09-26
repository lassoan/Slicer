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

#include "vtkMRMLFileWriter.h"

#include "vtkMRMLIOProperties.h"

#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkStringArray.h>

#include <algorithm>

vtkStandardNewMacro(vtkMRMLFileWriter);

//----------------------------------------------------------------------------
vtkMRMLFileWriter::vtkMRMLFileWriter()
{
  this->WrittenNodeIDs = vtkSmartPointer<vtkStringArray>::New();
}

//----------------------------------------------------------------------------
vtkMRMLFileWriter::~vtkMRMLFileWriter() = default;

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "NodeClassNames:";
  for (const std::string& className : this->NodeClassNames)
  {
    os << " " << className;
  }
  os << "\n";
  os << indent << "ConfidenceForMatchingClass: " << this->ConfidenceForMatchingClass << "\n";
}

//----------------------------------------------------------------------------
double vtkMRMLFileWriter::CanWriteObjectConfidence(vtkObject* object)
{
  return this->CanWriteObject(object) ? this->ConfidenceForMatchingClass : 0.0;
}

//----------------------------------------------------------------------------
bool vtkMRMLFileWriter::CanWriteObject(vtkObject* object)
{
  if (!object)
  {
    return false;
  }
  for (const std::string& className : this->NodeClassNames)
  {
    if (object->IsA(className.c_str()))
    {
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::GetNameFiltersForObject(vtkObject* vtkNotUsed(object), vtkStringArray* nameFilters)
{
  this->GetNameFilters(nameFilters);
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkMRMLFileWriter::GetNameFiltersListForObject(vtkObject* object)
{
  vtkNew<vtkStringArray> nameFiltersArray;
  this->GetNameFiltersForObject(object, nameFiltersArray);
  std::vector<std::string> nameFilters;
  for (vtkIdType i = 0; i < nameFiltersArray->GetNumberOfValues(); ++i)
  {
    nameFilters.push_back(nameFiltersArray->GetValue(i));
  }
  return nameFilters;
}

//----------------------------------------------------------------------------
bool vtkMRMLFileWriter::Write(vtkMRMLIOProperties* vtkNotUsed(properties))
{
  // A writer that does its writing elsewhere (in Python, say) is called through its owner.
  this->ClearWrittenNodeIDs();
  return false;
}

//----------------------------------------------------------------------------
vtkStringArray* vtkMRMLFileWriter::GetWrittenNodeIDs() const
{
  return this->WrittenNodeIDs;
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::SetWrittenNodeIDs(vtkStringArray* nodeIDs)
{
  this->WrittenNodeIDs->Reset();
  for (vtkIdType i = 0; nodeIDs && i < nodeIDs->GetNumberOfValues(); ++i)
  {
    this->WrittenNodeIDs->InsertNextValue(nodeIDs->GetValue(i));
  }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::SetWrittenNodeIDs(const std::vector<std::string>& nodeIDs)
{
  this->WrittenNodeIDs->Reset();
  for (const std::string& nodeID : nodeIDs)
  {
    this->WrittenNodeIDs->InsertNextValue(nodeID);
  }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::AddWrittenNodeID(const char* nodeID)
{
  if (!nodeID)
  {
    return;
  }
  this->WrittenNodeIDs->InsertNextValue(nodeID);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::ClearWrittenNodeIDs()
{
  if (this->WrittenNodeIDs->GetNumberOfValues() == 0)
  {
    return;
  }
  this->WrittenNodeIDs->Reset();
  this->Modified();
}

//----------------------------------------------------------------------------
const char* vtkMRMLFileWriter::GetNodeClassName() const
{
  return this->NodeClassNames.empty() ? nullptr : this->NodeClassNames[0].c_str();
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::SetNodeClassName(const char* className)
{
  std::vector<std::string> classNames;
  if (className && className[0])
  {
    classNames.emplace_back(className);
  }
  this->SetNodeClassNames(classNames);
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::SetNodeClassNames(vtkStringArray* classNames)
{
  std::vector<std::string> classNamesVector;
  for (vtkIdType i = 0; classNames && i < classNames->GetNumberOfValues(); ++i)
  {
    classNamesVector.push_back(classNames->GetValue(i));
  }
  this->SetNodeClassNames(classNamesVector);
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::SetNodeClassNames(const std::vector<std::string>& classNames)
{
  if (this->NodeClassNames == classNames)
  {
    return;
  }
  this->NodeClassNames = classNames;
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::GetNodeClassNames(vtkStringArray* classNames) const
{
  if (!classNames)
  {
    return;
  }
  classNames->Reset();
  for (const std::string& className : this->NodeClassNames)
  {
    classNames->InsertNextValue(className);
  }
}

//----------------------------------------------------------------------------
void vtkMRMLFileWriter::AddNodeClassName(const char* className)
{
  if (!className || !className[0])
  {
    return;
  }
  if (std::find(this->NodeClassNames.begin(), this->NodeClassNames.end(), std::string(className)) != this->NodeClassNames.end())
  {
    return;
  }
  this->NodeClassNames.emplace_back(className);
  this->Modified();
}
