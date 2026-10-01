CMAKE_MINIMUM_REQUIRED( VERSION 2.8.4 )
# allow more human readable "if then else" constructs
#SET( CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS TRUE )

# Add the local Find*.cmake scripts
LIST (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

PROJECT( CellSearch )
ENABLE_TESTING()

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -Wall")
# _GLIBCXX_DEBUG is libstdc++-only; on macOS (libc++) it is harmless. -Werror is
# dropped on Apple because the newer clang toolchain warns on this old codebase.
IF (APPLE)
  SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -ggdb")
ELSE (APPLE)
  SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Werror -ggdb -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC")
ENDIF (APPLE)

# Set default build type
IF( NOT CMAKE_BUILD_TYPE )
  SET( CMAKE_BUILD_TYPE "release" )
ENDIF()

INCLUDE(FindPkgConfig)

# Find needed packages.
# On macOS, Apple clang needs Homebrew's libomp to enable OpenMP.
IF (APPLE)
  IF (NOT DEFINED OpenMP_ROOT)
    SET (OpenMP_ROOT /opt/homebrew/opt/libomp)
  ENDIF ()
ENDIF (APPLE)
FIND_PACKAGE( OpenMP )
# Apple clang does not ship OpenMP; wire up Homebrew's libomp manually when the
# standard detection fails (OpenMP is optional here - only used for #pragma omp).
if (APPLE AND NOT OpenMP_CXX_FOUND AND EXISTS "/opt/homebrew/opt/libomp/lib/libomp.dylib")
  set(OpenMP_CXX_FLAGS "-Xclang -fopenmp -I/opt/homebrew/opt/libomp/include")
  set(OpenMP_C_FLAGS "-Xclang -fopenmp -I/opt/homebrew/opt/libomp/include")
  set(OpenMP_CXX_LIBRARIES "/opt/homebrew/opt/libomp/lib/libomp.dylib")
  set(OpenMP_CXX_FOUND TRUE)
  set(OPENMP_FOUND TRUE)
  link_libraries("/opt/homebrew/opt/libomp/lib/libomp.dylib")
  message(STATUS "OpenMP: using Homebrew libomp for Apple clang")
endif()
if (OPENMP_FOUND OR OpenMP_CXX_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()
FIND_PACKAGE( Boost COMPONENTS thread system REQUIRED )
INCLUDE_DIRECTORIES("${Boost_INCLUDE_DIRS}")
FIND_PACKAGE( ITPP REQUIRED )
INCLUDE_DIRECTORIES("${ITPP_INCLUDE_DIR}")
# src/CMakeLists.txt links IT++ by bare name (-litpp), so make sure its library
# directory (e.g. /opt/homebrew/lib on macOS) is on the linker search path.
GET_FILENAME_COMPONENT(ITPP_LIBDIR "${ITPP_LIBRARY}" DIRECTORY)
LINK_DIRECTORIES("${ITPP_LIBDIR}")
FIND_PACKAGE( BLAS REQUIRED )
FIND_PACKAGE( LAPACK REQUIRED )
FIND_PACKAGE( FFTW REQUIRED )
INCLUDE_DIRECTORIES("${FFTW_INCLUDE_DIR}")

# SDR front-ends. RTL-SDR is optional; SoapySDR (used for HackRF/PlutoSDR) is
# preferred on macOS. At least one backend must be present.
FIND_PACKAGE( RTLSDR )
IF (RTLSDR_FOUND)
  INCLUDE_DIRECTORIES("${RTLSDR_INCLUDE_DIR}")
  ADD_DEFINITIONS(-DHAVE_RTLSDR)
ENDIF (RTLSDR_FOUND)

FIND_PACKAGE( SoapySDR )
IF (SOAPYSDR_FOUND)
  INCLUDE_DIRECTORIES("${SOAPYSDR_INCLUDE_DIRS}")
  ADD_DEFINITIONS(-DHAVE_SOAPYSDR)
ENDIF (SOAPYSDR_FOUND)

IF (NOT RTLSDR_FOUND AND NOT SOAPYSDR_FOUND)
  MESSAGE(FATAL_ERROR "No SDR backend found: install librtlsdr and/or SoapySDR")
ENDIF ()

FIND_PACKAGE( Curses REQUIRED )
INCLUDE_DIRECTORIES("${CURSES_INCLUDE_DIR}")

# Add version information to build
SET( ${PROJECT_NAME}_MAJOR_VERSION 1 )
SET( ${PROJECT_NAME}_MINOR_VERSION 0 )
SET( ${PROJECT_NAME}_PATCH_LEVEL 0 )
CONFIGURE_FILE (
  "${PROJECT_SOURCE_DIR}/include/common.h.in"
  "${PROJECT_BINARY_DIR}/include/common.h"
)

# Where .h files are located
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES("${PROJECT_BINARY_DIR}/include")

# Descend into subdirectories
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(test)

# Uninstall target
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake)

