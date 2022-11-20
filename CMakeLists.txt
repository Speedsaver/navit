set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.navitproject.navit")
set(MACOSX_BUNDLE_BUNDLE_NAME "Navit")
message(STATUS "Building with CMake V${CMAKE_VERSION}")
project(navit C)
enable_language(CXX)

cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
set(NAVIT_VERSION_MAJOR "0")
set(NAVIT_VERSION_MINOR "5")
set(NAVIT_VERSION_PATCH "1")
set(PACKAGE_VERSION "${NAVIT_VERSION_MAJOR}.${NAVIT_VERSION_MINOR}.${NAVIT_VERSION_PATCH}")

set(PACKAGE_NAME "navit-git")
set(PACKAGE "navit" CACHE STRING "Navit package name")
set(PACKAGE_STRING "${PACKAGE} ${PACKAGE_VERSION}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
include("${PROJECT_SOURCE_DIR}/cmake/navit_macros.cmake")

IF(NOT CMAKE_BUILD_TYPE)
   SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

add_definitions ("-D_LARGEFILE_SOURCE=1")
add_definitions ("-D_FILE_OFFSET_BITS=64")

# binary name
set(NAVIT_BINARY navit CACHE STRING "Navit binary name")
# install path
set(BIN_DIR bin CACHE PATH "Navit bin path")
add_definitions ("-DBIN_DIR=\"${BIN_DIR}\"")
set(SHARE_DIR share/navit CACHE PATH "Navit share path")
add_definitions ("-DSHARE_DIR=\"${SHARE_DIR}\"")
set(LOCALE_DIR share/locale CACHE PATH "Navit locale path")
add_definitions ("-DLOCALE_DIR=\"${LOCALE_DIR}\"")
set(MAN_DIR share/man/man1 CACHE PATH "Navit man path")
add_definitions ("-DMAN_DIR=\"${MAN_DIR}\"")
# LIB_DIR
IF (NOT LIBDIR)
    MESSAGE(STATUS "LIBDIR variable is not defined. It will be autodetected now.")
    MESSAGE(STATUS "You can set it manually with -DLIBDIR=<value>")
    # check 64 bit
    IF (CMAKE_SIZEOF_VOID_P EQUAL 4)
    	SET(LIB_DIR lib/navit CACHE PATH "Navit 32bit bin path")
        MESSAGE(STATUS "   LIB_DIR (highest subdirectory if LIBDIR) is set to '${LIB_DIR}'")
    ELSE (CMAKE_SIZEOF_VOID_P EQUAL 4)
    	SET(LIB_DIR lib64/navit CACHE PATH "Navit 64bit bin path")
        MESSAGE(STATUS "   LIB_DIR (highest subdirectory if LIBDIR) is set to '${LIB_DIR}'")
    ENDIF (CMAKE_SIZEOF_VOID_P EQUAL 4)
ELSE (NOT LIBDIR)
    GET_FILENAME_COMPONENT (LIB_DIR ${LIBDIR} NAME)
    MESSAGE(STATUS "   LIB_DIR (highest subdirectory if LIBDIR) is set to '${LIB_DIR}'")
ENDIF (NOT LIBDIR)
foreach(EXTRA_MODULE ${EXTRA_MODULES})
   add_module(${EXTRA_MODULE} "extra module specified" TRUE)
endforeach()
add_definitions ("-DLIB_DIR=\"${LIB_DIR}\"")

if (EXTRA_LIBS)
	list(APPEND NAVIT_LIBS ${EXTRA_LIBS})
endif(EXTRA_LIBS)

if (EXTRA_INCLUDES)
    include_directories(${EXTRA_INCLUDES})
endif(EXTRA_INCLUDES)

if (EXTRA_LIBDIR)
    link_directories(${EXTRA_LIBDIR})
endif(EXTRA_LIBDIR)

### Detect environment

add_module(graphics/ssd1306 "Required library not found" TRUE)
add_module(gui/speedsaver "Default" TRUE)
add_module(vehicle/gpsd "gpsd lib not found" FALSE)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cpack.cmake")
set(CPACK_PACKAGE_VENDOR "Navit team")
set(CPACK_PACKAGE_VERSION_MAJOR ${NAVIT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${NAVIT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH "${NAVIT_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/GPL-2")
set(CPACK_PACKAGE_FILE_NAME ${PACKAGE})
set(CPACK_PACKAGE_EXECUTABLES "navit;Navit")
INCLUDE (CPack)

INCLUDE (CheckIncludeFiles)
INCLUDE (CheckLibraryExists)
INCLUDE (CheckFunctionExists)
INCLUDE (CheckSymbolExists)
find_package(ZLIB REQUIRED)

#pkg-config based detection
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPS libgps REQUIRED)
#Simple checks
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
CHECK_SYMBOL_EXISTS(system stdlib.h HAVE_SYSTEM)

pkg_search_module(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})
list(APPEND NAVIT_LIBS ${GLIB_LDFLAGS})
include_directories(${ZLIB_INCLUDE_DIRS})
list(APPEND NAVIT_LIBS ${ZLIB_LIBRARIES})

set_with_reason(vehicle/gpsd "gpsd lib found" TRUE ${LIBGPS_LDFLAGS})

#Modules without test yet
# add_module(plugin/ssd1306 "Default" TRUE)

# other features
add_feature(BUILD_MAPTOOL "default" TRUE)

### Platform specific settings
if(CMAKE_SIZEOF_VOID_P LESS 8)
   set_with_reason(BUILD_MAPTOOL "maptool works only on 64 bit architectures" FALSE)
endif()

set(LOCALEDIR "${LOCALE_DIR}")

set(CMAKE_APPBUNDLE_PATH "")

# Compile with -Wall -Wextra. We need all the help we can get from the compiler :-).
# Disabled warnings:
# -Wno-missing-field-initializers: Used a lot, does not seem problematic.
# -Wno-unused-parameter: Unfortunately occurs frequently because of
#                        functions implementing the interface of a plugin.
# -Wno-sign-compare: We currently just use int almost everywhere.
#                    Unclear if it's really worth correcting.
if(CMAKE_COMPILER_IS_GNUCC OR CCMAKE_COMPILER_IS_GNUCXX)
   set(COMMON_COMPILER_FLAGS "-Wall -Wundef -Wcast-align -Wpointer-arith -Wno-unused-parameter -Wno-sign-compare")
   set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${COMMON_COMPILER_FLAGS}")
   set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILER_FLAGS}")
   # flags not available in old GCC versions, or not for both C and C++
   add_compiler_flag_if_available("-Wno-missing-field-initializers")
   add_compiler_flag_if_available("-Wextra")
   add_compiler_flag_if_available("-Wmissing-prototypes")
   add_compiler_flag_if_available("-Wstrict-prototypes ")
