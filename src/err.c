#include "err.h"
#include "internal.h"

#include <stdio.h>
#include <stdarg.h>

PRINTF_LIKE(3, 4) void err_ret (FILE *stream, unsigned level, const char *fmt, ...)
{
    char buf[BUFSIZE];
    va_list argp;
    va_start (argp, fmt);

	if (fmt) {
    	vsnprintf (buf, sizeof buf, fmt, argp);
    	LOG_MSG (stream, buf, level);
	}
    va_end (argp);
}


