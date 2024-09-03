/*=auto=========================================================================

Portions (c) Copyright 2005 Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Program:   3D Slicer
Module:    $RCSfile: vtkMRMLColorTableStorageNode.cxx,v $
Date:      $Date: 2006/03/17 15:10:10 $
Version:   $Revision: 1.6 $

=========================================================================auto=*/

// MRML include
#include "vtkMRMLColorTableStorageNode.h"
#include "vtkMRMLColorTableNode.h"
#include "vtkMRMLI18N.h"
#include "vtkMRMLMessageCollection.h"
#include "vtkMRMLScene.h"

// VTK include
#include <vtkDelimitedTextReader.h>
#include <vtkDelimitedTextWriter.h>
#include <vtkLookupTable.h>
#include <vtkObjectFactory.h>
#include <vtkStringArray.h>
#include <vtkTable.h>

// STD include
#include <sstream>

//------------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLColorTableStorageNode);

//----------------------------------------------------------------------------
vtkMRMLColorTableStorageNode::vtkMRMLColorTableStorageNode()
{
  // When a color table file contains very large numbers then most likely
  // it is not a valid file (probably it is some other text file and not
  // a color table). The highest acceptable color ID is specified in MaximumColorID.
  this->MaximumColorID = 1000000;
  this->DefaultWriteFileExtension = "csv";
}

//----------------------------------------------------------------------------
vtkMRMLColorTableStorageNode::~vtkMRMLColorTableStorageNode() = default;

//----------------------------------------------------------------------------
void vtkMRMLColorTableStorageNode::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkMRMLStorageNode::PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
bool vtkMRMLColorTableStorageNode::CanReadInReferenceNode(vtkMRMLNode* refNode)
{
  return refNode->IsA("vtkMRMLColorTableNode");
}

//----------------------------------------------------------------------------
int vtkMRMLColorTableStorageNode::ReadDataInternal(vtkMRMLNode* refNode)
{
  // cast the input node
  vtkMRMLColorTableNode* colorNode = vtkMRMLColorTableNode::SafeDownCast(refNode);
  if (colorNode == nullptr)
  {
    vtkErrorMacro("ReadData: unable to cast input node " << refNode->GetID() << " to a known color table node");
    return 0;
  }

  std::string fullName = this->GetFullNameFromFileName();
  if (fullName.empty())
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadDataInternal",
      "Filename is not specified (" << (this->ID ? this->ID : "(unknown)") << ").");
    return 0;
  }

  // check that the file exists
  if (vtksys::SystemTools::FileExists(fullName.c_str()) == false)
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadDataInternal",
      "Color table file '" << fullName.c_str() << "' is not found while trying to read node (" << (this->ID ? this->ID : "(unknown)") << ").");
    return 0;
  }

  // compute file prefix
  std::string extension = vtkMRMLStorageNode::GetLowercaseExtensionFromFileName(fullName);
  if (extension.empty())
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadDataInternal",
      "Color table file '" << fullName.c_str() << "' has no file extension while trying to read node (" << (this->ID ? this->ID : "(unknown)") << ").");
    return 0;
  }

  vtkDebugMacro("ReadDataInternal (" << (this->ID ? this->ID : "(unknown)") << "): extension = " << extension.c_str());

  if (extension == std::string(".ctbl") || extension == std::string(".txt"))
  {
    return this->ReadCtblFile(fullName, colorNode);
  }
  else if (extension == std::string(".csv") || extension == std::string(".tsv"))
  {
    return this->ReadCsvFile(fullName, colorNode);
  }
  else
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadDataInternal",
      "Color table file '" << fullName.c_str() << "' has unknown file extension while trying to read node (" << (this->ID ? this->ID : "(unknown)") << ").");
    return 0;
  }
}

//----------------------------------------------------------------------------
std::string vtkMRMLColorTableStorageNode::GetFieldDelimiterCharacters(std::string filename)
{
  std::string lowercaseFileExt = vtkMRMLStorageNode::GetLowercaseExtensionFromFileName(filename);
  std::string fieldDelimiterCharacters;
  if (lowercaseFileExt == std::string(".tsv") || lowercaseFileExt == std::string(".txt"))
  {
    fieldDelimiterCharacters = "\t";
  }
  else if (lowercaseFileExt == std::string(".csv"))
  {
    fieldDelimiterCharacters = ",";
  }
  else
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::GetFieldDelimiterCharacters",
      "Cannot determine field delimiter character from file extension: '" << lowercaseFileExt << "'.");
  }
  return fieldDelimiterCharacters;
}

