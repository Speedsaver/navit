/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2009 Navit Team
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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>
#include <math.h>
#include <time.h>
#include "debug.h"
#include "navit.h"
#include "callback.h"
#include "item.h"
#include "xmlconfig.h"
#include "projection.h"
#include "map.h"
#include "mapset.h"
#include "main.h"
#include "coord.h"
#include "point.h"
#include "transform.h"
#include "param.h"
#include "track.h"
#include "vehicle.h"
#include "attr.h"
#include "event.h"
#include "file.h"
#include "navit_nls.h"
#include "map.h"
#include "util.h"
#include "route.h"
#include "vehicleprofile.h"

/* define string for bookmark handling */
#define TEXTFILE_COMMENT_NAVI_STOPPED "# navigation stopped\n"

/**
 * @defgroup navit The navit core instance
 * @brief navit is the object containing most global data structures.
 *
 * Among others:
 * - a set of maps
 * - one or more vehicles
 * - a graphics object for rendering the map
 * - a route object
 * - a navigation object
 * @{
 */

//! The vehicle used for navigation.
struct navit_vehicle {
	int follow;
	/*! Limit of the follow counter. See navit_add_vehicle */
	int follow_curr;
	/*! Deprecated : follow counter itself. When it reaches 'update' counts, map is recentered*/
	struct coord coord;
	int dir;
	int speed;
	struct coord last; /*< Position of the last update of this vehicle */
	struct vehicle *vehicle;
	struct attr callback;
	int animate_cursor;
};

struct navit {
	NAVIT_OBJECT
	struct attr self;
	GList *mapsets;
	struct action *action;
	struct transformation *trans, *trans_cursor;
	struct compass *compass;
	struct route *route;
	struct navigation *navigation;
	struct speech *speech;
	struct tracking *tracking;
	int ready;
	struct window *win;
	struct displaylist *displaylist;
	int tracking_flag;
	int orientation;
	int recentdest_count;
	int osd_configuration;
	GList *vehicles;
	GList *windows_items;
	struct navit_vehicle *vehicle;
	struct callback_list *attr_cbl;
	struct callback *nav_speech_cb, *roadbook_callback, *popup_callback, *route_cb, *progress_cb;
	struct datawindow *roadbook_window;
	struct map *former_destination;
	struct point pressed, last, current;
	int button_pressed,moved,popped,zoomed;
	int center_timeout;
	int autozoom_secs;
	int autozoom_min;
	int autozoom_max;
	int autozoom_active;
	int autozoom_paused;
	struct event_timeout *button_timeout, *motion_timeout;
	struct callback *motion_timeout_callback;
	int ignore_button;
	int ignore_graphics_events;
	struct pcoord destination;
	int destination_valid;
	int blocked;	/**< Whether draw operations are currently blocked. This can be a combination of the
					     following flags:
					     1: draw operations are blocked
					     2: draw operations are pending, requiring a redraw once draw operations are unblocked */
	int w,h;
	int drag_bitmap;
	int use_mousewheel;
	struct callback *resize_callback,*button_callback,*motion_callback,*predraw_callback;
	struct vehicleprofile *vehicleprofile;
	GList *vehicleprofiles;
	int pitch;
	int follow_cursor;
	int prevTs;
	int graphics_flags;
	int zoom_min, zoom_max;
	int radius;
	int flags;
		 /* 1=No graphics ok */
		 /* 2=No gui ok */
	int border;
	int imperial;
	int waypoints_flag;
	struct coord_geo center;
	int auto_switch; /*auto switching between day/night layout enabled ?*/
};

struct attr_iter {
	void *iter;
	union {
		GList *list;
		struct mapset_handle *mapset_handle;
	} u;
};

static void navit_vehicle_update_position(struct navit *this_, struct navit_vehicle *nv);
static int navit_add_vehicle(struct navit *this_, struct vehicle *v);
static int navit_set_attr_do(struct navit *this_, struct attr *attr, int init);
static int navit_get_cursor_pnt(struct navit *this_, struct point *p, int keep_orientation, int *dir);
static void navit_set_vehicle(struct navit *this_, struct navit_vehicle *nv);
static int navit_set_vehicleprofile(struct navit *this_, struct vehicleprofile *vp);
struct object_func navit_func;

struct navit *global_navit;

void
navit_add_mapset(struct navit *this_, struct mapset *ms)
{
	this_->mapsets = g_list_append(this_->mapsets, ms);
}

struct mapset *
navit_get_mapset(struct navit *this_)
{
	if(this_->mapsets){
		return this_->mapsets->data;
	} else {
		dbg(lvl_error,"No mapsets enabled! Is it on purpose? Navit can't draw a map. Please check your navit.xml\n");
	}
	return NULL;
}

