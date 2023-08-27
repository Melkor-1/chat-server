#ifndef PIPE_H
#define PIPE_H

int set_pipe_nonblock (void);
void close_pipe (void);
int read_pipe (void);

#endif /* PIPE_H */
