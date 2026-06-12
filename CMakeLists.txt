cmake_minimum_required(VERSION 3.25)
project(wire_cell_arrow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress spurious warnings present in GCC 14/15 headers (would otherwise be
# fatal under -Werror below).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "15")
      add_compile_options(-Wno-stringop-overflow)
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16")
      add_compile_options(-Wno-array-bounds)
    endif()
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# When using a Spack view, the installed shared libraries may have been built
# with a newer GCC runtime (e.g. GCC 15) than the system compiler provides.
# This causes versioned-symbol link errors (GLIBCXX_3.4.32, CXXABI_1.3.15)
# when any target directly links a Spack WireCell or Boost library.
#
# Fix: explicitly link the Spack libstdc++.so.6 (resolved from the symlink in
# the view's lib/ dir) for any target that needs it, and propagate it via the
# WIRECELL_RUNTIME_LIBS variable used in test/CMakeLists.txt.
set(WIRECELL_RUNTIME_LIBS "")
foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
  if(EXISTS "${_prefix}/lib/libstdc++.so.6")
    link_directories(BEFORE "${_prefix}/lib")
    file(REAL_PATH "${_prefix}/lib/libstdc++.so.6" _stdc_real)
    get_filename_component(_stdc_dir "${_stdc_real}" DIRECTORY)
    # Use the full .so.6 path (no bare -lstdc++ symlink in Spack runtime dirs).
    set(WIRECELL_RUNTIME_LIBS "${_stdc_real}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS
      " -Wl,-rpath,${_stdc_dir}")
    break()
  endif()
endforeach()

# Apache Arrow ships its own CMake package config (Arrow provides
# ArrowConfig.cmake under lib/cmake/Arrow), so no custom finder is needed.
find_package(Arrow CONFIG REQUIRED)
find_package(WireCell REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

add_compile_options(-Wall -Wpedantic -Werror)

# ---------------------------------------------------------------------------
# wire_cell_arrow — shared library
#
# Houses the IData<->Arrow converters and the ArrowXxx IData facades.  Those
# sources land via the converter/facade issues (blocked on this build system);
# for now the library carries only Version.cpp so the SHARED target links.
# Append new sources to this list as they are implemented.
# ---------------------------------------------------------------------------
add_library(wire_cell_arrow SHARED
  wire_cell_arrow/Version.cpp
)

target_compile_definitions(wire_cell_arrow
  PRIVATE WIRE_CELL_ARROW_VERSION="${PROJECT_VERSION}"
)

target_include_directories(wire_cell_arrow
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(wire_cell_arrow
  PUBLIC
    Arrow::arrow_shared
    WireCell::Iface
    WireCell::Aux
    WireCell::Util
    Boost::json
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(test)

# ---------------------------------------------------------------------------
# Install rules
#
# The wire_cell_arrow SHARED library and its public headers are installed so
# downstream CMake projects can find them via find_package(wire_cell_arrow).
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

# Shared library
install(TARGETS wire_cell_arrow
  EXPORT wire_cell_arrowTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# Public headers
install(DIRECTORY wire_cell_arrow/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wire_cell_arrow"
  FILES_MATCHING PATTERN "*.h"
)

# CMake package config (so find_package(wire_cell_arrow) works downstream)
install(EXPORT wire_cell_arrowTargets
  FILE wire_cell_arrowTargets.cmake
  NAMESPACE wire_cell_arrow::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/wire_cell_arrow"
)

# Bundle FindWireCell.cmake so the installed config can re-find WireCell.
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindWireCell.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/wire_cell_arrow"
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_arrowConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/wire_cell_arrowConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_arrowConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/wire_cell_arrow"
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_arrowConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/wire_cell_arrowConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/wire_cell_arrow"
)
