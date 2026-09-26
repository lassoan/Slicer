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

#ifndef __vtkMRMLFileIOManager_h
#define __vtkMRMLFileIOManager_h

#include "vtkMRMLLogicExport.h"

#include <vtkObject.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include <string>
#include <vector>

class vtkCollection;
class vtkImageData;
class vtkMRMLMessageCollection;
class vtkMRMLNode;
class vtkMRMLScene;
class vtkMRMLStorableNode;
class vtkMRMLStorageNode;
class vtkMRMLApplicationLogic;
class vtkMRMLFileIOHandler;
class vtkMRMLFileReader;
class vtkMRMLFileWriter;
class vtkMRMLIOProperties;
class vtkMRMLNodeWriter;
class vtkStringArray;

/// Manages reading and writing of files: keeps the list of readers and writers of the application,
/// chooses which reader or writer to use for a file or a node, and loads and saves nodes using them.
///
/// The modules of the application register what they can read and write, and everything that
/// loads or saves uses this class. The application logic owns the manager of the application
/// (vtkMRMLApplicationLogic::GetFileIOManager()).
///
/// qSlicerCoreIOManager is the Qt interface of this class, kept for backward compatibility.
///
/// Events are invoked so that a user interface can follow what is available and what is loaded:
/// - HandlerRegisteredEvent and HandlerUnregisteredEvent, with the handler as call data.
/// - NewFileLoadedEvent, with the vtkMRMLIOProperties of the loaded file as call data.
///   The properties contain the properties passed to the reader and "fileType" and "nodeIDs".
/// - FileSavedEvent, with the vtkMRMLIOProperties of the saved file as call data.
class VTK_MRML_LOGIC_EXPORT vtkMRMLFileIOManager : public vtkObject
{
public:
  static vtkMRMLFileIOManager* New();
  vtkTypeMacro(vtkMRMLFileIOManager, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  enum
  {
    HandlerRegisteredEvent = 24100,
    HandlerUnregisteredEvent,
    NewFileLoadedEvent,
    FileSavedEvent,
  };

  /// The scene the handlers read into and write from.
  virtual void SetScene(vtkMRMLScene* scene);
  vtkMRMLScene* GetScene() const;

  /// Application logic that owns this manager (not reference counted).
  /// Readers and writers may use it for accessing application-wide logic, such as the color logic.
  void SetApplicationLogic(vtkMRMLApplicationLogic* applicationLogic);
  vtkMRMLApplicationLogic* GetApplicationLogic() const;

  //@{
  /// Add a reader or a writer. The manager keeps a reference to it; registering the same object
  /// twice does nothing.
  void RegisterReader(vtkMRMLFileReader* reader);
  void RegisterWriter(vtkMRMLFileWriter* writer);
  /// Convenience method for creating and registering a vtkMRMLNodeWriter,
  /// which writes nodes of the specified classes using their storage nodes.
  /// Returns the registered writer.
  vtkMRMLNodeWriter* RegisterNodeWriter(const char* description,
                                          const char* fileType,
                                          const std::vector<std::string>& nodeClassNames,
                                          bool supportUseCompression = true);
  void Unregister(vtkMRMLFileIOHandler* handler);
  /// Remove every handler that something registered ("python:ImportMimics"), which is what a
  /// module does when it is unloaded or reloaded.
  void UnregisterOwner(const char* owner);
  //@}

  //@{
  /// Get a registered reader or writer by its class name (exact match). Returns nullptr if not found.
  vtkMRMLFileReader* GetReaderByClassName(const char* className) const;
  vtkMRMLFileWriter* GetWriterByClassName(const char* className) const;
  //@}

  //@{
  /// What is registered.
  int GetNumberOfReaders() const;
  vtkMRMLFileReader* GetNthReader(int index) const;
  int GetNumberOfWriters() const;
  vtkMRMLFileWriter* GetNthWriter(int index) const;
  //@}

  //@{
  /// Readers

  /// The readers that can read this file, the one that is surest of itself first. Only readers
  /// that answer a confidence above zero are in the list.
  void GetReadersForFile(const char* filePath, vtkCollection* readers);
  /// The reader that is surest it can read this file, or nullptr.
  vtkMRMLFileReader* GetReaderForFile(const char* filePath);
  /// The file type of the reader that is surest it can read this file ("VolumeFile"), or an empty string.
  std::string GetFileTypeForFile(const char* filePath);
  /// All the file types that can be used for reading the file, most likely first.
  void GetFileTypesForFile(const char* filePath, vtkStringArray* fileTypes);
  /// The descriptions ("Volume", "Segmentation", ...) of the readers that can read the file, most likely first.
  void GetFileDescriptionsForFile(const char* filePath, vtkStringArray* descriptions);

  /// The reader that has the specified description, or nullptr.
  vtkMRMLFileReader* GetReaderByDescription(const char* description);
  /// The file type of the reader that has the specified description, or an empty string.
  std::string GetFileTypeFromDescription(const char* description);
  /// Descriptions of all the readers of a file type. Usually there is only one reader for a file type.
  void GetFileDescriptionsByType(const char* fileType, vtkStringArray* descriptions);

  /// Iterates through readers looking at the file list to see if there is an entry that can serve as
  /// an archetype for loading multiple files. If so, the reader removes the recognized
  /// files from the list and sets the ioProperties so that the corresponding
  /// loader will read these files. The "fileName" property is set to the archetype file.
  /// Returns the reader that recognized the files or nullptr if no pattern is recognized.
  /// The specific motivating use case is when the file list contains a set of related files,
  /// such as a list of image files that are recognized as a volume.
  vtkMRMLFileReader* ExamineFileList(vtkStringArray* fileList, vtkMRMLIOProperties* ioProperties);
  //@}

  //@{
  /// Writers

  /// The writers that can write this node, the one that is surest of itself first.
  void GetWritersForObject(vtkObject* object, vtkCollection* writers);
  /// Return the best file writer for this object.
  /// Best match is the writer that supports the node type and the specific format (name filter, such as "NRRD (.nrrd)"),
  /// closest match is the writer that supports the node type but not that specific format.
  /// If there are multiple matches then the one with the highest confidence is returned.
  vtkMRMLFileWriter* GetWriterForObject(vtkObject* object, const char* format = nullptr);
  /// The file type of the best file writer for this object, or an empty string.
  std::string GetFileWriterFileType(vtkObject* object, const char* format = nullptr);
  /// Same as GetFileWriterFileType(object).
  std::string GetFileTypeForObject(vtkObject* object);
  /// Descriptions of all writers of a file type.
  void GetFileWriterDescriptions(const char* fileType, vtkStringArray* descriptions);
  /// Name filters of all writers that can write the object, the most confident writer's first.
  void GetFileWriterExtensions(vtkObject* object, vtkStringArray* nameFilters);
  //@}

  //@{
  /// The file types that are registered, and what belongs to each of them.
  void GetReaderFileTypes(vtkStringArray* fileTypes);
  void GetWriterFileTypes(vtkStringArray* fileTypes);
  /// The extensions of every reader (or writer) of a file type: ".mcs", ".mcs.gz".
  void GetExtensionsForFileType(const char* fileType, bool writers, vtkStringArray* extensions);
  /// The name filters of every reader (or writer) of a file type: "Mimics project (*.mcs)".
  void GetNameFiltersForFileType(const char* fileType, bool writers, vtkStringArray* nameFilters);
  /// What a file type is called ("Volume"), from the first handler that has a description.
  std::string GetDescriptionForFileType(const char* fileType, bool writers);
  /// The readers of a file type, in the order they were registered.
  void GetReadersForFileType(const char* fileType, vtkCollection* readers);
  /// The writers of a file type, in the order they were registered.
  void GetWritersForFileType(const char* fileType, vtkCollection* writers);
  //@}

  //@{
  /// File extensions and names

  /// All the writable file extensions for all registered types of storage nodes. Includes the leading dot.
  void GetAllWritableFileExtensions(vtkStringArray* extensions);
  /// All the readable file extensions for all registered types of storage nodes. Includes the leading dot.
  void GetAllReadableFileExtensions(vtkStringArray* extensions);

  /// Returns a full extension for this storable node that is recognised by Slicer IO.
  /// Consults the storage node for a list of known suffixes. If no match is found then "." is returned.
  /// Always includes the leading dot.
  std::string GetCompleteSlicerWritableFileNameSuffix(vtkMRMLStorableNode* node);

  /// Characters that are allowed in file names by ForceFileNameValidCharacters,
  /// as a regular expression character set.
  static std::string GetFileNameValidCharactersPattern();

  /// Remove characters that are likely to cause problems in a filename
  static std::string ForceFileNameValidCharacters(const std::string& filename);

  /// Clamp the length of a filename to a maximum number of characters.
  /// The length of the filename extension must also be specified so that it is not included in the shortened section.
  /// If the extension is not known then ExtractKnownExtension() method can be used to get the extension.
  /// If maxLength is negative, the default maximum length (DefaultMaximumFileNameLength) is used.
  std::string ForceFileNameMaxLength(const std::string& filename, int extensionLength, int maxLength = -1);

  /// Default maximum length for a filename. It is used when maximum filename length is enforced by calling
  /// ForceFileNameMaxLength() without providing a specific maximum length value.
  vtkGetMacro(DefaultMaximumFileNameLength, int);
  vtkSetMacro(DefaultMaximumFileNameLength, int);

  /// If fileName ends with an extension that is associated with the object,
  /// then return that extension. Otherwise return an empty string.
  /// If there are multiple candidates (such as for "something.seg.nrrd" both
  /// ".nrrd" and ".seg.nrrd" extensions match) then the longest is returned.
  std::string ExtractKnownExtension(const std::string& fileName, vtkObject* object);

  /// If fileName ends with an extension that is associated with the object,
  /// then return a stripped version of fileName, where that extension
  /// has been chopped off. If the extension is duplicated in the
  /// tail of fileName, then all duplicates are stripped away.
  std::string StripKnownExtension(const std::string& fileName, vtkObject* object);
  //@}

  //@{
  /// Loading

  /// Load nodes using the readers of the file type.
  /// The properties must contain "fileName" (a string or a list of strings) and may contain more specific properties.
  /// For example, the volume reader can be called with the following parameters: "labelmap" (bool), "center" (bool).
  /// Nodes that are loaded are added to loadedNodes (if not nullptr).
  /// If a valid userMessages is passed then additional error or warning information is added to it.
  /// Returns false if loading fails.
  virtual bool LoadNodes(const char* fileType,
                         vtkMRMLIOProperties* properties,
                         vtkCollection* loadedNodes = nullptr,
                         vtkMRMLMessageCollection* userMessages = nullptr);

  /// Load a list of files. Each item in propertiesList is a vtkMRMLIOProperties object that
  /// specifies the "fileType" property in addition to the properties needed by the reader.
  virtual bool LoadNodesFromList(vtkCollection* propertiesList, vtkCollection* loadedNodes = nullptr, vtkMRMLMessageCollection* userMessages = nullptr);

  /// Load nodes and return the first loaded node.
  vtkMRMLNode* LoadNodesAndGetFirst(const char* fileType, vtkMRMLIOProperties* properties, vtkMRMLMessageCollection* userMessages = nullptr);

  /// Load or import a scene.
  bool LoadScene(const char* fileName, bool clear = true, vtkMRMLMessageCollection* userMessages = nullptr);

  /// Load a file. All the options (e.g., file type) are chosen by default.
  bool LoadFile(const char* fileName, vtkMRMLMessageCollection* userMessages = nullptr);
  //@}

  //@{
  /// Saving

  /// Save nodes (or scene) using the registered writers.
  /// Properties are typically: "fileName"; for nodes: "nodeID", "useCompression", "fileFormat".
  /// If a valid scene is passed then the writers use that scene instead of the scene of the manager.
  /// Returns true on success.
  bool SaveNodes(const char* fileType,
                 vtkMRMLIOProperties* properties,
                 vtkMRMLMessageCollection* userMessages = nullptr,
                 vtkMRMLScene* scene = nullptr);

  /// Export nodes using the registered writers. Unlike SaveNodes(), this function creates a temporary scene
  /// while saving, in order to avoid modifying storage nodes in the current scene.
  /// Each item in propertiesList is a vtkMRMLIOProperties object that specifies a "nodeID" (ID of a node
  /// in the main scene), a "fileName" (an absolute file path), a "fileFormat" (e.g. "NRRD (.nrrd)"),
  /// and any other options that the associated writer may use.
  /// If hardenTransforms is true then transforms are temporarily hardened before export.
  bool ExportNodes(vtkCollection* propertiesList, bool hardenTransforms, vtkMRMLMessageCollection* userMessages = nullptr);

  /// Export nodes, with the same properties for all nodes.
  bool ExportNodes(vtkStringArray* nodeIDs,
                   vtkStringArray* fileNames,
                   vtkMRMLIOProperties* commonProperties,
                   bool hardenTransforms,
                   vtkMRMLMessageCollection* userMessages = nullptr);

  /// Save the scene. Equivalent to calling SaveNodes with "SceneFile" file type
  /// with the fileName and screenShot set as properties.
  bool SaveScene(const char* fileName, vtkImageData* screenShot = nullptr, vtkMRMLMessageCollection* userMessages = nullptr);

  /// Create default storage nodes for all storable nodes that are to be saved
  /// with the scene and do not have a storage node already.
  void AddDefaultStorageNodes();

  /// Create and add default storage node
  static vtkMRMLStorageNode* CreateAndAddDefaultStorageNode(vtkMRMLStorableNode* node);
  //@}

  /// Invoke NewFileLoadedEvent. It may be used by readers that load files without the manager.
  void InvokeNewFileLoadedEvent(vtkMRMLIOProperties* loadedFileProperties);
  /// Invoke FileSavedEvent. It may be used by writers that save files without the manager.
  void InvokeFileSavedEvent(vtkMRMLIOProperties* savedFileProperties);

  /// Defines the file format that should be offered by default when the scene is saved.
  /// Valid options are defined by the scene writer (for example, "MRML Scene (.mrml)"
  /// or "Medical Reality Bundle (.mrb)").
  std::string GetDefaultSceneFileType() const;
  void SetDefaultSceneFileType(const std::string& fileType);

protected:
  vtkMRMLFileIOManager();
  ~vtkMRMLFileIOManager() override;

  /// Handlers with their confidence, highest first; ties keep the order of registration.
  template <typename HandlerType>
  void SortedByConfidence(const std::vector<vtkSmartPointer<HandlerType>>& handlers, const std::vector<double>& confidences, vtkCollection* result);

  /// Writers that can write the file, writers that match the file extension first and generic writers
  /// (that can write any file) last.
  std::vector<vtkMRMLFileWriter*> GetWritersForFile(const char* fileType, vtkMRMLIOProperties* properties, vtkMRMLScene* scene);

  vtkWeakPointer<vtkMRMLScene> Scene;
  vtkWeakPointer<vtkMRMLApplicationLogic> ApplicationLogic;
  std::vector<vtkSmartPointer<vtkMRMLFileReader>> Readers;
  std::vector<vtkSmartPointer<vtkMRMLFileWriter>> Writers;

  std::string DefaultSceneFileType;
  int DefaultMaximumFileNameLength{ 1000 };

private:
  vtkMRMLFileIOManager(const vtkMRMLFileIOManager&) = delete;
  void operator=(const vtkMRMLFileIOManager&) = delete;
};

#endif
