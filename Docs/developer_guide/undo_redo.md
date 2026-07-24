# Undo/Redo

3D Slicer can undo and redo changes made to the MRML scene. This page describes how the mechanism
works and how to add undo/redo support to your own nodes, logic, and user interface.

## Overview

Undo/redo is implemented at the MRML scene level: the scene keeps a history of saved *states* and
can roll the scene back and forth between them.

- The **undo mechanism must be enabled** on the scene (`vtkMRMLScene::SetUndoOn()`). In the Slicer
  application this is controlled by the **Edit → Application Settings → General → Enable undo/redo**
  option (stored as the `EnableSceneUndo` application setting) and it is on by default.
- A **state is saved** by calling `vtkMRMLScene::SaveStateForUndo()`. This should be done
  *immediately before* a change is applied, so that undo can restore the pre-change state.
- A node participates in undo/redo only if **both** of the following are true:
  1. its class is registered as an *undoable node class* in the scene
     (`vtkMRMLScene::AddUndoableNodeClass()`), and
  2. its `UndoEnabled` flag is set (`vtkMRMLNode::SetUndoEnabled()`, which is `true` by default).

  The two-part condition makes it possible to enable undo selectively for a small set of node types
  (so that unrelated nodes - volumes, cameras, view nodes, etc. - are never swept into the undo
  history), while still allowing an individual node instance to opt out.

Saved states are kept in an undo stack and a redo stack. Saving a new state clears the redo stack.
The number of saved states is limited by `vtkMRMLScene::SetMaximumNumberOfSavedUndoStates()`
(20 by default); older states are discarded when the limit is exceeded.

## How a state is saved and restored

`SaveStateForUndo()` stores a snapshot of all *undoable* nodes currently in the scene. When you call
`vtkMRMLScene::Undo()`:

- nodes that were **added** since the saved state are removed,
- nodes that were **removed** since the saved state are re-added,
- nodes that were **modified** are restored to their saved content.

`vtkMRMLScene::Redo()` reapplies the change. The current scene state is pushed onto the opposite
stack during undo/redo, so the operation is reversible.