int vtkMRMLColorTableStorageNode::ReadCsvFile(std::string filename, vtkMRMLColorTableNode* colorNode)
{
  vtkNew<vtkDelimitedTextReader> reader;
  reader->SetFileName(filename.c_str());
  reader->SetHaveHeaders(true);
  reader->SetFieldDelimiterCharacters(this->GetFieldDelimiterCharacters(filename).c_str());
  // Make sure string delimiter characters are removed (somebody may have written a tsv with string delimiters)
  reader->SetUseStringDelimiter(true);
  // File contents is preserved better if we don't try to detect numeric columns
  reader->DetectNumericColumnsOff();

  // Read table
  vtkTable* table = nullptr;
  try
  {
    reader->Update();
    table = reader->GetOutput();
  }
  catch (...)
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Failed to read color table file: '" << filename << "'.");
    return 0;
  }

  if (!table)
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Failed to read color table file: '" << filename << "'.");
    return 0;
  }

  vtkStringArray* labelValueColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("LabelValue"));
  if (!labelValueColumn)
  {
    vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Failed to read color table file: '" << filename << "' due to missing 'LabelValue' column.");
    return 0;
  }

  vtkStringArray* nameColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Name"));
  if (!nameColumn)
  {
    vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Missing 'Name' column in color table file: '" << filename << "'.");
  }

  vtkStringArray* colorRColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Color.R"));
  if (!colorRColumn)
  {
    vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Missing 'Color.R' column in color table file: '" << filename << "'.");
  }
  vtkStringArray* colorGColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Color.G"));
  if (!colorGColumn)
  {
    vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Missing 'Color.G' column in color table file: '" << filename << "'.");
  }
  vtkStringArray* colorBColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Color.B"));
  if (!colorBColumn)
  {
    vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Missing 'Color.B' column in color table file: '" << filename << "'.");
  }
  vtkStringArray* colorAColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Color.A"));
  if (!colorAColumn)
  {
    vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
      "Missing 'Color.A' column in color table file: '" << filename << "'.");
  }

  vtkStringArray* categoryCSColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Category.CodingSchemeDesignator"));
  vtkStringArray* categoryCVColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Category.CodeValue"));
  vtkStringArray* categoryCMColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Category.CodeMeaning"));

  vtkStringArray* typeCSColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Type.CodingSchemeDesignator"));
  vtkStringArray* typeCVColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Type.CodeValue"));
  vtkStringArray* typeCMColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("Type.CodeMeaning"));
  vtkStringArray* typeModifierCSColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("TypeModifier.CodingSchemeDesignator"));
  vtkStringArray* typeModifierCVColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("TypeModifier.CodeValue"));
  vtkStringArray* typeModifierCMColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("TypeModifier.CodeMeaning"));

  vtkStringArray* anatomicRegionCSColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("AnatomicRegion.CodingSchemeDesignator"));
  vtkStringArray* anatomicRegionCVColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("AnatomicRegion.CodeValue"));
  vtkStringArray* anatomicRegionCMColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("AnatomicRegion.CodeMeaning"));
  vtkStringArray* anatomicRegionModifierCSColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("AnatomicRegionModifier.CodingSchemeDesignator"));
  vtkStringArray* anatomicRegionModifierCVColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("AnatomicRegionModifier.CodeValue"));
  vtkStringArray* anatomicRegionModifierCMColumn = vtkStringArray::SafeDownCast(table->GetColumnByName("AnatomicRegionModifier.CodeMeaning"));


  // clear out the table
  MRMLNodeModifyBlocker blocker(colorNode);

  // Set type to "File" by default if it has not been set yet.
  // It is important to only change type if it has not been set already
  // because otherwise "User" color node types would be always reverted to
  // read-only "File" type when the scene is saved and reloaded.
  if (colorNode->GetType()<colorNode->GetFirstType()
    || colorNode->GetType()>colorNode->GetLastType())
  {
    // no valid type has been set, set it to File
    colorNode->SetTypeToFile();
  }

  int numberOfTuples = labelValueColumn->GetNumberOfTuples();
  bool valid = false;
  int maxLabelValue = -1;
  for (vtkIdType row = 0; row < numberOfTuples; ++row)
  {
    if (labelValueColumn->GetValue(row).empty())
    {
      vtkWarningToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
        "labelValue is not specified in line " << row << " - skipping this line.");
      continue;
    }
    int labelValue = labelValueColumn->GetVariantValue(row).ToInt(&valid);
    if (!valid)
    {
      vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
        "labelValue '" << labelValueColumn->GetValue(row) << "' is invalid in line " << row << " - skipping this line.");
      continue;
    }
    if (labelValue < 0)
    {
      vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
        "labelValue '" << labelValueColumn->GetValue(row) << "' is invalid, the value must be positive, in line " << row << " - skipping this line.");
      continue;
    }
    if (labelValue > maxLabelValue)
    {
      maxLabelValue = labelValue;
    }
    if (maxLabelValue > this->MaximumColorID)
    {
      vtkErrorToMessageCollectionMacro(this->GetUserMessages(), "vtkMRMLColorTableStorageNode::ReadCsvFile",
        vtkMRMLI18N::Format(vtkMRMLTr("vtkMRMLColorTableStorageNode",
          "Cannot read '%1' file as a volume of type '%2'. Label values must not go above %3."),
          filename.c_str(),
          colorNode ? colorNode->GetNodeTagName() : "",
          std::to_string(this->MaximumColorID).c_str()));
      colorNode->SetNumberOfColors(0);
      return 0;
    }
    if (maxLabelValue >= colorNode->GetNumberOfColors())
    {
      // we add 1 to the maximum label value because first color's ID is 1 (ID=0 is reserved for background)
      colorNode->SetNumberOfColors(maxLabelValue + 1);
    }
  }

  if (colorNode->GetLookupTable())
  {
    colorNode->GetLookupTable()->SetTableRange(0, maxLabelValue);
  }
  // init the table to black/opacity 0 with no name, just in case we're missing values
  colorNode->SetColors(0, maxLabelValue, colorNode->GetNoName(), 0.0, 0.0, 0.0, 0.0);

  /*
  for (unsigned int i = 0; i < numberOfTuples; i++)
  {
    std::string name;
    if (nameColumn)
    {
      name = nameColumn->GetValue(i);
    }
    colorNode->SetColorName(labelValue, name.c_str());

    int id = 0;
    std::string name;
    double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
    ss >> id;
    ss >> name;
    ss >> r;
    ss >> g;
    ss >> b;
    // check that there's still something left in the stream
    if (ss.str().length() == 0)
    {
      std::cout << "Error in parsing line " << i << ", no alpha information!" << std::endl;
    }
    else
    {
      ss >> a;
    }
    if (!biggerThanOne &&
        (r > 1.0 || g > 1.0 || b > 1.0))
    {
      biggerThanOne = true;
    }
    // the file values are 0-255, color look up table needs 0-1
    // clamp the colors just in case
    r = r > 255.0 ? 255.0 : r;
    r = r < 0.0 ? 0.0 : r;
    g = g > 255.0 ? 255.0 : g;
    g = g < 0.0 ? 0.0 : g;
    b = b > 255.0 ? 255.0 : b;
    b = b < 0.0 ? 0.0 : b;
    a = a > 255.0 ? 255.0 : a;
    a = a < 0.0 ? 0.0 : a;
    // now shift to 0-1
    r = r / 255.0;
    g = g / 255.0;
    b = b / 255.0;
    a = a / 255.0;
    // if the name has ticks around it, from copying from a mrml file, trim
    // them off the string

    if (i < 10)
    {
      vtkDebugMacro("(first ten) Adding color at id " << id << ", name = " << name.c_str() << ", r = " << r << ", g = " << g << ", b = " << b << ", a = " << a);
    }
    if (colorNode->SetColor(id, name.c_str(), r, g, b, a) == 0)
    {
      vtkWarningMacro("ReadData: unable to set color " << id << " with name " << name.c_str()
        << ", breaking the loop over " << lines.size() << " lines in the file " << this->FileName);
      return 0;
    }
    colorNode->SetColorNameWithSpaces(id, name.c_str(), "_");
  }
  if (lines.size() > 0 && !biggerThanOne)
  {
    vtkWarningMacro("ReadDataInternal: possibly malformed color table file:\n" << this->FileName
      << ".\n\tNo RGB values are greater than 1. Valid values are 0-255");
  }
  */

  return 1;
}

