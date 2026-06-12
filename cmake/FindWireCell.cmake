#[============================================================[.rst:
FindWireCell
============

Finds the Wire-Cell Toolkit (WCT) headers and libraries.

Imported targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets when found:

``WireCell::Util``
  Core utilities (NamedFactory, PluginManager, Configuration, Logging)
``WireCell::Iface``
  Interface headers (INode, IData subclasses, IConfigurable)
``WireCell::Aux``
  Auxiliary helpers (Logger, SimpleDepo, SimpleFrame, …)
``WireCell::Apps``
  Application entry point (WireCell::Main)
``WireCell::Pgraph``
  Pgrapher single-threaded DFP executor
``WireCell::Tbb``
  TbbFlow multi-threaded DFP executor

Result variables
^^^^^^^^^^^^^^^^

``WireCell_FOUND``
  True if WCT was found.
``WireCell_VERSION``
  Version string reported by the ``wire-cell`` executable.
``WireCell_INCLUDE_DIR``
  Directory containing WCT headers.
``WireCell_LIBRARIES``
  List of all found ``WireCell::*`` targets.

Hints
^^^^^

``WIRECELL_FQ_DIR``
  Environment variable pointing at the WCT installation prefix.
``WIRECELL_VERSION``
  Environment variable with the version string (fallback if the
  ``wire-cell`` executable is not on PATH).

#]============================================================]

# wire-cell executable is optional: used for version detection only.
find_program(WIRE-CELL NAMES wire-cell HINTS ENV WIRECELL_FQ_DIR)
mark_as_advanced(WIRE-CELL)

# Libraries provided by WCT and their transitive link dependencies.
# ROOT is intentionally omitted: wire-cell-phlex does not use WireCell::Root.
set(_fwc_libs Util Iface Aux Apps Pgraph Tbb Gen SigProc Img Sio)

set(_fwc_transitive_deps_Apps     WireCell::Iface WireCell::Util)
set(_fwc_transitive_deps_Aux      WireCell::Iface WireCell::Util Boost::boost Eigen3::Eigen)
set(_fwc_transitive_deps_Gen      WireCell::Aux   WireCell::Iface WireCell::Util Boost::boost Eigen3::Eigen)
set(_fwc_transitive_deps_Iface    WireCell::Util  Boost::graph Boost::boost)
set(_fwc_transitive_deps_Img      WireCell::Aux   WireCell::Iface WireCell::Util)
set(_fwc_transitive_deps_Pgraph   WireCell::Iface WireCell::Util Boost::boost)
set(_fwc_transitive_deps_SigProc  WireCell::Aux   WireCell::Iface WireCell::Util)
set(_fwc_transitive_deps_Sio      WireCell::Aux   WireCell::Iface WireCell::Util Boost::iostreams)
set(_fwc_transitive_deps_Tbb      WireCell::Aux   WireCell::Iface WireCell::Util Boost::boost TBB::tbb)
set(_fwc_transitive_deps_Util
  Boost::date_time Boost::exception Boost::filesystem Boost::iostreams
  Eigen3::Eigen jsoncpp_lib jsonnet_lib spdlog::spdlog ${CMAKE_DL_LIBS})

# External packages that must be present for WCT to work.
# jsonnet is handled manually below (no CMake config; library is libgojsonnet.so).
set(_fwc_deps Boost Eigen3 jsoncpp spdlog)
set(_fwc_fp_Boost_args COMPONENTS graph headers date_time exception filesystem iostreams)

unset(_fwc_fphsa_extra_args)

