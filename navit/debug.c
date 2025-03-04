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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <sys/time.h>
#include "file.h"
#include "item.h"
#include "debug.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int debug_socket=-1;
static struct sockaddr_in debug_sin;


#define DEFAULT_DEBUG_LEVEL lvl_error
dbg_level max_debug_level=DEFAULT_DEBUG_LEVEL;
#define GLOBAL_DEBUG_LEVEL_UNSET lvl_unset
dbg_level global_debug_level=GLOBAL_DEBUG_LEVEL_UNSET;
int segv_level=0;
int timestamp_prefix=0;

static int dummy;
static GHashTable *debug_hash;
static gchar *gdb_program;

static FILE *debug_fp;

#include <unistd.h>
static void sigsegv(int sig)
{
	char buffer[256];
	if (segv_level > 1)
		sprintf(buffer, "gdb -ex bt %s %d", gdb_program, getpid());
	else
		sprintf(buffer, "gdb -ex bt -ex detach -ex quit %s %d", gdb_program, getpid());
	system(buffer);
	exit(1);
}

void
debug_init(const char *program_name)
{
	gdb_program=g_strdup(program_name);
	signal(SIGSEGV, sigsegv);
	debug_hash=g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	debug_fp = stderr;
}


static void
debug_update_level(gpointer key, gpointer value, gpointer user_data)
{
	if (max_debug_level < GPOINTER_TO_INT(value))
		max_debug_level = GPOINTER_TO_INT(value);
}

void
debug_set_global_level(dbg_level level, int override_old_value ) {
	if (global_debug_level == GLOBAL_DEBUG_LEVEL_UNSET || override_old_value) {
		global_debug_level=level;
		if (max_debug_level < global_debug_level){
			max_debug_level = global_debug_level;
		}
	}
}

void
debug_level_set(const char *name, dbg_level level)
{
	if (!strcmp(name, "segv")) {
		segv_level=level;
		if (segv_level)
			signal(SIGSEGV, sigsegv);
		else
			signal(SIGSEGV, NULL);
	} else if (!strcmp(name, "timestamps")) {
		timestamp_prefix=level;
	} else if (!strcmp(name, DEBUG_MODULE_GLOBAL)) {
		debug_set_global_level(level, 0);
	} else {
		g_hash_table_insert(debug_hash, g_strdup(name), GINT_TO_POINTER(level));
		g_hash_table_foreach(debug_hash, debug_update_level, NULL);
	}
}

static dbg_level
parse_dbg_level(struct attr *dbg_level_attr, struct attr *level_attr)
{
	if (dbg_level_attr) {
		if(!strcmp(dbg_level_attr->u.str,"error")){
			return lvl_error;
		}
		if(!strcmp(dbg_level_attr->u.str,"warning")){
			return lvl_warning;
		}
		if(!strcmp(dbg_level_attr->u.str,"info")){
			return lvl_info;
		}
		if(!strcmp(dbg_level_attr->u.str,"debug")){
			return lvl_debug;
		}
		dbg(lvl_error, "Invalid debug level in config: '%s'\n", dbg_level_attr->u.str);
	} else if (level_attr) {
		if (level_attr->u.num>= lvl_error &&
		    level_attr->u.num<= lvl_debug)
			return level_attr->u.num;
		dbg(lvl_error, "Invalid debug level in config: %ld\n", level_attr->u.num);
	}
	return lvl_unset;
}

