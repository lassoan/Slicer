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

#ifndef __vtkMRMLFileWriter_h
#define __vtkMRMLFileWriter_h

#include "vtkMRMLFileIOHandler.h"

#include <vtkSmartPointer.h>

class vtkMRMLIOProperties;
class vtkStringArray;

/// Something that writes a node (or the scene) to a kind of file.
///
/// As with vtkMRMLFileReader, a writer in C++ overrides CanWriteObjectConfidence and Write,
/// while a writer written in Python registers one of these to say what it can write and does the
/// writing itself.
///
/// qSlicerFileWriter is the Qt interface of this class, kept for backward compatibility.
class VTK_MRML_LOGIC_EXPORT vtkMRMLFileWriter : public vtkMRMLFileIOHandler
{
public:
  static vtkMRMLFileWriter* New();
  vtkTypeMacro(vtkMRMLFileWriter, vtkMRMLFileIOHandler);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  bool IsWriter() const override { return true; }

  /// Returns a positive number (>0) if the writer can write the object to file.
  /// The higher the returned value is the more confident the writer it is
  /// the most suitable class to write the object.
  /// By default, the method calls CanWriteObject and if it returns true then
  /// it returns ConfidenceForMatchingClass (0.5 by default).
  virtual double CanWriteObjectConfidence(vtkObject* object);

  /// Return true if the object is handled by the writer.
  /// By default it returns true if the object is an instance of any of the node class names.
  virtual bool CanWriteObject(vtkObject* object);

  /// Get the list of name filters supported for writing a particular object.
  /// Example: "Image (*.jpg *.png *.tiff)", "Model (.vtk)".
  /// By default, it returns the name filters of the writer (see SetNameFilters()).
  virtual void GetNameFiltersForObject(vtkObject* object, vtkStringArray* nameFilters);
  std::vector<std::string> GetNameFiltersListForObject(vtkObject* object);

  /// Write the node named by the "nodeID" property to the file named by "fileName". What it wrote
  /// is then in GetWrittenNodeIDs. The base class writes nothing: a writer whose work is done
  /// elsewhere is called through its owner.
  virtual bool Write(vtkMRMLIOProperties* properties);

  /// The nodes the last Write wrote, by node ID.
  vtkStringArray* GetWrittenNodeIDs() const;
  void SetWrittenNodeIDs(vtkStringArray* nodeIDs);
  void SetWrittenNodeIDs(const std::vector<std::string>& nodeIDs);
  void AddWrittenNodeID(const char* nodeID);
  void ClearWrittenNodeIDs();

  /// The class of node this writer is for ("vtkMRMLTextNode"). Setting it replaces all node class names
  /// with this single class name.
  /// Empty for a writer that decides for itself in CanWriteObjectConfidence.
  const char* GetNodeClassName() const;
  void SetNodeClassName(const char* className);

  /// The classes of nodes this writer is for.
  void SetNodeClassNames(vtkStringArray* classNames);
  void SetNodeClassNames(const std::vector<std::string>& classNames);
  void GetNodeClassNames(vtkStringArray* classNames) const;
  const std::vector<std::string>& GetNodeClassNames() const { return this->NodeClassNames; }
  void AddNodeClassName(const char* className);

  /// What a writer that only knows its node class answers for a node of that class.
  vtkGetMacro(ConfidenceForMatchingClass, double);
  vtkSetMacro(ConfidenceForMatchingClass, double);

protected:
  vtkMRMLFileWriter();
  ~vtkMRMLFileWriter() override;

  std::vector<std::string> NodeClassNames;
  double ConfidenceForMatchingClass{ 0.5 };
  vtkSmartPointer<vtkStringArray> WrittenNodeIDs;

private:
  vtkMRMLFileWriter(const vtkMRMLFileWriter&) = delete;
  void operator=(const vtkMRMLFileWriter&) = delete;
};

#endif
