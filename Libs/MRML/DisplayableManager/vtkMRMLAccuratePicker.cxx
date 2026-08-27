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

#include "vtkMRMLAccuratePicker.h"

// VTK includes
#include <vtkAbstractCellLocator.h>
#include <vtkActor.h>
#include <vtkDataSet.h>
#include <vtkMapper.h>
#include <vtkObjectFactory.h>
#include <vtkPropCollection.h>
#include <vtkRenderer.h>
#include <vtkStaticCellLocator.h>

// STD includes
#include <set>

vtkStandardNewMacro(vtkMRMLAccuratePicker);

//----------------------------------------------------------------------------
vtkMRMLAccuratePicker::vtkMRMLAccuratePicker() = default;

//----------------------------------------------------------------------------
vtkMRMLAccuratePicker::~vtkMRMLAccuratePicker() = default;

//----------------------------------------------------------------------------
void vtkMRMLAccuratePicker::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "MinimumCellCountToIndex: " << this->MinimumCellCountToIndex << "\n";
  os << indent << "Cached locators: " << this->Locators.size() << "\n";
}

//----------------------------------------------------------------------------
void vtkMRMLAccuratePicker::UpdateLocators(vtkRenderer* renderer)
{
  this->RemoveAllLocators();
  if (!renderer)
  {
    this->Locators.clear();
    return;
  }

  std::set<vtkDataSet*> shownSurfaces;
  vtkPropCollection* props = renderer->GetViewProps();
  vtkCollectionSimpleIterator propIterator;
  props->InitTraversal(propIterator);
  for (vtkProp* prop = props->GetNextProp(propIterator); prop != nullptr; prop = props->GetNextProp(propIterator))
  {
    if (!prop->GetPickable() || !prop->GetVisibility())
    {
      continue;
    }
    vtkActor* actor = vtkActor::SafeDownCast(prop);
    if (!actor)
    {
      continue;
    }
    // Any mapper, and the data set it was given: that is what vtkCellPicker scans and what
    // it matches a locator against, by identity. Asking for a vtkPolyDataMapper instead
    // would miss the mappers that are not one - an unstructured grid model is drawn through
    // a vtkDataSetMapper - and those meshes would silently keep the unindexed linear scan
    // that this class exists to avoid.
    vtkMapper* mapper = vtkMapper::SafeDownCast(actor->GetMapper());
    if (!mapper)
    {
      continue;
    }
    vtkDataSet* dataSet = mapper->GetInput();
    if (!dataSet || dataSet->GetNumberOfCells() < this->MinimumCellCountToIndex)
    {
      continue;
    }

    shownSurfaces.insert(dataSet);
    CachedLocator& cached = this->Locators[dataSet];
    if (!cached.Locator)
    {
      vtkNew<vtkStaticCellLocator> locator;
      locator->SetDataSet(dataSet);
      cached.Locator = locator;
      cached.BuildMTime = 0;
    }
    // Build once, and rebuild only when the mesh itself changes, so repeated
    // picks over an unchanging mesh pay the build cost at most once.
    if (cached.BuildMTime != dataSet->GetMTime())
    {
      cached.Locator->BuildLocator();
      cached.BuildMTime = dataSet->GetMTime();
    }
    this->AddLocator(cached.Locator);
  }

  // Release locators for meshes that are no longer shown (a cached locator
  // holds a reference to its data set).
  for (auto it = this->Locators.begin(); it != this->Locators.end();)
  {
    if (shownSurfaces.find(it->first) == shownSurfaces.end())
    {
      it = this->Locators.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

//----------------------------------------------------------------------------
int vtkMRMLAccuratePicker::Pick(double selectionX, double selectionY, double selectionZ, vtkRenderer* renderer)
{
  this->UpdateLocators(renderer);
  return this->Superclass::Pick(selectionX, selectionY, selectionZ, renderer);
}
