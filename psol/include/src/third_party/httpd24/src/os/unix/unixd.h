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
 * @file  unixd.h
 * @brief common stuff that unix MPMs will want
 *
 * @addtogroup APACHE_OS_UNIX
 * @{
 */

#ifndef UNIXD_H
#define UNIXD_H

#include "httpd.h"
#include "http_config.h"
#include "ap_listen.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include "apr_hooks.h"
#include "apr_thread_proc.h"
#include "apr_proc_mutex.h"
#include "apr_global_mutex.h"

#include <pwd.h>
#include <grp.h>
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uid_t uid;
    gid_t gid;
    int userdir;
} ap_unix_identity_t;

AP_DECLARE_HOOK(ap_unix_identity_t *, get_suexec_identity,(const request_rec *r))


/* Default user name and group name. These may be specified as numbers by
 * placing a # before a number */

#ifndef DEFAULT_USER
#define DEFAULT_USER "#-1"
#endif
#ifndef DEFAULT_GROUP
#define DEFAULT_GROUP "#-1"
#endif

typedef struct {
    const char *user_name;
    const char *group_name;
    uid_t user_id;
    gid_t group_id;
    int suexec_enabled;
    const char *chroot_dir;
    const char *suexec_disabled_reason; /* suitable msg if !suexec_enabled */
} unixd_config_rec;
AP_DECLARE_DATA extern unixd_config_rec ap_unixd_config;

#if defined(RLIMIT_CPU) || defined(RLIMIT_DATA) || defined(RLIMIT_VMEM) || defined(RLIMIT_NPROC) || defined(RLIMIT_AS)
AP_DECLARE(void) ap_unixd_set_rlimit(cmd_parms *cmd, struct rlimit **plimit,
                                     const char *arg,
                                     const char * arg2, int type);
#endif

/**
 * One of the functions to set mutex permissions should be called in
 * the parent process on platforms that switch identity when the
 * server is started as root.
 * If the child init logic is performed before switching identity
 * (e.g., MPM setup for an accept mutex), it should only be called
 * for SysV semaphores.  Otherwise, it is safe to call it for all
 * mutex types.
 */
AP_DECLARE(apr_status_t) ap_unixd_set_proc_mutex_perms(apr_proc_mutex_t *pmutex);
AP_DECLARE(apr_status_t) ap_unixd_set_global_mutex_perms(apr_global_mutex_t *gmutex);
AP_DECLARE(apr_status_t) ap_unixd_accept(void **accepted, ap_listen_rec *lr, apr_pool_t *ptrans);

#ifdef HAVE_KILLPG
#define ap_unixd_killpg(x, y)   (killpg ((x), (y)))
#define ap_os_killpg(x, y)      (killpg ((x), (y)))
#else /* HAVE_KILLPG */
#define ap_unixd_killpg(x, y)   (kill (-(x), (y)))
#define ap_os_killpg(x, y)      (kill (-(x), (y)))
#endif /* HAVE_KILLPG */

#ifdef __cplusplus
}
#endif

#endif
/** @} */
