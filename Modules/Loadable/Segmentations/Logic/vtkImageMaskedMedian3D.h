/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImageMaskedMedian3D.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkImageMaskedMedian3D
 * @brief   Median Filter
 *
 * vtkImageMaskedMedian3D is a modified a Median filter that works the same way as
 * vtkImageMaskedMedian3D filter except that it ignores 0 values. It can be
 * used for finding what is the most dominant non-background label value around each point.
 * This operation is intended for slightly expanding label boundaries in a labelmap volume.
 *
 * Implementation is based on vtkImageMedian3D, with only two small changes: do not include
 * background labels in the computation and do not interpolate values if kernel size is even.
 */

#ifndef vtkImageMaskedMedian3D_h
#define vtkImageMaskedMedian3D_h

#include "vtkImageSpatialAlgorithm.h"
#include "vtkSlicerSegmentationsModuleLogicExport.h"

class VTK_SLICER_SEGMENTATIONS_LOGIC_EXPORT vtkImageMaskedMedian3D : public vtkImageSpatialAlgorithm
{
public:
  static vtkImageMaskedMedian3D* New();
  vtkTypeMacro(vtkImageMaskedMedian3D, vtkImageSpatialAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * This method sets the size of the neighborhood.  It also sets the
   * default middle of the neighborhood
   */
  void SetKernelSize(int size0, int size1, int size2);

  ///@{
  /**
   * Return the number of elements in the median mask
   */
  vtkGetMacro(NumberOfElements, int);
  ///@}

protected:
  vtkImageMaskedMedian3D();
  ~vtkImageMaskedMedian3D() override;

  int NumberOfElements;

  void ThreadedRequestData(vtkInformation* request, vtkInformationVector** inputVector,
    vtkInformationVector* outputVector, vtkImageData*** inData, vtkImageData** outData,
    int outExt[6], int id) override;

private:
  vtkImageMaskedMedian3D(const vtkImageMaskedMedian3D&) = delete;
  void operator=(const vtkImageMaskedMedian3D&) = delete;
};

#endif
