extern void builtin_init(void); 
void	module_map_binfile_init(void);
void	module_graphics_ssd1306_init(void);
void    module_vehicle_gpsd_init(void);
void    module_gui_speedsaver_init(void);
void builtin_init(void) { 
    module_map_binfile_init();
    module_graphics_ssd1306_init();
    module_vehicle_gpsd_init();
    module_gui_speedsaver_init();
}
