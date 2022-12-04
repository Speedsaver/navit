/*
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
 
/** @file vehicle.c
 * @brief Generic components of the vehicle object.
 * 
 * This file implements the generic vehicle interface, i.e. everything which is
 * not specific to a single data source.
 *
 * @author Navit Team
 * @date 2005-2014
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <time.h>
#include "debug.h"
#include "coord.h"
#include "item.h"
#include "xmlconfig.h"
#include "plugin.h"
#include "transform.h"
#include "util.h"
#include "event.h"
#include "coord.h"
#include "transform.h"
#include "projection.h"
#include "point.h"
#include "callback.h"
#include "vehicle.h"
#include "navit_nls.h"

struct vehicle {
	NAVIT_OBJECT
	struct vehicle_methods meth;
	struct vehicle_priv *priv;
	struct callback_list *cbl;
	char *gpx_desc;

	struct callback *animate_callback;
	struct event_timeout *animate_timer;
	struct transformation *trans;
	int angle;
	int speed;
	int sequence;
	GHashTable *log_to_cb;
};

struct object_func vehicle_func;

static void vehicle_set_default_name(struct vehicle *this);



/**
 * @brief Creates a new vehicle
 *
 * @param parent
 * @param attrs Points to a null-terminated array of pointers to the attributes
 * for the new vehicle type.
 *
 * @return The newly created vehicle object
 */
struct vehicle *
vehicle_new(struct attr *parent, struct attr **attrs)
{
	struct vehicle *this_;
	struct attr *source;
	struct vehicle_priv *(*vehicletype_new) (struct vehicle_methods *
						 meth,
						 struct callback_list *
						 cbl,
						 struct attr ** attrs);
	char *type, *colon;
	struct pcoord center;

	dbg(lvl_debug, "enter\n");
	source = attr_search(attrs, NULL, attr_source);
	if (!source) {
		dbg(lvl_error, "incomplete vehicle definition: missing attribute 'source'\n");
		return NULL;
	}

	type = g_strdup(source->u.str);
	colon = strchr(type, ':');
	if (colon)
		*colon = '\0';
	dbg(lvl_debug, "source='%s' type='%s'\n", source->u.str, type);

	vehicletype_new = plugin_get_category_vehicle(type);
	if (!vehicletype_new) {
		dbg(lvl_error, "invalid source '%s': unknown type '%s'\n", source->u.str, type);
		g_free(type);
		return NULL;
	}
	g_free(type);
	this_ = g_new0(struct vehicle, 1);
	this_->func=&vehicle_func;
	navit_object_ref((struct navit_object *)this_);
	this_->cbl = callback_list_new();
	this_->priv = vehicletype_new(&this_->meth, this_->cbl, attrs);
	if (!this_->priv) {
		dbg(lvl_error, "vehicletype_new failed\n");
		callback_list_destroy(this_->cbl);
		g_free(this_);
		return NULL;
	}
	this_->attrs=attr_list_dup(attrs);

	center.pro=projection_screen;
	center.x=0;
	center.y=0;
	this_->trans=transform_new(&center, 16, 0);
	vehicle_set_default_name(this_);

	dbg(lvl_debug, "leave\n");
	this_->log_to_cb=g_hash_table_new(NULL,NULL);
	return this_;
}

/**
 * @brief Destroys a vehicle
 * 
 * @param this_ The vehicle to destroy
 */
void
vehicle_destroy(struct vehicle *this_)
{
	dbg(lvl_debug,"enter\n");
	if (this_->animate_callback) {
		callback_destroy(this_->animate_callback);
		event_remove_timeout(this_->animate_timer);
	}
	transform_destroy(this_->trans);
	this_->meth.destroy(this_->priv);
	callback_list_destroy(this_->cbl);
	attr_list_free(this_->attrs);
	g_free(this_);
}

/**
 * Creates an attribute iterator to be used with vehicles
 */
struct attr_iter *
vehicle_attr_iter_new(void)
{
	return (struct attr_iter *)g_new0(void *,1);
}

/**
 * Destroys a vehicle attribute iterator
 *
 * @param iter a vehicle attr_iter
 */
