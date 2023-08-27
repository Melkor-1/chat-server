#ifndef UTILS_H
#define UTILS_H

/**
*	\brief	Opens the LOG_FILE, and sets the stream associated with it to line-buffered mode.
*	\return	0 on success, or -1 on failure.
*/
int open_logfile (void);

/**
*	\brief	Enables line buffering for the stream associated with the LOG_FILE.
*	\return 0 on success, or -1 on failure.
*/
int enable_line_buf (void);

/**
*	\brief	Sets a socket to non-blocking mode.
*	\param	fd - A file descriptor
*	\return 0 if it succeeds, or -1 on failure.
*/
int enable_nonblocking (int fd);

static inline int max (int x, int y)
{
    return x > y ? x : y;
}

struct client_info *ss_search (int size, struct client_info *p_slaves[size],
                        struct client_info *const *ptr, int (*func) (const void *,
                                                              const void *));
int close_log_file (void);
void sig_handler (int sig);
void close_descriptor (int fd);

#endif /* UTILS_H */
