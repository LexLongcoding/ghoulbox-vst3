/* Windows stand-in for <sys/wait.h> (see Source/unistd.h). */
#pragma once

#define WIFEXITED(s)   (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)

static inline int waitpid(int pid, int *status, int options)
{
    (void)pid; (void)options;
    if (status) *status = 0;
    return -1;
}