struct tracking *
navit_get_tracking(struct navit *this_)
{
	return this_->tracking;
}

/**
 * @brief	Get the user data directory.
 * @param[in]	 create	- create the directory if it does not exist
 *
 * @return	char * to the data directory string.
 *
 * returns the directory used to store user data files (center.txt,
 * destination.txt, bookmark.txt, ...)
 *
 */
char*
navit_get_user_data_directory(int create) {
	char *dir;
	dir = getenv("NAVIT_USER_DATADIR");
	if (create && !file_exists(dir)) {
		dbg(lvl_debug,"creating dir %s\n", dir);
		if (file_mkdir(dir,0)) {
			dbg(lvl_error,"failed creating dir %s\n", dir);
			return NULL;
		}
	}
	return dir;
}

static int
navit_restrict_to_range(int value, int min, int max){
	if (value>max) {
		value = max;
	}
	if (value<min) {
		value = min;
	}
	return value;
}

static void
navit_restrict_map_center_to_world_boundingbox(struct transformation *tr, struct coord *new_center){
	new_center->x = navit_restrict_to_range(new_center->x, WORLD_BOUNDINGBOX_MIN_X, WORLD_BOUNDINGBOX_MAX_X);
	new_center->y = navit_restrict_to_range(new_center->y, WORLD_BOUNDINGBOX_MIN_Y, WORLD_BOUNDINGBOX_MAX_Y);
}


void
navit_set_timeout(struct navit *this_)
{
	struct attr follow;
	follow.type=attr_follow;
	follow.u.num=this_->center_timeout;
	navit_set_attr(this_, &follow);
}

static void
navit_scale(struct navit *this_, long scale, struct point *p, int draw)
{
	struct coord c1, c2, *center;
	if (scale < this_->zoom_min)
		scale=this_->zoom_min;
	if (scale > this_->zoom_max)
		scale=this_->zoom_max;
	if (p)
		transform_reverse(this_->trans, p, &c1);
	transform_set_scale(this_->trans, scale);
	if (p) {
		transform_reverse(this_->trans, p, &c2);
		center = transform_center(this_->trans);
		center->x += c1.x - c2.x;
		center->y += c1.y - c2.y;
	}
}

static struct attr **
navit_get_coord(struct navit *this, struct attr **in, struct pcoord *pc)
{
	if (!in)
		return NULL;
	if (!in[0])
		return NULL;
	pc->pro = transform_get_projection(this->trans);
	if (ATTR_IS_STRING(in[0]->type)) {
		struct coord c;
		coord_parse(in[0]->u.str, pc->pro, &c);
		pc->x=c.x;
		pc->y=c.y;
		in++;	
	} else if (ATTR_IS_COORD(in[0]->type)) {
		pc->x=in[0]->u.coord->x;
		pc->y=in[0]->u.coord->y;
		in++;
	} else if (ATTR_IS_PCOORD(in[0]->type)) {
		*pc=*in[0]->u.pcoord;
		in++;
	} else if (in[1] && in[2] && ATTR_IS_INT(in[0]->type) && ATTR_IS_INT(in[1]->type) && ATTR_IS_INT(in[2]->type)) {
		pc->pro=in[0]->u.num;
		pc->x=in[1]->u.num;
		pc->y=in[2]->u.num;
		in+=3;
	} else if (in[1] && ATTR_IS_INT(in[0]->type) && ATTR_IS_INT(in[1]->type)) {
		pc->x=in[0]->u.num;
		pc->y=in[1]->u.num;
		in+=2;
	} else
		return NULL;
	return in;
}

struct navit *
navit_new(struct attr *parent, struct attr **attrs)
{
	struct navit *this_=g_new0(struct navit, 1);
	struct pcoord center;
	struct coord co;
	struct coord_geo g;
	enum projection pro=projection_mg;
	int zoom = 256;
	g.lat=53.13;
	g.lng=11.70;

	this_->func=&navit_func;
	navit_object_ref((struct navit_object *)this_);
	this_->attrs=attr_list_dup(attrs);
	this_->self.type=attr_navit;
	this_->self.u.navit=this_;
	this_->attr_cbl=callback_list_new();

	this_->orientation=-1;
	this_->tracking_flag=1;
	this_->recentdest_count=10;
	this_->osd_configuration=-1;

	this_->center_timeout = 10;
	this_->use_mousewheel = 1;
	this_->autozoom_secs = 10;
	this_->autozoom_min = 7;
	this_->autozoom_active = 0;
	this_->autozoom_paused = 0;
	this_->zoom_min = 1;
	this_->zoom_max = 2097152;
	this_->autozoom_max = this_->zoom_max;
	this_->follow_cursor = 1;
	this_->radius = 30;
	this_->border = 16;
	this_->auto_switch = TRUE;

	transform_from_geo(pro, &g, &co);
	center.x=co.x;
	center.y=co.y;
	center.pro = pro;
	this_->trans = transform_new(&center, zoom, (this_->orientation != -1) ? this_->orientation : 0);
	this_->trans_cursor = transform_new(&center, zoom, (this_->orientation != -1) ? this_->orientation : 0);

	this_->prevTs=0;

	for (;*attrs; attrs++) {
		navit_set_attr_do(this_, *attrs, 1);
	}

	dbg(lvl_debug,"return %p\n",this_);
	
	return this_;
}

