# - Find FFTW
# Find the native FFTW includes and library
# This module defines
#  FFTW_INCLUDE_DIR, where to find fftw3.h, etc.
#  FFTW_LIBRARIES, the libraries needed to use FFTW.
#  FFTW_FOUND, If false, do not try to use FFTW.
# also defined, but not for general use are
#  FFTW_LIBRARY, where to find the FFTW library.

#MESSAGE("FFTW_DIR set to ${FFTW_DIR}" )

FIND_PATH(FFTW_INCLUDE_DIR fftw3.h
  ${FFTW_DIR}/include
  /usr/pkgs64/include
  /usr/include
  /usr/local/include
  /opt/homebrew/include
  /opt/homebrew/opt/itpp/include
  /opt/homebrew/opt/fftw/include
  /opt/homebrew/opt/librtlsdr/include
)

FIND_LIBRARY(FFTW_LIBRARY
  NAMES fftw3
  PATHS ${FFTW_DIR}/libs
  "${FFTW_DIR}\\win32\\lib"
  /usr/lib/x86_64-linux-gnu
  /usr/pkgs64/lib
  /usr/lib64
  /usr/lib
  /usr/local/lib
  /opt/homebrew/lib
  /opt/homebrew/opt/itpp/lib
  /opt/homebrew/opt/fftw/lib
  /opt/homebrew/opt/librtlsdr/lib
  NO_DEFAULT_PATH
)

IF (FFTW_LIBRARY AND FFTW_INCLUDE_DIR)
  SET(FFTW_LIBRARIES ${FFTW_LIBRARY})
  SET(FFTW_FOUND "YES")
ELSE (FFTW_LIBRARY AND FFTW_INCLUDE_DIR)
  SET(FFTW_FOUND "NO")
ENDIF (FFTW_LIBRARY AND FFTW_INCLUDE_DIR)

IF (FFTW_FOUND)
  IF (NOT FFTW_FIND_QUIETLY)
    MESSAGE(STATUS "Found FFTW: ${FFTW_LIBRARIES}")
  ENDIF (NOT FFTW_FIND_QUIETLY)
ELSE (FFTW_FOUND)
  IF (FFTW_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find FFTW library")
  ENDIF (FFTW_FIND_REQUIRED)
ENDIF (FFTW_FOUND)

# Deprecated declarations.
GET_FILENAME_COMPONENT (NATIVE_FFTW_LIB_PATH ${FFTW_LIBRARY} PATH)

MARK_AS_ADVANCED(

  FFTW_LIBRARY
  FFTW_INCLUDE_DIR
)

