/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2018 Navit Team
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
// style with: clang-format -style=WebKit -i *

#include <glib.h>
#include <stdlib.h>
#include <math.h>

extern "C" {
#include "item.h"		/* needs to be first, as attr.h depends on it */

#include "callback.h"
#include "debug.h"
#include "event.h"

#include "point.h"		/* needs to be before graphics.h */
#include "coord.h"

#include "navit.h"
#include "xmlconfig.h"
#include "vehicle.h"
#include "transform.h"
#include "track.h"
}
#include <sys/sysinfo.h>
#include <time.h>
#include "ArduiPi_OLED_lib.h"
#include "Adafruit_GFX.h"
#include "ArduiPi_OLED.h"
#include "graphics_init_animation.h"

const size_t init_animation_frames = 3;
const size_t init_animation_images = 3;
const size_t init_animation_count = init_animation_frames * init_animation_images;
const int refresh_rate_ms = 100;
const int version_timeout = 5000;
const double moving_speed_threshold = 8;  // In KPH
const int display_blank_timeout = 10 * 60; // measured in seconds, aka 10 minutes

ArduiPi_OLED display;
simple_bm *init_animation[init_animation_count];
simple_bm *qr_logo;
const char* tone_cmd = "true";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32


struct graphics_priv {
	struct navit *nav;
	int width;
	int height;
	int imperial = 0;
	long tone_next = 0;
	long last_draw_time = 0;
	// screensaver state
	int ss_xpos = 0;
	int ss_ypos = 0;
};

struct latlong_pos {
	double lat;
	double lng;
};

static void
show_start_animation()
{
	static int xpos[3] = { 0, 6*8, 11*8 };
	static size_t step = 0;
#ifdef SIMPLE_BM_DEBUG
	simple_bm *bm = init_animation[0];
	display.drawBitmap(xpos[0],0,bm->get_bm(),bm->get_width(),bm->get_height(),WHITE);
#else
	for(size_t i = 0 ; i < init_animation_images ; i++ ) {
		simple_bm *bm = init_animation[step*init_animation_frames+i];
		display.drawBitmap(xpos[i],0,bm->get_bm(),bm->get_width(),bm->get_height(),WHITE);
	}
	step = (step + 1) % init_animation_images;
#endif
}

static long
get_uptime()
{
	struct sysinfo s_info;
	int error = sysinfo(&s_info);
	if (error != 0) {
		printf("code error = %d\n", error);
	}
	return s_info.uptime;
}

static double
get_native_speed(struct graphics_priv *ssd1306, double speed)
{
	if (ssd1306->imperial) {
		speed /= 1.609344;
	}
	return speed;
}

static double
get_vehicle_speed(struct graphics_priv *ssd1306, const struct attr &attr, latlong_pos &current_pos)
{
	double speed = -1;
	struct attr position_attr;
	if (vehicle_get_attr(attr.u.vehicle, attr_position_coord_geo, &position_attr, NULL)) {
		dbg(lvl_info, "%f %f\n", position_attr.u.coord_geo->lat, position_attr.u.coord_geo->lng);
		current_pos.lat = position_attr.u.coord_geo->lat;
		current_pos.lng = position_attr.u.coord_geo->lng;
		struct attr speed_attr;
		vehicle_get_attr(attr.u.vehicle, attr_position_speed, &speed_attr, NULL);
		speed = get_native_speed(ssd1306, *speed_attr.u.numd);
		dbg(lvl_debug, "speed : %0.0f (%f)\n", speed, speed);
	} else {
		dbg(lvl_error, "vehicle_get_attr failed\n");
	}
	return speed;
}

static double
get_route_speed(struct graphics_priv *ssd1306)
{
	double routespeed = -1;
	struct tracking *tracking = navit_get_tracking(ssd1306->nav);
	if (tracking) {
		struct attr maxspeed_attr;
		int *flags = tracking_get_current_flags(tracking);
		if (flags
		    && (*flags & AF_SPEED_LIMIT)
		    && tracking_get_attr(tracking, attr_maxspeed, &maxspeed_attr, NULL)) {
			routespeed = get_native_speed(ssd1306, maxspeed_attr.u.num);
		} else {
			dbg(lvl_warning, "Flag=(%p,%x)\n",flags,flags?*flags:0);
		}
	} else {
		dbg(lvl_warning, "Not tracking\n");
	}
	return routespeed;
}

