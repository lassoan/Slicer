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

#include "vtkSlicerScriptedFileIOParent.h"

// MRML includes
#include <vtkMRMLFileIOHandler.h>
#include <vtkMRMLFileReader.h>
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLScene.h>

// VTK includes
#include <vtkObjectFactory.h>

vtkStandardNewMacro(vtkSlicerScriptedFileIOParent);

//----------------------------------------------------------------------------
vtkSlicerScriptedFileIOParent::vtkSlicerScriptedFileIOParent() = default;

//----------------------------------------------------------------------------
vtkSlicerScriptedFileIOParent::~vtkSlicerScriptedFileIOParent() = default;

//----------------------------------------------------------------------------
vtkMRMLFileIOHandler* vtkSlicerScriptedFileIOParent::GetFileIOHandler()
{
  return this->FileIOHandler;
}

//----------------------------------------------------------------------------
void vtkSlicerScriptedFileIOParent::SetFileIOHandler(vtkMRMLFileIOHandler* handler)
{
  this->FileIOHandler = handler;
}

//----------------------------------------------------------------------------
vtkMRMLScene* vtkSlicerScriptedFileIOParent::GetScene()
{
  return this->FileIOHandler ? this->FileIOHandler->GetScene() : nullptr;
}

//----------------------------------------------------------------------------
vtkMRMLMessageCollection* vtkSlicerScriptedFileIOParent::userMessages()
{
  return this->FileIOHandler ? this->FileIOHandler->GetUserMessages() : nullptr;
}

//----------------------------------------------------------------------------
std::vector<std::string> vtkSlicerScriptedFileIOParent::supportedNameFilters(const std::string& filePath)
{
  vtkMRMLFileReader* reader = vtkMRMLFileReader::SafeDownCast(this->FileIOHandler);
  if (!reader)
  {
    return std::vector<std::string>();
  }
  return reader->GetSupportedNameFilters(filePath);
}
