#include <map>
#include "vtkTeemNRRDWriter.h"
#include "vtkImageData.h"
#include "vtkPointData.h"
#include "vtkObjectFactory.h"
#include "vtkInformation.h"
#include <vtkVersion.h>

#include <vnl/vnl_math.h>
#include <vnl/vnl_double_3.h>

#include "itkNumberToString.h"

class AttributeMapType: public std::map<std::string, std::string> {};
class AxisInfoMapType : public std::map<unsigned int, std::string> {};

vtkStandardNewMacro(vtkTeemNRRDWriter);

//----------------------------------------------------------------------------
vtkTeemNRRDWriter::vtkTeemNRRDWriter()
{
  this->FileName = NULL;
  this->BValues = vtkDoubleArray::New();
  this->DiffusionGradients = vtkDoubleArray::New();
  this->IJKToRASMatrix = vtkMatrix4x4::New();
  this->MeasurementFrameMatrix = vtkMatrix4x4::New();
  this->UseCompression = 1;
  // use default CompressionLevel
  this->CompressionLevel = -1;
  this->DiffusionWeigthedData = 0;
  this->FileType = VTK_BINARY;
  this->WriteErrorOff();
  this->Attributes = new AttributeMapType;
  this->AxisLabels = new AxisInfoMapType;
  this->AxisUnits = new AxisInfoMapType;
  this->VectorAxisKind = nrrdKindUnknown;
#ifdef NRRD_CHUNK_IO_AVAILABLE
  this->WriteMultipleImagesAsImageList = false;
  this->NumberOfImages = 1;
  this->CurrentImageIndex = 0;
#endif
  this->nrrd = NULL;
  this->nio = NULL;
}

//----------------------------------------------------------------------------
vtkTeemNRRDWriter::~vtkTeemNRRDWriter()
{
  this->SetFileName(NULL);
  this->SetDiffusionGradients(NULL);
  this->SetBValues(NULL);
  this->SetIJKToRASMatrix(NULL);
  this->SetMeasurementFrameMatrix(NULL);
  delete this->Attributes;
  this->Attributes = NULL;
  delete this->AxisLabels;
  this->AxisLabels = NULL;
  delete this->AxisUnits;
  this->AxisUnits = NULL;
}

//----------------------------------------------------------------------------
vtkImageData* vtkTeemNRRDWriter::GetInput()
{
  return vtkImageData::SafeDownCast(this->Superclass::GetInput());
}

//----------------------------------------------------------------------------
vtkImageData* vtkTeemNRRDWriter::GetInput(int port)
{
  return vtkImageData::SafeDownCast(this->Superclass::GetInput(port));
}

//----------------------------------------------------------------------------
int vtkTeemNRRDWriter::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;

}

//----------------------------------------------------------------------------
// Writes all the data from the input.
void vtkTeemNRRDWriter::vtkImageDataInfoToNrrdInfo(vtkImageData *in, int &kind, size_t &numComp, int &vtkType, void **buffer)
{
  if (in == NULL)
    {
    vtkErrorMacro(<< "vtkTeemNRRDWriter::vtkImageDataInfoToNrrdInfo: "
                     "no input vtkImageData found.");
    return;
    }

  vtkDataArray *array;
  this->DiffusionWeigthedData = 0;
  if ((array = static_cast<vtkDataArray *> (in->GetPointData()->GetScalars())))
    {
    numComp = array->GetNumberOfComponents();
    vtkType = array->GetDataType();
    (*buffer) = array->GetVoidPointer(0);

    switch (numComp)
      {
      case 1:
        kind = nrrdKindScalar;
        break;
      case 2:
        kind = nrrdKindComplex;
        break;
      case 3:
        kind = nrrdKindRGBColor;
        break;
      case 4:
        kind = nrrdKindRGBAColor;
        break;
      default:
        size_t numGrad = this->DiffusionGradients->GetNumberOfTuples();
        size_t numBValues = this->BValues->GetNumberOfTuples();
        if (numGrad == numBValues && numGrad == numComp && numGrad>6)
          {
          kind = nrrdKindList;
          this->DiffusionWeigthedData = 1;
          }
        else
          {
          kind = nrrdKindList;
          }
       }
     }
   else if ((array = static_cast<vtkDataArray *> ( in->GetPointData()->GetVectors())))
     {
     *buffer = array->GetVoidPointer(0);
     vtkType = array->GetDataType();
     kind = nrrdKindVector;
     }
   else if ((array = static_cast<vtkDataArray *> ( in->GetPointData()->GetNormals())))
     {
     *buffer = array->GetVoidPointer(0);
     vtkType = array->GetDataType();
     kind = nrrdKindVector;
     numComp = array->GetNumberOfComponents();
     }
   else if ((array = static_cast<vtkDataArray *> ( in->GetPointData()->GetTensors())))
     {
     *buffer = array->GetVoidPointer(0);
     vtkType = array->GetDataType();
     kind = nrrdKind3DMatrix;
     numComp = array->GetNumberOfComponents();
     }

  if (this->VectorAxisKind != nrrdKindUnknown)
    {
    kind = this->VectorAxisKind;
    }
}

