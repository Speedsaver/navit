/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

void module_map_binfile_init(void);
void module_graphics_ssd1306_init(void);
void module_vehicle_gpsd_init(void);
void module_gui_speedsaver_init(void);
void builtin_init(void) {
    module_map_binfile_init();
    module_graphics_ssd1306_init();
    module_vehicle_gpsd_init();
    module_gui_speedsaver_init();
}

#include "start_real.h"

int
main(int argc, char **argv)
{
	return main_real(argc, argv);
}
