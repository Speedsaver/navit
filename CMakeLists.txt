project(navit C)
enable_language(CXX)
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
add_definitions ("-D_LARGEFILE_SOURCE=1")
add_definitions ("-D_FILE_OFFSET_BITS=64")

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
add_definitions ("-DLIB_DIR=\"${LIB_DIR}\"")
find_package(ZLIB REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPS libgps REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})
list(APPEND NAVIT_LIBS ${GLIB_LDFLAGS})
include_directories(${ZLIB_INCLUDE_DIRS})
list(APPEND NAVIT_LIBS ${ZLIB_LIBRARIES})
list(APPEND NAVIT_LIBS ${LIBGPS_LDFLAGS})

set(BUILD_MAPTOOL FALSE)

### Platform specific settings
if(CMAKE_SIZEOF_VOID_P GREATER_EQUAL 16)
   set(BUILD_MAPTOOL TRUE CACHE BOOL)
endif()

# Compile with -Wall -Wextra. We need all the help we can get from the compiler :-).
# Disabled warnings:
# -Wno-missing-field-initializers: Used a lot, does not seem problematic.
# -Wno-unused-parameter: Unfortunately occurs frequently because of
#                        functions implementing the interface of a plugin.
# -Wno-sign-compare: We currently just use int almost everywhere.
#                    Unclear if it's really worth correcting.
if(CMAKE_COMPILER_IS_GNUCC OR CCMAKE_COMPILER_IS_GNUCXX)
   set(COMMON_COMPILER_FLAGS "-Wall -Wundef -Wcast-align -Wpointer-arith -Wno-unused-parameter -Wno-sign-compare -Wextra -Wno-missing-field-initializers -Wmissing-prototypes -Wstrict-prototypes")
   set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${COMMON_COMPILER_FLAGS}")
   set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${COMMON_COMPILER_FLAGS}")
endif()

add_subdirectory (navit)