# Version detection (best-effort; WIRE-CELL is optional)
if(NOT WireCell_VERSION)
  if(WIRE-CELL)
    execute_process(COMMAND ${WIRE-CELL} -v
      OUTPUT_VARIABLE WireCell_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  endif()
  if(NOT WireCell_VERSION AND DEFINED ENV{WIRECELL_VERSION})
    string(REGEX REPLACE "^v?([0-9._-]+).*$" "\\1"
      WireCell_VERSION "$ENV{WIRECELL_VERSION}")
    string(REGEX REPLACE "[_-]" "." WireCell_VERSION "${WireCell_VERSION}")
  endif()
endif()

# Find headers
find_path(WireCell_INCLUDE_DIR WireCellApps/Main.h PATH_SUFFIXES include)
mark_as_advanced(WireCell_INCLUDE_DIR)

# Derive prefix from include dir so we can set hints for deps with non-standard paths.
if(WireCell_INCLUDE_DIR)
  get_filename_component(_fwc_prefix "${WireCell_INCLUDE_DIR}" DIRECTORY)
endif()

# Eigen3 is installed under share/eigen3/cmake/ (non-standard lowercase path).
# Set the hint before find_package(Eigen3) so CMake finds it without a manual -D.
if(_fwc_prefix AND NOT DEFINED Eigen3_DIR)
  set(Eigen3_DIR "${_fwc_prefix}/share/eigen3/cmake" CACHE PATH
    "Path to Eigen3Config.cmake (set by FindWireCell)" FORCE)
endif()

unset(_fwc_missing_deps)
foreach(_fwc_dep IN LISTS _fwc_deps)
  find_package(${_fwc_dep} ${_fwc_fp_${_fwc_dep}_args} QUIET)
  if(NOT ${_fwc_dep}_FOUND)
    string(JOIN " " _missing " ${_fwc_dep}" ${_fwc_fp_${_fwc_dep}_args})
    list(APPEND _fwc_missing_deps "${_missing}")
  endif()
endforeach()

# jsonnet: WCT is built against libgojsonnet.so (Go implementation of jsonnet).
# There is no CMake config for this library; create an IMPORTED target manually.
find_library(WireCell_gojsonnet_LIBRARY NAMES gojsonnet
  HINTS "${_fwc_prefix}/lib")
mark_as_advanced(WireCell_gojsonnet_LIBRARY)
if(NOT WireCell_gojsonnet_LIBRARY)
  list(APPEND _fwc_missing_deps " gojsonnet (libgojsonnet.so)")
endif()

if(_fwc_missing_deps)
  set(_fwc_fphsa_extra_args
    REASON_FAILURE_MESSAGE "missing dependencies:${_fwc_missing_deps}")
  unset(_fwc_missing_deps)
endif()

# Find each WCT library
unset(WireCell_LIBRARIES)
if(WireCell_INCLUDE_DIR)
  foreach(_fwc_lib IN LISTS _fwc_libs)
    find_library(WireCell_${_fwc_lib}_LIBRARY NAMES WireCell${_fwc_lib}
      HINTS "${_fwc_prefix}/lib")
    mark_as_advanced(WireCell_${_fwc_lib}_LIBRARY)
    if(WireCell_${_fwc_lib}_LIBRARY)
      list(APPEND WireCell_LIBRARIES "WireCell::${_fwc_lib}")
    endif()
  endforeach()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WireCell
  REQUIRED_VARS WireCell_INCLUDE_DIR WireCell_LIBRARIES
  VERSION_VAR WireCell_VERSION
  ${_fwc_fphsa_extra_args}
)
unset(_fwc_fphsa_extra_args)

# Create imported targets
if(WireCell_FOUND)
  # jsonnet_lib: manual IMPORTED target for libgojsonnet.so
  if(WireCell_gojsonnet_LIBRARY AND NOT TARGET jsonnet_lib)
    add_library(jsonnet_lib SHARED IMPORTED)
    set_target_properties(jsonnet_lib PROPERTIES
      IMPORTED_NO_SONAME TRUE
      IMPORTED_LOCATION  "${WireCell_gojsonnet_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${WireCell_INCLUDE_DIR}")
  endif()

  foreach(_fwc_lib IN LISTS _fwc_libs)
    if(WireCell_${_fwc_lib}_LIBRARY AND NOT TARGET WireCell::${_fwc_lib})
      add_library(WireCell::${_fwc_lib} SHARED IMPORTED)
      set_target_properties(WireCell::${_fwc_lib} PROPERTIES
        IMPORTED_NO_SONAME TRUE
        IMPORTED_LOCATION  "${WireCell_${_fwc_lib}_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${WireCell_INCLUDE_DIR}")
      foreach(_fwc_tdep IN LISTS _fwc_transitive_deps_${_fwc_lib})
        set_property(TARGET WireCell::${_fwc_lib}
          APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${_fwc_tdep}")
      endforeach()
    endif()
  endforeach()
endif()

unset(_fwc_lib)
unset(_fwc_libs)
unset(_fwc_prefix)
