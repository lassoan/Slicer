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

#ifndef __vtkMRMLFileReader_h
#define __vtkMRMLFileReader_h

#include "vtkMRMLFileIOHandler.h"

#include <vtkSmartPointer.h>

class vtkMRMLIOProperties;
class vtkStringArray;

/// Something that reads a kind of file into the scene.
///
/// A reader in C++ overrides CanLoadFileConfidence (if the file extension is not enough to decide
/// if the reader can load the file) and Load. A reader written in Python registers one of these to
/// say what it can read, and is asked to do the reading itself.
///
/// qSlicerFileReader is the Qt interface of this class, kept for backward compatibility.
class VTK_MRML_LOGIC_EXPORT vtkMRMLFileReader : public vtkMRMLFileIOHandler
{
public:
  static vtkMRMLFileReader* New();
  vtkTypeMacro(vtkMRMLFileReader, vtkMRMLFileIOHandler);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Returns a positive number (>0) if the reader can load this file.
  /// The higher the returned value is the more confident the reader it is
  /// the most suitable class to load the file.
  /// By default, the method calls CanLoadFile and if it returns true then
  /// the returned confidence value is ConfidenceForMatchingExtension (0.5 by default)
  /// + 0.01 * matchedFileExtensionLength.
  /// The additional confidence for longer matched file extensions allow prioritization of
  /// more specific readers. For example, "*.seg.nrrd" is more specific than "*.nrrd";
  /// "*.nrrd" is more specific than "*.*".
  virtual double CanLoadFileConfidence(const char* filePath);

  /// Returns true if the reader can load this file.
  /// Default implementation is a simple and fast, it just checks
  /// if file extension matches any of the name filters.
  virtual bool CanLoadFile(const char* filePath);

  /// Return the name filters that match the file. For example, if the file name is "my_image.nrrd"
  /// and the supported name filters are "Volumes (*.mha *.nrrd *.raw)", "Images (*.png *.jpg)", "DICOM (*)"
  /// then it returns "Volumes (*.mha *.nrrd *.raw)" and "DICOM (*)".
  /// No name filters are returned if the file is not a readable file.
  void GetSupportedNameFilters(const char* filePath, vtkStringArray* nameFilters);
  /// Same as GetSupportedNameFilters(filePath, nameFilters) but it also returns
  /// the length of the longest matched extension (it can be used to determine how specifically extension matched).
  std::vector<std::string> GetSupportedNameFilters(const std::string& filePath, int* longestExtensionMatch = nullptr);

  /// Read the file named by the "fileName" property into the scene. What it added is then in
  /// GetLoadedNodeIDs. The base class reads nothing: a reader whose work is done elsewhere (in
  /// Python, say) leaves this alone and is called through its owner.
  virtual bool Load(vtkMRMLIOProperties* properties);

  /// Examine the list of files to see if there is an entry that can serve as an archetype for loading multiple files.
  /// If so, the reader removes the recognized files (except the archetype) from the list, sets the ioProperties
  /// so that the reader will read these files, and returns the archetype file path.
  /// If no pattern is recognized then the method returns an empty string.
  /// The specific motivating use case is when the file list contains a set of related files, such as a list of image
  /// files that are recognized as a volume.
  virtual std::string ExamineFileList(vtkStringArray* fileList, vtkMRMLIOProperties* ioProperties);

  /// The nodes the last Load added, by node ID.
  vtkStringArray* GetLoadedNodeIDs() const;
  void SetLoadedNodeIDs(vtkStringArray* nodeIDs);
  void SetLoadedNodeIDs(const std::vector<std::string>& nodeIDs);
  void AddLoadedNodeID(const char* nodeID);
  void ClearLoadedNodeIDs();

  /// Confidence value returned by the default CanLoadFileConfidence implementation when the file matches
  /// one of the name filters. A small value is added for longer matched extensions.
  vtkGetMacro(ConfidenceForMatchingExtension, double);
  vtkSetMacro(ConfidenceForMatchingExtension, double);

protected:
  vtkMRMLFileReader();
  ~vtkMRMLFileReader() override;

  /// Read the first maxLength characters of a text file (to inspect a file header).
  /// Returns empty string if the file cannot be read.
  static std::string ReadFileHeader(const std::string& filePath, size_t maxLength);

  /// Returns true if the string ends with the suffix (case-insensitive).
  static bool EndsWithNoCase(const std::string& text, const std::string& suffix);

  double ConfidenceForMatchingExtension{ 0.5 };
  vtkSmartPointer<vtkStringArray> LoadedNodeIDs;

private:
  vtkMRMLFileReader(const vtkMRMLFileReader&) = delete;
  void operator=(const vtkMRMLFileReader&) = delete;
};

#endif