Because only undoable nodes are snapshotted, restoring a state must not leave dangling references
(see [Reference integrity](#reference-integrity-owned-vs-shared-nodes) below).

## Making a node type undoable (MRML + logic)

Undo support for a node type is normally set up in the module logic, where node types are
registered with the scene (for example in `RegisterNodes()`). To make `vtkMRMLMyNode` undoable:

```cpp
void vtkSlicerMyModuleLogic::RegisterNodes()
{
  vtkMRMLScene* scene = this->GetMRMLScene();
  // ... RegisterNodeClass() calls ...

  // Nodes of this class participate in undo/redo.
  scene->AddUndoableNodeClass("vtkMRMLMyNode");
}
```

`AddUndoableNodeClass()` only affects nodes whose class name matches exactly, so each concrete node
type must be registered (registering a base class does not make its subclasses undoable). The
registry is stored on the scene, so a module registers its types every time it is attached to a
scene.

### Reference integrity: owned vs. shared nodes

When an undoable node is added or removed by undo/redo, any node that is **created and destroyed
together with it** must be undoable as well; otherwise redoing the node's creation would leave a
dangling reference.

For a displayable/storable node this means its **display node(s)** and **storage node(s)** — they
are removed when their owner is removed, so they must be restored together with it. Register them
alongside the primary node type:

```cpp
scene->AddUndoableNodeClass("vtkMRMLMyNode");
scene->AddUndoableNodeClass("vtkMRMLMyDisplayNode");
scene->AddUndoableNodeClass("vtkMRMLMyStorageNode");
```

Do **not** register nodes that the undoable node merely references but does not own — for example a
parent transform, a color table, or an input volume. Those nodes:

- are *not* removed when the referencing node is removed, so on redo the reference resolves without
  them being undoable; and
- *should not* be part of the undo history, because undoing an edit to one node must not revert a
  shared transform or color table used by other nodes.

In short: **register a referenced node type as undoable if and only if its lifetime is bound to the
undoable node (owned), and leave independent/shared references out.** This distinction only matters
for add/remove undo; a node that is only ever *modified* by undo never removes its referenced nodes,
so their integrity is never at risk.

:::{note}
To help catch a missing registration, the scene emits a warning during undo/redo (once per
referenced node class) if an undoable node's owned display or storage node is not undoable. If you
see such a warning, register the reported node class with `AddUndoableNodeClass()`.
:::

:::{note}
`vtkMRMLNode::UndoEnabled` is `true` by default and is written to the scene file only when it is
`false`. Clear it on a specific node instance to exclude that instance from undo even though its
class supports undo.
:::

### Where to save states

An undo state represents a *user action*, so `SaveStateForUndo()` should be called from the **user
interface layer** - view interaction widgets (`vtkMRMLCameraWidget`, `vtkSlicerMarkupsWidget`, …),
module widgets, and toolbars - and **not from logic classes**. Logic methods (for example
`AddNewMarkupsNode()`) are also called from Python scripts, file loading, and other logic, where
creating an undo checkpoint (and clearing the redo stack) is usually not wanted; the caller in the
GUI decides whether the action should be undoable. For example, the Markups module widget and
toolbar save a state before creating a new markups node, but the logic that creates the node does
not.

Call `SaveStateForUndo(undoName)` right before applying a change. The `undoName` argument is an
optional, user-displayable, translatable description of the change, stored with the state and shown
in the user interface (see [Describing undo states](#describing-undo-states)). Use the
`vtkMRMLTr()` macro (C++/VTK) or `tr()` (Qt) so the text can be translated, and **include the name
of the affected node** so that the entry is meaningful in the undo history. Insert the node name
into the translatable text with `vtkMRMLI18N::Format()` (C++/VTK) or `QString::arg()` (Qt):

```cpp
// In an interaction/widget handler, before modifying the node:
markupsNode->GetScene()->SaveStateForUndo(
  vtkMRMLI18N::Format(vtkMRMLTr("vtkSlicerMarkupsWidget", "Move control point (%1)"), markupsNode->GetName()));
markupsNode->SetNthControlPointPosition(pointIndex, newPosition);
```

Guidelines for *where* and *how often* to save:

- **Save once per user action.** For a continuous interaction such as dragging a slider or a control
  point, save the state at the beginning of the drag only, not on every intermediate value, so that
  the whole drag is a single undo step.
- **Save the affected node.** The overloads `SaveStateForUndo(vtkMRMLNode*)`,
  `SaveStateForUndo(vtkCollection*)`, and `SaveStateForUndo(std::vector<vtkMRMLNode*>)` do nothing if
  the passed node is not undoable. Passing the node that is about to change keeps view manipulations
  (camera, slice, window/level) from generating undo states while those node types are not
  registered as undoable.
- **Do not save state during undo/redo, import, restore, or batch processing.** `SaveStateForUndo()`
  already ignores calls made while `IsUndoing()`, `IsRedoing()`, `IsImporting()`, `IsRestoring()`, or
  `IsBatchProcessing()` are true. If your logic auto-creates owned nodes in an `OnMRMLSceneNodeAdded`
  handler, guard it with these states too, so that a node restored by undo/redo does not get a
  spurious, duplicate owned node created before its real one is re-added:

  ```cpp
  void vtkSlicerMyModuleLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
  {
    if (this->GetMRMLScene()
        && (this->GetMRMLScene()->IsImporting() || this->GetMRMLScene()->IsRestoring()
            || this->GetMRMLScene()->IsBatchProcessing()
            || this->GetMRMLScene()->IsUndoing() || this->GetMRMLScene()->IsRedoing()))
    {
      // The node (and its owned display/storage nodes) is being restored with its references intact.
      return;
    }
    // ... auto-create display node, etc. ...
  }
  ```

## Other changes (changes made without saving an undo state)

The scene detects changes to undoable nodes that are made **without** a preceding
`SaveStateForUndo()` - for example from a Python script - and automatically turns them into their
own undoable step, named **"Other changes"**. Without this, such changes would not be represented
in the undo history and the next undo would silently revert them together with the previous change.

How it works:

- `SaveStateForUndo()` marks the start of a **tracked change period**. The application completes the
  period when the scene has been idle for one second (`vtkMRMLScene::MarkTrackedChangePeriodCompleted()`,
  driven by a timer in `qSlicerCoreApplication` that is restarted on every scene activity), so all
  responses triggered by the change - including ones executed from zero-timeout timers - belong to
  the same period.
- If an undoable node is **added or removed** outside a tracked period, the scene saves an
  "Other changes" state just before the change is applied.
- If an existing undoable node is **modified** outside a tracked period (detected by observing the
  node's content modified events - the same events that sequence recording uses - and its node
  reference events), the state before the change is provided by *clean state copies*: per-node
  copies taken when the last change period completed, refreshed incrementally while the application
  is idle.
- Consecutive external changes between two tracked changes are **collapsed into a single step**, so
  frequent small untracked changes (for example, display updates when hovering over a markup) do not
  fill the undo history.

For scripts this means undo keeps working without any extra calls. However, if a script performs a
change that should appear in the history as a *named* action (rather than "Other changes"), it
should call `SaveStateForUndo("description")` before the change, exactly like GUI code. In C++ the
`vtkMRMLScene::UndoStateGuard` RAII helper can be used instead; nested guards produce a single undo
state:

```cpp
{
  vtkMRMLScene::UndoStateGuard undoGuard(scene, vtkMRMLTr("vtkSlicerMyModule", "My action"));
  // ... modify undoable nodes ...
}
```

## Application settings and user interface (GUI)

In the Slicer application the generic parts of the feature (the scene undo flag and the undo/redo
user interface) are owned centrally by the main window (`qSlicerMainWindow`), while each module only
registers its undoable node types. This separation lets undo/redo be extended to more node types
without changing the GUI.

- **Enabling the feature.** The **Enable undo/redo** checkbox in the General settings panel stores
  the `EnableSceneUndo` application setting. The main window applies it by calling
  `vtkMRMLScene::SetUndoFlag()` (which also clears the undo/redo stacks when turning undo off) and by
  showing/hiding the Undo/Redo toolbar and Edit-menu actions. It is re-applied on startup, when the
  setting changes, and when the scene is replaced.
- **Undo/Redo commands.** The **Edit → Undo** (`Ctrl+Z`) and **Edit → Redo** (`Ctrl+Y`) actions and
  a dedicated Undo/Redo toolbar call `vtkMRMLScene::Undo()` / `Redo()`.
- **Keeping the UI in sync.** The scene invokes `vtkMRMLScene::UndoStackModifiedEvent` whenever a
  state is saved, applied, or cleared. Observe it to update the enabled state and tooltips of your
  undo/redo controls:

  ```cpp
  qvtkConnect(scene, vtkMRMLScene::UndoStackModifiedEvent, this, SLOT(updateUndoRedoActions()));
  ```

- **Descriptions and history.** `vtkMRMLScene::GetUndoStackNames()` and `GetRedoStackNames()` return
  the descriptions stored with each saved state, most-recent first. In the Slicer main window these
  are used to show the next change in the Undo/Redo action tooltips and to populate the history
  dropdown menus on the toolbar buttons (triggering the N-th entry applies N undo/redo steps).
  Python-wrappable overloads that fill a `vtkStringArray` are also available.

(describing-undo-states)=
### Describing undo states

Provide a short, translatable description whenever you save a state - it is what users see in the
tooltip and history menu:

- describe the *change*, e.g. "Move control point", "Delete control point", "Set opacity";
- **include the name of the affected node** (e.g. "Move control point (F)"), so that the entry is
  unambiguous when several nodes are edited;
- keep it consistent for the same kind of change so that repeated operations read naturally in the
  history list;
- translate it with `vtkMRMLTr("<class name>", "<text>")` in C++/VTK code or `tr("<text>")` in Qt
  code, and insert the node name with `vtkMRMLI18N::Format()` (C++/VTK) or `QString::arg()` (Qt),
  using a `%1` placeholder so word order can be adapted per language:

  ```cpp
  vtkMRMLI18N::Format(vtkMRMLTr("vtkSlicerMarkupsWidget", "Move control point (%1)"), markupsNode->GetName());
  ```

## Scripting (Python)

```python
scene = slicer.mrmlScene

# Enable the undo mechanism and make a node type (and its owned nodes) undoable.
scene.SetUndoOn()
scene.AddUndoableNodeClass("vtkMRMLMarkupsFiducialNode")
scene.AddUndoableNodeClass("vtkMRMLMarkupsDisplayNode")

# Save a state right before making a change (include the node name in the description).
scene.SaveStateForUndo(f"Add control point ({pointListNode.GetName()})")
pointListNode.AddControlPoint([0.0, 0.0, 0.0])

# Undo and redo.
scene.Undo()
scene.Redo()

# Inspect the history.
print(list(scene.GetUndoStackNames()))   # descriptions of the undo states, most recent first
print(list(scene.GetRedoStackNames()))
```

## Key API reference

MRML scene ([vtkMRMLScene](https://apidocs.slicer.org/main/classvtkMRMLScene.html)):

- `SetUndoOn()` / `SetUndoOff()` / `SetUndoFlag(bool)` / `GetUndoFlag()` - enable/disable the undo
  mechanism. Turning it off clears the undo/redo stacks.
- `SaveStateForUndo(const std::string& undoName = "")` and node/collection/vector overloads - save
  the current state, with an optional user-displayable description.
- `Undo()` / `Redo()` - roll the scene back/forward one state.
- `ClearUndoStack()` / `ClearRedoStack()` / `GetNumberOfUndoLevels()` / `GetNumberOfRedoLevels()`.
- `SetMaximumNumberOfSavedUndoStates(int)` - maximum number of saved states (20 by default).
- `AddUndoableNodeClass()` / `RemoveUndoableNodeClass()` / `IsUndoableNodeClass()` /
  `IsNodeUndoable(vtkMRMLNode*)` - manage the set of node classes that participate in undo/redo.
- `GetUndoStackNames()` / `GetRedoStackNames()` - descriptions stored with the saved states.
- `UndoStackModifiedEvent` - invoked when the undo/redo stacks change.
- `IsUndoing()` / `IsRedoing()` - true while an undo/redo operation is in progress.
- `MarkTrackedChangePeriodCompleted()` - called by the application when the scene has been idle,
  to complete the current tracked change period (see [Other changes](#other-changes-changes-made-without-saving-an-undo-state)).
- `SceneActivityEvent` - invoked when the content or references of an undoable node change; the
  application uses it to detect when the scene is idle.
- `UndoStateGuard` - RAII helper that saves an undo state on construction; nested guards produce a
  single undo state.

MRML node ([vtkMRMLNode](https://apidocs.slicer.org/main/classvtkMRMLNode.html)):

- `SetUndoEnabled(bool)` / `GetUndoEnabled()` - per-instance participation in undo/redo (`true` by
  default).
