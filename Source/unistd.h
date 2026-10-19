/* Windows stand-in for <unistd.h>, only so firmware host-test code
   (tests/hostsim.c) compiles under clang-cl. The fork/pipe/read/write
   helpers just fail; the plugin never runs those test paths. */
#pragma once
#include <stdio.h>
#include <time.h>

typedef int pid_t;

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static inline int clock_gettime(int clk, struct timespec *ts)
{
    (void)clk;
    return timespec_get(ts, TIME_UTC) ? 0 : -1;
}

static inline int   pipe(int fd[2])                      { (void)fd; return -1; }
static inline pid_t fork(void)                           { return -1; }
static inline long  read(int fd, void *b, unsigned long n)        { (void)fd; (void)b; (void)n; return -1; }
static inline long  write(int fd, const void *b, unsigned long n) { (void)fd; (void)b; (void)n; return -1; }
static inline int   close(int fd)                        { (void)fd; return -1; }
