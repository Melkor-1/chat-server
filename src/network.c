#include "network.h"
#include "err.h"
#include "internal.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <errno.h>
#include <stddef.h>

int send_internal (int slave_fd, const char *line, size_t *len)
{
    size_t total = 0;
    size_t bytes_left = *len;
    ssize_t ret_val;

    for (ret_val = 0; total < *len && ret_val != -1;
         total += (size_t) ret_val) {
		/*
		 * Disable SIGHUP, because a closed connection would generate a 
		 * send error, which would kill the server process.
		 */
        ret_val = send (slave_fd, line + total, bytes_left, MSG_NOSIGNAL);
        bytes_left -= (size_t) ret_val;
    }
    *len = total;
    return ret_val == -1 ? -1 : 0;
}

void send_response (size_t nbytes, const char *line, int sender_fd,
                           int master_fd, fd_set master, int fd_max)
{
    for (int i = 0; i <= fd_max && FD_ISSET (i, &master); i++) {
        /*
         * Send it to everyone. 
         */
        if (i != master_fd && i != sender_fd && i != pfds[0]) {
            /*
             * Excluding the master, sender, and the write end of the pipe. 
             */
            size_t len = nbytes;

            if (send_internal (i, line, &len) == -1) {
                perror ("send()");
            } else if (len != nbytes) {
                err_ret (log_fp, LOG_FULLTIME, logs[SS_SEND_ERROR],
                         PROGRAM_NAME, len);
            }
        }
    }
}

/** 
*	\brief	Get the number of bytes that are immediately available for reading.
*	\param	slave_fd - Socket to peek.	
*   \return	The number of bytes available, or -1 on failure.
*/
static int get_bytes (int slave_fd)
{
    int flag = 0;

    if (ioctl (slave_fd, FIONREAD, &flag) == -1) {
        perror ("ioctl()");
        return -1;
    }
    return flag;
}

char *get_response (size_t *nbytes, int slave_fd,
                           unsigned *err_code)
{
    /*
     * This is an arbitrary limit.
     * Does anyone know how to do this without a limit? 
     */
    const size_t page_size = BUFSIZE;
    char *buf = 0;
    int flag = 0;
    ssize_t ret_val = 0;
    size_t total = 0;
	
    do {
        if (total > (BUFSIZE * 10)) {
            /*
             * Likely a DOS attack.
             */
            *err_code = SS_CLOSE_CONN;
            goto out_free;
        }
        char *new = realloc (buf, total + page_size);

        if (!new) {
            *err_code = SS_NO_MEMORY;
            perror ("realloc()");
            goto out_free;
        }
        buf = new;

		/*
		* Disable SIGHUP, because a dropped connection causes a write error, which 
		* would make server process exit.
		*/
        if ((ret_val = recv (slave_fd, buf + total, page_size - 1, MSG_NOSIGNAL)) > 0) {
            total += (size_t) ret_val;
            buf[ret_val] = '\0';

            if ((flag = get_bytes (slave_fd)) == -1) {
                *err_code = SS_CLOSE_CONN;
                goto out_free;
            }
        } else {
            flag = !flag;
        }
    } while (flag > 0);

    if (ret_val == 0) {
        err_ret (log_fp, LOG_FULLTIME, logs[SS_CLOSED_CONN],
                 PROGRAM_NAME, slave_fd);
        *err_code = SS_CLOSE_CONN;
        goto out_free;
    }
    if (ret_val == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            *err_code = SS_WOULD_BLOCK;
            goto out_free;
        }
        *err_code = SS_CLOSE_CONN;
        goto out_free;
    }
    *nbytes = total;
    return buf;

  out_free:
    free (buf);
    return 0;
}


