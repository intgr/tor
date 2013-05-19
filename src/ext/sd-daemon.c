/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  Copyright 2010 Lennart Poettering

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
***/

#include "sd-daemon.h"

#ifndef __linux__

/* Dummy implementation for other platforms */

int sd_listen_fds(int unset_environment) {
        return 0;
}

#else /* __linux__ */

/* Actual implementation */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

int sd_listen_fds(int unset_environment) {
        int r, fd;
        const char *e;
        char *p = NULL;
        unsigned long l;

        e = getenv("LISTEN_PID");
        if (!e) {
                r = 0;
                goto finish;
        }

        errno = 0;
        l = strtoul(e, &p, 10);

        if (errno > 0) {
                r = -errno;
                goto finish;
        }

        if (!p || p == e || *p || l <= 0) {
                r = -EINVAL;
                goto finish;
        }

        /* Is this for us? */
        if (getpid() != (pid_t) l) {
                r = 0;
                goto finish;
        }

        e = getenv("LISTEN_FDS");
        if (!e) {
                r = 0;
                goto finish;
        }

        errno = 0;
        l = strtoul(e, &p, 10);

        if (errno > 0) {
                r = -errno;
                goto finish;
        }

        if (!p || p == e || *p) {
                r = -EINVAL;
                goto finish;
        }

        for (fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + (int) l; fd ++) {
                int flags;

                flags = fcntl(fd, F_GETFD);
                if (flags < 0) {
                        r = -errno;
                        goto finish;
                }

                if (flags & FD_CLOEXEC)
                        continue;

                if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
                        r = -errno;
                        goto finish;
                }
        }

        r = (int) l;

finish:
        if (unset_environment) {
                unsetenv("LISTEN_PID");
                unsetenv("LISTEN_FDS");
        }

        return r;
}

#endif /* __linux__ */
