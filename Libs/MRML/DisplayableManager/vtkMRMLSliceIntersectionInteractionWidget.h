/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkMRMLSliceIntersectionInteractionWidget.cxx

  Copyright (c) Ebatinca S.L., Las Palmas de Gran Canaria, Spain
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkMRMLSliceIntersectionInteractionWidget
 * @brief   Show slice intersection lines
 *
 * The vtkMRMLSliceIntersectionInteractionWidget allows moving slices by interacting with
 * displayed slice intersection lines.
 *
 *
*/
#ifndef vtkMRMLSliceIntersectionInteractionWidget_h
#define vtkMRMLSliceIntersectionInteractionWidget_h

#include "vtkMRMLDisplayableManagerExport.h"

#include "vtkMRMLAbstractWidget.h"
#include "vtkMRMLSliceLogic.h"
#include "vtkMRMLSliceNode.h"

#include <vtkCallbackCommand.h>
#include <vtkCollection.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

#include "vtkMRMLInteractionWidget.h"
#include "vtkWidgetCallbackMapper.h"

class vtkSliceIntersectionRepresentation2D;
class vtkMRMLApplicationLogic;
class vtkMRMLSegmentationDisplayNode;
class vtkMRMLApplicationLogic;
class vtkMRMLDisplayableNode;
class vtkMRMLInteractionEventData;
class vtkMRMLInteractionNode;
class vtkIdList;
class vtkPolyData;
class vtkTransform;
class vtkMRMLTransformDisplayNode;
class vtkMRMLTransformNode;

class VTK_MRML_DISPLAYABLEMANAGER_EXPORT vtkMRMLSliceIntersectionInteractionWidget : public vtkMRMLInteractionWidget
{
public:
    /**
     * Instantiate this class.
     */
    static vtkMRMLSliceIntersectionInteractionWidget* New();

    //@{
    /**
     * Standard VTK class macros.
     */
    vtkTypeMacro(vtkMRMLSliceIntersectionInteractionWidget, vtkMRMLInteractionWidget);
    void PrintSelf(ostream& os, vtkIndent indent) override;
    //@}

    /// Create the default widget representation and initializes the widget and representation.
    virtual void CreateDefaultRepresentation(vtkMRMLSliceNode* sliceNode, vtkMRMLAbstractViewNode* viewNode, vtkRenderer* renderer);

    virtual vtkMRMLSliceNode* GetSliceNode();

    virtual vtkMatrix4x4* GetHandlesInteractionTransformMatrix ();
    virtual void UpdateHandlesInteractionTransformMatrix (vtkMatrix4x4* transform);

    int GetActiveComponentType() override;
    void SetActiveComponentType(int type) override;

    int GetActiveComponentIndex() override;
    void SetActiveComponentIndex(int type) override;

protected:
    vtkMRMLSliceIntersectionInteractionWidget();
    ~vtkMRMLSliceIntersectionInteractionWidget() override;

    void ApplyTransform(vtkTransform* transform) override;

private:
    vtkMRMLSliceIntersectionInteractionWidget(const vtkMRMLSliceIntersectionInteractionWidget&) = delete;
    void operator=(const vtkMRMLSliceIntersectionInteractionWidget&) = delete;
};

#endif