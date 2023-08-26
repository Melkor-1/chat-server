#ifndef ERR_H
#define ERR_H

#if defined(__GNUC__) || defined(__clang__)
#	define PRINTF_LIKE(n, m)       __attribute__((format(printf, n, m)))
#else
#	define PRINTF_LIKE(n, m)       /* If only */
#endif  /* __GNUC__ */

#include <stdio.h>

PRINTF_LIKE(3, 4) void err_ret (FILE *stream, unsigned level, const char *fmt, ...);

#endif /* ERR_H */