struct vehicleprofile *
navit_get_vehicleprofile(struct navit *this_)
{
	return this_->vehicleprofile;
}

GList *
navit_get_vehicleprofiles(struct navit *this_)
{
	return this_->vehicleprofiles;
}

static void
navit_projection_set(struct navit *this_, enum projection pro, int draw)
{
	struct coord_geo g;
	struct coord *c;

	c=transform_center(this_->trans);
	transform_to_geo(transform_get_projection(this_->trans), c, &g);
	transform_set_projection(this_->trans, pro);
	transform_from_geo(pro, &g, c);
}

void
navit_init(struct navit *this_)
{
	struct mapset *ms;
	struct map *map;
	int callback;
	char *center_file;

	dbg(lvl_info,"Initializing graphics\n");
	graphics_new(this_);
	dbg(lvl_info,"Setting Vehicle\n");
	navit_set_vehicle(this_, this_->vehicle);
	dbg(lvl_info,"Adding dynamic maps to mapset %p\n",this_->mapsets);
	if (this_->mapsets) {
		struct mapset_handle *msh;
		ms=this_->mapsets->data;
		if (this_->route) {
			if ((map=route_get_map(this_->route))) {
				struct attr map_a;
				map_a.type=attr_map;
				map_a.u.map=map;
				mapset_add_attr(ms, &map_a);
			}
			if ((map=route_get_graph_map(this_->route))) {
				struct attr map_a,active;
				map_a.type=attr_map;
				map_a.u.map=map;
				active.type=attr_active;
				active.u.num=0;
				mapset_add_attr(ms, &map_a);
				map_set_attr(map, &active);
			}
			route_set_mapset(this_->route, ms);
			route_set_projection(this_->route, transform_get_projection(this_->trans));
		}
		if (this_->tracking) {
			tracking_set_mapset(this_->tracking, ms);
			if (this_->route)
				tracking_set_route(this_->tracking, this_->route);
		}
		if (this_->tracking) {
			if ((map=tracking_get_map(this_->tracking))) {
				struct attr map_a,active;
				map_a.type=attr_map;
				map_a.u.map=map;
				active.type=attr_active;
				active.u.num=0;
				mapset_add_attr(ms, &map_a);
				map_set_attr(map, &active);
			}
		}
	} else {
		dbg(lvl_error, "FATAL: No mapset available. Please add a (valid) mapset to your configuration.\n");
		exit(1);
	}
	global_navit=this_;

	callback_list_call_attr_1(this_->attr_cbl, attr_navit, this_);
	callback=(this_->ready == 2);
	this_->ready|=1;
	dbg(lvl_info,"ready=%d\n",this_->ready);
	if (callback)
		callback_list_call_attr_1(this_->attr_cbl, attr_graphics_ready, this_);
}

