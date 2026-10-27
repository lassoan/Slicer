# -------------------------------------------------------------------------
# Find and install TBB Libs
# -------------------------------------------------------------------------

set(TBB_INSTALL_LIB_DIR "${Slicer_INSTALL_LIB_DIR}")

if(NOT EXISTS "${TBB_BIN_DIR}")
  message(FATAL_ERROR "CMake variable TBB_BIN_DIR is set to a nonexistent directory: ${TBB_BIN_DIR}")
endif()
if(NOT EXISTS "${TBB_LIB_DIR}")
  message(FATAL_ERROR "CMake variable TBB_LIB_DIR is set to a nonexistent directory: ${TBB_LIB_DIR}")
endif()

if(WIN32)
  set(_tbb_files
    ${TBB_BIN_DIR}/tbb12.dll
    ${TBB_BIN_DIR}/tbbmalloc.dll
    )
  # tbbmalloc_proxy is not built for Windows ARM64
  if(NOT Slicer_ARCHITECTURE STREQUAL "arm64")
    list(APPEND _tbb_files ${TBB_BIN_DIR}/tbbmalloc_proxy.dll)
  endif()
  install(FILES ${_tbb_files} DESTINATION bin COMPONENT Runtime)
elseif(APPLE)
  message(FATAL_ERROR "This CMake module does NOT support macOS")
elseif(UNIX)
  foreach(file IN ITEMS
    ${TBB_LIB_DIR}/libtbb.so.12
    ${TBB_LIB_DIR}/libtbbmalloc.so.2.5
    ${TBB_LIB_DIR}/libtbbmalloc_proxy.so.2.5
    )
    slicerInstallLibrary(FILE ${file} DESTINATION ${TBB_INSTALL_LIB_DIR} COMPONENT Runtime STRIP)
  endforeach()
endif()
