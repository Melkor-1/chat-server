#include "pipe.h"
#include "internal.h"
#include "utils.h"

#include <unistd.h>
#include <stddef.h>
#include <errno.h>

int set_pipe_nonblock (void)
{
    int status = 0;

    for (size_t i = 0; i < ARRAY_CARDINALITY (pfds); i++) {
        if ((status = enable_nonblocking (pfds[i])) == -1) {
            break;
        }
    }
    return status;
}

void close_pipe (void)
{
    for (size_t i = 0; i < ARRAY_CARDINALITY (pfds); i++) {
        close_descriptor (pfds[i]);
    }
}

int read_pipe (void)
{
    char ch = '\0';

    if (read (pfds[0], &ch, 1) == -1
        && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0;
    }
    return -1;
}

