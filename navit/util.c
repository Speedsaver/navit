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

#include <stdlib.h>
#include <glib.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include "util.h"
#include "debug.h"
#include "config.h"

void
strtoupper(char *dest, const char *src)
{
	while (*src)
		*dest++=toupper(*src++);
	*dest='\0';
}

void
strtolower(char *dest, const char *src)
{
	while (*src)
		*dest++=tolower(*src++);
	*dest='\0';
}

int
navit_utf8_strcasecmp(const char *s1, const char *s2)
{
        char *s1_folded,*s2_folded;
	int cmpres;
	s1_folded=g_utf8_casefold(s1,-1);
	s2_folded=g_utf8_casefold(s2,-1);
	cmpres=strcmp(s1_folded,s2_folded);
	dbg(lvl_debug,"Compared %s with %s, got %d\n",s1_folded,s2_folded,cmpres);
	g_free(s1_folded);
	g_free(s2_folded);
	return cmpres;
}

static void
hash_callback(gpointer key, gpointer value, gpointer user_data)
{
	GList **l=user_data;
	*l=g_list_prepend(*l, value);
}

GList *
g_hash_to_list(GHashTable *h)
{
	GList *ret=NULL;
	g_hash_table_foreach(h, hash_callback, &ret);

	return ret;
}

static void
hash_callback_key(gpointer key, gpointer value, gpointer user_data)
{
	GList **l=user_data;
	*l=g_list_prepend(*l, key);
}

GList *
g_hash_to_list_keys(GHashTable *h)
{
	GList *ret=NULL;
	g_hash_table_foreach(h, hash_callback_key, &ret);

	return ret;
}

gchar *
g_strconcat_printf(gchar *buffer, gchar *fmt, ...)
{
	gchar *str,*ret;
	va_list ap;

	va_start(ap, fmt);
        str=g_strdup_vprintf(fmt, ap);
        va_end(ap);
	if (! buffer)
		return str;
	ret=g_strconcat(buffer, str, NULL);
	g_free(buffer);
	g_free(str);
	return ret;
}

#ifndef HAVE_GLIB
int g_utf8_strlen_force_link(gchar *buffer, int max);
int
g_utf8_strlen_force_link(gchar *buffer, int max)
{
	return g_utf8_strlen(buffer, max);
}
#endif

#if defined (__APPLE__)
char *stristr(const char *String, const char *Pattern)
{
      char *pptr, *sptr, *start;

      for (start = (char *)String; *start != (int)NULL; start++)
      {
            /* find start of pattern in string */
            for ( ; ((*start!=(int)NULL) && (toupper(*start) != toupper(*Pattern))); start++)
                  ;
            if ((int)NULL == *start)
                  return NULL;

            pptr = (char *)Pattern;
            sptr = (char *)start;

            while (toupper(*sptr) == toupper(*pptr))
            {
                  sptr++;
                  pptr++;

                  /* if end of pattern then pattern was found */

                  if ((int)NULL == *pptr)
                        return (start);
            }
      }
      return NULL;
}

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif
#ifndef SSIZE_MAX
# define SSIZE_MAX ((ssize_t) (SIZE_MAX / 2))
#endif
#if !HAVE_FLOCKFILE
# undef flockfile
# define flockfile(x) ((void) 0)
#endif
#if !HAVE_FUNLOCKFILE
# undef funlockfile
# define funlockfile(x) ((void) 0)
#endif

/* Some systems, like OSF/1 4.0 and Woe32, don't have EOVERFLOW.  */
#ifndef EOVERFLOW
# define EOVERFLOW E2BIG
#endif


#ifndef HAVE_GETDELIM
/**
 * @brief Reads the part of a file up to a delimiter to a string.
 * <p> 
 * Read up to (and including) a DELIMITER from FP into *LINEPTR (and NUL-terminate it).
 *
 * @param lineptr Pointer to a pointer returned from malloc (or NULL), pointing to a buffer. It is
 * realloc'ed as necessary and will receive the data read.
 * @param n Size of the buffer.  
 *
 * @return Number of characters read (not including the null terminator), or -1 on error or EOF.
*/
ssize_t
getdelim (char **lineptr, size_t *n, int delimiter, FILE *fp)
{
  int result;
  size_t cur_len = 0;

  if (lineptr == NULL || n == NULL || fp == NULL)
    {
      return -1;
    }

  flockfile (fp);

  if (*lineptr == NULL || *n == 0)
    {
      *n = 120;
      *lineptr = (char *) realloc (*lineptr, *n);
      if (*lineptr == NULL)
	{
	  result = -1;
	  goto unlock_return;
	}
    }

  for (;;)
    {
      int i;

      i = getc (fp);
      if (i == EOF)
	{
	  result = -1;
	  break;
	}

      /* Make enough space for len+1 (for final NUL) bytes.  */
      if (cur_len + 1 >= *n)
	{
	  size_t needed_max=SIZE_MAX;
	  size_t needed = 2 * *n + 1;   /* Be generous. */
	  char *new_lineptr;
	  if (needed_max < needed)
	    needed = needed_max;
	  if (cur_len + 1 >= needed)
	    {
	      result = -1;
	      goto unlock_return;
	    }

	  new_lineptr = (char *) realloc (*lineptr, needed);
	  if (new_lineptr == NULL)
	    {
	      result = -1;
	      goto unlock_return;
	    }

	  *lineptr = new_lineptr;
	  *n = needed;
	}

      (*lineptr)[cur_len] = i;
      cur_len++;

      if (i == delimiter)
	break;
    }
  (*lineptr)[cur_len] = '\0';
  result = cur_len ? cur_len : result;

 unlock_return:
  funlockfile (fp); /* doesn't set errno */

  return result;
}
#endif

