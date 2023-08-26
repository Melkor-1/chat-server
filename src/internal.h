#ifndef INTERNAL_H
#define INTERNAL_H

#include "log.h"

#define PROGRAM_NAME "selectserver"
#define PORT "9909"             /* Port we are listening on. */
#define MAX_LOG_TEXT 2048       /* Max text length for logging. */

#ifdef  BUFSIZ                  /* Max client response length. */
#define BUFSIZE BUFSIZ
#else
#define BUFSIZE 4096
#endif

#define LOG_FILE "server.log"
#define MAX_SLAVES 1022
#define ARRAY_CARDINALITY(x) (sizeof(x) / sizeof ((x)[0]))

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

#define LOG_MSG(stream, msg, flags) log_msg (stream, msg, flags); \
											log_msg (0, msg, flags)

extern FILE *log_fp;
extern int pfds[2];
extern const char *const logs[];

enum log_codes {
    SS_SEND_ERROR = 0,
    SS_CLOSED_CONN,
    SS_CONN_SURPLUS,
    SS_FAILED_EXCUSE,
    SS_NEW_CONN,
    SS_OVERLOAD,
    SS_SOCKET_ERROR,
    SS_FCLOSE_ERROR,
    SS_INITIATE
};


#endif /* INTERNAL_H */
