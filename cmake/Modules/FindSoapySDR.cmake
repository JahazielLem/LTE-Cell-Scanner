message(STATUS "FINDING SOAPY.")
if(NOT SOAPYSDR_FOUND)
  pkg_check_modules (SOAPYSDR_PKG SoapySDR)

  # NOTE: the include directory returned is the PARENT of the "SoapySDR" folder,
  # because the sources include <SoapySDR/Device.h>. On case-insensitive
  # filesystems (default on macOS) adding the SoapySDR folder directly to the
  # include path would shadow system headers such as <time.h>/<types.h> with
  # SoapySDR's Time.h/Types.h, breaking unrelated compilations.
  find_path(SOAPYSDR_INCLUDE_DIRS
    NAMES SoapySDR/Device.h
    PATHS ${SOAPYSDR_PKG_INCLUDE_DIRS}
          /usr/include
          /usr/local/include
          /opt/homebrew/include
  )

  find_library(SOAPYSDR_LIBRARIES
    NAMES SoapySDR
    PATHS ${SOAPYSDR_PKG_LIBRARY_DIRS}
          /usr/lib
          /usr/local/lib
          /usr/lib/arm-linux-gnueabihf
          /opt/homebrew/lib
  )


if(SOAPYSDR_INCLUDE_DIRS AND SOAPYSDR_LIBRARIES)
  set(SOAPYSDR_FOUND TRUE CACHE INTERNAL "libSOAPYSDR found")
  message(STATUS "Found libSOAPYSDR: ${SOAPYSDR_INCLUDE_DIRS}, ${SOAPYSDR_LIBRARIES}")
else(SOAPYSDR_INCLUDE_DIRS AND SOAPYSDR_LIBRARIES)
  set(SOAPYSDR_FOUND FALSE CACHE INTERNAL "libSOAPYSDR found")
  message(STATUS "libSOAPYSDR not found.")
endif(SOAPYSDR_INCLUDE_DIRS AND SOAPYSDR_LIBRARIES)

mark_as_advanced(SOAPYSDR_LIBRARIES SOAPYSDR_INCLUDE_DIRS)

endif(NOT SOAPYSDR_FOUND)
