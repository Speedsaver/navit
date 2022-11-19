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

#include <string.h>
#include <glib.h>
#include "config.h"
#include "plugin.h"
#include "file.h"
#define PLUGIN_C
#include "plugin.h"
#include "item.h"
#include "debug.h"

struct plugin {
	int active;
	int lazy;
	int ondemand;
	char *name;
	void (*init)(void);
};

struct plugins {
	GHashTable *hash;
	GList *list;
} *pls;

static struct plugin *
plugin_new_from_path(char *plugin)
{
	return NULL;
}

int
plugin_load(struct plugin *pl)
{
	return 0;
}

char *
plugin_get_name(struct plugin *pl)
{
	return pl->name;
}

int
plugin_get_active(struct plugin *pl)
{
	return pl->active;
}

void
plugin_set_active(struct plugin *pl, int active)
{
	pl->active=active;
}

void
plugin_set_lazy(struct plugin *pl, int lazy)
{
	pl->lazy=lazy;
}

static void
plugin_set_ondemand(struct plugin *pl, int ondemand)
{
	pl->ondemand=ondemand;
}

void
plugin_call_init(struct plugin *pl)
{
	pl->init();
}

void
plugin_unload(struct plugin *pl)
{
}

void
plugin_destroy(struct plugin *pl)
{
	g_free(pl);
}

struct plugins *
plugins_new(void)
{
	struct plugins *ret=g_new0(struct plugins, 1);
	ret->hash=g_hash_table_new(g_str_hash, g_str_equal);
	pls=ret;
	return ret;
}

struct plugin *
plugin_new(struct attr *parent, struct attr **attrs) {
    return 0;
}

void
plugins_init(struct plugins *pls)
{
}

void
plugins_destroy(struct plugins *pls)
{
	GList *l;
	struct plugin *pl;

	l=pls->list;
	while (l) {
		pl=l->data;
		plugin_unload(pl);
		plugin_destroy(pl);
	}
	g_list_free(pls->list);
	g_hash_table_destroy(pls->hash);
	g_free(pls);
}

static void *
find_by_name(enum plugin_category category, const char *name)
{
	GList *name_list=plugin_categories[category];
	while (name_list) {
		struct name_val *nv=name_list->data;
		if (!g_ascii_strcasecmp(nv->name, name))
			return nv->val;
		name_list=g_list_next(name_list);
	}
	return NULL;
}

void *
plugin_get_category(enum plugin_category category, const char *category_name, const char *name)
{
	GList *plugin_list;
	struct plugin *pl;
	char *mod_name, *filename=NULL, *corename=NULL;
	void *result=NULL;

	dbg(lvl_debug, "category=\"%s\", name=\"%s\"\n", category_name, name);

	if ((result=find_by_name(category, name))) {
		return result;
	}
	if (!pls)
		return NULL;
	plugin_list=pls->list;
	filename=g_strjoin("", "lib", category_name, "_", name, NULL);
	corename=g_strjoin("", "lib", category_name, "_", "core", NULL);
	while (plugin_list) {
		pl=plugin_list->data;
		if ((mod_name=g_strrstr(pl->name, "/")))
			mod_name++;
		else
			mod_name=pl->name;
		if (!g_ascii_strncasecmp(mod_name, filename, strlen(filename)) || !g_ascii_strncasecmp(mod_name, corename, strlen(corename))) {
			dbg(lvl_debug, "Loading module \"%s\"\n",pl->name) ;
			if (plugin_get_active(pl)) 
				if (!plugin_load(pl)) 
					plugin_set_active(pl, 0);
			if (plugin_get_active(pl)) 
				plugin_call_init(pl);
			if ((result=find_by_name(category, name))) {
				g_free(filename);
				g_free(corename);
				return result;
			}
		}
		plugin_list=g_list_next(plugin_list);
	}
	g_free(filename);
	g_free(corename);
	return NULL;
}