#ifndef HAVE_GETLINE
ssize_t
getline (char **lineptr, size_t *n, FILE *stream)
{
  return getdelim (lineptr, n, '\n', stream);
}
#endif

char * newSysString(const char *toconvert)
{
	return g_strdup(toconvert);
}
#endif

/**
 * @brief Converts an ISO 8601-style time string into epoch time.
 *
 * @param iso8601 Time in ISO 8601 format.
 *
 * @return The number of seconds elapsed since January 1, 1970, 00:00:00 UTC.
 */
unsigned int
iso8601_to_secs(char *iso8601)
{
	int a,b,d,val[6],i=0;
	char *start=iso8601,*pos=iso8601;
	while (*pos && i < 6) {
		if (*pos < '0' || *pos > '9') {
			val[i++]=atoi(start);
			pos++;
			start=pos;
		} 
		if(*pos)
			pos++;
	}
	
	a=val[0]/100;
	b=2-a+a/4;

	if (val[1] < 2) {
		val[0]--;
		val[1]+=12;
	}

	d=1461*(val[0]+4716)/4+306001*(val[1]+1)/10000+val[2]+b-2442112;

	return ((d*24+val[3])*60+val[4])*60+val[5];
}

/**
 * @brief Outputs local system time in ISO 8601 format.
 *
 * @return Time in ISO 8601 format
 */
char *
current_to_iso8601(void)
{
	char *timep=NULL;
	char buffer[32];
	time_t tnow;
	struct tm *tm;
	tnow = time(0);
	tm = gmtime(&tnow);
	if (tm) {
		strftime(buffer, sizeof(buffer), "%Y-%m-%dT%TZ", tm);
		timep=g_strdup(buffer);	
	}
	return timep;
}


struct spawn_process_info {
	pid_t pid; // = -1 if non-blocking spawn isn't supported
	int status; // exit status if non-blocking spawn isn't supported
};


/**
 * Escape and quote string for shell
 *
 * @param in arg string to escape
 * @returns escaped string
 */
char *
shell_escape(char *arg) 
{
	char *r;
	int arglen=strlen(arg);
	int i,j,rlen;
	{
		// Will use hard quoting for the whole string
		// and replace each singular quote found with a '\'' sequence.
		rlen=arglen+3;
		r=g_new(char,rlen);
		r[0]='\'';
		for(i=0,j=1;i<arglen;i++) {
			if(arg[i]=='\'') {
				rlen+=3;
				r=g_realloc(r,rlen);
				g_strlcpy(r+j,"'\\''",rlen-j);
			} else {
				r[j++]=arg[i];
			}
		}
		r[j++]='\'';
		r[j++]=0;
	}
	return r;
}

#ifndef _POSIX_C_SOURCE
static char*
spawn_process_compose_cmdline(char **argv)
{
	int i,j;
	char *cmdline=shell_escape(argv[0]);
	for(i=1,j=strlen(cmdline);argv[i];i++) {
		char *arg=shell_escape(argv[i]);
		int arglen=strlen(arg);
		cmdline[j]=' ';
		cmdline=g_realloc(cmdline,j+1+arglen+1);
		memcpy(cmdline+j+1,arg,arglen+1);
		g_free(arg);
		j=j+1+arglen;
	}
	return cmdline;
}
#endif

#ifdef _POSIX_C_SOURCE

#if 0 /* def _POSIX_THREADS */
#define spawn_process_sigmask(how,set,old) pthread_sigmask(how,set,old)
#else
#define spawn_process_sigmask(how,set,old) sigprocmask(how,set,old)
#endif

GList *spawn_process_children=NULL;

#endif


/**
 * Call external program
 *
 * @param in argv NULL terminated list of parameters,
 *    zeroeth argument is program name
 * @returns 0 - success, >0 - return code, -1 - error
 */
