# Locate SilverLining
# This module defines
# SILVERLINING_LIBRARY
# SILVERLINING_FOUND, if false, do not try to link to SilverLining
# SILVERLINING_INCLUDE_DIR, where to find the headers
#
# $SILVERLINING_DIR is an environment variable that would
# correspond to the ./configure --prefix=$SILVERLINING_DIR
#
# Created by Robert Hauck.

SET(SILVERLINING_DIR "" CACHE PATH "Location of SilverLining SDK")

IF (MSVC90)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc9/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc9/win32")
	ENDIF()
ENDIF()

IF (MSVC80)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc8/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc8/win32")
	ENDIF()
ENDIF()

IF (MSVC10)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc10/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc10/win32")
	ENDIF()
ENDIF()

IF (MSVC11)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc11/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc11/win32")
	ENDIF()
ENDIF()

IF (MSVC12)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc12/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc12/win32")
	ENDIF()
ENDIF()

IF (MSVC14)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc14/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc14/win32")
	ENDIF()
ENDIF()

IF (MSVC15)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc15/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc15/win32")
	ENDIF()
ENDIF()

IF (MSVC16)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc16/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc16/win32")
	ENDIF()
ENDIF()

IF (MSVC17)
	IF (CMAKE_CL_64)
		SET(SILVERLINING_ARCH "vc17/x64")
	ELSE()
		SET(SILVERLINING_ARCH "vc17/win32")
	ENDIF()
ENDIF()

IF (MSVC71)
	SET(SILVERLINING_ARCH "vc7")
ENDIF()

IF (MSVC60)
	SET(SILVERLINING_ARCH "vc6")
ENDIF()

IF (UNIX)
	SET(SILVERLINING_ARCH "linux")
ENDIF()

FIND_PATH(SILVERLINING_INCLUDE_DIR Atmosphere.h
    "${SILVERLINING_DIR}/Public Headers"
    "$ENV{SILVERLINING_PATH}/Public Headers"
    $ENV{SILVERLINING_PATH}
    ${SILVERLINING_DIR}/include
    $ENV{SILVERLINING_DIR}/include
    $ENV{SILVERLINING_DIR}
    /usr/local/include
    /usr/include
    /sw/include # Fink
    /opt/local/include # DarwinPorts
    /opt/csw/include # Blastwave
    /opt/include
    /usr/freeware/include
    "C:/SilverLining SDK/Public Headers"
)

MACRO(FIND_SILVERLINING_LIBRARY MYLIBRARY MYLIBRARYNAME)

    FIND_LIBRARY(${MYLIBRARY}
    NAMES ${MYLIBRARYNAME}
    PATHS
		${SILVERLINING_DIR}/lib
		$ENV{SILVERLINING_DIR}/lib
		$ENV{SILVERLINING_DIR}
		$ENV{SILVERLINING_PATH}/lib
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
		/usr/freeware/lib64
        "C:/SilverLining SDK/lib"
	PATH_SUFFIXES
		${SILVERLINING_ARCH}
    )

ENDMACRO()


SET(_SL_LIBRARY_LINUX "SilverLiningOpenGL")
# Linux SilverLining builds the GL engine into different static libraries. Detect
# whether we need GL3 (Core Profile) library or standard GL2 / compatibility mode
# library, by checking for OSG_GL_FIXED_FUNCTION_AVAILABLE in osg/GL header.
if(NOT WIN32 AND EXISTS "${OPENSCENEGRAPH_INCLUDE_DIR}/osg/GL")
    FILE(READ "${OPENSCENEGRAPH_INCLUDE_DIR}/osg/GL" _OSG_GL)
    STRING(FIND "${_OSG_GL}" "undef OSG_GL_FIXED_FUNCTION_AVAILABLE" _OSG_NEEDS_GL3)
    if(NOT _OSG_NEEDS_GL3 EQUAL -1)
        SET(_SL_LIBRARY_LINUX "SilverLiningOpenGL32Core")
    endif()
    UNSET(_OSG_GL)
endif()
FIND_SILVERLINING_LIBRARY(SILVERLINING_LIBRARY       "SilverLining-MT-DLL;SilverLining;${_SL_LIBRARY_LINUX}")
FIND_SILVERLINING_LIBRARY(SILVERLINING_LIBRARY_DEBUG "SilverLining-MTD-DLL;${_SL_LIBRARY_LINUX}")

SET(SILVERLINING_FOUND FALSE)
IF (SILVERLINING_INCLUDE_DIR AND SILVERLINING_LIBRARY AND SILVERLINING_LIBRARY_DEBUG)
   SET(SILVERLINING_FOUND TRUE)
   add_library(OE::SILVERLINING SHARED IMPORTED)
   set_target_properties(OE::SILVERLINING PROPERTIES
       INTERFACE_INCLUDE_DIRECTORIES "${SILVERLINING_INCLUDE_DIR}"
       INTERFACE_LINK_LIBRARIES "OpenGL::GL;OpenGL::GLU"
   )
   if(WIN32)
       set_target_properties(OE::SILVERLINING PROPERTIES
           IMPORTED_IMPLIB "${SILVERLINING_LIBRARY}"
           IMPORTED_IMPLIB_DEBUG "${SILVERLINING_LIBRARY_DEBUG}"
       )
   else()
       set_target_properties(OE::SILVERLINING PROPERTIES
           IMPORTED_LOCATION "${SILVERLINING_LIBRARY}"
           IMPORTED_LOCATION_DEBUG "${SILVERLINING_LIBRARY_DEBUG}"
       )
   endif()
   #SET(SILVERLINING_LIBRARY debug ${SILVERLINING_LIBRARY_DEBUG} optimized ${SILVERLINING_LIBRARY_RELEASE})
ENDIF()

IF (SILVERLINING_FOUND)
   IF (NOT SILVERLINING_FIND_QUIETLY)
      MESSAGE(STATUS "Found SilverLining: ${SILVERLINING_LIBRARY}")
   ENDIF()
ELSE()
   IF (SILVERLINING_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find SilverLining")
   ENDIF()
ENDIF()
