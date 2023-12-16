# Deploy abstract scripted plugin and init file that
# imports all deployed scripted plugins deployed by modules
set(SubjectHierarchyWidgets_PYTHON_SCRIPTS
  SubjectHierarchyPlugins/__init__
  SubjectHierarchyPlugins/AbstractScriptedSubjectHierarchyPlugin
  ParameterNodeWrapperPlugins/__init__
  ParameterNodeWrapperPlugins/SubjectHierarchyGuiConnectors
  )

set(SubjectHierarchyWidgets_PYTHON_RESOURCES
  )

ctkMacroCompilePythonScript(
  TARGET_NAME SubjectHierarchyWidgets
  SCRIPTS "${SubjectHierarchyWidgets_PYTHON_SCRIPTS}"
  RESOURCES "${SubjectHierarchyWidgets_PYTHON_RESOURCES}"
  DESTINATION_DIR ${Slicer_BINARY_DIR}/${Slicer_QTSCRIPTEDMODULES_LIB_DIR}
  INSTALL_DIR ${Slicer_INSTALL_QTSCRIPTEDMODULES_LIB_DIR}
  NO_INSTALL_SUBDIR
  )
