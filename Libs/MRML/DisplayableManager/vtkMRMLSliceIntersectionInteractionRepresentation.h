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
#include "vtkMRMLSliceNode.h"
#include "vtkMRMLSliceCompositeNode.h"

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

    virtual vtkMRMLSliceNode* GetSliceNode();
    virtual void SetSliceNode(vtkMRMLSliceNode* sliceNode);

    virtual vtkMatrix4x4* GetHandlesInteractionTransformMatrix ();
    virtual void UpdateHandlesInteractionTransformMatrix (vtkMatrix4x4* transform);

    void UpdateInteractionPipeline() override;
    vtkMRMLSliceCompositeNode* FindSliceCompositeNode();

    double GetInteractionScale() override; // Size relative to screen
    double GetInteractionSize() override; // Size in mm
    bool GetInteractionSizeAbsolute() override; // True -> size in mm; false -> relative to screen

    bool IsDisplayable() override;

protected:
    vtkMRMLSliceIntersectionInteractionRepresentation();
    ~vtkMRMLSliceIntersectionInteractionRepresentation() override;

    vtkSmartPointer<vtkMRMLSliceNode> SliceNode;

    // Transform calculated based on the interaction of the user with the handles
    vtkSmartPointer<vtkMatrix4x4> HandlesInteractionTransformMatrix ;

private:
    vtkMRMLSliceIntersectionInteractionRepresentation(const vtkMRMLSliceIntersectionInteractionRepresentation&) = delete;
    void operator=(const vtkMRMLSliceIntersectionInteractionRepresentation&) = delete;
};

#endif