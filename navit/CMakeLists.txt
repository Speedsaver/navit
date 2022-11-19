include_directories( "${PROJECT_BINARY_DIR}")
include_directories( "${PROJECT_SOURCE_DIR}")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}")
include_directories( "${CMAKE_CURRENT_BINARY_DIR}")
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/support")

# navit cre
set(NAVIT_SRC announcement.c atom.c attr.c cache.c callback.c command.c config_.c coord.c country.c data_window.c debug.c
   event.c file.c geom.c graphics.c gui.c item.c layout.c log.c main.c map.c maps.c
   linguistics.c mapset.c maptype.c menu.c messages.c bookmarks.c navit.c navit_nls.c navigation.c osd.c param.c phrase.c plugin.c popup.c
   profile.c profile_option.c projection.c roadprofile.c route.c script.c search.c speech.c start_real.c sunriset.c transform.c track.c
   search_houseno_interpol.c util.c vehicle.c vehicleprofile.c xmlconfig.c )

list(APPEND NAVIT_SRC  ${CMAKE_CURRENT_BINARY_DIR}/builtin.c)

list(APPEND NAVIT_SRC event_glib.c)

set(NAVIT_LIBNAME navit_core)


foreach ( PLUGIN_PATH ${ALL_PLUGINS})
   if (${PLUGIN_PATH})
      include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/${PLUGIN_PATH}")
      
      string(REPLACE "/" "_" MODULE_INC ${PLUGIN_PATH})
      list(APPEND NAVIT_SUPPORT_LIBS ${MODULE_INC})
   endif()
endforeach()

foreach ( MODULE_PATH ${ALL_MODULES})
   if ( ${MODULE_PATH} )
      include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE_PATH}")
      
      string(REPLACE "/" "_" MODULE_INC ${MODULE_PATH})
      list(APPEND MODULES_PROTOTYPE "\nvoid\tmodule_${MODULE_INC}_init(void)")
      list(APPEND MODULES_FUNC "\nmodule_${MODULE_INC}_init()")
      
      list(APPEND MODULES_NAME ${MODULE_INC})
   endif()
endforeach()

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/builtin.c.in ${CMAKE_CURRENT_BINARY_DIR}/builtin.c)

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/fib-1.1")

# Add module paths
foreach (CURRENT_MODULE ${ALL_PLUGINS} ${ALL_MODULE_PLUGINS} ${ALL_MODULES})
   if (${CURRENT_MODULE})
      add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/${CURRENT_MODULE}")
   endif()
endforeach()

add_subdirectory (fib-1.1)

set(NAVIT_START_SRC start.c)
if (BUILD_BUNDLE)
   list(APPEND NAVIT_START_SRC resources/share resources/Icon.png resources/Default.png)
   add_custom_command(OUTPUT resources/Icon.png COMMAND convert -scale 79x79 -crop 60x60+8+19 ${CMAKE_CURRENT_SOURCE_DIR}/icons/desktop_icons/128x128/navit.png resources/Icon.png)
   add_custom_command(OUTPUT resources/Default.png COMMAND convert -scale 79x79 -crop 60x60+8+19 ${CMAKE_CURRENT_SOURCE_DIR}/icons/desktop_icons/128x128/navit.png resources/Default.png)
endif()
add_executable(navit ${NAVIT_START_SRC})
target_link_libraries (navit ${NAVIT_LIBNAME})
if(DEFINED NAVIT_BINARY)
        set_target_properties(navit PROPERTIES OUTPUT_NAME ${NAVIT_BINARY})
endif(DEFINED NAVIT_BINARY)
if (BUILD_BUNDLE)
   add_custom_command(OUTPUT resources/share COMMAND mkdir -p resources/share)
   set_source_files_properties(resources/share resources/Icon.png resources/Default.png PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
   set_target_properties(navit PROPERTIES MACOSX_BUNDLE TRUE)
   set_target_properties(navit PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
   add_dependencies(navit navit_config_xml_resource)
   add_dependencies(navit images_resource)
   add_dependencies(navit locale_resource)
endif()

if (SHARED_LIBNAVIT)
  add_library (${NAVIT_LIBNAME} SHARED ${NAVIT_SRC} )
else(SHARED_LIBNAVIT)
  add_library (${NAVIT_LIBNAME} STATIC ${NAVIT_SRC} )
endif(SHARED_LIBNAVIT)
SET(NAVIT_LIBS ${NAVIT_LIBS} m)
target_link_libraries(${NAVIT_LIBNAME}  ${MODULES_NAME} ${NAVIT_SUPPORT_LIBS} fib ${NAVIT_LIBS} ) 
set_target_properties(${NAVIT_LIBNAME} PROPERTIES COMPILE_DEFINITIONS "MODULE=navit;LIBDIR=\"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}\";PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
if (DEFINED NAVIT_COMPILE_FLAGS)
   set_target_properties(${NAVIT_LIBNAME} PROPERTIES COMPILE_FLAGS "${NAVIT_COMPILE_FLAGS}")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/navit_shipped.xml ${CMAKE_CURRENT_BINARY_DIR}/navit.xml COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/navit.dtd ${CMAKE_CURRENT_BINARY_DIR}/navit.dtd COPYONLY)

add_custom_target( navit_config_xml_resource DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/navit.xml COMMAND mkdir -p resources/share/navit COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/navit.xml resources/share/navit)
add_custom_target( locale_resource DEPENDS locales COMMAND mkdir -p resources/share COMMAND cp -a ${CMAKE_CURRENT_BINARY_DIR}/../locale resources/share/locale)
add_custom_target( navit_config_xml ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/navit.xml)


add_subdirectory (maptool)

install(TARGETS navit
        DESTINATION ${BIN_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/navit.xml
        DESTINATION ${SHARE_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

get_directory_property(INCLUDE_DIRECTORIES INCLUDE_DIRECTORIES)
WRITE_FILE("cmake_plugin_settings.txt"
	"set(MODULE_BUILD_TYPE \"${MODULE_BUILD_TYPE}\" CACHE STRING init)\n"
	"set(NAVIT_COMPILE_FLAGS \"${NAVIT_COMPILE_FLAGS}\" CACHE STRING init)\n"
	"set(navit_SOURCE_DIR \"${navit_SOURCE_DIR}\" CACHE STRING init)\n"
	"set(NAVIT_LIBNAME \"${NAVIT_LIBNAME}\" CACHE STRING init)\n"
	"set(CMAKE_TOOLCHAIN_FILE \"${CMAKE_TOOLCHAIN_FILE}\" CACHE STRING init)\n"
	"set(INCLUDE_DIRECTORIES \"${INCLUDE_DIRECTORIES}\" CACHE STRING init)\n"
	"set(LIB_DIR \"${LIB_DIR}\" CACHE STRING init)\n"
	"set(CMAKE_INSTALL_PREFIX \"${CMAKE_INSTALL_PREFIX}\" CACHE STRING init)\n"
	)