static int
navit_set_attr_do(struct navit *this_, struct attr *attr, int init)
{
	int dir=0, orient_old=0, attr_updated=0;
	struct coord co;
	long zoom;
	GList *l;
	struct navit_vehicle *nv;
	struct attr active;
	active.type=attr_active;
	active.u.num=0;

	dbg(lvl_debug, "enter, this_=%p, attr=%p (%s), init=%d\n", this_, attr, attr_to_name(attr->type), init);

	switch (attr->type) {
	case attr_autozoom:
		attr_updated=(this_->autozoom_secs != attr->u.num);
		this_->autozoom_secs = attr->u.num;
		break;
	case attr_autozoom_active:
		attr_updated=(this_->autozoom_active != attr->u.num);
		this_->autozoom_active = attr->u.num;
		break;
	case attr_center:
		transform_from_geo(transform_get_projection(this_->trans), attr->u.coord_geo, &co);
		dbg(lvl_debug,"0x%x,0x%x\n",co.x,co.y);
		transform_set_center(this_->trans, &co);
		break;
	case attr_drag_bitmap:
		attr_updated=(this_->drag_bitmap != !!attr->u.num);
		this_->drag_bitmap=!!attr->u.num;
		break;
	case attr_flags:
		attr_updated=(this_->flags != attr->u.num);
		this_->flags=attr->u.num;
		break;
	case attr_flags_graphics:
		attr_updated=0;
		break;
	case attr_follow:
		if (!this_->vehicle)
			return 0;
		attr_updated=(this_->vehicle->follow_curr != attr->u.num);
		this_->vehicle->follow_curr = attr->u.num;
		break;
	case attr_layout:
		break;
	case attr_layout_name:
		return 0;
	case attr_map_border:
		if (this_->border != attr->u.num) {
			this_->border=attr->u.num;
			attr_updated=1;
		}
		break;
	case attr_orientation:
		orient_old=this_->orientation;
		this_->orientation=attr->u.num;
		if (!init) {
			if (this_->orientation != -1) {
				dir = this_->orientation;
			} else {
				if (this_->vehicle) {
					dir = this_->vehicle->dir;
				}
			}
			transform_set_yaw(this_->trans, dir);
			if (orient_old != this_->orientation) {
				attr_updated=1;
			}
		}
		break;
	case attr_osd_configuration:
		dbg(lvl_debug,"setting osd_configuration to %ld (was %d)\n", attr->u.num, this_->osd_configuration);
		attr_updated=(this_->osd_configuration != attr->u.num);
		this_->osd_configuration=attr->u.num;
		break;
	case attr_pitch:
		attr_updated=(this_->pitch != attr->u.num);
		this_->pitch=attr->u.num;
		transform_set_pitch(this_->trans, round(this_->pitch*sqrt(240*320)/sqrt(this_->w*this_->h))); // Pitch corrected for window resolution
		break;
	case attr_projection:
		if(this_->trans && transform_get_projection(this_->trans) != attr->u.projection) {
			navit_projection_set(this_, attr->u.projection, !init);
			attr_updated=1;
		}
		break;
	case attr_radius:
		attr_updated=(this_->radius != attr->u.num);
		this_->radius=attr->u.num;
		break;
	case attr_recent_dest:
		attr_updated=(this_->recentdest_count != attr->u.num);
		this_->recentdest_count=attr->u.num;
		break;
	case attr_speech:
        	if(this_->speech && this_->speech != attr->u.speech) {
			attr_updated=1;
			this_->speech = attr->u.speech;
        	}
		break;
	case attr_timeout:
		attr_updated=(this_->center_timeout != attr->u.num);
		this_->center_timeout = attr->u.num;
		break;
	case attr_tracking:
		attr_updated=(this_->tracking_flag != !!attr->u.num);
		this_->tracking_flag=!!attr->u.num;
		break;
	case attr_transformation:
		this_->trans=attr->u.transformation;
		break;
	case attr_use_mousewheel:
		attr_updated=(this_->use_mousewheel != !!attr->u.num);
		this_->use_mousewheel=!!attr->u.num;
		break;
	case attr_vehicle:
		if (!attr->u.vehicle) {
			if (this_->vehicle) {
				vehicle_set_attr(this_->vehicle->vehicle, &active);
				navit_set_vehicle(this_, NULL);
				attr_updated=1;
			}
			break;
		}
		l=this_->vehicles;
		while(l) {
			nv=l->data;
			if (nv->vehicle == attr->u.vehicle) {
				if (!this_->vehicle || this_->vehicle->vehicle != attr->u.vehicle) {
					if (this_->vehicle)
						vehicle_set_attr(this_->vehicle->vehicle, &active);
					active.u.num=1;
					vehicle_set_attr(nv->vehicle, &active);
					attr_updated=1;
				}
				navit_set_vehicle(this_, nv);
			}
			l=g_list_next(l);
		}
		break;
	case attr_vehicleprofile:
		attr_updated=navit_set_vehicleprofile(this_, attr->u.vehicleprofile);
		break;
	case attr_zoom:
		zoom=transform_get_scale(this_->trans);
		attr_updated=(zoom != attr->u.num);
		transform_set_scale(this_->trans, attr->u.num);
		break;
	case attr_zoom_min:
		attr_updated=(attr->u.num != this_->zoom_min);
		this_->zoom_min=attr->u.num;
		break;
	case attr_zoom_max:
		attr_updated=(attr->u.num != this_->zoom_max);
		this_->zoom_max=attr->u.num;
		break;
	case attr_message:
		break;
	case attr_follow_cursor:
		attr_updated=(this_->follow_cursor != !!attr->u.num);
		this_->follow_cursor=!!attr->u.num;
		break;
	case attr_imperial:
		attr_updated=(this_->imperial != attr->u.num);
		this_->imperial=attr->u.num;
		break;
	case attr_waypoints_flag:
		attr_updated=(this_->waypoints_flag != !!attr->u.num);
		this_->waypoints_flag=!!attr->u.num;
		break;
	default:
		dbg(lvl_debug, "calling generic setter method for attribute type %s\n", attr_to_name(attr->type))
		return navit_object_set_attr((struct navit_object *) this_, attr);
	}
	if (attr_updated && !init) {
		callback_list_call_attr_2(this_->attr_cbl, attr->type, this_, attr);
	}
	return 1;
}

