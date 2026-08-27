# Checks that the VTK source tree can tell what version it is, before it is built.
#
# VTK does not read its version from a file: VTKDetermineVersion.cmake runs "git describe" in
# the source tree and believes whatever tag it lands on. A clone without the release tags -
# a fork pushed without them, or a shallow clone - therefore makes VTK declare the version of
# whatever ancient tag is still reachable, and the build then fails much later and somewhere
# else entirely: ITK's VtkGlue falls back to the pre-9 target names and compiles with no VTK
# include directories, and Slicer's find_package(VTK 9.x) rejects the VTK that was just built.
# Both failures point at ITK or at Slicer, not at the missing tags, so the mismatch is caught
# here instead - after VTK is cloned, before the hours it takes to build it.
#
# Expects: VTK_SOURCE_DIR, EXPECTED_VERSION, GIT_EXECUTABLE

if(NOT EXISTS "${VTK_SOURCE_DIR}/.git")
  # An exported tarball ships its version, so there is nothing to determine.
  return()
endif()

if(NOT EXISTS "${GIT_EXECUTABLE}")
  message(STATUS "VTK version check skipped: git was not found")
  return()
endif()

execute_process(
  COMMAND "${GIT_EXECUTABLE}" describe
  WORKING_DIRECTORY "${VTK_SOURCE_DIR}"
  RESULT_VARIABLE describe_result
  OUTPUT_VARIABLE describe_output
  ERROR_VARIABLE describe_error
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
  )

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" expected_major_minor "${EXPECTED_VERSION}")

string(CONCAT fix_instructions
  "    git -C \"${VTK_SOURCE_DIR}\" fetch --tags https://gitlab.kitware.com/vtk/vtk.git
"
  "and build again. If VTK is cloned from a fork, push the tags to it as well "
  "(git push <fork-remote> --tags), otherwise every fresh build hits this again."
  )

if(NOT describe_result EQUAL 0)
  message(FATAL_ERROR
    "VTK cannot determine its own version: 'git describe' failed in\n"
    "  ${VTK_SOURCE_DIR}\n"
    "  ${describe_error}\n"
    "\n"
    "VTK takes its version number from the release tag that 'git describe' finds, so a "
    "clone with no tags in reach builds as the wrong version - which later breaks ITK's "
    "VtkGlue and Slicer's find_package(VTK) with errors that name neither VTK nor the "
    "missing tags. Fetch the tags:\n"
    "\n"
    "${fix_instructions}")
endif()

if(NOT "${describe_output}" MATCHES "^v?${expected_major_minor}\\.")
  message(FATAL_ERROR
    "VTK would build as the wrong version.\n"
    "  source directory: ${VTK_SOURCE_DIR}\n"
    "  'git describe' says: ${describe_output}\n"
    "  Slicer expects: ${EXPECTED_VERSION}\n"
    "\n"
    "VTK takes its version number from the release tag that 'git describe' finds, so this "
    "means the checked out commit has no ${expected_major_minor} tag in its history - "
    "usually a fork that was pushed without tags, or a shallow clone. Building anyway "
    "produces libraries named after the wrong version, which later breaks ITK's VtkGlue "
    "('cannot open include file vtkVersion.h') and Slicer's find_package(VTK) with errors "
    "that name neither VTK nor the missing tags. Fetch the tags:\n"
    "\n"
    "${fix_instructions}")
endif()

message(STATUS "VTK version verified: ${describe_output} (expected ${EXPECTED_VERSION})")
