/*=auto=========================================================================

Portions (c) Copyright 2017 Brigham and Women's Hospital (BWH) All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

=========================================================================auto=*/

#include "vtkCodedEntry.h"

// VTK include
#include <vtkObjectFactory.h>

// STD include
#include <sstream>

vtkStandardNewMacro(vtkCodedEntry);

//----------------------------------------------------------------------------
vtkCodedEntry::vtkCodedEntry() = default;

//----------------------------------------------------------------------------
vtkCodedEntry::~vtkCodedEntry()
{
  this->Initialize();
}

//----------------------------------------------------------------------------
void vtkCodedEntry::Initialize()
{
  this->SetCodeValue(nullptr);
  this->SetCodingSchemeDesignator(nullptr);
  this->SetCodeMeaning(nullptr);
}

//----------------------------------------------------------------------------
void vtkCodedEntry::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);
  os << indent << "CodeValue: " << (this->CodeValue?this->CodeValue:"(none)") << "\n";
  os << indent << "CodingSchemeDesignator: " << (this->CodingSchemeDesignator ? this->CodingSchemeDesignator : "(none)") << "\n";
  os << indent << "CodeMeaning: " << (this->CodeMeaning ? this->CodeMeaning : "(none)") << "\n";
}

//----------------------------------------------------------------------------
void vtkCodedEntry::Copy(vtkCodedEntry* aEntry)
{
  if (!aEntry)
    {
    return;
    }
  this->SetCodeValue(aEntry->GetCodeValue());
  this->SetCodingSchemeDesignator(aEntry->GetCodingSchemeDesignator());
  this->SetCodeMeaning(aEntry->GetCodeMeaning());
}

//----------------------------------------------------------------------------
void vtkCodedEntry::SetValueSchemeMeaning(const std::string& value,
  const std::string& scheme, const std::string& meaning)
{
  this->SetCodeValue(value.c_str());
  this->SetCodingSchemeDesignator(scheme.c_str());
  this->SetCodeMeaning(meaning.c_str());
}

//----------------------------------------------------------------------------
std::string vtkCodedEntry::GetAsPrintableString()
{
  std::string printable = std::string("(")
    + (this->CodeValue ? this->CodeValue : "(none)") + ", "
    + (this->CodingSchemeDesignator ? this->CodingSchemeDesignator : "(none)") + ", \""
    + (this->CodeMeaning ? this->CodeMeaning : "") + "\")";
  return printable;
}

//----------------------------------------------------------------------------
std::string vtkCodedEntry::GetAsString()
{
  std::string str;
  if (this->CodeValue)
    {
    str += "CodeValue:";
    str += this->CodeValue;
    }
  if (this->CodingSchemeDesignator)
    {
    if (!str.empty())
      {
      str += "|";
      }
    str += "CodingSchemeDesignator:";
    str += this->CodingSchemeDesignator;
    }
  if (this->CodeMeaning)
    {
    if (!str.empty())
      {
      str += "|";
      }
    str += "CodeMeaning:";
    str += this->CodeMeaning;
    }
  return str;
}

//----------------------------------------------------------------------------
bool vtkCodedEntry::SetFromString(const std::string& content)
{
  this->Initialize();
  bool success = true;
  std::stringstream attributes(content);
  std::string attribute;
  while (std::getline(attributes, attribute, '|'))
    {
    int colonIndex = attribute.find(':');
    std::string name = attribute.substr(0, colonIndex);
    std::string value = attribute.substr(colonIndex + 1);
    if (name == "CodeValue")
      {
      this->SetCodeValue(value.c_str());
      }
    else if (name == "CodingSchemeDesignator")
      {
      this->SetCodingSchemeDesignator(value.c_str());
      }
    else if (name == "CodeMeaning")
      {
      this->SetCodeMeaning(value.c_str());
      }
    else
      {
      vtkWarningMacro("Parsing coded entry string failed: unknown name " << name << " in " + content);
      success = false;
      }
    }
  if (this->GetCodeValue() == nullptr)
    {
    vtkWarningMacro("Parsing coded entry string failed: CodeValue is not specified in " + content);
    success = false;
    }
  if (this->GetCodingSchemeDesignator() == nullptr)
    {
    vtkWarningMacro("Parsing coded entry string failed: CodingSchemeDesignator is not specified in " + content);
    success = false;
    }
  // CodeMeaning is optional
  return success;
}

//----------------------------------------------------------------------------
bool vtkCodedEntry::IsMatching(vtkCodedEntry* other)
{
  // Get scheme and value from this object as strings
  std::string thisCodingSchemeDesignator;
  std::string thisCodeValue;
  if (this->CodingSchemeDesignator)
    {
    thisCodingSchemeDesignator = this->CodingSchemeDesignator;
    }
  if (this->CodeValue)
    {
    thisCodeValue = this->CodeValue;
    }

  // Get scheme and value from the other object as strings
  std::string otherCodingSchemeDesignator;
  std::string otherCodeValue;
  if (other)
    {
    if (other->GetCodingSchemeDesignator())
      {
      otherCodingSchemeDesignator = other->GetCodingSchemeDesignator();
      }
    if (other->GetCodeValue())
      {
      otherCodeValue = other->GetCodeValue();
      }
    }

  // Compare
  return (thisCodingSchemeDesignator == otherCodingSchemeDesignator
    && thisCodeValue == otherCodeValue);
}

//----------------------------------------------------------------------------
bool vtkCodedEntry::IsEmpty()
{
  if (this->CodeValue)
    {
    if (strlen(this->CodeValue) > 0)
      {
      return false;
      }
    }
  if (this->CodingSchemeDesignator)
    {
    if (strlen(this->CodingSchemeDesignator) > 0)
      {
      return false;
      }
    }
  if (this->CodeMeaning)
    {
    if (strlen(this->CodeMeaning) > 0)
      {
      return false;
      }
    }
  return true;
}

//----------------------------------------------------------------------------
bool vtkCodedEntry::IsEqual(vtkCodedEntry* other)
{
  if (!this->IsMatching(other))
    {
    return false;
    }

  std::string thisCodeMeaning;
  if (this->CodeMeaning)
    {
    thisCodeMeaning = this->CodeMeaning;
    }
  std::string otherCodeMeaning;
  if (other && other->GetCodeMeaning())
    {
    otherCodeMeaning = other->GetCodeMeaning();
    }

  // Compare
  return (thisCodeMeaning == otherCodeMeaning);
}
