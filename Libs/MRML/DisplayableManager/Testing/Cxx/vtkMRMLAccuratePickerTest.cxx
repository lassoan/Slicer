/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// This guards the interactive performance of accurate picking in 3D views.
// vtkMRMLAccuratePicker is the picker shared per view (by
// vtkMRMLThreeDViewInteractorStyle) and used on every mouse move for markup
// dragging and hover read-out. A plain vtkCellPicker tests every cell of every
// pickable surface, so over a large mesh (for example a segmentation closed
// surface with millions of cells) each pick costs tens to hundreds of
// milliseconds and interaction becomes janky. vtkMRMLAccuratePicker indexes
// large surfaces with cell locators; this test checks that repeated picks over
// a large mesh stay fast.

// MRMLDisplayableManager includes
#include "vtkMRMLAccuratePicker.h"

// VTK includes
#include <vtkActor.h>
#include <vtkAppendFilter.h>
#include <vtkDataSetMapper.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkTimerLog.h>
#include <vtkUnstructuredGrid.h>

// STD includes
#include <cstdlib>
#include <iostream>

namespace
{

/// Renders what the renderer holds, then times repeated picks at the middle of the view.
/// Returns false if a pick misses or if the picks are too slow to have been indexed.
bool PicksAreFastFor(vtkRenderer* renderer, const char* description, vtkIdType numberOfCells)
{
  renderer->ResetCamera();
  renderer->GetRenderWindow()->Render();

  vtkNew<vtkMRMLAccuratePicker> picker;
  picker->SetTolerance(0.005);

  const int x = 150;
  const int y = 150;

  // First pick: must hit the mesh, and pays the one-time locator build.
  if (!picker->Pick(x, y, 0, renderer))
  {
    std::cerr << "Failed: pick did not hit the " << description << " at the view center" << std::endl;
    return false;
  }
  if (picker->GetActor() == nullptr)
  {
    std::cerr << "Failed: pick did not report the actor of the " << description << std::endl;
    return false;
  }

  // Repeated picks (as during a drag or hover) must be fast because the picker
  // is locator-accelerated. Without the locator each pick is O(number of cells)
  // and takes tens to hundreds of milliseconds on this many cells.
  const int numberOfPicks = 50;
  vtkNew<vtkTimerLog> timer;
  timer->StartTimer();
  for (int i = 0; i < numberOfPicks; ++i)
  {
    picker->Pick(x + (i % 7) - 3, y + (i % 5) - 2, 0, renderer);
  }
  timer->StopTimer();
  const double millisecondsPerPick = timer->GetElapsedTime() / numberOfPicks * 1000.0;
  std::cout << description << " (" << numberOfCells << " cells): " << millisecondsPerPick << " ms/pick" << std::endl;

  // Generous threshold: the accelerated pick is well under a millisecond, while
  // an un-indexed brute-force pick over this mesh is far above 30 ms on any
  // hardware. The wide gap keeps the test insensitive to machine speed.
  const double thresholdMillisecondsPerPick = 30.0;
  if (millisecondsPerPick > thresholdMillisecondsPerPick)
  {
    std::cerr << "Failed: " << description << " " << millisecondsPerPick << " ms/pick (threshold "
              << thresholdMillisecondsPerPick
              << " ms). Picks over it are not locator-accelerated, so control-point dragging "
              << "and hover read-out would be janky while it is shown." << std::endl;
    return false;
  }
  return true;
}

} // namespace

int vtkMRMLAccuratePickerTest(int vtkNotUsed(argc), char* vtkNotUsed(argv)[])
{
  // A large, pickable surface standing in for a segmentation closed surface. It is drawn
  // through a vtkPolyDataMapper, as poly data models are.
  vtkNew<vtkSphereSource> sphere;
  sphere->SetThetaResolution(900);
  sphere->SetPhiResolution(900);
  sphere->Update();

  vtkNew<vtkPolyDataMapper> surfaceMapper;
  // SetInputData (not a pipeline connection) so the mapper's input is available for locator
  // building without a render/update.
  surfaceMapper->SetInputData(sphere->GetOutput());
  vtkNew<vtkActor> surfaceActor;
  surfaceActor->SetMapper(surfaceMapper);

  vtkNew<vtkRenderer> surfaceRenderer;
  surfaceRenderer->AddActor(surfaceActor);
  vtkNew<vtkRenderWindow> surfaceWindow;
  surfaceWindow->SetOffScreenRendering(1);
  surfaceWindow->SetSize(300, 300);
  surfaceWindow->AddRenderer(surfaceRenderer);

  if (!PicksAreFastFor(surfaceRenderer, "poly data surface", sphere->GetOutput()->GetNumberOfCells()))
  {
    return EXIT_FAILURE;
  }

  // A large volumetric mesh, as a CFD or FE result is. Slicer draws an unstructured grid
  // model through a vtkDataSetMapper rather than a vtkPolyDataMapper, so a picker that only
  // indexes vtkPolyDataMapper leaves this case on the unindexed linear scan - which is what
  // this half of the test is here to catch.
  vtkNew<vtkImageData> image;
  image->SetDimensions(101, 101, 101);
  image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  vtkNew<vtkAppendFilter> toUnstructuredGrid;
  toUnstructuredGrid->SetInputData(image);
  toUnstructuredGrid->Update();
  vtkUnstructuredGrid* grid = toUnstructuredGrid->GetOutput();

  vtkNew<vtkDataSetMapper> gridMapper;
  gridMapper->SetInputData(grid);
  vtkNew<vtkActor> gridActor;
  gridActor->SetMapper(gridMapper);

  vtkNew<vtkRenderer> gridRenderer;
  gridRenderer->AddActor(gridActor);
  vtkNew<vtkRenderWindow> gridWindow;
  gridWindow->SetOffScreenRendering(1);
  gridWindow->SetSize(300, 300);
  gridWindow->AddRenderer(gridRenderer);

  if (!PicksAreFastFor(gridRenderer, "unstructured grid", grid->GetNumberOfCells()))
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