int
navit_set_attr(struct navit *this_, struct attr *attr)
{
	return navit_set_attr_do(this_, attr, 0);
}

int
navit_get_attr(struct navit *this_, enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
	struct coord *c;
	int len,offset;
	int ret=1;

	switch (type) {
	case attr_message:
		break;
        case attr_imperial: 
                attr->u.num=this_->imperial; 
                break; 
	case attr_bookmark_map:
		break;
	case attr_bookmarks:
		break;
	case attr_callback_list:
		attr->u.callback_list=this_->attr_cbl;
		break;
	case attr_center:
		c=transform_get_center(this_->trans);
		transform_to_geo(transform_get_projection(this_->trans), c, &this_->center);
		attr->u.coord_geo=&this_->center;
		break;
	case attr_destination:
		if (! this_->destination_valid)
			return 0;
		attr->u.pcoord=&this_->destination;
		break;
	case attr_displaylist:
		attr->u.displaylist=this_->displaylist;
		return (attr->u.displaylist != NULL);
	case attr_follow:
		if (!this_->vehicle)
			return 0;
		attr->u.num=this_->vehicle->follow_curr;
		break;
	case attr_former_destination_map:
		attr->u.map=this_->former_destination;
		break;
	case attr_graphics:
		ret=0;
		break;
	case attr_gui:
		ret=0;
		break;
	case attr_layer:
		ret=attr_generic_get_attr(this_->attrs, NULL, type, attr, iter?(struct attr_iter *)&iter->iter:NULL);
		break;
	case attr_layout:
		break;
	case attr_map:
		if (iter && this_->mapsets) {
			if (!iter->u.mapset_handle) {
				iter->u.mapset_handle=mapset_open((struct mapset *)this_->mapsets->data);
			}
			attr->u.map=mapset_next(iter->u.mapset_handle, 0);
			if(!attr->u.map) {
				mapset_close(iter->u.mapset_handle);
				return 0;
			}
		} else {
			return 0;
		}
		break;
	case attr_mapset:
		attr->u.mapset=this_->mapsets->data;
		ret=(attr->u.mapset != NULL);
		break;
	case attr_navigation:
		attr->u.navigation=this_->navigation;
		break;
	case attr_orientation:
		attr->u.num=this_->orientation;
		break;
	case attr_osd:
		ret=attr_generic_get_attr(this_->attrs, NULL, type, attr, iter?(struct attr_iter *)&iter->iter:NULL);
		break;
	case attr_osd_configuration:
		attr->u.num=this_->osd_configuration;
		break;
	case attr_pitch:
		attr->u.num=round(transform_get_pitch(this_->trans)*sqrt(this_->w*this_->h)/sqrt(240*320)); // Pitch corrected for window resolution
		break;
	case attr_projection:
		if(this_->trans) {
			attr->u.num=transform_get_projection(this_->trans);
		} else {
			return 0;
		}
		break;
	case attr_route:
		attr->u.route=this_->route;
		break;
	case attr_speech:
                if(this_->speech) {
                        attr->u.speech=this_->speech;
		} else {
                        return  0;
                }
	        break;
	case attr_timeout:
		attr->u.num=this_->center_timeout;
		break;
	case attr_tracking:
		attr->u.num=this_->tracking_flag;
		break;
	case attr_trackingo:
		attr->u.tracking=this_->tracking;
		break;
	case attr_transformation:
		attr->u.transformation=this_->trans;
		break;
	case attr_vehicle:
		if(iter) {
			if(iter->u.list) {
				iter->u.list=g_list_next(iter->u.list);
			} else { 
				iter->u.list=this_->vehicles;
			}
			if(!iter->u.list)
				return 0;
			attr->u.vehicle=((struct navit_vehicle*)iter->u.list->data)->vehicle;
		} else {
			if(this_->vehicle) {
				attr->u.vehicle=this_->vehicle->vehicle;
			} else {
				return 0;
			}
		}
		break;
	case attr_vehicleprofile:
		if (iter) {
			if(iter->u.list) {
				iter->u.list=g_list_next(iter->u.list);
			} else { 
				iter->u.list=this_->vehicleprofiles;
			}
			if(!iter->u.list)
				return 0;
			attr->u.vehicleprofile=iter->u.list->data;
		} else {
			attr->u.vehicleprofile=this_->vehicleprofile;
		}
		break;
	case attr_zoom:
		attr->u.num=transform_get_scale(this_->trans);
		break;
	case attr_autozoom_active:
		attr->u.num=this_->autozoom_active;
		break;
	case attr_follow_cursor:
		attr->u.num=this_->follow_cursor;
		break;
	case attr_waypoints_flag:
		attr->u.num=this_->waypoints_flag;
		break;
	default:
		dbg(lvl_debug, "calling generic getter method for attribute type %s\n", attr_to_name(type))
		return navit_object_get_attr((struct navit_object *) this_, type, attr, iter);
	}
	attr->type=type;
	return ret;
}