struct spawn_process_info*
spawn_process(char **argv)
{
	struct spawn_process_info*r=g_new(struct spawn_process_info,1);
#ifdef _POSIX_C_SOURCE
	{
		pid_t pid;
		
		sigset_t set, old;
		dbg(lvl_debug,"spawning process for '%s'\n", argv[0]);
		sigemptyset(&set);
		sigaddset(&set,SIGCHLD);
		spawn_process_sigmask(SIG_BLOCK,&set,&old);
		pid=fork();
		if(pid==0) {
			execvp(argv[0], argv);
			/*Shouldn't reach here*/
			exit(1);
		} else if(pid>0) {
			r->status=-1;
			r->pid=pid;
			spawn_process_children=g_list_prepend(spawn_process_children,r);
		} else {
			dbg(lvl_error,"fork() returned error.\n");
			g_free(r);
			r=NULL;
		}
		spawn_process_sigmask(SIG_SETMASK,&old,NULL);
		return r;
	}
#else
	{
		char *cmdline=spawn_process_compose_cmdline(argv);
		int status;
		dbg(lvl_error,"Unblocked spawn_process isn't availiable on this platform.\n");
		status=system(cmdline);
		g_free(cmdline);
		r->status=status;
		r->pid=0;
		return r;
	}
#endif
}

/**
 * Check external program status
 *
 * @param in *pi pointer to spawn_process_info structure
 * @param in block =0 do not block =1 block until child terminated
 * @returns -1 - still running, >=0 program exited, 
 *     =255 trminated abnormally or wasn't run at all.
 * 
 */
int spawn_process_check_status(struct spawn_process_info *pi, int block)
{
	if(pi==NULL) {
		dbg(lvl_error,"Trying to get process status of NULL, assuming process is terminated.\n");
		return 255;
	}
#ifdef _POSIX_C_SOURCE
	if(pi->status!=-1) {
		return pi->status;
	}
	while(1) {
		int status;
		pid_t w=waitpid(pi->pid,&status,block?0:WNOHANG);
		if(w>0) {
			if(WIFEXITED(status))
				pi->status=WEXITSTATUS(status);
				return pi->status;
			if(WIFSTOPPED(status)) {
				dbg(lvl_debug,"child is stopped by %i signal\n",WSTOPSIG(status));
			} else if (WIFSIGNALED(status)) {
				dbg(lvl_debug,"child terminated by signal %i\n",WEXITSTATUS(status));
				pi->status=255;
				return 255;
			}
			if(!block)
				return -1;
		} else if(w==0) {
			if(!block)
				return -1;
		} else {
			if(pi->status!=-1) // Signal handler has changed pi->status while in this function
				return pi->status;
			dbg(lvl_error,"waitpid() indicated error, reporting process termination.\n");
			return 255;
		}
	}
#else
	dbg(lvl_error, "Non-blocking spawn_process isn't availiable for this platform, repoting process exit status.\n");
	return pi->status;
#endif
}

void spawn_process_info_free(struct spawn_process_info *pi)
{
	if(pi==NULL)
		return;
#ifdef _POSIX_C_SOURCE
	{
		sigset_t set, old;
		sigemptyset(&set);
		sigaddset(&set,SIGCHLD);
		spawn_process_sigmask(SIG_BLOCK,&set,&old);
		spawn_process_children=g_list_remove(spawn_process_children,pi);
		spawn_process_sigmask(SIG_SETMASK,&old,NULL);
	}
#endif
	g_free(pi);
}

#ifdef _POSIX_C_SOURCE
static void spawn_process_sigchld(int sig)
{
	int status;
	pid_t pid;
	while ((pid=waitpid(-1, &status, WNOHANG)) > 0) {
		GList *el=g_list_first(spawn_process_children);
		while(el) {
			struct spawn_process_info *p=el->data;
			if(p->pid==pid) {
				p->status=status;
			}
			el=g_list_next(el);
		}
	}
}
#endif

void spawn_process_init()
{
#ifdef _POSIX_C_SOURCE
	struct sigaction act;
	act.sa_handler=spawn_process_sigchld;
	act.sa_flags=0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGCHLD, &act, NULL);
#endif
	return;
}

/** Get printable compass direction from an angle. */
void
get_compass_direction(char *buffer, int angle, int mode)
{
	angle=angle%360;
	switch (mode) {
	case 0:
		sprintf(buffer,"%d",angle);
		break;
	case 1:
		if (angle < 69 || angle > 291)
			*buffer++='N';
		if (angle > 111 && angle < 249)
			*buffer++='S';
		if (angle > 22 && angle < 158)
			*buffer++='E';
		if (angle > 202 && angle < 338)
			*buffer++='W';
		*buffer++='\0';
		break;
	case 2:
		angle=(angle+15)/30;
		if (! angle)
			angle=12;
		sprintf(buffer,"%d H", angle);
		break;
	}
}