void
vehicle_attr_iter_destroy(struct attr_iter *iter)
{
	g_free(iter);
}



/**
 * Generic get function
 *
 * @param this_ Pointer to a vehicle structure
 * @param type The attribute type to look for
 * @param attr Pointer to a {@code struct attr} to store the attribute
 * @param iter A vehicle attr_iter. This is only used for generic attributes; for attributes specific to the vehicle object it is ignored.
 * @return True for success, false for failure
 */
int
vehicle_get_attr(struct vehicle *this_, enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
	int ret;
	if (type == attr_log_gpx_desc) {
		attr->u.str = this_->gpx_desc;
		return 1;
	}
	if (this_->meth.position_attr_get) {
		ret=this_->meth.position_attr_get(this_->priv, type, attr);
		if (ret)
			return ret;
	}
	return attr_generic_get_attr(this_->attrs, NULL, type, attr, iter);
}

/**
 * Generic set function
 *
 * @param this_ A vehicle
 * @param attr The attribute to set
 * @return False on success, true on failure
 */
int
vehicle_set_attr(struct vehicle *this_, struct attr *attr)
{
	int ret=1;
	if (attr->type == attr_log_gpx_desc) {
		g_free(this_->gpx_desc);
		this_->gpx_desc = g_strdup(attr->u.str);
	} else if (this_->meth.set_attr)
		ret=this_->meth.set_attr(this_->priv, attr);
	/* attr_profilename probably is never used by vehicle itself but it's used to control the
	  routing engine. So any vehicle should allow to set and read it. */
	if(attr->type == attr_profilename)
		ret=1;
	if (ret == 1 && attr->type != attr_navit && attr->type != attr_pdl_gps_update)
		this_->attrs=attr_generic_set_attr(this_->attrs, attr);
	return ret != 0;
}

/**
 * Generic add function
 *
 * @param this_ A vehicle
 * @param attr The attribute to add
 *
 * @return true if the attribute was added, false if not.
 */
int
vehicle_add_attr(struct vehicle *this_, struct attr *attr)
{
	int ret=1;
	switch (attr->type) {
	case attr_callback:
		callback_list_add(this_->cbl, attr->u.callback);
		break;
	case attr_log:
		break;
	case attr_cursor:
		break;
	default:
		break;
	}
	if (ret)
		this_->attrs=attr_generic_add_attr(this_->attrs, attr);
	return ret;
}

/**
 * @brief Generic remove function.
 *
 * Used to remove a callback from the vehicle.
 * @param this_ A vehicle
 * @param attr
 */
int
vehicle_remove_attr(struct vehicle *this_, struct attr *attr)
{
	struct callback *cb;
	switch (attr->type) {
	case attr_callback:
		callback_list_remove(this_->cbl, attr->u.callback);
		break;
	case attr_log:
		break;
	default:
		this_->attrs=attr_generic_remove_attr(this_->attrs, attr);
		return 0;
	}
	return 1;
}



static void vehicle_set_default_name(struct vehicle *this_)
{
	struct attr default_name;
	if (!attr_search(this_->attrs, NULL, attr_name)) {
		default_name.type=attr_name;
		// Safe cast: attr_generic_set_attr does not modify its parameter.
		default_name.u.str=(char*)_("Unnamed vehicle");
		this_->attrs=attr_generic_set_attr(this_->attrs, &default_name);
		dbg(lvl_error, "Incomplete vehicle definition: missing attribute 'name'. Default name set.\n");
	}
}


static void
vehicle_draw_do(struct vehicle *this_)
{
}

struct object_func vehicle_func = {
	attr_vehicle,
	(object_func_new)vehicle_new,
	(object_func_get_attr)vehicle_get_attr,
	(object_func_iter_new)vehicle_attr_iter_new,
	(object_func_iter_destroy)vehicle_attr_iter_destroy,
	(object_func_set_attr)vehicle_set_attr,
	(object_func_add_attr)vehicle_add_attr,
	(object_func_remove_attr)vehicle_remove_attr,
	(object_func_init)NULL,
	(object_func_destroy)vehicle_destroy,
	(object_func_dup)NULL,
	(object_func_ref)navit_object_ref,
	(object_func_unref)navit_object_unref,
};