static void
show_moving_display(struct graphics_priv *ssd1306, double speed, double routespeed, long current_tick)
{
	ssd1306->last_draw_time = current_tick;
	int speeding = routespeed != -1 && (speed > routespeed + 1);
	if (speeding && current_tick >= ssd1306->tone_next) {
		system(tone_cmd);
		ssd1306->tone_next = current_tick + 2;
	}
	if ( current_tick % 10 ) {
		dbg(lvl_debug,"General speed display\n");
		char snum[32];
		sprintf(snum, "%3.0f", speed);
		display.setTextSize(3);
		display.setCursor(1, 6);
		if (routespeed == -1) {
			display.printf(snum);
			display.setTextColor(BLACK, WHITE);
			display.setCursor(60, 6);
			display.printf("???");
			display.setTextColor(WHITE, BLACK);
		} else {
			dbg(lvl_debug, "route speed : %0.0f\n", routespeed);
			display.drawRect(62, 2, 62, display.height() - 4, WHITE);
			display.setCursor(66, 6);
			sprintf(snum, "%3.0f", routespeed);
			display.printf(snum);
			display.setCursor(1, 6);
			sprintf(snum, "%3.0f", speed);
			if (speeding && current_tick % 2) {
				display.setTextColor(BLACK, WHITE);	// 'inverted' text
				display.printf(snum);
			} else {
				display.setTextColor(WHITE, BLACK);
				display.printf(snum);
			}
		}
	} else {
		dbg(lvl_debug,"General Units display\n");
		display.setTextSize(3);
		display.setCursor(1, 6);
		display.printf(ssd1306->imperial ? "MPH" : "KM/H");
	}
}

static void
show_stationary_display(struct graphics_priv *ssd1306, const latlong_pos &current_pos, long current_tick)
{
	dbg(lvl_info,"Tick %ld, pos=%f,%f\n", current_tick, current_pos.lat, current_pos.lng);
	bool isBlanked = ssd1306->last_draw_time + display_blank_timeout < current_tick;
	if ( isBlanked ) {
		const char *msg = "Speedsaver";
		int width = -((int)strlen(msg) * 6);
		int height= display.height() - 8;
		if( ssd1306->ss_xpos <= width ) {
			ssd1306->ss_xpos = display.width();
			ssd1306->ss_ypos = rand()%height;
			dbg(lvl_info,"New start pos=%d,%d\n", ssd1306->ss_xpos, ssd1306->ss_ypos);
		} else {
			ssd1306->ss_xpos--;
		}
		display.setTextSize(1);
		display.setTextColor(WHITE);
		display.setCursor(ssd1306->ss_xpos, ssd1306->ss_ypos);
		display.print(msg);
	} else {
		char lat_buff[20];
		char lng_buff[20];
		snprintf(lat_buff, sizeof(lat_buff), "%c%9.5f",current_pos.lat >= 0 ? 'N' : 'S', fabs(current_pos.lat));
		snprintf(lng_buff, sizeof(lng_buff), "%c%9.5f",current_pos.lng >= 0 ? 'E' : 'W', fabs(current_pos.lng));
		display.setTextSize(2);
		display.setTextColor(WHITE);
		display.setCursor(0, 0);
		display.print(lat_buff);
		display.setCursor(0, 16);
		display.print(lng_buff);
	}
}

