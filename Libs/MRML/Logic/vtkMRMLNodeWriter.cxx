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

#include "vtkMRMLNodeWriter.h"

#include "vtkMRMLFileIOManager.h"
#include "vtkMRMLIOOptionsDescription.h"
#include "vtkMRMLIOProperties.h"

// MRML includes
#include <vtkMRMLI18N.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLStorableNode.h>
#include <vtkMRMLStorageNode.h>

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>

vtkStandardNewMacro(vtkMRMLNodeWriter);

//----------------------------------------------------------------------------
vtkMRMLNodeWriter::vtkMRMLNodeWriter() = default;

//----------------------------------------------------------------------------
vtkMRMLNodeWriter::~vtkMRMLNodeWriter() = default;

//----------------------------------------------------------------------------
void vtkMRMLNodeWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "SupportUseCompression: " << (this->SupportUseCompression ? "true" : "false") << "\n";
}

//----------------------------------------------------------------------------
bool vtkMRMLNodeWriter::CanWriteObject(vtkObject* object)
{
  if (!vtkMRMLStorableNode::SafeDownCast(object))
  {
    return false;
  }
  return this->Superclass::CanWriteObject(object);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeWriter::GetNameFiltersForObject(vtkObject* object, vtkStringArray* nameFilters)
{
  if (!nameFilters)
  {
    return;
  }
  nameFilters->Reset();
  vtkMRMLStorableNode* storableNode = vtkMRMLStorableNode::SafeDownCast(object);
  if (!storableNode)
  {
    return;
  }
  vtkMRMLStorageNode* storageNode = vtkMRMLFileIOManager::CreateAndAddDefaultStorageNode(storableNode);
  if (!storageNode)
  {
    return;
  }
  vtkStringArray* supportedWriteFileTypes = storageNode->GetSupportedWriteFileTypes();
  for (vtkIdType formatIt = 0; formatIt < supportedWriteFileTypes->GetNumberOfValues(); ++formatIt)
  {
    nameFilters->InsertNextValue(supportedWriteFileTypes->GetValue(formatIt));
  }
}

//----------------------------------------------------------------------------
bool vtkMRMLNodeWriter::Write(vtkMRMLIOProperties* properties)
{
  this->ClearWrittenNodeIDs();
  if (!properties)
  {
    vtkErrorMacro("Write failed: invalid properties");
    return false;
  }
  std::string nodeID = properties->GetStringProperty("nodeID");
  vtkMRMLStorableNode* node = vtkMRMLStorableNode::SafeDownCast(this->GetNodeByID(nodeID.c_str()));
  if (this->CanWriteObjectConfidence(node) <= 0.0)
  {
    return false;
  }
  vtkMRMLStorageNode* storageNode = vtkMRMLFileIOManager::CreateAndAddDefaultStorageNode(node);
  if (!storageNode)
  {
    vtkDebugMacro("No storage node for node " << nodeID);
    return false;
  }
  std::string fileName = properties->GetStringProperty("fileName");
  storageNode->SetFileName(fileName.c_str());
  std::string fileFormat = properties->GetStringProperty("fileFormat");
  if (!fileFormat.empty())
  {
    storageNode->SetWriteFileFormat(fileFormat.c_str());
  }
  storageNode->SetURI(nullptr);
  if (properties->HasProperty("useCompression"))
  {
    storageNode->SetUseCompression(properties->GetIntProperty("useCompression"));
    if (properties->HasProperty("compressionParameter"))
    {
      storageNode->SetCompressionParameter(properties->GetStringProperty("compressionParameter"));
    }
  }
  bool success = storageNode->WriteData(node);
  if (success)
  {
    this->AddWrittenNodeID(node->GetID());
  }
  this->GetUserMessages()->AddMessages(storageNode->GetUserMessages());
  return success;
}

//----------------------------------------------------------------------------
vtkMRMLNode* vtkMRMLNodeWriter::GetNodeByID(const char* id)
{
  if (!this->GetScene() || !id)
  {
    return nullptr;
  }
  return this->GetScene()->GetNodeByID(id);
}

//----------------------------------------------------------------------------
void vtkMRMLNodeWriter::GetOptionsDescription(vtkMRMLIOOptionsDescription* description)
{
  if (!description)
  {
    return;
  }
  vtkMRMLIOProperties* properties = description->GetProperties();
  vtkMRMLStorableNode* storableNode =
    properties ? vtkMRMLStorableNode::SafeDownCast(this->GetNodeByID(properties->GetStringProperty("nodeID").c_str())) : nullptr;
  vtkMRMLStorageNode* storageNode = storableNode ? storableNode->GetStorageNode() : nullptr;
  // If the node does not have a storage node yet then use the default values of the storage node
  // that will be created when writing, so that writing does not change the default compression.
  vtkSmartPointer<vtkMRMLStorageNode> defaultStorageNode;
  if (!storageNode && storableNode)
  {
    defaultStorageNode = vtkSmartPointer<vtkMRMLStorageNode>::Take(storableNode->CreateDefaultStorageNode());
  }
  vtkMRMLStorageNode* storageNodeForDefaults = storageNode ? storageNode : defaultStorageNode.GetPointer();

  description->AddBoolOption("useCompression",
                             vtkMRMLTr("qSlicerNodeWriterOptionsWidget", "Compress"),
                             "",
                             storageNodeForDefaults ? storageNodeForDefaults->GetUseCompression() != 0 : false);
  description->SetOptionEnabled("useCompression", storageNode != nullptr);
  description->SetOptionVisible("useCompression", this->SupportUseCompression);

  std::string defaultCompressionParameter = storageNodeForDefaults ? storageNodeForDefaults->GetCompressionParameter() : std::string();
  description->AddEnumOption("compressionParameter", "", "", vtkVariant(defaultCompressionParameter));
  std::vector<vtkMRMLStorageNode::CompressionPreset> presets;
  if (storageNode)
  {
    presets = storageNode->GetCompressionPresets();
  }
  for (const vtkMRMLStorageNode::CompressionPreset& preset : presets)
  {
    description->AddEnumChoice("compressionParameter", vtkVariant(preset.CompressionParameter), preset.DisplayName);
  }
  description->SetOptionEnabled("compressionParameter", storageNode != nullptr && description->GetBoolOptionValue("useCompression"));
  description->SetOptionVisible("compressionParameter", this->SupportUseCompression && !presets.empty());
}
