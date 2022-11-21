include_directories( "${CMAKE_CURRENT_SOURCE_DIR}")

# navit cre
set(NAVIT_SRC announcement.c atom.c attr.c cache.c callback.c command.c config_.c coord.c country.c data_window.c debug.c
   event.c file.c geom.c graphics.c gui.c item.c layout.c log.c main.c map.c maps.c
   linguistics.c mapset.c maptype.c menu.c messages.c bookmarks.c navit.c navit_nls.c navigation.c osd.c param.c phrase.c plugin.c popup.c
   profile.c profile_option.c projection.c roadprofile.c route.c script.c search.c speech.c start_real.c sunriset.c transform.c track.c
   search_houseno_interpol.c util.c vehicle.c vehicleprofile.c xmlconfig.c
   event_glib.c start.c
   map/binfile/binfile.c
   fib-1.1/fib.c
   graphics/ssd1306/graphics_ssd1306 graphics/ssd1306/graphics_ssd1306.cpp graphics/ssd1306/graphics_init_animation.cpp
   vehicle/gpsd/vehicle_gpsd.c
   gui/speedsaver/gui_speedsaver.c
   builtin.c)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/graphics/ssd1306/tone7.wav
       DESTINATION ${SHARE_DIR}
       PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
list(APPEND NAVIT_LIBS "-lArduiPi_OLED")

add_executable(navit ${NAVIT_SRC})
target_link_libraries (navit ${NAVIT_LIBS})
set_target_properties(navit PROPERTIES COMPILE_DEFINITIONS "LIBDIR=\"${CMAKE_INSTALL_PREFIX}/${LIB_DIR}\";PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")

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
