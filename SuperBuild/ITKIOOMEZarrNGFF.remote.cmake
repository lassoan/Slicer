# Remote module manifest for ITKIOOMEZarrNGFF.
#
# This file is copied into the ITK source tree (Modules/Remote) by
# External_ITK.cmake because the Slicer ITK fork does not ship it.
# It allows enabling the module with -DModule_IOOMEZarrNGFF:BOOL=ON.
#
# The module provides itk::OMEZarrNGFFImageIO (registered with the ITK IO
# factory) for reading/writing OME-Zarr / NGFF chunked multiscale images,
# wrapping Google TensorStore behind a CMake build.
itk_fetch_module(
  IOOMEZarrNGFF
  "IO for images stored in Zarr-backed OME-NGFF. https://github.com/InsightSoftwareConsortium/ITKIOOMEZarrNGFF"
  GIT_REPOSITORY https://github.com/InsightSoftwareConsortium/ITKIOOMEZarrNGFF.git
  GIT_TAG 0aa5c3d72d878dc624269834b177c2a9980c14c6
)
