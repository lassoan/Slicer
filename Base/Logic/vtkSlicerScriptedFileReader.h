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

#ifndef __vtkSlicerScriptedFileReader_h
#define __vtkSlicerScriptedFileReader_h

// Slicer includes
#include "vtkSlicerBaseLogicExport.h"

// MRML includes
#include <vtkMRMLFileReader.h>

// STD includes
#include <string>

class vtkSlicerScriptedFileIOParent;

/// \brief File reader that is implemented in Python.
///
/// The reader calls methods of a Python class, which is typically named
/// <ModuleName>FileReader and defined in the Python file of a scripted module.
/// The Python class is instantiated with a vtkSlicerScriptedFileIOParent object
/// as argument, which is typically stored as \c self.parent.
///
/// Methods of the Python class:
/// - description(): returns the description of the reader (such as "Volume") - required
/// - fileType(): returns the file type (such as "VolumeFile") - required
/// - extensions(): returns the list of name filters (such as ["Volume (*.nrrd)"]) - required
/// - canLoadFile(filePath): returns True if the reader can load the file - optional
/// - canLoadFileConfidence(filePath): returns the confidence (between 0.0 and 1.0) that
///   the reader can load the file - optional
/// - load(properties): loads the file (properties is a dict, containing at least "fileName")
///   and returns True on success. IDs of loaded nodes are reported by setting
///   \c self.parent.loadedNodes. Messages for the user can be added to \c self.parent.userMessages().
///   - required
/// - getOptionsDescription(description): describes the options of the reader in the
///   provided vtkMRMLIOOptionsDescription object - optional
///
/// \sa vtkSlicerScriptedFileWriter, vtkSlicerScriptedFileIOParent
class VTK_SLICER_BASE_LOGIC_EXPORT vtkSlicerScriptedFileReader : public vtkMRMLFileReader
{
public:
  static vtkSlicerScriptedFileReader* New();
  vtkTypeMacro(vtkSlicerScriptedFileReader, vtkMRMLFileReader);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Instantiate the Python class that implements the reader.
  /// The Python module must be already loaded (it is loaded when the scripted module is loaded).
  /// If \a className is empty then <ModuleName>FileReader is used.
  /// If \a missingClassIsExpected is true then no error is logged if the class is not found.
  /// Returns true on success.
  bool SetPythonSource(const std::string& filePath, const std::string& className = std::string(), bool missingClassIsExpected = true);

  /// Path of the Python file that contains the reader class.
  std::string GetPythonSource() const;

  /// Name of the Python class that implements the reader.
  std::string GetPythonClassName() const;

  /// Object that the Python class received as parent.
  vtkSlicerScriptedFileIOParent* GetParent() const;

  bool CanLoadFile(const char* filePath) override;
  double CanLoadFileConfidence(const char* filePath) override;
  bool Load(vtkMRMLIOProperties* properties) override;
  void GetOptionsDescription(vtkMRMLIOOptionsDescription* description) override;

protected:
  vtkSlicerScriptedFileReader();
  ~vtkSlicerScriptedFileReader() override;

private:
  vtkSlicerScriptedFileReader(const vtkSlicerScriptedFileReader&) = delete;
  void operator=(const vtkSlicerScriptedFileReader&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif
