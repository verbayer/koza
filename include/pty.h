#ifndef PTY_H
#define PTY_H

int pty_setup_child(int slave_fd);
int pty_run(int master_fd);

#endif
