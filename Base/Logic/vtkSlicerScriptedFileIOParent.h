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

#ifndef __vtkSlicerScriptedFileIOParent_h
#define __vtkSlicerScriptedFileIOParent_h

// Slicer includes
#include "vtkSlicerBaseLogicExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkWeakPointer.h>

// STD includes
#include <string>
#include <vector>

class vtkMRMLFileIOHandler;
class vtkMRMLMessageCollection;
class vtkMRMLScene;

/// \brief Object that is passed as \c parent to scripted file readers and writers.
///
/// Scripted file readers and writers (Python classes named <ModuleName>FileReader
/// and <ModuleName>FileWriter) receive this object in their constructor and
/// typically store it as \c self.parent. It provides the same API that scripted
/// readers and writers could use when they were implemented using Qt:
/// - \c self.parent.userMessages(): messages to display to the user
/// - \c self.parent.supportedNameFilters(filePath): name filters that match the file
/// - \c self.parent.loadedNodes = [...]: IDs of the loaded nodes (readers)
/// - \c self.parent.writtenNodes = [...]: IDs of the written nodes (writers)
///
/// The file IO handler is referenced by a weak pointer to avoid a reference loop
/// between the handler (that owns the Python object) and the Python object.
///
/// \sa vtkSlicerScriptedFileReader, vtkSlicerScriptedFileWriter
class VTK_SLICER_BASE_LOGIC_EXPORT vtkSlicerScriptedFileIOParent : public vtkObject
{
public:
  static vtkSlicerScriptedFileIOParent* New();
  vtkTypeMacro(vtkSlicerScriptedFileIOParent, vtkObject);

  /// Reader or writer that uses the Python class.
  vtkMRMLFileIOHandler* GetFileIOHandler();
  void SetFileIOHandler(vtkMRMLFileIOHandler* handler);

  /// Scene where nodes are loaded to or saved from.
  vtkMRMLScene* GetScene();

  /// Messages that are displayed to the user after reading or writing is completed.
  vtkMRMLMessageCollection* userMessages();

  /// Return the name filters of the reader that match the file path (empty if the reader cannot load the file).
  std::vector<std::string> supportedNameFilters(const std::string& filePath);

protected:
  vtkSlicerScriptedFileIOParent();
  ~vtkSlicerScriptedFileIOParent() override;

  vtkWeakPointer<vtkMRMLFileIOHandler> FileIOHandler;

private:
  vtkSlicerScriptedFileIOParent(const vtkSlicerScriptedFileIOParent&) = delete;
  void operator=(const vtkSlicerScriptedFileIOParent&) = delete;
};

#endif
