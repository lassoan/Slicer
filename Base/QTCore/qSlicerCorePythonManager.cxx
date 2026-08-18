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

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Qt includes
#include <QBitArray>
#include <QSettings>

// CTK includes
#include <ctkVTKPythonQtWrapperFactory.h>

// PythonQt includes
#include <PythonQt.h>

// Slicer includes
#include "qSlicerCoreApplication.h"
#include "qSlicerCorePythonManager.h"
#include "qSlicerScriptedUtils_p.h"

// VTK includes
#include <vtkPythonUtil.h>
#include <vtkVersion.h>

namespace
{

//-----------------------------------------------------------------------------
/// VTK PythonQt wrapper factory that supports the lazily-loading "vtk" module.
///
/// A VTK class is only registered with vtkPythonUtil when the vtkmodules
/// submodule that provides it is imported, and the "vtk" Python module is a
/// lazily-loading shim (see GenerateLazyVtkModule.py) that defers those
/// imports until a class is first accessed from Python code. A VTK object can
/// therefore cross the PythonQt boundary (for example, a scripted module
/// calling sliceView.renderWindow(), declared as returning vtkRenderWindow*)
/// before the module that provides its class has been imported. Plain
/// ctkVTKPythonQtWrapperFactory would then wrap the object as the nearest
/// registered base class (vtkWindow, from the always-loaded vtkCommonCore) and
/// vtkPythonUtil would cache that incomplete wrapper for the lifetime of the
/// object. This factory first makes sure the declared class is registered:
/// looking the class name up on the lazy "vtk" shim imports exactly the
/// vtkmodules submodule that provides it. Class names that are already
/// registered (including all wrapped MRML/Slicer classes, registered when the
/// slicer package loads its kits) only cost a hash lookup, and unknown names
/// are ignored.
class qSlicerVTKPythonQtWrapperFactory : public ctkVTKPythonQtWrapperFactory
{
public:
  PyObject* wrap(const QByteArray& classname, void* ptr) override
  {
    if (classname.startsWith("vtk") && !vtkPythonUtil::FindClassTypeObject(classname.constData()))
    {
      if (PyObject* vtkModule = PyImport_ImportModule("vtk"))
      {
        PyObject* vtkClass = PyObject_GetAttrString(vtkModule, classname.constData());
        Py_XDECREF(vtkClass);
        Py_DECREF(vtkModule);
      }
      PyErr_Clear();
    }
    return ctkVTKPythonQtWrapperFactory::wrap(classname, ptr);
  }
};

} // namespace

//-----------------------------------------------------------------------------
qSlicerCorePythonManager::qSlicerCorePythonManager(QObject* _parent)
  : Superclass(_parent)
{
  this->Factory = nullptr;

  // https://docs.python.org/3/c-api/init_config.html#init-config
  PyStatus status;
  PyConfig config;
  PyConfig_InitPythonConfig(&config);

  config.parse_argv = 0;

  // If it applies, disable import of user site packages
  QString noUserSite = qgetenv("PYTHONNOUSERSITE");
  config.user_site_directory = noUserSite.toInt(); // disable user site packages

  status = PyConfig_SetString(&config, &config.program_name, L"Slicer");
  if (PyStatus_Exception(status))
  {
    PyConfig_Clear(&config);
    Py_ExitStatusException(status);
  }

  status = Py_InitializeFromConfig(&config);
  if (PyStatus_Exception(status))
  {
    PyConfig_Clear(&config);
    Py_ExitStatusException(status);
  }

  PyConfig_Clear(&config);

  int flags = this->initializationFlags();
  flags |= PythonQt::PythonAlreadyInitialized; // Set bit
  this->setInitializationFlags(flags);
}

//-----------------------------------------------------------------------------
qSlicerCorePythonManager::~qSlicerCorePythonManager()
{
  if (this->Factory)
  {
    delete this->Factory;
    this->Factory = nullptr;
  }
}

//-----------------------------------------------------------------------------
QStringList qSlicerCorePythonManager::pythonPaths()
{
  return Superclass::pythonPaths();
}

//-----------------------------------------------------------------------------
void qSlicerCorePythonManager::preInitialization()
{
  Superclass::preInitialization();
  this->Factory = new qSlicerVTKPythonQtWrapperFactory;
  this->addWrapperFactory(this->Factory);
  qSlicerCoreApplication* app = qSlicerCoreApplication::application();
  if (app)
  {
    // Add object to python interpreter context
    this->addObjectToPythonMain("_qSlicerCoreApplicationInstance", app);
  }
}

//-----------------------------------------------------------------------------
void qSlicerCorePythonManager::addVTKObjectToPythonMain(const QString& name, vtkObject* object)
{
  // Split name using '.'
  QStringList moduleNameList = name.split('.', Qt::SkipEmptyParts);

  // Remove the last part
  QString attributeName = moduleNameList.takeLast();

  bool success = qSlicerScriptedUtils::setModuleAttribute(moduleNameList.join("."), attributeName, vtkPythonUtil::GetObjectFromPointer(object));
  if (!success)
  {
    qCritical() << "qSlicerCorePythonManager::addVTKObjectToPythonMain - "
                   "Failed to add VTK object:"
                << name;
  }
}

//-----------------------------------------------------------------------------
void qSlicerCorePythonManager::appendPythonPath(const QString& path)
{
  // TODO Make sure PYTHONPATH is updated
  this->executeString(QString("import sys, os\n"
                              "___path = os.path.abspath(%1)\n"
                              "if ___path not in sys.path:\n"
                              "  sys.path.append(___path)\n"
                              "del sys, os")
                        .arg(qSlicerCorePythonManager::toPythonStringLiteral(path)));
}

//-----------------------------------------------------------------------------
void qSlicerCorePythonManager::appendPythonPaths(const QStringList& paths)
{
  for (const QString& path : paths)
  {
    this->appendPythonPath(path);
  }
}

//-----------------------------------------------------------------------------
QString qSlicerCorePythonManager::toPythonStringLiteral(QString path)
{
  return ctkAbstractPythonManager::toPythonStringLiteral(path);
}