int vtkMRMLColorTableStorageNode::ReadCtblFile(std::string fullName, vtkMRMLColorTableNode* colorNode)
{

  // open the file for reading input
  fstream fstr;
  fstr.open(fullName.c_str(), fstream::in);

  if (fstr.is_open())
  {
    vtkErrorMacro("ERROR opening color file " << this->FileName << endl);
    return 0;
  }

  // clear out the table
  MRMLNodeModifyBlocker blocker(colorNode);

  // Set type to "File" by default if it has not been set yet.
  // It is important to only change type if it has not been set already
  // because otherwise "User" color node types would be always reverted to
  // read-only "File" type when the scene is saved and reloaded.
  if (colorNode->GetType() < colorNode->GetFirstType()
    || colorNode->GetType() > colorNode->GetLastType())
  {
    // no valid type has been set, set it to File
    colorNode->SetTypeToFile();
  }

  std::string line;
  // save the valid lines in a vector, parse them once know the max id
  std::vector<std::string>lines;
  int maxID = 0;
  while (fstr.good())
  {
    std::getline(fstr, line);

    // does it start with a #?
    if (line[0] == '#')
    {
      vtkDebugMacro("Comment line, skipping:\n\"" << line << "\"");
      // sanity check: does the procedural header match?
      if (line.compare(0, 23, "# Color procedural file") == 0)
      {
        vtkErrorMacro("ReadDataInternal:\nfound a comment that this file "
          << " is a procedural color file, returning:\n"
          << line);
        return 0;
      }
    }
    else
    {
      // is it empty?
      if (line[0] == '\0')
      {
        vtkDebugMacro("Empty line, skipping:\n\"" << line << "\"");
      }
      else
      {
        vtkDebugMacro("got a line: \n\"" << line << "\"");
        lines.emplace_back(line);
        std::stringstream ss;
        ss << line;
        int id;
        ss >> id;
        if (id > maxID)
        {
          maxID = id;
        }
      }
    }
  }
  fstr.close();
  // now parse out the valid lines and set up the color lookup table
  vtkDebugMacro("The largest id is " << maxID);
  if (maxID > this->MaximumColorID)
  {
    vtkErrorMacro("ReadData: maximum color id " << maxID << " is > "
      << this->MaximumColorID << ", invalid color file: "
      << this->GetFileName());
    colorNode->SetNumberOfColors(0);
    return 0;
  }
  // extra one for zero, also resizes the names array
  colorNode->SetNumberOfColors(maxID + 1);
  if (colorNode->GetLookupTable())
  {
    colorNode->GetLookupTable()->SetTableRange(0, maxID);
  }
  // init the table to black/opacity 0 with no name, just in case we're missing values
  colorNode->SetColors(0, maxID, colorNode->GetNoName(), 0.0, 0.0, 0.0, 0.0);

  // do a little sanity check, if never get an rgb bigger than 1.0, report
  // it as a possibly miswritten file
  bool biggerThanOne = false;
  for (unsigned int i = 0; i < lines.size(); i++)
  {
    std::stringstream ss;
    ss << lines[i];
    int id = 0;
    std::string name;
    double r = 0.0, g = 0.0, b = 0.0, a = 0.0;
    ss >> id;
    ss >> name;
    ss >> r;
    ss >> g;
    ss >> b;
    // check that there's still something left in the stream
    if (ss.str().length() == 0)
    {
      std::cout << "Error in parsing line " << i << ", no alpha information!" << std::endl;
    }
    else
    {
      ss >> a;
    }
    if (!biggerThanOne &&
      (r > 1.0 || g > 1.0 || b > 1.0))
    {
      biggerThanOne = true;
    }
    // the file values are 0-255, color look up table needs 0-1
    // clamp the colors just in case
    r = r > 255.0 ? 255.0 : r;
    r = r < 0.0 ? 0.0 : r;
    g = g > 255.0 ? 255.0 : g;
    g = g < 0.0 ? 0.0 : g;
    b = b > 255.0 ? 255.0 : b;
    b = b < 0.0 ? 0.0 : b;
    a = a > 255.0 ? 255.0 : a;
    a = a < 0.0 ? 0.0 : a;
    // now shift to 0-1
    r = r / 255.0;
    g = g / 255.0;
    b = b / 255.0;
    a = a / 255.0;
    // if the name has ticks around it, from copying from a mrml file, trim
    // them off the string
    if (name.find("'") != std::string::npos)
    {
      size_t firstnottick = name.find_first_not_of("'");
      size_t lastnottick = name.find_last_not_of("'");
      std::string withoutTicks = name.substr(firstnottick, (lastnottick - firstnottick) + 1);
      vtkDebugMacro("ReadDataInternal: Found ticks around name \"" << name << "\", using name without ticks instead:  \"" << withoutTicks << "\"");
      name = withoutTicks;
    }
    if (i < 10)
    {
      vtkDebugMacro("(first ten) Adding color at id " << id << ", name = " << name.c_str() << ", r = " << r << ", g = " << g << ", b = " << b << ", a = " << a);
    }
    if (colorNode->SetColor(id, name.c_str(), r, g, b, a) == 0)
    {
      vtkWarningMacro("ReadData: unable to set color " << id << " with name " << name.c_str()
        << ", breaking the loop over " << lines.size() << " lines in the file " << this->FileName);
      return 0;
    }
    colorNode->SetColorNameWithSpaces(id, name.c_str(), "_");
  }
  if (lines.size() > 0 && !biggerThanOne)
  {
    vtkWarningMacro("ReadDataInternal: possibly malformed color table file:\n" << this->FileName
      << ".\n\tNo RGB values are greater than 1. Valid values are 0-255");
  }

  return 1;
}

