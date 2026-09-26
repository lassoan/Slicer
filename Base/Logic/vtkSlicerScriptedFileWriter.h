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

#ifndef __vtkSlicerScriptedFileWriter_h
#define __vtkSlicerScriptedFileWriter_h

// Slicer includes
#include "vtkSlicerBaseLogicExport.h"

// MRML includes
#include <vtkMRMLFileWriter.h>

// STD includes
#include <string>

class vtkSlicerScriptedFileIOParent;

/// \brief File writer that is implemented in Python.
///
/// The writer calls methods of a Python class, which is typically named
/// <ModuleName>FileWriter and defined in the Python file of a scripted module.
/// The Python class is instantiated with a vtkSlicerScriptedFileIOParent object
/// as argument, which is typically stored as \c self.parent.
///
/// Methods of the Python class:
/// - description(): returns the description of the writer (such as "Volume") - required
/// - fileType(): returns the file type (such as "VolumeFile") - required
/// - extensions(object): returns the list of name filters (such as ["Volume (*.nrrd)"])
///   that can be used for writing the object - required
/// - canWriteObject(object): returns True if the writer can write the object - optional
/// - canWriteObjectConfidence(object): returns the confidence (between 0.0 and 1.0) that
///   the writer can write the object - optional
/// - write(properties): writes the node (properties is a dict, containing at least "nodeID"
///   and "fileName") and returns True on success. IDs of written nodes are reported by setting
///   \c self.parent.writtenNodes. Messages for the user can be added to \c self.parent.userMessages().
///   - required
/// - getOptionsDescription(description): describes the options of the writer in the
///   provided vtkMRMLIOOptionsDescription object - optional
///
/// \sa vtkSlicerScriptedFileReader, vtkSlicerScriptedFileIOParent
class VTK_SLICER_BASE_LOGIC_EXPORT vtkSlicerScriptedFileWriter : public vtkMRMLFileWriter
{
public:
  static vtkSlicerScriptedFileWriter* New();
  vtkTypeMacro(vtkSlicerScriptedFileWriter, vtkMRMLFileWriter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Instantiate the Python class that implements the writer.
  /// The Python module must be already loaded (it is loaded when the scripted module is loaded).
  /// If \a className is empty then <ModuleName>FileWriter is used.
  /// If \a missingClassIsExpected is true then no error is logged if the class is not found.
  /// Returns true on success.
  bool SetPythonSource(const std::string& filePath, const std::string& className = std::string(), bool missingClassIsExpected = true);

  /// Path of the Python file that contains the writer class.
  std::string GetPythonSource() const;

  /// Name of the Python class that implements the writer.
  std::string GetPythonClassName() const;

  /// Object that the Python class received as parent.
  vtkSlicerScriptedFileIOParent* GetParent() const;

  bool CanWriteObject(vtkObject* object) override;
  double CanWriteObjectConfidence(vtkObject* object) override;
  void GetNameFiltersForObject(vtkObject* object, vtkStringArray* nameFilters) override;
  bool Write(vtkMRMLIOProperties* properties) override;
  void GetOptionsDescription(vtkMRMLIOOptionsDescription* description) override;

protected:
  vtkSlicerScriptedFileWriter();
  ~vtkSlicerScriptedFileWriter() override;

private:
  vtkSlicerScriptedFileWriter(const vtkSlicerScriptedFileWriter&) = delete;
  void operator=(const vtkSlicerScriptedFileWriter&) = delete;

  class vtkInternal;
  vtkInternal* Internal;
};

#endif
