# Remote module manifest for ITKIOOMEZarrNGFF.
#
# This file is copied into the ITK source tree (Modules/Remote) by
# External_ITK.cmake because the Slicer ITK fork does not ship it.
# It allows enabling the module with -DModule_IOOMEZarrNGFF:BOOL=ON.
#
# The module provides itk::OMEZarrNGFFImageIO (registered with the ITK IO
# factory) for reading/writing OME-Zarr / NGFF chunked multiscale images,
# wrapping Google TensorStore behind a CMake build.
#
# A fork is used until these changes are available upstream:
# - configure with an ITK that uses a system zlib (ITK_USE_SYSTEM_ZLIB),
#   which Slicer does; without it the module cannot be configured at all
# - configurable chunk cache pool (SetCachePoolTotalBytesLimit), needed so
#   that reading a region in several pieces (progressive display of
#   multiscale volumes) does not re-download the chunks that consecutive
#   pieces share
itk_fetch_module(
  IOOMEZarrNGFF
  "IO for images stored in Zarr-backed OME-NGFF. https://github.com/InsightSoftwareConsortium/ITKIOOMEZarrNGFF"
  GIT_REPOSITORY https://github.com/lassoan/ITKIOOMEZarrNGFF.git
  GIT_TAG 5f8fcbbc24d08d6308c3e71b7ca56613652d3c02 # slicer-omezarr-streaming branch
)
