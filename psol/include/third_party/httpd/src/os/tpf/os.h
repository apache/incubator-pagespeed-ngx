/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file tpf/os.h
 * @brief This file in included in all Apache source code. It contains definitions
 * of facilities available on _this_ operating system (HAVE_* macros),
 * and prototypes of OS specific functions defined in os.c or os-inline.c
 *
 * @defgroup APACHE_OS_TPF tpf
 * @ingroup  APACHE_OS
 * @{
 */

#ifndef APACHE_OS_H
#define APACHE_OS_H

#define PLATFORM "TPF"

#ifdef errno
#undef errno
#endif

#include "apr.h"
#include "ap_config.h"
#include <strings.h>
#ifndef __strings_h

#define FD_SETSIZE    2048 
 
typedef long fd_mask;

#define NBBY    8    /* number of bits in a byte */
#define NFDBITS (sizeof(fd_mask) * NBBY)
#define  howmany(x, y)  (((x)+((y)-1))/(y))

typedef struct fd_set { 
        fd_mask fds_bits [howmany(FD_SETSIZE, NFDBITS)];
} fd_set; 

#define FD_CLR(n, p)((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)((p)->fds_bits[(n)/NFDBITS] & (1 <<((n) % NFDBITS)))
#define  FD_ZERO(p)   memset((char *)(p), 0, sizeof(*(p)))
#endif
    
#ifdef FD_SET
#undef FD_SET
#define FD_SET(n, p) (0)
#endif

#include <i$netd.h>
struct apache_input {
    INETD_SERVER_INPUT  inetd_server;
    void                *scoreboard_heap;   /* scoreboard system heap address */
    int                 scoreboard_fd;      /* scoreboard file descriptor */
    int                 slot;               /* child number */
    int                 generation;         /* server generation number */
    int                 listeners[10];
    time_t              restart_time;
};

typedef struct apache_input APACHE_TPF_INPUT;

extern int tpf_child;

struct server_rec;
pid_t os_fork(struct server_rec *s, int slot);
int os_check_server(char *server);

extern char *ap_server_argv0;
extern int scoreboard_fd;
#include <signal.h>
#ifndef SIGPIPE
#define SIGPIPE 14
#endif
#ifdef NSIG
#undef NSIG
#endif
#endif /*! APACHE_OS_H*/
/** @} */