endif()

if (EXTRA_DEFINES)
	add_definitions("${EXTRA_DEFINES}")
endif(EXTRA_DEFINES)

if (NOT NAVIT_DEPENDENCY_ERROR)
   message("\nSummary:\n")

   set(SUMMARY_COMPONENTS ${ALL_PLUGINS} ${ALL_MODULE_PLUGINS} ${ALL_MODULES})
   list(SORT SUMMARY_COMPONENTS)

   set(LAST_TYPE NONE)
   foreach ( SUMMARY_COMP ${SUMMARY_COMPONENTS})
      # split path to type and name
      string(REPLACE "/" ";" SUMMARY_COMP_LIST ${SUMMARY_COMP})
      list(GET SUMMARY_COMP_LIST 0 SUMMARY_COMP_TYPE)
      list(LENGTH SUMMARY_COMP_LIST SUMMARY_COMP_LIST_LENGTH)
      if ( SUMMARY_COMP_LIST_LENGTH GREATER 1 )
         list(GET SUMMARY_COMP_LIST 1 SUMMARY_COMP_NAME)
      else()
         set(SUMMARY_COMP_NAME ${SUMMARY_COMP})
      endif()
      if ( NOT ${LAST_TYPE} STREQUAL ${SUMMARY_COMP_TYPE})
         message("\n--->>> ${SUMMARY_COMP_TYPE}")
         set(LAST_TYPE ${SUMMARY_COMP_TYPE})
      endif()
      if (${SUMMARY_COMP})
         message("Enabled   ${SUMMARY_COMP_NAME} ( ${${SUMMARY_COMP}_REASON} )")
      else()
         message("Disabled  ${SUMMARY_COMP_NAME} ( ${${SUMMARY_COMP}_REASON} )")
      endif()
   endforeach()

   list(SORT ALL_FEATURES)
   message("\n--->>> Features")
   foreach ( FEATURE ${ALL_FEATURES})
      if ( ${FEATURE} )
         message("Enabled   ${FEATURE} ( ${${FEATURE}_REASON} )")
      else()
         message("Disabled  ${FEATURE} ( ${${FEATURE}_REASON} )")
      endif()
   endforeach()

   message("\nTo configure your build use 'cmake -L' to find changeable variables and run cmake again with 'cmake -D <var-name>=<your value> ...'.")
endif(NOT NAVIT_DEPENDENCY_ERROR)

add_subdirectory (navit)