//----------------------------------------------------------------------------
void vtkTeemNRRDWriter::updateNRRDDatapointer(vtkImageData *in)
{
  if (in == NULL)
    {
    vtkErrorMacro(<< "vtkTeemNRRDWriter::updateNRRDDatapointer: "
                     "no input vtkImageData found.");
    this->nrrd->data = NULL;
    return;
    }

  void *buffer;
  vtkDataArray *array;

  if ((array = static_cast<vtkDataArray *> (in->GetPointData()->GetScalars())))
    {
    buffer = array->GetVoidPointer(0);
    }
  else if ((array = static_cast<vtkDataArray *> (in->GetPointData()->GetVectors())))
    {
    buffer = array->GetVoidPointer(0);
    }
  else if ((array = static_cast<vtkDataArray *> (in->GetPointData()->GetNormals())))
    {
    buffer = array->GetVoidPointer(0);
    }
  else if ((array = static_cast<vtkDataArray *> (in->GetPointData()->GetTensors())))
    {
    buffer = array->GetVoidPointer(0);
    }
  else
    {
    vtkErrorMacro(<< "vtkTeemNRRDWriter::updateNRRDDatapointer: "
                     "the type of the input vtkImageData is not compatible.");
    this->nrrd->data = NULL;
    return;
    }

  this->nrrd->data = buffer;
}

//----------------------------------------------------------------------------
int vtkTeemNRRDWriter::VTKToNrrdPixelType( const int vtkPixelType )
  {
  switch( vtkPixelType )
    {
    default:
    case VTK_VOID:
      return nrrdTypeDefault;
      break;
    case VTK_CHAR:
      return nrrdTypeChar;
      break;
    case VTK_UNSIGNED_CHAR:
      return nrrdTypeUChar;
      break;
    case VTK_SHORT:
      return nrrdTypeShort;
      break;
    case VTK_UNSIGNED_SHORT:
      return nrrdTypeUShort;
      break;
      //    case nrrdTypeLLong:
      //      return LONG ;
      //      break;
      //    case nrrdTypeULong:
      //      return ULONG;
      //      break;
    case VTK_INT:
      return nrrdTypeInt;
      break;
    case VTK_UNSIGNED_INT:
      return nrrdTypeUInt;
      break;
    case VTK_FLOAT:
      return nrrdTypeFloat;
      break;
    case VTK_DOUBLE:
      return nrrdTypeDouble;
      break;
    }
  }

