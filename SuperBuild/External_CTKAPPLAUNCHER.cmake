
if(Slicer_USE_CTKAPPLAUNCHER)

  set(proj CTKAPPLAUNCHER)

  # Sanity checks
  if(DEFINED CTKAPPLAUNCHER_DIR AND NOT EXISTS ${CTKAPPLAUNCHER_DIR})
    message(FATAL_ERROR "CTKAPPLAUNCHER_DIR variable is defined but corresponds to nonexistent directory")
  endif()

  # Set dependency list
  set(${proj}_DEPENDENCIES "")
  if(WIN32)
    set(${proj}_DEPENDENCIES CTKResEdit)
  endif()

  # Include dependent projects if any
  ExternalProject_Include_Dependencies(${proj} PROJECT_VAR proj DEPENDS_VAR ${proj}_DEPENDENCIES)

  if(Slicer_USE_SYSTEM_${proj})
    message(FATAL_ERROR "Enabling Slicer_USE_SYSTEM_${proj} is not supported !")
  endif()

  if(NOT DEFINED CTKAppLauncher_DIR)

    SlicerMacroGetOperatingSystemArchitectureBitness(VAR_PREFIX CTKAPPLAUNCHER)
    # Qt 6 based launcher released from https://github.com/lassoan/AppLauncher,
    # available for amd64 and arm64 on all platforms (commontk/AppLauncher only
    # releases i386 Windows and amd64 Linux and macOS binaries).
    set(launcher_version "0.1.35")
    set(launcher_github_repo "lassoan/AppLauncher")
    set(sha256_win-amd64 "8cf16af2123f3d0a40954071ffb8632f1ee546eec8d1d5b6f915ca98b1e1dcd1")
    set(sha256_win-arm64 "c2bb06afaee7d828c4ab4af613e39c635929bb945f4144a69e829d9f8dd854f5")
    set(sha256_linux-amd64 "c5607c7d274351259cd29ca08c15dbf53766340c95f4eb1b66c864e8eb7cb4e7")
    set(sha256_linux-arm64 "9785d3db3754e5dce2186877b75b9f9b9211fa6d42c8219482a4c4f72d61d5bc")
    set(sha256_macosx-amd64 "3d20f4d0ed9c3c70e16cc8c202a7bf645cbe2f73ef8e08b042ef3f6861a72cf9")
    set(sha256_macosx-arm64 "3f308399a3e8cd2e1a920c91acdbe563f43bdbde114614c4b7093c4bf46b3a55")
    set(sha256 "${sha256_${CTKAPPLAUNCHER_OS}-${CTKAPPLAUNCHER_ARCHITECTURE}}")
    if(NOT sha256)
      message(FATAL_ERROR "CTKAppLauncher ${launcher_version} is not available for ${CTKAPPLAUNCHER_OS}-${CTKAPPLAUNCHER_ARCHITECTURE}")
    endif()

    set(EP_BINARY_DIR ${CMAKE_BINARY_DIR}/${proj})

    set(CTKAppLauncherFileName CTKAppLauncher-${launcher_version}-${CTKAPPLAUNCHER_OS}-${CTKAPPLAUNCHER_ARCHITECTURE}.tar.gz)
    ExternalProject_Add(${proj}
      ${${proj}_EP_ARGS}
      URL https://github.com/${launcher_github_repo}/releases/download/v${launcher_version}/${CTKAppLauncherFileName}
      URL_HASH SHA256=${sha256}
      DOWNLOAD_DIR ${CMAKE_BINARY_DIR}
      SOURCE_DIR ${EP_BINARY_DIR}
      BUILD_IN_SOURCE 1
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
      DEPENDS
        ${${proj}_DEPENDENCIES}
      )

    ExternalProject_GenerateProjectDescription_Step(${proj}
      VERSION ${launcher_version}
      LICENSE_FILES "https://raw.githubusercontent.com/${launcher_github_repo}/v${launcher_version}/LICENSE_Apache_20"
      )

    set(CTKAppLauncher_DIR ${EP_BINARY_DIR})

  else()
    ExternalProject_Add_Empty(${proj} DEPENDS ${${proj}_DEPENDENCIES})
  endif()

  mark_as_superbuild(
    VARS
      CTKAppLauncher_DIR:PATH
    LABELS "FIND_PACKAGE"
    )

endif()
