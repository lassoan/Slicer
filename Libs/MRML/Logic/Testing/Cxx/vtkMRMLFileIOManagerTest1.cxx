/*==============================================================================

  Program: 3D Slicer

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// Slicer includes
#include "vtkMRMLFileIOManager.h"
#include "vtkMRMLFileReader.h"
#include "vtkMRMLFileWriter.h"
#include "vtkMRMLIOOptionsDescription.h"
#include "vtkMRMLIOProperties.h"
#include "vtkMRMLCoreTestingMacros.h"

// MRML includes
#include <vtkMRMLMessageCollection.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLTextNode.h>

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkCollection.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkStringArray.h>
#include <vtksys/FStream.hxx>
#include <vtksys/SystemTools.hxx>

// STD includes
#include <cmath>
#include <cstring>
#include <iostream>

namespace
{

/// A reader that looks inside files: it is sure of the ones whose name says "sure".
class vtkSureReader : public vtkMRMLFileReader
{
public:
  static vtkSureReader* New();
  vtkTypeMacro(vtkSureReader, vtkMRMLFileReader);
  double CanLoadFileConfidence(const char* filePath) override
  {
    if (filePath && std::strstr(filePath, "sure"))
    {
      return 0.9;
    }
    return this->Superclass::CanLoadFileConfidence(filePath);
  }
};
vtkStandardNewMacro(vtkSureReader);

/// A reader that adds a text node to the scene with the content of the file.
class vtkTextFileReader : public vtkMRMLFileReader
{
public:
  static vtkTextFileReader* New();
  vtkTypeMacro(vtkTextFileReader, vtkMRMLFileReader);
  bool Load(vtkMRMLIOProperties* properties) override
  {
    this->ClearLoadedNodeIDs();
    vtkMRMLTextNode* node =
      vtkMRMLTextNode::SafeDownCast(this->GetScene()->AddNewNodeByClass("vtkMRMLTextNode", properties->GetStringProperty("name", "Text")));
    node->SetText(properties->GetStringProperty("fileName").c_str());
    this->AddLoadedNodeID(node->GetID());
    this->GetUserMessages()->AddMessage(vtkCommand::WarningEvent, "loaded");
    return true;
  }
};
vtkStandardNewMacro(vtkTextFileReader);

/// A writer that writes the text of text nodes.
class vtkTextFileWriter : public vtkMRMLFileWriter
{
public:
  static vtkTextFileWriter* New();
  vtkTypeMacro(vtkTextFileWriter, vtkMRMLFileWriter);
  bool Write(vtkMRMLIOProperties* properties) override
  {
    this->ClearWrittenNodeIDs();
    vtkMRMLTextNode* node = vtkMRMLTextNode::SafeDownCast(this->GetScene()->GetNodeByID(properties->GetStringProperty("nodeID")));
    if (!node)
    {
      return false;
    }
    vtksys::ofstream file(properties->GetStringProperty("fileName").c_str());
    file << node->GetText();
    this->AddWrittenNodeID(node->GetID());
    return true;
  }
};
vtkStandardNewMacro(vtkTextFileWriter);

/// A reader that describes options, with defaults that depend on the file name and on other options.
class vtkOptionsReader : public vtkMRMLFileReader
{
public:
  static vtkOptionsReader* New();
  vtkTypeMacro(vtkOptionsReader, vtkMRMLFileReader);
  void GetOptionsDescription(vtkMRMLIOOptionsDescription* description) override
  {
    bool isLabel = description->GetFileName().find("label") != std::string::npos;
    description->AddBoolOption("labelmap", "Label map", "Load as label map", isLabel);
    description->AddEnumOption("colorNodeID", "Color", "", vtkVariant(description->GetBoolOptionValue("labelmap") ? "Labels" : "Grey"));
    description->AddEnumChoice("colorNodeID", vtkVariant("Grey"), "Grey \"scale\"");
    description->AddEnumChoice("colorNodeID", vtkVariant("Labels"), "Labels");
    description->AddIntOption("count", "Count", "", 3, 0, 10);
    description->SetOptionEnabled("count", !description->GetBoolOptionValue("labelmap"));
    description->AddStringListOption("name", "", "", std::vector<std::string>{ "a", "b" });
  }
};
vtkStandardNewMacro(vtkOptionsReader);

/// Count events
int NewFileLoadedEventCount = 0;
int FileSavedEventCount = 0;
void onFileIOManagerEvent(vtkObject*, unsigned long eid, void*, void* callData)
{
  vtkMRMLIOProperties* properties = reinterpret_cast<vtkMRMLIOProperties*>(callData);
  if (eid == vtkMRMLFileIOManager::NewFileLoadedEvent && properties && properties->HasProperty("nodeIDs"))
  {
    ++NewFileLoadedEventCount;
  }
  if (eid == vtkMRMLFileIOManager::FileSavedEvent && properties)
  {
    ++FileSavedEventCount;
  }
}

//-----------------------------------------------------------------------------
std::string createFile(const std::string& directory, const std::string& name)
{
  std::string path = directory + "/" + name;
  vtksys::ofstream file(path.c_str());
  file << "test";
  return path;
}

} // namespace

//-----------------------------------------------------------------------------
int vtkMRMLFileIOManagerTest1(int argc, char* argv[])
{
  // Files that the readers are asked about must exist.
  // They are created in the temporary directory (first argument), or in the current directory.
  const std::string baseDirectory = (argc > 1) ? std::string(argv[1]) : vtksys::SystemTools::GetCurrentWorkingDirectory();
  const std::string directory = baseDirectory + "/vtkMRMLFileIOManagerTest1";
  vtksys::SystemTools::RemoveADirectory(directory);
  vtksys::SystemTools::MakeDirectory(directory);
  const std::string sureSegFile = createFile(directory, "sure.seg.nrrd");
  const std::string segFile = createFile(directory, "Head.seg.nrrd");
  const std::string nrrdFile = createFile(directory, "Head.NRRD");
  const std::string vtkFile = createFile(directory, "Head.vtk");
  const std::string textFile = createFile(directory, "Note.txt");
  const std::string missingFile = directory + "/Missing.nrrd";

  // Name filter parsing and wildcard matching
  CHECK_INT(static_cast<int>(vtkMRMLFileIOHandler::NameFilterToWildcards("Image (*.jpg *.png)").size()), 2);
  CHECK_STD_STRING(vtkMRMLFileIOHandler::NameFilterToWildcards("*.mrml")[0], "*.mrml");
  CHECK_STD_STRING(vtkMRMLFileIOHandler::NameFilterToWildcards("All Files (*)")[0], "*");
  CHECK_BOOL(vtkMRMLFileIOHandler::WildcardMatch("*.seg.nrrd", "Head.SEG.nrrd"), true);
  CHECK_BOOL(vtkMRMLFileIOHandler::WildcardMatch("*.nrrd", "Head.nrrd.gz"), false);
  CHECK_BOOL(vtkMRMLFileIOHandler::WildcardMatch("image?.[pj]ng", "image1.png"), true);
  CHECK_BOOL(vtkMRMLFileIOHandler::WildcardMatch("image?.[!pj]ng", "image1.png"), false);
  CHECK_BOOL(vtkMRMLFileIOHandler::WildcardMatch("*", "anything"), true);

  vtkNew<vtkMRMLFileIOManager> manager;
  vtkNew<vtkMRMLScene> scene;
  manager->SetScene(scene);

  // A reader that goes by the extension, and one that looks inside the file
  vtkNew<vtkMRMLFileReader> extensionReader;
  extensionReader->SetFileType("VolumeFile");
  extensionReader->SetDescription("Volume");
  extensionReader->SetNameFilters(std::vector<std::string>{ "Volume (*.nrrd *.seg.nrrd)" });
  vtkNew<vtkSureReader> sureReader;
  sureReader->SetFileType("SegmentationFile");
  sureReader->SetDescription("Segmentation");
  sureReader->SetNameFilters(std::vector<std::string>{ "Segmentation (*.seg.nrrd)" });
  sureReader->SetOwner("python:Test");

  manager->RegisterReader(extensionReader);
  manager->RegisterReader(sureReader);
  manager->RegisterReader(sureReader); // registering again does nothing
  CHECK_INT(manager->GetNumberOfReaders(), 2);
  CHECK_POINTER(extensionReader->GetFileIOManager(), manager.GetPointer());
  CHECK_POINTER(extensionReader->GetScene(), scene.GetPointer());

  // Extensions from the name filters, compound ones included
  CHECK_STD_STRING(extensionReader->GetMatchedExtension("/data/Head.seg.nrrd"), ".seg.nrrd");
  CHECK_STD_STRING(extensionReader->GetMatchedExtension("/data/Head.NRRD"), ".nrrd");
  CHECK_BOOL(extensionReader->MatchesExtension("/data/Head.vtk"), false);

  // Description may contain brackets, extensions are taken from the last pair of brackets
  vtkNew<vtkMRMLFileReader> bracketReader;
  bracketReader->SetNameFilters(std::vector<std::string>{ "Markups (JSON) (*.mrk.json)" });
  CHECK_INT(static_cast<int>(bracketReader->GetExtensions().size()), 1);
  CHECK_STD_STRING(bracketReader->GetMatchedExtension("/data/F.mrk.json"), ".mrk.json");

  // Default confidence: 0.5 + 0.01 * length of the matched extension, only for existing files
  CHECK_DOUBLE_TOLERANCE(extensionReader->CanLoadFileConfidence(segFile.c_str()), 0.59, 1e-6);
  CHECK_DOUBLE_TOLERANCE(extensionReader->CanLoadFileConfidence(nrrdFile.c_str()), 0.55, 1e-6);
  CHECK_DOUBLE(extensionReader->CanLoadFileConfidence(vtkFile.c_str()), 0.0);
  CHECK_DOUBLE(extensionReader->CanLoadFileConfidence(missingFile.c_str()), 0.0);
  vtkNew<vtkStringArray> supportedNameFilters;
  extensionReader->GetSupportedNameFilters(segFile.c_str(), supportedNameFilters);
  CHECK_INT(supportedNameFilters->GetNumberOfValues(), 1);

  // The reader that is surest comes first; ties keep the order of registration
  vtkNew<vtkCollection> readers;
  manager->GetReadersForFile(sureSegFile.c_str(), readers);
  CHECK_INT(readers->GetNumberOfItems(), 2);
  CHECK_POINTER(readers->GetItemAsObject(0), sureReader.GetPointer());
  CHECK_POINTER(manager->GetReaderForFile(segFile.c_str()), extensionReader.GetPointer());
  CHECK_STD_STRING(manager->GetFileTypeForFile(sureSegFile.c_str()), "SegmentationFile");
  CHECK_NULL(manager->GetReaderForFile(vtkFile.c_str()));
  CHECK_STD_STRING(manager->GetFileTypeForFile(vtkFile.c_str()), "");
  vtkNew<vtkStringArray> fileTypes;
  manager->GetFileTypesForFile(sureSegFile.c_str(), fileTypes);
  CHECK_INT(fileTypes->GetNumberOfValues(), 2);
  CHECK_STD_STRING(fileTypes->GetValue(0), "SegmentationFile");
  CHECK_STD_STRING(manager->GetFileTypeFromDescription("Volume"), "VolumeFile");
  CHECK_POINTER(manager->GetReaderByDescription("Segmentation"), sureReader.GetPointer());

  // What the file types are
  manager->GetReaderFileTypes(fileTypes);
  CHECK_INT(fileTypes->GetNumberOfValues(), 2);
  CHECK_STD_STRING(manager->GetDescriptionForFileType("SegmentationFile", false), "Segmentation");
  vtkNew<vtkStringArray> extensions;
  manager->GetExtensionsForFileType("VolumeFile", false, extensions);
  CHECK_INT(extensions->GetNumberOfValues(), 2);

  // What an owner registered goes away with it
  manager->UnregisterOwner("python:Test");
  CHECK_INT(manager->GetNumberOfReaders(), 1);
  CHECK_POINTER(manager->GetNthReader(0), extensionReader.GetPointer());
  CHECK_NULL(sureReader->GetFileIOManager());
  manager->Unregister(extensionReader);
  CHECK_INT(manager->GetNumberOfReaders(), 0);

  // Loading
  vtkNew<vtkCallbackCommand> callback;
  callback->SetCallback(onFileIOManagerEvent);
  manager->AddObserver(vtkMRMLFileIOManager::NewFileLoadedEvent, callback);
  manager->AddObserver(vtkMRMLFileIOManager::FileSavedEvent, callback);
  vtkNew<vtkTextFileReader> textReader;
  textReader->SetFileType("TextFile");
  textReader->SetDescription("Text");
  textReader->SetNameFilters(std::vector<std::string>{ "Text (*.txt)" });
  manager->RegisterReader(textReader);
  vtkNew<vtkCollection> loadedNodes;
  vtkNew<vtkMRMLMessageCollection> userMessages;
  CHECK_BOOL(manager->LoadFile(textFile.c_str(), userMessages), true);
  CHECK_INT(scene->GetNumberOfNodesByClass("vtkMRMLTextNode"), 1);
  CHECK_INT(NewFileLoadedEventCount, 1);
  CHECK_INT(userMessages->GetNumberOfMessagesOfType(vtkCommand::WarningEvent), 1);
  // List of files, with names
  vtkNew<vtkMRMLIOProperties> loadProperties;
  loadProperties->SetStringListProperty("fileName", std::vector<std::string>{ textFile, textFile });
  loadProperties->SetStringListProperty("name", std::vector<std::string>{ "First", "Second" });
  CHECK_BOOL(manager->LoadNodes("TextFile", loadProperties, loadedNodes), true);
  CHECK_INT(loadedNodes->GetNumberOfItems(), 2);
  CHECK_STRING(vtkMRMLNode::SafeDownCast(loadedNodes->GetItemAsObject(1))->GetName(), "Second");
  CHECK_INT(NewFileLoadedEventCount, 3);
  // Files that do not exist or that no reader can read
  userMessages->ClearMessages();
  CHECK_BOOL(manager->LoadFile(missingFile.c_str(), userMessages), false);
  CHECK_BOOL(userMessages->GetNumberOfMessagesOfType(vtkCommand::ErrorEvent) > 0, true);

  // Writers
  vtkNew<vtkTextFileWriter> writer;
  writer->SetFileType("TextFile");
  writer->SetDescription("Text");
  writer->SetNameFilters(std::vector<std::string>{ "Text (.txt)" });
  writer->SetNodeClassName("vtkMRMLTextNode");
  manager->RegisterWriter(writer);
  CHECK_INT(manager->GetNumberOfWriters(), 1);
  CHECK_BOOL(writer->IsWriter(), true);
  vtkMRMLNode* textNode = vtkMRMLNode::SafeDownCast(loadedNodes->GetItemAsObject(0));
  CHECK_POINTER(manager->GetWriterForObject(textNode), writer.GetPointer());
  CHECK_POINTER(manager->GetWriterForObject(textNode, "Text (.txt)"), writer.GetPointer());
  CHECK_STD_STRING(manager->GetFileWriterFileType(textNode), "TextFile");
  CHECK_STD_STRING(manager->GetFileWriterFileType(scene), "");
  CHECK_STD_STRING(manager->ExtractKnownExtension("Note.txt", textNode), ".txt");
  CHECK_STD_STRING(manager->StripKnownExtension("Note.txt.txt", textNode), "Note");

  vtkNew<vtkMRMLIOProperties> saveProperties;
  const std::string savedFile = directory + "/subdirectory/Saved.txt";
  saveProperties->SetStringProperty("fileName", savedFile.c_str());
  saveProperties->SetStringProperty("nodeID", textNode->GetID());
  CHECK_BOOL(manager->SaveNodes("TextFile", saveProperties), true);
  CHECK_BOOL(vtksys::SystemTools::FileExists(savedFile, true), true);
  CHECK_INT(FileSavedEventCount, 1);
  // Writing to a file that the writer does not support
  saveProperties->SetStringProperty("fileName", (directory + "/Saved.nrrd").c_str());
  TESTING_OUTPUT_ASSERT_ERRORS_BEGIN();
  CHECK_BOOL(manager->SaveNodes("TextFile", saveProperties), false);
  TESTING_OUTPUT_ASSERT_ERRORS_END();

  // File name utilities
  CHECK_STD_STRING(vtkMRMLFileIOManager::ForceFileNameValidCharacters(" a/b:c*d?(e) "), "abcd(e)");
  manager->SetDefaultSceneFileType("MRML Scene (.mrml)");
  CHECK_STD_STRING(manager->GetDefaultSceneFileType(), "MRML Scene (.mrml)");

  // Properties: typed values, defaults, order, copy
  vtkNew<vtkMRMLIOProperties> properties;
  properties->SetStringProperty("fileName", "/data/Head.nrrd");
  properties->SetBoolProperty("labelmap", true);
  properties->SetIntProperty("count", 3);
  properties->SetDoubleProperty("spacing", 0.5);
  properties->SetStringListProperty("fileNames", std::vector<std::string>{ "a.png", "b.png" });
  vtkNew<vtkImageData> image;
  properties->SetObjectProperty("screenShot", image);
  CHECK_STD_STRING(properties->GetStringProperty("fileName"), "/data/Head.nrrd");
  CHECK_BOOL(properties->GetBoolProperty("labelmap"), true);
  CHECK_BOOL(properties->IsBoolProperty("labelmap"), true);
  CHECK_BOOL(properties->IsBoolProperty("count"), false);
  CHECK_INT(properties->GetIntProperty("count"), 3);
  CHECK_DOUBLE(properties->GetDoubleProperty("spacing"), 0.5);
  CHECK_INT(properties->GetIntProperty("missing", 7), 7);
  CHECK_BOOL(properties->HasProperty("missing"), false);
  CHECK_BOOL(properties->IsStringListProperty("fileNames"), true);
  CHECK_INT(static_cast<int>(properties->GetStringListProperty("fileNames").size()), 2);
  CHECK_BOOL(properties->IsObjectProperty("screenShot"), true);
  CHECK_POINTER(properties->GetObjectProperty("screenShot"), image.GetPointer());
  CHECK_STD_STRING(properties->GetPropertyNames()[0], "fileName");
  vtkNew<vtkMRMLIOProperties> copy;
  copy->Copy(properties);
  CHECK_INT(static_cast<int>(copy->GetPropertyNames().size()), 6);
  CHECK_BOOL(copy->IsBoolProperty("labelmap"), true);
  CHECK_INT(static_cast<int>(copy->GetStringListProperty("fileNames").size()), 2);
  properties->RemoveProperty("count");
  CHECK_BOOL(properties->HasProperty("count"), false);

  // Options description
  vtkNew<vtkOptionsReader> optionsReader;
  vtkNew<vtkMRMLIOProperties> context;
  context->SetStringProperty("fileName", "/data/mylabel.nrrd");
  vtkNew<vtkMRMLIOProperties> optionValues;
  optionsReader->GetOptionValues(context, optionValues);
  CHECK_BOOL(optionValues->GetBoolProperty("labelmap"), true);
  CHECK_STD_STRING(optionValues->GetStringProperty("colorNodeID"), "Labels");
  CHECK_INT(optionValues->GetIntProperty("count"), 3);
  CHECK_INT(static_cast<int>(optionValues->GetStringListProperty("name").size()), 2);
  // Value set by the user overrides the default, dependent default follows it
  context->SetBoolProperty("labelmap", false);
  optionsReader->GetOptionValues(context, optionValues);
  CHECK_STD_STRING(optionValues->GetStringProperty("colorNodeID"), "Grey");
  // Application default is used if the user did not set a value
  optionsReader->GetOptionDefaults()->SetIntProperty("count", 7);
  optionsReader->GetOptionValues(context, optionValues);
  CHECK_INT(optionValues->GetIntProperty("count"), 7);
  std::string json = optionsReader->GetOptionsDescriptionJSON(context);
  std::cout << json << std::endl;
  CHECK_BOOL(json.find("\"property\": \"labelmap\"") != std::string::npos, true);
  CHECK_BOOL(json.find("\"value\": \"Grey\"") != std::string::npos, true);
  CHECK_BOOL(json.find("Grey \\\"scale\\\"") != std::string::npos, true); // escaped quotes
  CHECK_BOOL(json.find("\"value\": 7") != std::string::npos, true);
  CHECK_BOOL(json.find("\"value\": [\"a\", \"b\"]") != std::string::npos, true);
  CHECK_STD_STRING(textReader->GetOptionsDescriptionJSON(context), ""); // no options

  vtksys::SystemTools::RemoveADirectory(directory);

  std::cout << "vtkMRMLFileIOManagerTest1 passed" << std::endl;
  return EXIT_SUCCESS;
}