//----------------------------------------------------------------------------
void* vtkTeemNRRDWriter::MakeNRRD()
  {
  Nrrd *nrrd = nrrdNew();
  int kind[NRRD_DIM_MAX];
  size_t size[NRRD_DIM_MAX];
  unsigned int nrrdDim, baseDim, spaceDim;
  double spaceDir[NRRD_DIM_MAX][NRRD_SPACE_DIM_MAX];
  double origin[NRRD_DIM_MAX];
  void *buffer;
  int vtkType;

  // Find Pixel type from data and select a buffer.
  this->vtkImageDataInfoToNrrdInfo(this->GetInput(),kind[0],size[0],vtkType, &buffer);

  spaceDim = 3; // VTK is always 3D volumes.
#ifdef NRRD_CHUNK_IO_AVAILABLE
  if (size[0] > 1 || this->GetWriteMultipleImagesAsImageList())
#else
  if (size[0] > 1)
#endif
  {
    // the range axis has no space direction
    for (unsigned int saxi=0; saxi < spaceDim; saxi++)
      {
      spaceDir[0][saxi] = AIR_NAN;
      }
    baseDim = 1;
    }
  else
    {
    baseDim = 0;
    }
  nrrdDim = baseDim + spaceDim;

  unsigned int axi;
  for (axi = 0; axi < spaceDim; axi++)
    {
#ifndef NRRD_CHUNK_IO_AVAILABLE
    size[axi+baseDim] = this->GetInput()->GetDimensions()[axi];
    kind[axi+baseDim] = nrrdKindDomain;
#else
    size[axi] = this->GetInput()->GetDimensions()[axi];
    kind[axi] = nrrdKindDomain;
#endif
    origin[axi] = this->IJKToRASMatrix->GetElement((int) axi, 3);
    //double spacing = this->GetInput()->GetSpacing()[axi];
    for (unsigned int saxi=0; saxi < spaceDim; saxi++)
      {
#ifndef NRRD_CHUNK_IO_AVAILABLE
      spaceDir[axi+baseDim][saxi] = this->IJKToRASMatrix->GetElement(saxi,axi);
#else
      spaceDir[axi][saxi] = this->IJKToRASMatrix->GetElement(saxi,axi);
#endif
      }
    }
#ifdef NRRD_CHUNK_IO_AVAILABLE
  if (this->GetWriteMultipleImagesAsImageList())
    {
    size[3] = this->GetNumberOfImages();
    kind[3] = nrrdKindList;
    for (unsigned int saxi=0; saxi < spaceDim; saxi++)
      {
      spaceDir[3][saxi] = AIR_NAN;
      }
    }
#endif

  if (nrrdWrap_nva(nrrd, const_cast<void *> (buffer),
                   this->VTKToNrrdPixelType( vtkType ),
                   nrrdDim, size)
      || nrrdSpaceDimensionSet(nrrd, spaceDim)
      || nrrdSpaceOriginSet(nrrd, origin))
    {
    char *err = biffGetDone(NRRD); // would be nice to free(err)
    vtkErrorMacro(<< "vtkTeemNRRDWriter::MakeNRRD : Error wrapping nrrd for "
                  << this->GetFileName() << ":\n" << err);
    // Free the nrrd struct but don't touch nrrd->data
    nrrd = nrrdNix(nrrd);
    this->WriteErrorOn();
    return NULL;
    }
  nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoKind, kind);
  nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoSpaceDirection, spaceDir);
  nrrd->space = nrrdSpaceRightAnteriorSuperior;

  if (!this->AxisLabels->empty())
    {
    const char* labels[NRRD_DIM_MAX] = { 0 };
    for (unsigned int axi = 0; axi < NRRD_DIM_MAX; axi++)
      {
      if (this->AxisLabels->find(axi) != this->AxisLabels->end())
        {
        labels[axi] = (*this->AxisLabels)[axi].c_str();
        }
      }
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoLabel, labels);
    }

  if (!this->AxisUnits->empty())
    {
    const char* units[NRRD_DIM_MAX] = { 0 };
    for (unsigned int axi = 0; axi < NRRD_DIM_MAX; axi++)
      {
      if (this->AxisUnits->find(axi) != this->AxisUnits->end())
        {
        units[axi] = (*this->AxisUnits)[axi].c_str();
        }
      }
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoUnits, units);
    }

  // Write out attributes, diffusion information and the measurement frame.
  //

  // 0. Write out any attributes. Write non-specific attributes first. This allows
  // variables like the diffusion gradients and B-values that are saved as
  // attributes on disk to overwrite the attributes later. (This
  // should ensure the version written to disk has the best information.)
  AttributeMapType::iterator ait;
  for (ait = this->Attributes->begin(); ait != this->Attributes->end(); ++ait)
    {
    // Don't set `space` as k-v. it is handled above, and needs to be a nrrd *field*.
    if (ait->first == "space") { continue; }

    nrrdKeyValueAdd(nrrd, (*ait).first.c_str(), (*ait).second.c_str());
    }

  // 1. Measurement Frame
  if (this->MeasurementFrameMatrix)
    {
    for (unsigned int saxi=0; saxi < nrrd->spaceDim; saxi++)
      {
      for (unsigned int saxj=0; saxj < nrrd->spaceDim; saxj++)
        {
        // Note the transpose: each entry in the nrrd measurementFrame
        // is a column of the matrix
        nrrd->measurementFrame[saxi][saxj] = this->MeasurementFrameMatrix->GetElement(saxj,saxi);
        }
      }
    }

  // use double-conversion library (via ITK) for better
  // float64 string representation.
  itk::NumberToString<double> DoubleConvert;

  // 2. Take care about diffusion data
  if (this->DiffusionWeigthedData)
    {
    unsigned int numGrad = this->DiffusionGradients->GetNumberOfTuples();
    unsigned int numBValues = this->BValues->GetNumberOfTuples();

    if (kind[0] == nrrdKindList && numGrad == size[0] && numBValues == size[0])
      {
      // This is diffusion Data
      vnl_double_3 grad;
      double bVal, factor;
      double maxbVal = this->BValues->GetRange()[1];

      std::string modality_key("modality");
      std::string modality_value("DWMRI");
      nrrdKeyValueAdd(nrrd, modality_key.c_str(), modality_value.c_str());

      std::string bval_key("DWMRI_b-value");
      std::stringstream bval_value;
      bval_value << DoubleConvert(maxbVal);
      nrrdKeyValueAdd(nrrd, bval_key.c_str(), bval_value.str().c_str());

      for (unsigned int ig =0; ig < numGrad; ig++)
        {
        // key
        std::stringstream key_stream;
        key_stream << "DWMRI_gradient_" << setfill('0') << setw(4) << ig;

        // gradient value
        grad.copy_in(this->DiffusionGradients->GetTuple3(ig));

        bVal = this->BValues->GetValue(ig);
        // per NA-MIC DWI convention
        factor = sqrt(bVal/maxbVal);
        std::stringstream value_stream;
        value_stream << std::setprecision(17) <<
                        DoubleConvert(grad[0] * factor) << "   " <<
                        DoubleConvert(grad[1] * factor) << "   " <<
                        DoubleConvert(grad[2] * factor);

        nrrdKeyValueAdd(nrrd, key_stream.str().c_str(), value_stream.str().c_str());
        }
      }
    }
  return nrrd;
}

