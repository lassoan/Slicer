/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMRMLSliceIntersectionInteractionRepresentation.cxx

  Copyright (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkMRMLSliceIntersectionInteractionRepresentation
 * @brief   represent intersections of other slice views in the current slice view
 *
 * @sa
 * vtkSliceIntersectionWidget vtkWidgetRepresentation vtkAbstractWidget
*/
#ifndef vtkMRMLSliceIntersectionInteractionRepresentation_h
#define vtkMRMLSliceIntersectionInteractionRepresentation_h

#include "vtkMRMLDisplayableManagerExport.h"

// MRMLDM includes
#include "vtkMRMLInteractionWidgetRepresentation.h"

// MRML includes
#include <vtkMRMLTransformDisplayNode.h>
#include "vtkMRMLSliceNode.h"

class VTK_MRML_DISPLAYABLEMANAGER_EXPORT vtkMRMLSliceIntersectionInteractionRepresentation : public vtkMRMLInteractionWidgetRepresentation
{
public:
    /**
     * Instantiate this class.
     */
    static vtkMRMLSliceIntersectionInteractionRepresentation* New();

    //@{
    /**
     * Standard VTK class macros.
     */
    vtkTypeMacro(vtkMRMLSliceIntersectionInteractionRepresentation, vtkMRMLInteractionWidgetRepresentation);
    void PrintSelf(ostream& os, vtkIndent indent) override;
    //@}

    virtual int GetActiveComponentType() override;
    virtual void SetActiveComponentType(int type) override;
    virtual int GetActiveComponentIndex() override;
    virtual void SetActiveComponentIndex(int type) override;

    virtual vtkMRMLTransformDisplayNode* GetDisplayNode();
    virtual void SetDisplayNode(vtkMRMLTransformDisplayNode* displayNode);

    virtual vtkMRMLTransformNode* GetTransformNode();

    void UpdateInteractionPipeline() override;

    double GetInteractionScale() override; // Size relative to screen
    double GetInteractionSize() override; // Size in mm
    bool GetInteractionSizeAbsolute() override; // True -> size in mm; false -> relative to screen

    bool IsDisplayable() override;

protected:
    vtkMRMLSliceIntersectionInteractionRepresentation();
    ~vtkMRMLSliceIntersectionInteractionRepresentation() override;

    vtkSmartPointer<vtkMRMLTransformDisplayNode> DisplayNode{ nullptr };

private:
    vtkMRMLSliceIntersectionInteractionRepresentation(const vtkMRMLSliceIntersectionInteractionRepresentation&) = delete;
    void operator=(const vtkMRMLSliceIntersectionInteractionRepresentation&) = delete;
};

#endif