//----------------------------------------------------------------------------
int vtkMRMLColorTableStorageNode::WriteDataInternal(vtkMRMLNode *refNode)
{
  std::string fullName = this->GetFullNameFromFileName();
  if (fullName.empty())
  {
    vtkErrorMacro("vtkMRMLColorTableStorageNode: File name not specified");
    return 0;
  }

  // cast the input node
  vtkMRMLColorTableNode *colorNode = nullptr;
  if ( refNode->IsA("vtkMRMLColorTableNode") )
  {
    colorNode = dynamic_cast <vtkMRMLColorTableNode *> (refNode);
  }

  if (colorNode == nullptr)
  {
    vtkErrorMacro("WriteData: unable to cast input node " << refNode->GetID() << " to a known color table node");
    return 0;
  }

  // open the file for writing
  fstream of;

  of.open(fullName.c_str(), fstream::out);

  if (!of.is_open())
  {
    vtkErrorMacro("WriteData: unable to open file " << fullName.c_str() << " for writing");
    return 0;
  }

  // put down a header
  of << "# Color table file " << (this->GetFileName() != nullptr ? this->GetFileName() : "null") << endl;
  if (colorNode->GetLookupTable() != nullptr)
  {
    of << "# " << colorNode->GetLookupTable()->GetNumberOfTableValues() << " values" << endl;
    for (int i = 0; i < colorNode->GetLookupTable()->GetNumberOfTableValues(); i++)
    {
      // is it an unnamed color?
      if (colorNode->GetNoName() &&
          colorNode->GetColorName(i) &&
          strcmp(colorNode->GetNoName(), colorNode->GetColorName(i)) == 0)
      {
        continue;
      }

      double *rgba;
      rgba = colorNode->GetLookupTable()->GetTableValue(i);
      // the color look up table uses 0-1, file values are 0-255,
      double r = rgba[0] * 255.0;
      double g = rgba[1] * 255.0;
      double b = rgba[2] * 255.0;
      double a = rgba[3] * 255.0;
      of << i;
      of << " ";
      of << colorNode->GetColorNameWithoutSpaces(i, "_");
      of << " ";
      of << r;
      of << " ";
      of << g;
      of << " ";
      of << b;
      of << " ";
      of << a;
      of << endl;
    }
  }
  of.close();

  return 1;
}

//----------------------------------------------------------------------------
void vtkMRMLColorTableStorageNode::InitializeSupportedReadFileTypes()
{
  //: File format name
  this->SupportedReadFileTypes->InsertNextValue(vtkMRMLTr("vtkMRMLColorTableStorageNode", "MRML Color Table") + " (.ctbl)");
  //: File format name
  this->SupportedReadFileTypes->InsertNextValue(vtkMRMLTr("vtkMRMLColorTableStorageNode", "MRML Color Table") + " (.txt)");
}

//----------------------------------------------------------------------------
void vtkMRMLColorTableStorageNode::InitializeSupportedWriteFileTypes()
{
  //: File format name
  this->SupportedWriteFileTypes->InsertNextValue(vtkMRMLTr("vtkMRMLColorTableStorageNode", "MRML Color Table") + " (.ctbl)");
  //: File format name
  this->SupportedWriteFileTypes->InsertNextValue(vtkMRMLTr("vtkMRMLColorTableStorageNode", "MRML Color Table") + " (.txt)");
}
