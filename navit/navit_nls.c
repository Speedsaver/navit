#include "config.h"
#include "debug.h"
#include <glib.h>
#include <navit_nls.h>
#include <stdlib.h>
#ifdef HAVE_API_WIN32_CE
#include "libc.h"
#endif

char *
navit_nls_add_textdomain(const char *package, const char *dir)
{
	return NULL;
}

void
navit_nls_remove_textdomain(const char *package)
{
}

const char *
navit_nls_gettext(const char *msgid)
{
	return msgid;
}

const char *
navit_nls_ngettext(const char *msgid, const char *msgid_plural, unsigned long int n)
{
	if (n == 1) {
		return msgid;
	} else {
		return msgid_plural;
	}
}

void
navit_nls_main_init(void)
{
}
