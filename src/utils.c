#include "utils.h"
#include "internal.h"
#include "err.h"

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

int open_logfile (void)
{
    if (!(log_fp = fopen (LOG_FILE, "a"))) {
        perror ("fopen()");
        return -1;
    }
    return 0;
}

int enable_line_buf (void)
{
    /*
     * Be less efficient here, to solve a problem more important
     * than mere efficiency.
     */
	errno = 0;

    if (setvbuf (log_fp, 0, _IOLBF, 0)) {
        if (errno) {
			perror ("setvbuf()");
		}
		return -1;
    }
    return 0;
}

int close_log_file (void)
{
    if (fclose (log_fp) == EOF) {
        err_ret (log_fp, LOG_FULLTIME, logs[SS_FCLOSE_ERROR],
                 PROGRAM_NAME);
    }
    return -1;
}

void sig_handler (int sig)
{
    const int saved_errno = errno;

    if (write (pfds[1], "x", 1) == -1
        && (errno != EAGAIN || errno != EWOULDBLOCK)) {
        signal (sig, SIG_DFL);
        raise (sig);
    }
    errno = saved_errno;
}

void close_descriptor (int fd)
{
    if (close (fd) == -1) {
        perror ("close()");
    }
}

int enable_nonblocking (int fd)
{
    int cmd = F_GETFL;
    int flags;

	/* The loop would exit after the second iteration because fcntl()
	*  would return 0.
	*/
    for (flags = 0; flags = fcntl (fd, cmd, flags); flags |= O_NONBLOCK) {
        cmd = F_SETFL;
    }
    return flags;
}

struct client_info *ss_search (int size, struct client_info *p_slaves[size],
                        struct client_info *const *ptr, int (*func) (const void *,
                                                              const void *))
{
    for (int i = 0; i < size; i++) {
        if (!func (ptr, &p_slaves[i])) {
            return p_slaves[i];
        }
    }
    return 0;
}