int
navit_add_attr(struct navit *this_, struct attr *attr)
{
	int ret=1;
	switch (attr->type) {
	case attr_callback:
		navit_add_callback(this_, attr->u.callback);
		break;
	case attr_log:
		break;
	case attr_gui:
		break;
	case attr_graphics:
		break;
	case attr_layout:
		break;
	case attr_route:
		this_->route=attr->u.route;
		break;
	case attr_mapset:
		this_->mapsets = g_list_append(this_->mapsets, attr->u.mapset);
		break;
	case attr_navigation:
		this_->navigation=attr->u.navigation;
		break;
	case attr_osd:
		break;
	case attr_recent_dest:
		this_->recentdest_count = attr->u.num;
		break;
	case attr_speech:
		this_->speech=attr->u.speech;
		break;
	case attr_trackingo:
		this_->tracking=attr->u.tracking;
		break;
	case attr_vehicle:
		ret=navit_add_vehicle(this_, attr->u.vehicle);
		break;
	case attr_vehicleprofile:
		this_->vehicleprofiles=g_list_append(this_->vehicleprofiles, attr->u.vehicleprofile);
		break;
	case attr_autozoom_min:
		this_->autozoom_min = attr->u.num;
		break;
	case attr_autozoom_max:
		this_->autozoom_max = attr->u.num;
		break;
	case attr_layer:
	default:
		return 0;
	}
	if (ret)
		this_->attrs=attr_generic_add_attr(this_->attrs, attr);
	callback_list_call_attr_2(this_->attr_cbl, attr->type, this_, attr);
	return ret;
}

int
navit_remove_attr(struct navit *this_, struct attr *attr)
{
	int ret=1;
	switch (attr->type) {
	case attr_callback:
		navit_remove_callback(this_, attr->u.callback);
		break;
	case attr_vehicle:
	case attr_osd:
		this_->attrs=attr_generic_remove_attr(this_->attrs, attr);
		return 1;
	default:
		return 0;
	}
	return ret;
}

struct attr_iter *
navit_attr_iter_new(void)
{
	return g_new0(struct attr_iter, 1);
}

void
navit_attr_iter_destroy(struct attr_iter *iter)
{
	g_free(iter);
}

void
navit_add_callback(struct navit *this_, struct callback *cb)
{
	callback_list_add(this_->attr_cbl, cb);
}

void
navit_remove_callback(struct navit *this_, struct callback *cb)
{
	callback_list_remove(this_->attr_cbl, cb);
}

static int
coord_not_set(struct coord c){
	return !(c.x || c.y);
}

/**
 * @brief Called when the position of a vehicle changes.
 *
 * This function is called when the position of any configured vehicle changes and triggers all actions
 * that need to happen in response, such as:
 * <ul>
 * <li>Switching between day and night layout (based on the new position timestamp)</li>
 * <li>Updating position, bearing and speed of {@code nv} with the data of the active vehicle
 * (which may be different from the vehicle reporting the update)</li>
 * <li>Invoking callbacks for {@code navit}'s {@code attr_position} and {@code attr_position_coord_geo}
 * attributes</li>
 * <li>Triggering an update of the vehicle's position on the map and, if needed, an update of the
 * visible map area ad orientation</li>
 * <li>Logging a new track point, if enabled</li>
 * <li>Updating the position on the route</li>
 * <li>Stopping navigation if the destination has been reached</li>
 * </ul>
 *
 * @param this_ The navit object
 * @param nv The {@code navit_vehicle} which reported a new position
 */
