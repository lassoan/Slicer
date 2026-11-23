import importlib
import unittest

import slicer


class SlicerKitsTest(unittest.TestCase):
    """The classes of Slicer's libraries are in the slicer namespace, where Slicer's Python code and
    modules use them (slicer.vtkMRMLScene), whichever way the libraries are built and wrapped."""

    # A class of each kit (slicer.kits.available_kits) and where it comes from
    KIT_CLASSES = {
        "mrml": "vtkMRMLScene",
        "logic": "vtkSlicerApplicationLogic",
        "vtkSegmentationCore": "vtkSegmentation",
        "vtkAddon": "vtkAddonMathUtilities",
    }

    def test_kit_classes_in_slicer_namespace(self):
        for kit, className in self.KIT_CLASSES.items():
            with self.subTest(kit=kit):
                self.assertTrue(hasattr(slicer, className), f"slicer.{className} (from the {kit} kit) is missing")

    def test_kit_modules_import(self):
        """Each kit module imports on its own, and gives the classes of its library."""
        for moduleName, className in (("mrml", "vtkMRMLScene"), ("slicer.logic", "vtkSlicerApplicationLogic"),
                                      ("vtkSegmentationCore", "vtkSegmentation"), ("vtkITK", "vtkITKArchetypeImageSeriesScalarReader"),
                                      ("vtkTeem", "vtkTeemNRRDReader")):
            with self.subTest(module=moduleName):
                module = importlib.import_module(moduleName)
                self.assertTrue(hasattr(module, className), f"{moduleName} has no {className}")

    def test_kit_classes_instantiate(self):
        """The classes are wrapped with their base classes: they can be created, and are VTK objects."""
        import vtk

        for className in ("vtkMRMLScene", "vtkSlicerApplicationLogic", "vtkSegmentation"):
            with self.subTest(className=className):
                instance = getattr(slicer, className)()
                self.assertIsInstance(instance, vtk.vtkObject)


if __name__ == "__main__":
    unittest.main()
