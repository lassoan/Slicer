set(proj python-SimpleITK)

# Set dependency list
set(${proj}_DEPENDENCIES
  python
  python-pip
  python-setuptools
  python-numpy
  )

if(NOT DEFINED Slicer_USE_SYSTEM_${proj})
  set(Slicer_USE_SYSTEM_${proj} ${Slicer_USE_SYSTEM_python})
endif()

# Include dependent projects if any
ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj DEPENDS_VAR ${proj}_DEPENDENCIES)

if(Slicer_USE_SYSTEM_${proj})
  ExternalProject_FindPythonPackage(
    MODULE_NAME "SimpleITK"
    REQUIRED
    )
endif()

if(NOT Slicer_USE_SYSTEM_${proj})
  set(requirements_file ${CMAKE_BINARY_DIR}/${proj}-requirements.txt)
  file(WRITE ${requirements_file} [===[
  # [simpleitk]
  # Hashes correspond to the following packages:
  #  - simpleitk-2.5.5-cp311-abi3-macosx_10_9_x86_64.whl
  #  - simpleitk-2.5.5-cp311-abi3-macosx_11_0_arm64.whl
  #  - simpleitk-2.5.5-cp311-abi3-manylinux2014_aarch64.manylinux_2_17_aarch64.whl
  #  - simpleitk-2.5.5-cp311-abi3-manylinux2014_x86_64.manylinux_2_17_x86_64.whl
  #  - simpleitk-2.5.5-cp311-abi3-win_amd64.whl
  simpleitk==2.5.5 --hash=sha256:e831d15f5ea5b0e02ed3a6ac5f2dd34ba0a0eeaa4ebacfb1d970999836b19ee0 \
                   --hash=sha256:ee87416622cec6ed6b96747a0648c82c6e88f10b6553668ce90736f09ab5a994 \
                   --hash=sha256:847f42057d7bf01b5721b107dd3e00d4ee8d514f6f1ff85794ffedacaf4afc53 \
                   --hash=sha256:8ace5e392be00d5bec28c87ef1d73016f8ebe671dafffe9e9d045f20b0968033 \
                   --hash=sha256:a5fcfcfe9242d3d509b254b3213ca0f5db2c15903f2fc375d9ac5d38e57d415d
  # [/simpleitk]
  ]===])

  if(WIN32 AND Slicer_ARCHITECTURE STREQUAL "arm64")
    # SimpleITK does not provide pre-built packages for Windows ARM64, therefore
    # the package is built from the source distribution. SimpleITK core libraries
    # are built first in a directory with short path (the ITK source directory path
    # must be shorter than 50 characters), then the Python package is built from them.
    # Version must be kept in sync with the requirements file above.
    set(_sitk_version "2.5.5")
    set(_sitk_sdist_url "https://files.pythonhosted.org/packages/28/8f/c21ee6dc4dbede4fc385b5d70703d45f749db2738c7b0c175546f43c9f86/simpleitk-2.5.5.tar.gz")
    set(_sitk_sdist_sha256 "254f70febed55868801c89f310e650eb1b87148d82b1d4e3628d8f0181a7f529")
    # ITK version used by SimpleITK 2.5.5 (v5.4.5) does not build for Windows ARM64 (GDCM issue)
    set(_sitk_itk_git_tag "v5.4.7")

    set(_sitk_source_dir ${CMAKE_BINARY_DIR}/SimpleITK)
    set(_sitk_superbuild_dir ${CMAKE_BINARY_DIR}/SimpleITK-sb)
    set(_sitk_wheel_dir ${CMAKE_BINARY_DIR}/SimpleITK-wheel)

    # Fix incorrect number of arguments in the source distribution, which prevents
    # building the package from pre-built SimpleITK core libraries.
    set(_patch_script ${CMAKE_BINARY_DIR}/${proj}_patch_step.cmake)
    file(WRITE ${_patch_script} [===[
file(READ "${SOURCE_DIR}/CMakeLists.txt" content)
string(REGEX REPLACE "sitk_ExternalProject_Add_Empty\\(([ \t\r\n]*)SimpleITK-superbuild([ \t\r\n]*)\\)"
  "sitk_ExternalProject_Add_Empty(\\1SimpleITK-superbuild \"\"\\2)" content "${content}")
file(WRITE "${SOURCE_DIR}/CMakeLists.txt" "${content}")
]===])

    ExternalProject_Add(${proj}-source
      URL ${_sitk_sdist_url}
      URL_HASH SHA256=${_sitk_sdist_sha256}
      DOWNLOAD_DIR ${CMAKE_BINARY_DIR}
      SOURCE_DIR ${_sitk_source_dir}
      PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR:PATH=${_sitk_source_dir} -P ${_patch_script}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
      )

    ExternalProject_Add(${proj}-superbuild
      DOWNLOAD_COMMAND ""
      SOURCE_DIR ${_sitk_source_dir}/SimpleITK/SuperBuild
      BINARY_DIR ${_sitk_superbuild_dir}
      CMAKE_CACHE_ARGS
        -DBUILD_EXAMPLES:BOOL=OFF
        -DBUILD_TESTING:BOOL=OFF
        -DBUILD_DOXYGEN:BOOL=OFF
        -DWRAP_DEFAULT:BOOL=OFF
        -DSimpleITK_INSTALL_DOC_DIR:STRING=SimpleITK
        -DSimpleITK_BUILD_STRIP:BOOL=ON
        -DSimpleITK_BUILD_DISTRIBUTE:BOOL=ON
        -DSimpleITK_GIT_PROTOCOL:STRING=https
        -DSKBUILD:BOOL=ON
        -DITK_GIT_TAG:STRING=${_sitk_itk_git_tag}
        -DGIT_EXECUTABLE:FILEPATH=${GIT_EXECUTABLE}
      # The Python package is always built in Release mode
      BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
      INSTALL_COMMAND ""
      DEPENDS
        ${proj}-source
      )

    set(_sitk_skbuild_options
      -DSimpleITK_DIR:PATH=${_sitk_superbuild_dir}/SimpleITK-build
      -DITK_DIR:PATH=${_sitk_superbuild_dir}/ITK-prefix/lib/cmake/ITK
      -DSWIG_EXECUTABLE:FILEPATH=${_sitk_superbuild_dir}/swigwin/swig.exe
      # Limited Python API is disabled because it results in incorrect module filename on Windows
      -DSimpleITK_PYTHON_USE_LIMITED_API:BOOL=OFF
      )
    list(JOIN _sitk_skbuild_options " " _sitk_skbuild_options)

    ExternalProject_Add(${proj}
      ${${proj}_EP_ARGS}
      DOWNLOAD_COMMAND ""
      SOURCE_DIR ${_sitk_source_dir}
      BUILD_IN_SOURCE 1
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ${CMAKE_COMMAND} -E env
        "CMAKE_GENERATOR=${CMAKE_GENERATOR}"
        "CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "SKBUILD_CONFIGURE_OPTIONS=${_sitk_skbuild_options}"
        ${PYTHON_EXECUTABLE} -m pip wheel . --no-deps --wheel-dir ${_sitk_wheel_dir}
      INSTALL_COMMAND ${PYTHON_EXECUTABLE} -m pip install --no-deps --no-index --find-links ${_sitk_wheel_dir} simpleitk==${_sitk_version}
      LOG_BUILD 1
      LOG_INSTALL 1
      DEPENDS
        ${${proj}_DEPENDENCIES}
        ${proj}-superbuild
      )
  else()
    ExternalProject_Add(${proj}
      ${${proj}_EP_ARGS}
      DOWNLOAD_COMMAND ""
      SOURCE_DIR ${CMAKE_BINARY_DIR}/${proj}
      BUILD_IN_SOURCE 1
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ${PYTHON_EXECUTABLE} -m pip install --require-hashes -r ${requirements_file}
      LOG_INSTALL 1
      DEPENDS
        ${${proj}_DEPENDENCIES}
      )
  endif()

  #-----------------------------------------------------------------------------
  # Sanity checks

  foreach(varname IN ITEMS
      python_DIR
      PYTHON_SITE_PACKAGES_SUBDIR
      )
    if("${${varname}}" STREQUAL "")
      message(FATAL_ERROR "${varname} CMake variable is expected to be set")
    endif()
  endforeach()

  #-----------------------------------------------------------------------------
  # Launcher setting specific to build tree

  set(${proj}_PYTHONPATH_LAUNCHER_BUILD
    ${python_DIR}/${PYTHON_SITE_PACKAGES_SUBDIR}
    )
  mark_as_superbuild(
    VARS ${proj}_PYTHONPATH_LAUNCHER_BUILD
    LABELS "PYTHONPATH_LAUNCHER_BUILD"
    )

else()
  ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDENCIES})
endif()