struct debug *
debug_new(struct attr *parent, struct attr **attrs)
{
	struct attr *name,*dbg_level_attr,*level_attr;
	dbg_level level;
	name=attr_search(attrs, NULL, attr_name);
	dbg_level_attr=attr_search(attrs, NULL, attr_dbg_level);
	level_attr=attr_search(attrs, NULL, attr_level);
	level = parse_dbg_level(dbg_level_attr,level_attr);
	if (!name && level==lvl_unset) {
		struct attr *socket_attr=attr_search(attrs, NULL, attr_socket);
		char *p,*s;
		if (!socket_attr)
			return NULL;
		s=g_strdup(socket_attr->u.str);
        	p=strchr(s,':');
		if (!p) {
			g_free(s);
			return NULL;
		}
		*p++='\0';
		debug_sin.sin_family=AF_INET;
		if (!inet_aton(s, &debug_sin.sin_addr)) {
			g_free(s);
			return NULL;
		}
        	debug_sin.sin_port=ntohs(atoi(p));
        	if (debug_socket == -1) 
                	debug_socket=socket(PF_INET, SOCK_DGRAM, 0);
		g_free(s);
		return (struct debug *)&dummy;	
	}
	if (!name || level==lvl_unset)
		return NULL;
	debug_level_set(name->u.str, level);
	return (struct debug *)&dummy;
}


dbg_level
debug_level_get(const char *message_category)
{
	if (!debug_hash)
		return DEFAULT_DEBUG_LEVEL;
	gpointer level = g_hash_table_lookup(debug_hash, message_category);
	if (!level) {
		return DEFAULT_DEBUG_LEVEL;
	}
	return GPOINTER_TO_INT(level);
}

static void debug_timestamp(char *buffer)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1)
		return;
	/* Timestamps are UTC */
	sprintf(buffer,
		"%02d:%02d:%02d.%03d|",
		(int)(tv.tv_sec/3600)%24,
		(int)(tv.tv_sec/60)%60,
		(int)tv.tv_sec % 60,
		(int)tv.tv_usec/1000);
}

static char* dbg_level_to_string(dbg_level level)
{
	switch(level) {
		case lvl_unset:
			return "-unset-";
		case lvl_error:
			return "error";
		case lvl_warning:
			return "warning";
		case lvl_info:
			return "info";
		case lvl_debug:
			return "debug";
	}
	return "-invalid level-";
}

void
debug_vprintf(dbg_level level, const char *module, const int mlen, const char *function, const int flen, int prefix, const char *fmt, va_list ap)
{
	char message_origin[mlen+flen+3];

	sprintf(message_origin, "%s:%s", module, function);
	if (global_debug_level >= level || debug_level_get(module) >= level || debug_level_get(message_origin) >= level) {
		char debug_message[4096];
		debug_message[0]='\0';
		if (prefix) {
			if (timestamp_prefix)
				debug_timestamp(debug_message);
			strcpy(debug_message+strlen(debug_message),dbg_level_to_string(level));
			strcpy(debug_message+strlen(debug_message),":");
			strcpy(debug_message+strlen(debug_message),message_origin);
			strcpy(debug_message+strlen(debug_message),":");
		}
		vsnprintf(debug_message+strlen(debug_message),4095-strlen(debug_message),fmt,ap);
		if (debug_socket != -1) {
			sendto(debug_socket, debug_message, strlen(debug_message), 0, (struct sockaddr *)&debug_sin, sizeof(debug_sin));
			return;
		}
		FILE *fp=debug_fp;
		if (! fp)
			fp = stderr;
		fprintf(fp,"%s",debug_message);
		fflush(fp);
	}
}

void
debug_printf(dbg_level level, const char *module, const int mlen,const char *function, const int flen, int prefix, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	debug_vprintf(level, module, mlen, function, flen, prefix, fmt, ap);
	va_end(ap);
}

void
debug_assert_fail(const char *module, const int mlen,const char *function, const int flen, const char *file, int line, const char *expr)
{
	debug_printf(lvl_error,module,mlen,function,flen,1,"%s:%d assertion failed:%s\n", file, line, expr);
	abort();
}

void
debug_destroy(void)
{
	if (!debug_fp)
		return;
	if (debug_fp == stderr || debug_fp == stdout)
		return;
	fclose(debug_fp);
	debug_fp = NULL;
}

void debug_set_logfile(const char *path)
{
	FILE *fp;
	fp = fopen(path, "a");
	if (fp) {
		debug_destroy();
		debug_fp = fp;
		fprintf(debug_fp, "Navit log started\n");
		fflush(debug_fp);
	}
}

struct malloc_head {
	int magic;
	int size;
	char *where;
	void *return_address[8];
	struct malloc_head *prev;
	struct malloc_head *next;
} *malloc_heads;

