# - Try to find Fontconfig
# Once done this will define
#
#  FONTCONFIG_FOUND - system has Fontconfig
#  FONTCONFIG_INCLUDE_DIR - the Fontconfig include directory
#  FONTCONFIG_LIBRARIES - Link these to use Fontconfig
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if ( FONTCONFIG_INCLUDE_DIR AND FONTCONFIG_LIBRARIES )
   # in cache already
   SET(Fontconfig_FIND_QUIETLY TRUE)
endif ( FONTCONFIG_INCLUDE_DIR AND FONTCONFIG_LIBRARIES )

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
if( NOT WIN32 )
  find_package(PkgConfig)

  pkg_check_modules(FONTCONFIG fontconfig)
endif( NOT WIN32 )

FIND_PATH(FONTCONFIG_INCLUDE_DIR NAMES fontconfig/fontconfig.h
  PATHS
  /usr/include
  /usr/X11/include
  /usr/local/include
  ${FONTCONFIG_INCLUDE_DIRS}
)

FIND_LIBRARY(FONTCONFIG_LIBRARIES NAMES fontconfig
  PATHS
  /usr/lib
  /usr/X11/lib
  /usr/local/lib
  ${FONTCONFIG_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Fontconfig DEFAULT_MSG FONTCONFIG_INCLUDE_DIR FONTCONFIG_LIBRARIES )

# show the FONTCONFIG_INCLUDE_DIR and FONTCONFIG_LIBRARIES variables only in the advanced view
MARK_AS_ADVANCED(FONTCONFIG_INCLUDE_DIR FONTCONFIG_LIBRARIES )