static void
navit_vehicle_update_position(struct navit *this_, struct navit_vehicle *nv) {
	struct attr attr_valid, attr_dir, attr_speed, attr_pos;
	struct pcoord cursor_pc;
	struct point cursor_pnt, *pnt=&cursor_pnt;
	struct tracking *tracking=NULL;
	struct pcoord *pc;
	enum projection pro=transform_get_projection(this_->trans_cursor);
	int count;
	int (*get_attr)(void *, enum attr_type, struct attr *, struct attr_iter *);
	void *attr_object;
	char *destination_file;
	char *description;

	if (this_->vehicle == nv && this_->tracking_flag)
		tracking=this_->tracking;
	if (tracking) {
		tracking_update(tracking, nv->vehicle, this_->vehicleprofile, pro);
		attr_object=tracking;
		get_attr=(int (*)(void *, enum attr_type, struct attr *, struct attr_iter *))tracking_get_attr;
	} else {
		attr_object=nv->vehicle;
		get_attr=(int (*)(void *, enum attr_type, struct attr *, struct attr_iter *))vehicle_get_attr;
	}
	if (get_attr(attr_object, attr_position_valid, &attr_valid, NULL))
		if (!attr_valid.u.num != attr_position_valid_invalid)
			return;
	if (! get_attr(attr_object, attr_position_direction, &attr_dir, NULL) ||
	    ! get_attr(attr_object, attr_position_speed, &attr_speed, NULL) ||
	    ! get_attr(attr_object, attr_position_coord_geo, &attr_pos, NULL)) {
		return;
	}
	nv->dir=*attr_dir.u.numd;
	nv->speed=*attr_speed.u.numd;
	transform_from_geo(pro, attr_pos.u.coord_geo, &nv->coord);
	cursor_pc.x = nv->coord.x;
	cursor_pc.y = nv->coord.y;
	cursor_pc.pro = pro;
	if (this_->route) {
		if (tracking)
			route_set_position_from_tracking(this_->route, tracking, pro);
		else
			route_set_position(this_->route, &cursor_pc);
	}
	callback_list_call_attr_0(this_->attr_cbl, attr_position);
	if (this_->ready == 3) {
		transform(this_->trans_cursor, pro, &nv->coord, &cursor_pnt, 1, 0, 0, NULL);

		if (nv->follow_curr > 1)
			nv->follow_curr--;
		else
			nv->follow_curr=nv->follow;
	}
	callback_list_call_attr_2(this_->attr_cbl, attr_position_coord_geo, this_, nv->vehicle);
}

/**
 * @brief Called when a status attribute of a vehicle changes.
 *
 * This function is called when the {@code position_fix_type}, {@code position_sats_used} or {@code position_hdop}
 * attribute of any configured vehicle changes.
 *
 * The function checks if {@code nv} refers to the active vehicle and if {@code type} is one of the above types.
 * If this is the case, it invokes the callback functions for {@code navit}'s respective attributes.
 *
 * Future actions that need to happen when one of these three attribute changes for any vehicle should be
 * implemented here.
 *
 * @param this_ The navit object
 * @param nv The {@code navit_vehicle} which reported a new status attribute
 * @param type The type of attribute with has changed
 */
static void
navit_vehicle_update_status(struct navit *this_, struct navit_vehicle *nv, enum attr_type type) {
	if (this_->vehicle != nv)
		return;
	switch(type) {
	case attr_position_fix_type:
	case attr_position_sats_used:
	case attr_position_hdop:
		callback_list_call_attr_2(this_->attr_cbl, type, this_, nv->vehicle);
		break;
	default:
		return;
	}
}

/**
 * Set the position of the vehicle
 *
 * @param navit The navit instance
 * @param c The coordinate to set as position
 * @returns nothing
 */

void
navit_set_position(struct navit *this_, struct pcoord *c)
{
	if (this_->route) {
		route_set_position(this_->route, c);
		callback_list_call_attr_0(this_->attr_cbl, attr_position);
	}
}

static int
navit_set_vehicleprofile(struct navit *this_, struct vehicleprofile *vp)
{
	if (this_->vehicleprofile == vp)
		return 0;
	this_->vehicleprofile=vp;
	if (this_->route)
		route_set_profile(this_->route, this_->vehicleprofile);
	return 1;
}

int
navit_set_vehicleprofile_name(struct navit *this_, char *name)
{
	struct attr attr;
	GList *l;
	l=this_->vehicleprofiles;
	while (l) {
		if (vehicleprofile_get_attr(l->data, attr_name, &attr, NULL)) {
			if (!strcmp(attr.u.str, name)) {
				navit_set_vehicleprofile(this_, l->data);
				return 1;
			}
		}
		l=g_list_next(l);
	}
	return 0;
}

static void
navit_set_vehicle(struct navit *this_, struct navit_vehicle *nv)
{
	struct attr attr;
	this_->vehicle=nv;
	if (nv && vehicle_get_attr(nv->vehicle, attr_profilename, &attr, NULL)) {
		if (navit_set_vehicleprofile_name(this_, attr.u.str))
			return;
	}
	if (!this_->vehicleprofile) { // When deactivating vehicle, keep the last profile if any
		if (!navit_set_vehicleprofile_name(this_,"car")) {
			/* We do not have a fallback "car" profile
			* so lets set any profile */
			GList *l;
			l=this_->vehicleprofiles;
			if (l) {
		    		this_->vehicleprofile=l->data;
		    		if (this_->route)
				route_set_profile(this_->route, this_->vehicleprofile);
			}
		}
	}
	else {
		if (this_->route)
			route_set_profile(this_->route, this_->vehicleprofile);
	}
}

