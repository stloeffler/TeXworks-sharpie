# - Try to find Poppler and Poppler-Qt4
# Once done this will define
#
#  POPPLER_FOUND - system has Poppler and Poppler-Qt4
#  POPPLER_HAS_XPDF - A boolean indicating if Poppler XPDF headers are available
#  POPPLER_NEEDS_FONTCONFIG - A boolean indicating if libpoppler depends on libfontconfig
#  POPPLER_XPDF_INCLUDE_DIR - the include directory for Poppler XPDF headers
#  POPPLER_QT4_INCLUDE_DIR - the include directory for Poppler-Qt4 headers
#  POPPLER_QT4_LIBRARIES  - Link this to use only libpoppler-Qt4
#  POPPLER_LIBRARIES - Link these to use Poppler and Poppler-Qt4
#
# Note: the Poppler-cpp include directory is currently not needed by TeXworks
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if ( POPPLER_LIBRARIES )
   # in cache already
   SET(Poppler_FIND_QUIETLY TRUE)
endif ( POPPLER_LIBRARIES )

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
if( NOT WIN32 )
  find_package(PkgConfig)

  pkg_check_modules(POPPLER_PKG QUIET poppler)
  pkg_check_modules(POPPLER_QT4_PKG QUIET poppler-qt4)
endif( NOT WIN32 )

# Check for Poppler XPDF headers (optional)
FIND_PATH(POPPLER_XPDF_INCLUDE_DIR NAMES poppler-config.h
  PATHS
    /usr/local/include
    /usr/include
  HINTS
    ${POPPLER_PKG_INCLUDE_DIRS} # Generated by pkg-config
  PATH_SUFFIXES poppler
)

IF( NOT(POPPLER_XPDF_INCLUDE_DIR) )

  MESSAGE( STATUS "Could not find poppler-config.h, disabling support for Xpdf headers." )

  SET( POPPLER_HAS_XPDF false )

ELSE( NOT(POPPLER_XPDF_INCLUDE_DIR) )

  SET( POPPLER_HAS_XPDF true )

ENDIF( NOT(POPPLER_XPDF_INCLUDE_DIR) )

# Find libpoppler, libpoppler-qt4 and associated header files (Required)
FIND_LIBRARY(POPPLER_LIBRARIES NAMES poppler ${POPPLER_PKG_LIBRARIES}
  PATHS
    /usr/local
    /usr
  HINTS
    ${POPPLER_PKG_LIBRARY_DIRS} # Generated by pkg-config
  PATH_SUFFIXES
    lib64
    lib
)
IF ( NOT(POPPLER_LIBRARIES) )
  MESSAGE(STATUS "Could not find libpoppler." )
ENDIF ()

# Scan poppler libraries for dependencies on Fontconfig
INCLUDE(GetPrerequisites)
MARK_AS_ADVANCED(gp_cmd)
GET_PREREQUISITES("${POPPLER_LIBRARIES}" POPPLER_PREREQS TRUE FALSE "" "")
IF ("${POPPLER_PREREQS}" MATCHES "fontconfig")
  SET(POPPLER_NEEDS_FONTCONFIG TRUE)
ELSE ()
  SET(POPPLER_NEEDS_FONTCONFIG FALSE)
ENDIF ()


FIND_PATH(POPPLER_QT4_INCLUDE_DIR NAMES poppler-qt4.h poppler-link.h
  PATHS
    /usr/local/include
    /usr/include
  HINTS
    ${POPPLER_QT4_PKG_INCLUDE_DIRS} # Generated by pkg-config
  PATH_SUFFIXES
    poppler/qt4
)
IF ( NOT(POPPLER_QT4_INCLUDE_DIR) )
  MESSAGE(STATUS "Could not find Poppler-Qt4 headers." )
ENDIF ()

FIND_LIBRARY(POPPLER_QT4_LIBRARIES NAMES poppler-qt4 ${POPPLER_QT4_PKG_LIBRARIES}
  PATHS
    /usr/local
    /usr
  HINTS
    ${POPPLER_PKG_LIBRARY_DIRS} # Generated by pkg-config
  PATH_SUFFIXES
    lib64
    lib
)
MARK_AS_ADVANCED(POPPLER_QT4_LIBRARIES)
IF ( NOT(POPPLER_QT4_LIBRARIES) )
  MESSAGE(STATUS "Could not find libpoppler-qt4." )
ENDIF ()
LIST(APPEND POPPLER_LIBRARIES ${POPPLER_QT4_LIBRARIES})


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Poppler DEFAULT_MSG POPPLER_LIBRARIES POPPLER_QT4_INCLUDE_DIR )


# show the POPPLER_(XPDF/QT4)_INCLUDE_DIR and POPPLER_LIBRARIES variables only in the advanced view
MARK_AS_ADVANCED(POPPLER_XPDF_INCLUDE_DIR POPPLER_QT4_INCLUDE_DIR POPPLER_LIBRARIES)

