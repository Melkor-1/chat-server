/**
*	@file	log.c
*
*	@brief	The implementation of the logging function.
*	
*	@author	Haris Salam
*	
*	@date	Tuesday, 2 February, 2023
*	
*	@bug	No known bugs.
*/

#include "log.h"

#define TS_BUF_LENGTH 50

extern int log_msg (FILE *stream, const char *msg, unsigned flags)
{
    FILE *fp = stderr;

    if (stream) {
        fp = stream;
    }

    static unsigned long long log_count = 0;
    char time_stamp[TS_BUF_LENGTH] = { 0 };
    char date_stamp[TS_BUF_LENGTH] = { 0 };
    const time_t time_val = flags & LOG_FULLTIME ? time (0) : 0;

    if (time_val == -1) {
        perror ("time()");
    }

    const struct tm *const tm_info =
        flags & LOG_FULLTIME ? localtime (&time_val) : 0;

    if (!tm_info && flags & LOG_FULLTIME) {
        perror ("time()");
    }

    log_count++;
    int n = 0;
    char log[BUFSIZ];

    if (flags & LOG_COUNT) {
        n += snprintf (log, sizeof log, "%lld, ", log_count);
    }
	// The ISO version is:
	// %04d-%02d-%02dT%02d:%02d:%02d, year, month, day, hours, minutes, seconds.
    if (flags & LOG_DATE && tm_info
        && strftime (date_stamp, TS_BUF_LENGTH, "%F (%a)", tm_info)) {
        n += snprintf (log + n, sizeof date_stamp, "%s, ", date_stamp);
    }
    if (flags & LOG_TIME && tm_info
        && strftime (time_stamp, TS_BUF_LENGTH, "%H:%M:%S", tm_info)) {
        n += snprintf (log + n, sizeof time_stamp, "%s, ", time_stamp);
    }
    if (msg) {
        snprintf (log + n, sizeof log - (size_t) n, "\"%s\"", msg);
    }
    n = fprintf (fp, "%s\n", log);
    return ferror (fp) ? -1 : n;
}