/**
 * @brief Registers a new vehicle.
 *
 * @param this_ The navit instance
 * @param v The vehicle to register
 * @return True for success
 */
static int
navit_add_vehicle(struct navit *this_, struct vehicle *v)
{
	struct navit_vehicle *nv=g_new0(struct navit_vehicle, 1);
	struct attr follow, active, animate;
	nv->vehicle=v;
	nv->follow=0;
	nv->last.x = 0;
	nv->last.y = 0;
	nv->animate_cursor=0;
	if ((vehicle_get_attr(v, attr_follow, &follow, NULL)))
		nv->follow=follow.u.num;
	nv->follow_curr=nv->follow;
	this_->vehicles=g_list_append(this_->vehicles, nv);
	if ((vehicle_get_attr(v, attr_active, &active, NULL)) && active.u.num)
		navit_set_vehicle(this_, nv);
	if ((vehicle_get_attr(v, attr_animate, &animate, NULL)))
		nv->animate_cursor=animate.u.num;
	nv->callback.type=attr_callback;
	nv->callback.u.callback=callback_new_attr_2(callback_cast(navit_vehicle_update_position), attr_position_coord_geo, this_, nv);
	vehicle_add_attr(nv->vehicle, &nv->callback);
	nv->callback.u.callback=callback_new_attr_3(callback_cast(navit_vehicle_update_status), attr_position_fix_type, this_, nv, attr_position_fix_type);
	vehicle_add_attr(nv->vehicle, &nv->callback);
	nv->callback.u.callback=callback_new_attr_3(callback_cast(navit_vehicle_update_status), attr_position_sats_used, this_, nv, attr_position_sats_used);
	vehicle_add_attr(nv->vehicle, &nv->callback);
	nv->callback.u.callback=callback_new_attr_3(callback_cast(navit_vehicle_update_status), attr_position_hdop, this_, nv, attr_position_hdop);
	vehicle_add_attr(nv->vehicle, &nv->callback);
	vehicle_set_attr(nv->vehicle, &this_->self);
	return 1;
}



struct transformation *
navit_get_trans(struct navit *this_)
{
	return this_->trans;
}

struct route *
navit_get_route(struct navit *this_)
{
	return this_->route;
}

int 
navit_set_vehicle_by_name(struct navit *n,const char *name) 
{
    struct vehicle *v;
    struct attr_iter *iter;
    struct attr vehicle_attr, name_attr;

	iter=navit_attr_iter_new();

    while (navit_get_attr(n,attr_vehicle,&vehicle_attr,iter)) {
		v=vehicle_attr.u.vehicle;
		vehicle_get_attr(v,attr_name,&name_attr,NULL);
		if (name_attr.type==attr_name) {
			if (!strcmp(name,name_attr.u.str)) {
				navit_set_attr(n,&vehicle_attr);				
				navit_attr_iter_destroy(iter);
				return 1;
			}
		}
	}
    navit_attr_iter_destroy(iter);
    return 0;
}

void
navit_destroy(struct navit *this_)
{
	dbg(lvl_debug,"enter %p\n",this_);
	callback_list_call_attr_1(this_->attr_cbl, attr_destroy, this_);
	attr_list_free(this_->attrs);

	callback_destroy(this_->roadbook_callback);
	callback_destroy(this_->motion_timeout_callback);
	callback_destroy(this_->progress_cb);

	callback_destroy(this_->resize_callback);
	callback_destroy(this_->motion_callback);
	callback_destroy(this_->predraw_callback);

        callback_destroy(this_->route_cb);
	if (this_->route)
		route_destroy(this_->route);

        map_destroy(this_->former_destination);

	g_free(this_);
}

struct object_func navit_func = {
	attr_navit,
	(object_func_new)navit_new,
	(object_func_get_attr)navit_get_attr,
	(object_func_iter_new)navit_attr_iter_new,
	(object_func_iter_destroy)navit_attr_iter_destroy,
	(object_func_set_attr)navit_set_attr,
	(object_func_add_attr)navit_add_attr,
	(object_func_remove_attr)navit_remove_attr,
	(object_func_init)navit_init,
	(object_func_destroy)navit_destroy,
	(object_func_dup)NULL,
	(object_func_ref)navit_object_ref,
	(object_func_unref)navit_object_unref,
};

/** @} */