struct malloc_tail {
	int magic;
};

int mallocs,debug_malloc_size,debug_malloc_size_m;

void
debug_dump_mallocs(void)
{
	struct malloc_head *head=malloc_heads;
	int i;
	dbg(lvl_debug,"mallocs %d\n",mallocs);
	while (head) {
		fprintf(stderr,"unfreed malloc from %s of size %d\n",head->where,head->size);
		for (i = 0 ; i < 8 ; i++)
			fprintf(stderr,"\tlist *%p\n",head->return_address[i]);
		head=head->next;
	}
}

void *
debug_malloc(const char *where, int line, const char *func, int size)
{
	struct malloc_head *head;
	struct malloc_tail *tail;
	if (!size)
		return NULL;
	mallocs++;
	debug_malloc_size+=size;
	if (debug_malloc_size/(1024*1024) != debug_malloc_size_m) {
		debug_malloc_size_m=debug_malloc_size/(1024*1024);
		dbg(lvl_debug,"malloced %d kb\n",debug_malloc_size/1024);
	}
	head=malloc(size+sizeof(*head)+sizeof(*tail));
	head->magic=0xdeadbeef;
	head->size=size;
	head->prev=NULL;
	head->next=malloc_heads;
	malloc_heads=head;
	if (head->next) 
		head->next->prev=head;
	head->where=g_strdup_printf("%s:%d %s",where,line,func);
	head->return_address[0]=__builtin_return_address(0);
	head->return_address[1]=__builtin_return_address(1);
	head->return_address[2]=__builtin_return_address(2);
	head->return_address[3]=__builtin_return_address(3);
	head->return_address[4]=__builtin_return_address(4);
	head->return_address[5]=__builtin_return_address(5);
	head->return_address[6]=__builtin_return_address(6);
	head->return_address[7]=__builtin_return_address(7);
	head++;
	tail=(struct malloc_tail *)((unsigned char *)head+size);
	tail->magic=0xdeadbef0;
	return head;
}


void *
debug_malloc0(const char *where, int line, const char *func, int size)
{
	void *ret=debug_malloc(where, line, func, size);
	if (ret)
		memset(ret, 0, size);
	return ret;
}

void *
debug_realloc(const char *where, int line, const char *func, void *ptr, int size)
{
	void *ret=debug_malloc(where, line, func, size);
	if (ret && ptr)
		memcpy(ret, ptr, size);
	debug_free(where, line, func, ptr);
	return ret;
}

char *
debug_strdup(const char *where, int line, const char *func, const char *ptr)
{ 
	int size;
	char *ret;

	if (!ptr)
		return NULL;
	size=strlen(ptr)+1;
	ret=debug_malloc(where, line, func, size);
	memcpy(ret, ptr, size);
	return ret;
}

char *
debug_guard(const char *where, int line, const char *func, char *str)
{
	char *ret=debug_strdup(where, line, func, str);
	g_free(str);
	return ret;
}

void
debug_free(const char *where, int line, const char *func, void *ptr)
{
	struct malloc_head *head;
        struct malloc_tail *tail;
	if (!ptr)
		return;
	mallocs--;
	head=(struct malloc_head *)((unsigned char *)ptr-sizeof(*head));
	tail=(struct malloc_tail *)((unsigned char *)ptr+head->size);
	debug_malloc_size-=head->size;
	if (head->magic != 0xdeadbeef || tail->magic != 0xdeadbef0) {
		fprintf(stderr,"Invalid free from %s:%d %s\n",where,line,func);
	}
	head->magic=0;
	tail->magic=0;
	if (head->prev) 
		head->prev->next=head->next;
	else
		malloc_heads=head->next;
	if (head->next)
		head->next->prev=head->prev;
	free(head->where);
	free(head);
}

void
debug_free_func(void *ptr)
{
	debug_free("unknown",0,"unknown",ptr);
}

void debug_finished(void) {
	debug_dump_mallocs();
	g_free(gdb_program);
	g_hash_table_destroy(debug_hash);
	debug_destroy();
}

