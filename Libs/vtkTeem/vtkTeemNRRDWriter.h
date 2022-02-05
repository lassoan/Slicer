
#ifndef __vtkTeemNRRDWriter_h
#define __vtkTeemNRRDWriter_h

#include "vtkWriter.h"

#include "vtkDoubleArray.h"
#include "vtkMatrix4x4.h"
#include "vtkSmartPointer.h"
#include "teem/nrrd.h"

#include "vtkTeemConfigure.h"

class vtkImageData;
class AttributeMapType;
class AxisInfoMapType;

/// \brief Writes PNG files.
///
/// vtkTeemNRRDWriter writes NRRD files.
///
/// \sa vtkTeemNRRDReader
class VTK_Teem_EXPORT vtkTeemNRRDWriter : public vtkWriter
{
public:

  vtkTypeMacro(vtkTeemNRRDWriter,vtkWriter);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  static vtkTeemNRRDWriter *New();

  ///
  /// Get the input to this writer.
  vtkImageData* GetInput();
  vtkImageData* GetInput(int port);

  ///
  /// Specify file name of vtk polygon data file to write.
  vtkSetStringMacro(FileName);
  vtkGetStringMacro(FileName);

  vtkSetObjectMacro(DiffusionGradients,vtkDoubleArray);
  vtkGetObjectMacro(DiffusionGradients,vtkDoubleArray);

  vtkSetObjectMacro(BValues,vtkDoubleArray);
  vtkGetObjectMacro(BValues,vtkDoubleArray);

  vtkSetObjectMacro(IJKToRASMatrix,vtkMatrix4x4);
  vtkGetObjectMacro(IJKToRASMatrix,vtkMatrix4x4);

  vtkSetObjectMacro(MeasurementFrameMatrix,vtkMatrix4x4);
  vtkGetObjectMacro(MeasurementFrameMatrix,vtkMatrix4x4);

  vtkSetMacro(UseCompression,int);
  vtkGetMacro(UseCompression,int);
  vtkBooleanMacro(UseCompression,int);

  vtkSetClampMacro(CompressionLevel, int, 0, 9);
  vtkGetMacro(CompressionLevel, int);

  vtkSetClampMacro(FileType,int,VTK_ASCII,VTK_BINARY);
  vtkGetMacro(FileType,int);
  void SetFileTypeToASCII() {this->SetFileType(VTK_ASCII);};
  void SetFileTypeToBinary() {this->SetFileType(VTK_BINARY);};

#ifdef NRRD_CHUNK_IO_AVAILABLE
  ///
  /// the nrrd file will be written as a list of images.
  /// The method vtkWriter::Write will write only one component at time on file.
  /// The volume that will be written on file is given by CurrentImageIndex
  ///
  /// It is assumed that the nrrd file will be formatted such as:
  /// "kinds: domain domain domain list"
  ///
  /// \sa NumberOfImages, CurrentImageIndex
  void WriteMultipleImagesAsImageListsOn()
  {
    WriteMultipleImagesAsImageList = true;
  }

  ///
  /// the volume to be saved is not a multi-component image list
  void WriteMultipleImagesAsImageListOff()
  {
    WriteMultipleImagesAsImageList = false;
  }

  ///
  /// Set/Get ReadImageListAsMultipleImages (true/false).
  /// \sa SetReadImageListAsMultipleImages(), GetReadImageListAsMultipleImages()
  vtkSetMacro(WriteMultipleImagesAsImageList,bool);
  vtkGetMacro(WriteMultipleImagesAsImageList,bool);

  ///
  /// Number of images
  vtkSetMacro(NumberOfImages,int);
  vtkGetMacro(NumberOfImages,int);

  ///
  /// Sequences frame to be read/written.
  /// The first image index must be zero
  vtkSetMacro(CurrentImageIndex,int);
  vtkGetMacro(CurrentImageIndex,int);
#endif

  vtkBooleanMacro(WriteError, int);
  vtkSetMacro(WriteError, int);
  vtkGetMacro(WriteError, int);

  /// Method to set an attribute that will be passed into the NRRD
  /// file on write
  void SetAttribute(const std::string& name, const std::string& value);

  /// Method to get an attribute that will be passed into the NRRD
  /// file on write
  std::string GetAttribute(const std::string& name);

  /// Method to set label for each axis
  void SetAxisLabel(unsigned int axis, const char* label);

  /// Method to set unit for each axis
  void SetAxisUnit(unsigned int axis, const char* unit);

  /// Method to force axis kind for the non-spatial axis (4th dimension).
  /// If not set (or set to nrrdKindUnknown) then axis kind is guessed
  /// from the number of components and scalar type.
  void SetVectorAxisKind(int kind);

  /// Utility function to return image as a Nrrd*
  void* MakeNRRD();

  /// Get the teem nrrd pointer
  Nrrd* GetNRRDTeem();

  /// Get the teem nrrdIo state pointer
  NrrdIoState* GetNRRDIoTeem();

protected:
  vtkTeemNRRDWriter();
  ~vtkTeemNRRDWriter();

  virtual int FillInputPortInformation(int port, vtkInformation *info) override;

  ///
  /// Write method. It is called by vtkWriter::Write();
  void WriteData() override;

  /// Initialize the teem nrrd pointer
  void InitializeNRRDTeem();

  ///
  /// Flag to set to on when a write error occurred
  int WriteError;

  char *FileName;

  Nrrd* nrrd;
  NrrdIoState *nio;

  vtkDoubleArray* BValues;
  vtkDoubleArray* DiffusionGradients;

  vtkMatrix4x4* IJKToRASMatrix;
  vtkMatrix4x4* MeasurementFrameMatrix;

  int UseCompression;
  int CompressionLevel;
#ifdef NRRD_CHUNK_IO_AVAILABLE
  int NumberOfImages;
  int CurrentImageIndex;
  bool WriteMultipleImagesAsImageList;
#endif
  int FileType;

  AttributeMapType *Attributes;
  AxisInfoMapType *AxisLabels;
  AxisInfoMapType *AxisUnits;
  int VectorAxisKind;

private:
  vtkTeemNRRDWriter(const vtkTeemNRRDWriter&);  /// Not implemented.
  void operator=(const vtkTeemNRRDWriter&);  /// Not implemented.
  void vtkImageDataInfoToNrrdInfo(vtkImageData *in, int &nrrdKind, size_t &numComp, int &vtkType, void **buffer);
  void updateNRRDDatapointer(vtkImageData *in);
  int VTKToNrrdPixelType( const int vtkPixelType );
  int DiffusionWeigthedData;
};

#endif
