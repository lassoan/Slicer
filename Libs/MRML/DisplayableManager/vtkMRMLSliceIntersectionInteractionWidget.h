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

class VTK_MRML_DISPLAYABLEMANAGER_EXPORT vtkMRMLSliceIntersectionInteractionWidget : public vtkMRMLAbstractWidget
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
    vtkTypeMacro(vtkMRMLSliceIntersectionInteractionWidget, vtkMRMLAbstractWidget);
    void PrintSelf(ostream& os, vtkIndent indent) override;
    //@}

    /**
     * Create the default widget representation if one is not set.
     */
    void CreateDefaultRepresentation();

    void SetSliceNode(vtkMRMLSliceNode* sliceNode);
    vtkMRMLSliceNode* GetSliceNode();

    void SetMRMLApplicationLogic(vtkMRMLApplicationLogic* applicationLogic) override;

    /// Return true if the widget can process the event.
    bool CanProcessInteractionEvent(vtkMRMLInteractionEventData* eventData, double& distance2) override;

    /// Process interaction event.
    bool ProcessInteractionEvent(vtkMRMLInteractionEventData* eventData) override;

    /// Called when the the widget loses the focus.
    void Leave(vtkMRMLInteractionEventData* eventData) override;

    /// Interaction with handles
    enum
      {
      InteractionNone,
      InteractionTranslationHandle,
      InteractionRotationHandle,
      InteractionSliceOffsetHandle,
      InteractionIntersectionLine,
      };

    /// Widget states
    enum
      {
      WidgetStateOnTranslationHandle = WidgetStateUser, // hovering over a translation interaction handle
      WidgetStateOnRotationHandle, // hovering over a rotation interaction handle
      WidgetStateOnSliceTranslationHandle, // hovering over a slice translation interaction handle
      WidgetStateOnIntersectionLine, // hovering over a intersection line
      WidgetStateTranslateSlice,
      WidgetStateTouchGesture,
      WidgetState_Last
      };

    /// Widget events
    enum
      {
      WidgetEventTouchGestureStart = WidgetEventUser,
      WidgetEventTouchGestureEnd,
      WidgetEventTouchRotateSliceIntersection,
      WidgetEventTouchZoomSlice,
      WidgetEventTouchTranslateSlice,
      WidgetEventBlendStart,
      WidgetEventBlendEnd,
      WidgetEventToggleLabelOpacity,
      WidgetEventToggleForegroundOpacity,
      WidgetEventIncrementSlice,
      WidgetEventDecrementSlice,
      WidgetEventZoomInSlice,
      WidgetEventZoomOutSlice,
      WidgetEventToggleSliceVisibility,
      WidgetEventToggleAllSlicesVisibility, // currently does not work, only toggles current slice
      WidgetEventResetFieldOfView,
      WidgetEventShowNextBackgroundVolume,
      WidgetEventShowPreviousBackgroundVolume,
      WidgetEventShowNextForegroundVolume,
      WidgetEventShowPreviousForegroundVolume,
      WidgetEventTranslateSliceStart,
      WidgetEventTranslateSliceEnd,
      WidgetEventZoomSliceStart,
      WidgetEventZoomSliceEnd,
      WidgetEventSetCrosshairPosition,
      };

    /// Action State values and management
    enum
      {
      ActionNone = 0,
      ActionTranslate = 1,
      ActionZoom = 2,
      ActionRotate = 4, /* not used */
      ActionBlend = 8, /* fg to bg, labelmap to bg */
      ActionBrowseSlice = 64,
      ActionShowSlice = 128,
      ActionAdjustLightbox = 256,
      ActionSelectVolume = 512,
      ActionSetCursorPosition = 1024, /* adjust cursor position in crosshair node as mouse is moved */
      ActionSetCrosshairPosition = 2048, /* adjust cursor position in crosshair node as mouse is moved */
      ActionTranslateSliceIntersection = 4096,
      ActionRotateSliceIntersection = 8192,
      ActionAll = ActionTranslate | ActionZoom | ActionRotate | ActionBlend
      | ActionBrowseSlice | ActionShowSlice | ActionAdjustLightbox | ActionSelectVolume
      | ActionSetCursorPosition | ActionSetCrosshairPosition
      | ActionTranslateSliceIntersection | ActionRotateSliceIntersection
      };

    /// Set exact list of actions to enable.
    void SetActionsEnabled(int actions);

    /// Set full list of enabled actions.
    int GetActionsEnabled();

    /// Enable/disable the specified action (Translate, Zoom, Blend, etc.).
    /// Multiple actions can be specified by providing the sum of action ids.
    /// Set the value to AllActionsMask to enable/disable all actions.
    /// All actions are enabled by default.
    void SetActionEnabled(int actionsMask, bool enable = true);
    /// Returns true if the specified action is allowed.
    /// If multiple actions are specified, the return value is true if all actions are enabled.
    bool GetActionEnabled(int actionsMask);

    void UpdateInteractionEventMapping();

protected:
    vtkMRMLSliceIntersectionInteractionWidget();
    ~vtkMRMLSliceIntersectionInteractionWidget() override;

    bool ProcessStartMouseDrag(vtkMRMLInteractionEventData* eventData);
    bool ProcessMouseMove(vtkMRMLInteractionEventData* eventData);
    bool ProcessEndMouseDrag(vtkMRMLInteractionEventData* eventData);

    bool ProcessRotateStart(vtkMRMLInteractionEventData* eventData);
    bool ProcessRotate(vtkMRMLInteractionEventData* eventData);
    bool ProcessSetCrosshair(vtkMRMLInteractionEventData* eventData);
    double GetSliceRotationAngleRad(double eventPos[2]);

    // Move slice in-plane by click-and-drag
    bool ProcessTranslateSlice(vtkMRMLInteractionEventData* eventData);

    bool ProcessTouchGestureStart(vtkMRMLInteractionEventData* eventData);
    bool ProcessTouchGestureEnd(vtkMRMLInteractionEventData* eventData);
    bool ProcessTouchRotate(vtkMRMLInteractionEventData* eventData);
    bool ProcessTouchTranslate(vtkMRMLInteractionEventData* eventData);

    /// Rotate the message by the specified amount. Used for touchpad events.
    bool Rotate(double sliceRotationAngleRad);

    static void SliceLogicsModifiedCallback(vtkObject* caller, unsigned long eid, void* clientData, void* callData);

    vtkWeakPointer<vtkCollection> SliceLogics;
    vtkWeakPointer<vtkMRMLSliceNode> SliceNode;
    vtkWeakPointer<vtkMRMLSliceLogic> SliceLogic;
    vtkNew<vtkCallbackCommand> SliceLogicsModifiedCommand;

    double StartEventPosition[2];
    double PreviousRotationAngleRad;
    int PreviousEventPosition[2];
    double StartRotationCenter[2];
    double StartRotationCenter_RAS[4];

    int ActionsEnabled;

    double TouchRotationThreshold;
    double TouchTranslationThreshold;
    double TouchZoomThreshold;
    double TotalTouchRotation;
    bool TouchRotateEnabled;
    double TotalTouchTranslation;
    bool TouchTranslationEnabled;
    double TotalTouchZoom;
    bool TouchZoomEnabled;

    int LastIntersectingSliceNodeIndex;

    int activeComponentType;
    int activeComponentIndex;

private:
    vtkMRMLSliceIntersectionInteractionWidget(const vtkMRMLSliceIntersectionInteractionWidget&) = delete;
    void operator=(const vtkMRMLSliceIntersectionInteractionWidget&) = delete;
};

#endif