//----------------------------------------------------------------------------
Nrrd *vtkTeemNRRDWriter::GetNRRDTeem()
{
  return this->nrrd;
}

//----------------------------------------------------------------------------
NrrdIoState *vtkTeemNRRDWriter::GetNRRDIoTeem()
{
  return this->nio;
}

//----------------------------------------------------------------------------
// Writes all the data from the input.
void vtkTeemNRRDWriter::WriteData()
{
  this->WriteErrorOff();
  if (this->GetFileName() == NULL)
    {
    vtkErrorMacro(<< "vtkTeemNRRDWriter::WriteData : "
                     "FileName has not been set. Cannot save file");
    this->WriteErrorOn();
    return;
    }

  // Check if GetWriteMultipleImagesAsImageList is on
#ifdef NRRD_CHUNK_IO_AVAILABLE
  int imageIndex = 0;
  int numberOfImages = 1;
  if (this->GetWriteMultipleImagesAsImageList())
    {
    imageIndex = this->GetCurrentImageIndex();
    numberOfImages = this->GetNumberOfImages();
    }
#endif

  if (this->nrrd == NULL || this->nio == NULL)
    {
    this->InitializeNRRDTeem();
    }

  this->updateNRRDDatapointer(this->GetInput());

  // set encoding for data: compressed (raw), (uncompressed) raw, or ascii
  if (this->GetUseCompression() && nrrdEncodingGzip->available())
    {
    // this is necessarily gzip-compressed *raw* data
    this->nio->encoding = nrrdEncodingGzip;
    this->nio->zlibLevel = this->CompressionLevel;
    }
  else
    {
    int fileType = this->GetFileType();
    switch (fileType)
      {
      default:
      case VTK_BINARY:
        this->nio->encoding = nrrdEncodingRaw;
        break;
      case VTK_ASCII:
        this->nio->encoding = nrrdEncodingAscii;
        break;
      }
    }

  // set endianness as unknown of output
  this->nio->endian = airEndianUnknown;

#ifdef NRRD_CHUNK_IO_AVAILABLE
  if (this->GetWriteMultipleImagesAsImageList())
    {
    long int numberOfImageElements = nrrdElementNumber(this->nrrd) / numberOfImages;
    this->nio->chunkElementCount = numberOfImageElements;
    this->nio->chunkStartElement = numberOfImageElements * imageIndex;
    if (imageIndex == 0)
      {
      this->nio->keepNrrdDataFileOpen = 0;
      }
    else
      {
      this->nio->keepNrrdDataFileOpen = 1;
      }
    }
#endif

  // Write the nrrd to file.
  if (nrrdSave(this->GetFileName(), this->nrrd, this->nio))
    {
    char *err = biffGetDone(NRRD); // would be nice to free(err)
    vtkErrorMacro(<< "vtkTeemNRRDWriter::WriteData : Error writing "
                  << this->GetFileName() << ":\n" << err);
    this->WriteErrorOn();
    }

#ifdef NRRD_CHUNK_IO_AVAILABLE
  if (!this->GetWriteMultipleImagesAsImageList())
    {
    // Free the nrrd struct but don't touch nrrd->data
    this->nrrd = nrrdNix(this->nrrd);
    this->nio = nrrdIoStateNix(this->nio);
    }
#endif

  return;
}

