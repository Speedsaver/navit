# list(APPEND NAVIT_LIBS "-lArduiPi_OLED")
# set(graphics_ssd1306_LIBS "-lssd1306")
set(graphics_ssd1306_LIBS "-lArduiPi_OLED")
module_add_library(graphics_ssd1306 graphics_ssd1306.cpp) 
