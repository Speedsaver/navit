/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2009 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef NAVIT_VEHICLE_H
#define NAVIT_VEHICLE_H


struct point;
struct vehicle_priv;

struct vehicle_methods {
	void (*destroy)(struct vehicle_priv *priv);
	int (*position_attr_get)(struct vehicle_priv *priv, enum attr_type type, struct attr *attr);
	int (*set_attr)(struct vehicle_priv *priv, struct attr *attr);
};

/* prototypes */
enum attr_type;
struct attr;
struct attr_iter;
struct point;
struct vehicle;
struct vehicle *vehicle_new(struct attr *parent, struct attr **attrs);
void vehicle_destroy(struct vehicle *this_);
struct attr_iter *vehicle_attr_iter_new(void);
void vehicle_attr_iter_destroy(struct attr_iter *iter);
int vehicle_get_attr(struct vehicle *this_, enum attr_type type, struct attr *attr, struct attr_iter *iter);
int vehicle_set_attr(struct vehicle *this_, struct attr *attr);
int vehicle_add_attr(struct vehicle *this_, struct attr *attr);
int vehicle_remove_attr(struct vehicle *this_, struct attr *attr);
void vehicle_log_gpx_add_tag(char *tag, char **logstr);
struct vehicle * vehicle_ref(struct vehicle *this_);
void vehicle_unref(struct vehicle *this_);
/* end of prototypes */


#endif

