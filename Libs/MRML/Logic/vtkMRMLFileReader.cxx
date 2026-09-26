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

#include "vtkMRMLFileReader.h"

#include "vtkMRMLIOProperties.h"

#include <vtkObjectFactory.h>
#include <vtkStringArray.h>
#include <vtksys/FStream.hxx>

#include <algorithm>
#include <cctype>

vtkStandardNewMacro(vtkMRMLFileReader);

//----------------------------------------------------------------------------
vtkMRMLFileReader::vtkMRMLFileReader()
{
  this->LoadedNodeIDs = vtkSmartPointer<vtkStringArray>::New();
}

//----------------------------------------------------------------------------
vtkMRMLFileReader::~vtkMRMLFileReader() = default;

//----------------------------------------------------------------------------
void vtkMRMLFileReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "ConfidenceForMatchingExtension: " << this->ConfidenceForMatchingExtension << "\n";
  os << indent << "LoadedNodeIDs: " << this->LoadedNodeIDs->GetNumberOfValues() << "\n";
}

//----------------------------------------------------------------------------
double vtkMRMLFileReader::CanLoadFileConfidence(const char* filePath)
{
  if (!filePath || !this->CanLoadFile(filePath))
  {
    return 0.0;
  }
  int longestExtensionMatch = 0;
  this->GetSupportedNameFilters(filePath, &longestExtensionMatch);
  // If longer extension is matched then the confidence that this is a good reader is
  // slightly higher. For example, for "somefile.seg.nrrd", a reader that is specifically
  // for ".seg.nrrd" files get slightly higher confidence than readers that of generic ".nrrd" files.
  return this->ConfidenceForMatchingExtension + 0.01 * longestExtensionMatch;
}

//----------------------------------------------------------------------------
bool vtkMRMLFileReader::CanLoadFile(const char* filePath)
{
  if (!filePath)
  {
    return false;
  }
  return !this->GetSupportedNameFilters(filePath).empty();
}

//----------------------------------------------------------------------------
void vtkMRMLFileReader::GetSupportedNameFilters(const char* filePath, vtkStringArray* nameFilters)
{
  if (!nameFilters)
  {
    return;
  }
  nameFilters->Reset();
  if (!filePath)
  {
    return;
  }
  for (const std::string& nameFilter : this->GetSupportedNameFilters(std::string(filePath)))
  {
    nameFilters->InsertNextValue(nameFilter);
  }
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkMRMLFileReader::GetSupportedNameFilters(const std::string& filePath, int* longestExtensionMatch /*=nullptr*/)
{
  return vtkMRMLFileIOHandler::GetMatchingNameFilters(filePath, this->NameFilters, /*requireReadableFile=*/true, longestExtensionMatch);
}

//----------------------------------------------------------------------------
bool vtkMRMLFileReader::Load(vtkMRMLIOProperties* vtkNotUsed(properties))
{
  // A reader that does its reading elsewhere (in Python, say) is called through its owner.
  this->ClearLoadedNodeIDs();
  return false;
}

//----------------------------------------------------------------------------
std::string vtkMRMLFileReader::ExamineFileList(vtkStringArray* vtkNotUsed(fileList), vtkMRMLIOProperties* vtkNotUsed(ioProperties))
{
  return std::string();
}

//----------------------------------------------------------------------------
vtkStringArray* vtkMRMLFileReader::GetLoadedNodeIDs() const
{
  return this->LoadedNodeIDs;
}

//----------------------------------------------------------------------------
void vtkMRMLFileReader::SetLoadedNodeIDs(vtkStringArray* nodeIDs)
{
  this->LoadedNodeIDs->Reset();
  for (vtkIdType i = 0; nodeIDs && i < nodeIDs->GetNumberOfValues(); ++i)
  {
    this->LoadedNodeIDs->InsertNextValue(nodeIDs->GetValue(i));
  }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLFileReader::SetLoadedNodeIDs(const std::vector<std::string>& nodeIDs)
{
  this->LoadedNodeIDs->Reset();
  for (const std::string& nodeID : nodeIDs)
  {
    this->LoadedNodeIDs->InsertNextValue(nodeID);
  }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLFileReader::AddLoadedNodeID(const char* nodeID)
{
  if (!nodeID)
  {
    return;
  }
  this->LoadedNodeIDs->InsertNextValue(nodeID);
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkMRMLFileReader::ClearLoadedNodeIDs()
{
  if (this->LoadedNodeIDs->GetNumberOfValues() == 0)
  {
    return;
  }
  this->LoadedNodeIDs->Reset();
  this->Modified();
}

//----------------------------------------------------------------------------
std::string vtkMRMLFileReader::ReadFileHeader(const std::string& filePath, size_t maxLength)
{
  vtksys::ifstream file(filePath.c_str(), std::ios::in | std::ios::binary);
  if (!file.is_open())
  {
    return std::string();
  }
  std::string header(maxLength, '\0');
  file.read(&header[0], static_cast<std::streamsize>(maxLength));
  header.resize(static_cast<size_t>(file.gcount()));
  return header;
}

//----------------------------------------------------------------------------
bool vtkMRMLFileReader::EndsWithNoCase(const std::string& text, const std::string& suffix)
{
  if (suffix.size() > text.size())
  {
    return false;
  }
  return std::equal(suffix.rbegin(),
                    suffix.rend(),
                    text.rbegin(),
                    [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
}
