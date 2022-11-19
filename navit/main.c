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

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <glib.h>
#include <sys/types.h>

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/wait.h>
#include <signal.h>

#include "file.h"
#include "debug.h"
#include "main.h"
#include "navit.h"
#include "gui.h"
#include "item.h"
#include "xmlconfig.h"
#include "coord.h"
#include "route.h"
#include "navigation.h"
#include "event.h"
#include "callback.h"
#include "navit_nls.h"
#include "util.h"

struct map_data *map_data_default;

struct callback_list *cbl;

/*
 * environment_vars[][0:name,1-3:mode]
 * ':'  replaced with NAVIT_PREFIX
 * '::' replaced with NAVIT_PREFIX and LIBDIR
 * '~'  replaced with HOME
*/
static char *environment_vars[][5]={
	{"NAVIT_LIBDIR",      ":",          ":/"LIB_DIR,     ":\\lib",      ":/lib"},
	{"NAVIT_SHAREDIR",    ":",          ":/"SHARE_DIR,   ":",           ":/share"},
	{"NAVIT_LOCALEDIR",   ":/../locale",":/"LOCALE_DIR,  ":\\locale",   ":/locale"},
	{"NAVIT_USER_DATADIR",":",          "~/.navit",      ":\\data",     ":/home"},
	{"NAVIT_LOGFILE",     NULL,         NULL,            ":\\navit.log",NULL},
	{"NAVIT_LIBPREFIX",   "*/.libs/",   NULL,            NULL,          NULL},
	{NULL,                NULL,         NULL,            NULL,          NULL},
};

static void
main_setup_environment(int mode)
{
	int i=0;
	char *var,*val,*homedir;
	while ((var=environment_vars[i][0])) {
		val=environment_vars[i][mode+1];
		if (val) {
			switch (val[0]) {
			case ':':
				if (val[1] == ':')
					val=g_strdup_printf("%s/%s%s", getenv("NAVIT_PREFIX"), LIBDIR+sizeof(PREFIX), val+2);
				else
					val=g_strdup_printf("%s%s", getenv("NAVIT_PREFIX"), val+1);
				break;
			case '~':
				homedir=getenv("HOME");
				if (!homedir)
					homedir="./";
				val=g_strdup_printf("%s%s", homedir, val+1);
				break;
			default:
				val=g_strdup(val);
				break;
			}
			setenv(var, val, 0);
			g_free(val);
		}
		i++;
	}
}

void
main_init(const char *program)
{
	char *s;

	spawn_process_init();

	cbl=callback_list_new();
	setenv("LC_NUMERIC","C",1);
	setlocale(LC_ALL,"");
	setlocale(LC_NUMERIC,"C");
	if (file_exists("navit.c") || file_exists("navit.o") || file_exists("navit.lo") || file_exists("version.h")) {
		char buffer[PATH_MAX];
		printf("%s",_("Running from source directory\n"));
		getcwd(buffer, PATH_MAX);		/* libc of navit returns "dummy" */
		setenv("NAVIT_PREFIX", buffer, 0);
		main_setup_environment(0);
	} else {
		if (!getenv("NAVIT_PREFIX")) {
			int l;
			int progpath_len;
			char *progpath="/bin/navit";
			l=strlen(program);
			progpath_len=strlen(progpath);
			if (l > progpath_len && !strcmp(program+l-progpath_len,progpath)) {
				s=g_strdup(program);
				s[l-progpath_len]='\0';
				if (strcmp(s, PREFIX))
					printf(_("setting '%s' to '%s'\n"), "NAVIT_PREFIX", s);
				setenv("NAVIT_PREFIX", s, 0);
				g_free(s);
			} else
				setenv("NAVIT_PREFIX", PREFIX, 0);
		}
		main_setup_environment(1);
	}

	s = getenv("NAVIT_WID");
	if (s) {
		setenv("SDL_WINDOWID", s, 0);
	}
}