static gboolean
graphics_ssd1306_idle(void *data)
{
	struct graphics_priv *ssd1306 = (struct graphics_priv *) data;
	long current_tick = get_uptime();
	bool ggf = getenv("GOTTA_GO_FAST") != NULL;

	dbg(lvl_info, "idle at %ld, ggf=%d\n", current_tick, ggf);

	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0, 0);

	struct attr attr, attr2, attr3;
	struct attr_iter *iter = navit_attr_iter_new();
	if (navit_get_attr(ssd1306->nav, attr_vehicle, &attr, iter) &&
	   !navit_get_attr(ssd1306->nav, attr_vehicle, &attr2, iter)) {
		navit_attr_iter_destroy(iter);

		int update_delta = 10;

		if (vehicle_get_attr(attr.u.vehicle, attr_position_last_update, &attr3, NULL)) {
			update_delta = abs(time(NULL) - attr3.u.num);
		}

		if (ggf || update_delta < 2) {
			latlong_pos current_pos = { 0, 0 };
			double speed = get_vehicle_speed(ssd1306, attr, current_pos);
			double routespeed = get_route_speed(ssd1306);

			if (ggf) {
				routespeed = 50;
				speed = 88;
			}

			if(speed > get_native_speed(ssd1306, moving_speed_threshold)) {
				show_moving_display(ssd1306, speed, routespeed, current_tick);
			} else {
				show_stationary_display(ssd1306, current_pos, current_tick);
			}
		} else {
			dbg(lvl_debug,"General animation display\n");
			show_start_animation();
		}
		display.display();
		display.display();	//!! FIXME
	}
	g_timeout_add(refresh_rate_ms, graphics_ssd1306_idle, data);
	return G_SOURCE_REMOVE;
}

static gboolean
show_version_info(void *data)
{
	char version_id[200] = "Unknown";
	FILE *fp = fopen("/etc/version_stamp","r");
	if ( fp ) {
		fgets(version_id, sizeof(version_id), fp);
		fclose(fp);
	}

	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.print(version_id);
	display.display();
	display.display();	//!! FIXME

	// When we're done, start using the regular display timeout handler.
	g_timeout_add(version_timeout, graphics_ssd1306_idle, data);
	return G_SOURCE_REMOVE;
}

#if 0
// The pixels are too point-like (surrounded by darkness) and the contrast
// it too high for QR recognisers on phones to make sense of.
// We might stand a better chance if
// - The OLED pixels solidly butted together (like any normal display)
// - We had the 1.3" display with 64 pixel rows, then the QR code could be displayed in a 50x50 block
// - The proprietary https://en.wikipedia.org/wiki/QR_code#IQR_code is made open so we could use all the pixels :)
static gboolean
show_qrcode(void *data)
{
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,0);
	display.drawBitmap(48,0,qr_logo->get_bm(),qr_logo->get_width(),qr_logo->get_height(),WHITE);
	dbg(lvl_info, "QR: %d %d\n", qr_logo->get_width(), qr_logo->get_height());
	display.display();
	display.display();	//!! FIXME
	g_timeout_add(version_timeout, graphics_ssd1306_idle, data);
	return G_SOURCE_REMOVE;
}
#endif // 0


extern "C" void graphics_new(struct navit *nav)
{
	tone_cmd = g_strdup_printf("aplay \"%s/tone7.wav\" 2>/dev/null >/dev/null&", getenv("NAVIT_SHAREDIR"));
	char *bug = getenv("SSD1306_DEBUG_LEVEL");
	if ( bug )
		debug_level_set(dbg_module,(dbg_level)(*bug-'0'));

	struct attr *attr, imperial_attr;
	struct graphics_priv *this_ = g_new0(struct graphics_priv, 1);

	if (nav) {
		if (navit_get_attr
		    (nav, attr_imperial, &imperial_attr, NULL)) {
			this_->imperial = imperial_attr.u.num;
		}
	}
	generate_init_animations(init_animation,init_animation_count);
	generate_qr_bm(qr_logo);

	if (!display.init(OLED_I2C_RESET, OLED_ADAFRUIT_I2C_128x32))
		exit(-1);

	display.begin();
	display.clearDisplay();
	display.display();

	// Hackery for lctechF1C200s, which sometimes fail to show
	// the display at start of day.  Restarting navit has generally
	// proved to be a reliable way of getting the display back.
	// So we simulate that by closing the display and re-opening it.
	display.close();
	sleep(1);
	(void)display.init(OLED_I2C_RESET, OLED_ADAFRUIT_I2C_128x32);
	display.begin();
	display.clearDisplay();
	display.display();
	// End of hack

	this_->nav = nav;
	this_->last_draw_time = get_uptime();

	show_version_info(this_);

	dbg(lvl_info, "initialized\n");
}
