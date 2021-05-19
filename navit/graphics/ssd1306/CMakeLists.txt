# list(APPEND NAVIT_LIBS "-lArduiPi_OLED")
# set(graphics_ssd1306_LIBS "-lssd1306")
set(graphics_ssd1306_LIBS "-lArduiPi_OLED")
module_add_library(graphics_ssd1306 graphics_ssd1306.cpp graphics_init_animation.cpp)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tone7.wav
       DESTINATION ${SHARE_DIR}
       PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