//----------------------------------------------------------------------------
void vtkTeemNRRDWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "RAS to IJK Matrix: ";
     this->IJKToRASMatrix->PrintSelf(os,indent);
  os << indent << "Measurement frame: ";
     this->MeasurementFrameMatrix->PrintSelf(os,indent);
}

//----------------------------------------------------------------------------
void vtkTeemNRRDWriter::SetAttribute(const std::string& name, const std::string& value)
{
  if (!this->Attributes)
    {
    return;
    }

  (*this->Attributes)[name] = value;
}

//----------------------------------------------------------------------------
std::string vtkTeemNRRDWriter::GetAttribute(const std::string& name)
{
  if (!this->Attributes)
    {
    return "";
    }

  return (*this->Attributes)[name];
}

//----------------------------------------------------------------------------
void vtkTeemNRRDWriter::SetAxisLabel(unsigned int axis, const char* label)
{
  if (!this->AxisLabels)
    {
    return;
    }
  if (label)
    {
    (*this->AxisLabels)[axis] = label;
    }
  else
    {
    this->AxisLabels->erase(axis);
    }
}

//----------------------------------------------------------------------------
void vtkTeemNRRDWriter::SetAxisUnit(unsigned int axis, const char* unit)
{
  if (!this->AxisUnits)
    {
    return;
    }
  if (unit)
    {
    (*this->AxisUnits)[axis] = unit;
    }
  else
    {
    this->AxisLabels->erase(axis);
    }
}

//----------------------------------------------------------------------------
void vtkTeemNRRDWriter::SetVectorAxisKind(int kind)
{
  this->VectorAxisKind = kind;
}

//----------------------------------------------------------------------------
void vtkTeemNRRDWriter::InitializeNRRDTeem()
{
  this->nrrd = (Nrrd*)this->MakeNRRD();
  if (this->nrrd == NULL)
    {
    vtkErrorMacro(<< "vtkTeemNRRDWriter::InitializeNRRDTeem : "
      "failed to instantiate nrrd teem pointer.");
    }

  this->nio = nrrdIoStateNew();
  if (this->nio == NULL)
    {
    vtkErrorMacro(<< "vtkTeemNRRDWriter::InitializeNRRDTeem : "
      "failed to instantiate nrrdIo teem pointer.");
    }
}
