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

/* The purpose of this file is to store the code that MOST mpm's will need
 * this does not mean a function only goes into this file if every MPM needs
 * it.  It means that if a function is needed by more than one MPM, and
 * future maintenance would be served by making the code common, then the
 * function belongs here.
 *
 * This is going in src/main because it is not platform specific, it is
 * specific to multi-process servers, but NOT to Unix.  Which is why it
 * does not belong in src/os/unix
 */

/**
 * @file  mpm_common.h
 * @brief Multi-Processing Modules functions
 *
 * @defgroup APACHE_MPM Multi-Processing Modules
 * @ingroup  APACHE
 * @{
 */

#ifndef APACHE_MPM_COMMON_H
#define APACHE_MPM_COMMON_H

#include "ap_config.h"

#if APR_HAVE_NETINET_TCP_H
#include <netinet/tcp.h>    /* for TCP_NODELAY */
#endif

#include "mpm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The maximum length of the queue of pending connections, as defined
 * by listen(2).  Under some systems, it should be increased if you
 * are experiencing a heavy TCP SYN flood attack.
 *
 * It defaults to 511 instead of 512 because some systems store it 
 * as an 8-bit datatype; 512 truncated to 8-bits is 0, while 511 is 
 * 255 when truncated.
 */
#ifndef DEFAULT_LISTENBACKLOG
#define DEFAULT_LISTENBACKLOG 511
#endif
        
/* Signal used to gracefully restart */
#define AP_SIG_GRACEFUL SIGUSR1

/* Signal used to gracefully restart (without SIG prefix) */
#define AP_SIG_GRACEFUL_SHORT USR1

/* Signal used to gracefully restart (as a quoted string) */
#define AP_SIG_GRACEFUL_STRING "SIGUSR1"

/* Signal used to gracefully stop */
#define AP_SIG_GRACEFUL_STOP SIGWINCH

/* Signal used to gracefully stop (without SIG prefix) */
#define AP_SIG_GRACEFUL_STOP_SHORT WINCH

/* Signal used to gracefully stop (as a quoted string) */
#define AP_SIG_GRACEFUL_STOP_STRING "SIGWINCH"

/**
 * Make sure all child processes that have been spawned by the parent process
 * have died.  This includes process registered as "other_children".
 * @warning This is only defined if the MPM defines 
 *          AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @param terminate Either 1 or 0.  If 1, send the child processes SIGTERM
 *        each time through the loop.  If 0, give the process time to die
 *        on its own before signalling it.
 * @tip This function requires that some macros are defined by the MPM: <pre>
 *  MPM_CHILD_PID -- Get the pid from the specified spot in the scoreboard
 *  MPM_NOTE_CHILD_KILLED -- Note the child died in the scoreboard
 * </pre>
 * @tip The MPM child processes which are reclaimed are those listed
 * in the scoreboard as well as those currently registered via
 * ap_register_extra_mpm_process().
 */
#ifdef AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
void ap_reclaim_child_processes(int terminate);
#endif

/**
 * Catch any child processes that have been spawned by the parent process
 * which have exited. This includes processes registered as "other_children".
 * @warning This is only defined if the MPM defines 
 *          AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @tip This function requires that some macros are defined by the MPM: <pre>
 *  MPM_CHILD_PID -- Get the pid from the specified spot in the scoreboard
 *  MPM_NOTE_CHILD_KILLED -- Note the child died in the scoreboard
 * </pre>
 * @tip The MPM child processes which are relieved are those listed
 * in the scoreboard as well as those currently registered via
 * ap_register_extra_mpm_process().
 */
#ifdef AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
void ap_relieve_child_processes(void);
#endif

/**
 * Tell ap_reclaim_child_processes() and ap_relieve_child_processes() about 
 * an MPM child process which has no entry in the scoreboard.
 * @warning This is only defined if the MPM defines
 *          AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @param pid The process id of an MPM child process which should be
 * reclaimed when ap_reclaim_child_processes() is called.
 * @tip If an extra MPM child process terminates prior to calling
 * ap_reclaim_child_processes(), remove it from the list of such processes
 * by calling ap_unregister_extra_mpm_process().
 */
#ifdef AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
void ap_register_extra_mpm_process(pid_t pid);
#endif

/**
 * Unregister an MPM child process which was previously registered by a
 * call to ap_register_extra_mpm_process().
 * @warning This is only defined if the MPM defines
 *          AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
 * @param pid The process id of an MPM child process which no longer needs to
 * be reclaimed.
 * @return 1 if the process was found and removed, 0 otherwise
 */
#ifdef AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
int ap_unregister_extra_mpm_process(pid_t pid);
#endif

/**
 * Safely signal an MPM child process, if the process is in the
 * current process group.  Otherwise fail.
 * @param pid the process id of a child process to signal
 * @param sig the signal number to send
 * @return APR_SUCCESS if signal is sent, otherwise an error as per kill(3);
 * APR_EINVAL is returned if passed either an invalid (< 1) pid, or if
 * the pid is not in the current process group
 */
#ifdef AP_MPM_WANT_RECLAIM_CHILD_PROCESSES
apr_status_t ap_mpm_safe_kill(pid_t pid, int sig);
#endif

/**
 * Determine if any child process has died.  If no child process died, then
 * this process sleeps for the amount of time specified by the MPM defined
 * macro SCOREBOARD_MAINTENANCE_INTERVAL.
 * @param status The return code if a process has died
 * @param ret The process id of the process that died
 * @param p The pool to allocate out of
 */
#ifdef AP_MPM_WANT_WAIT_OR_TIMEOUT
void ap_wait_or_timeout(apr_exit_why_e *status, int *exitcode, apr_proc_t *ret, 
                        apr_pool_t *p);
#endif

/**
 * Log why a child died to the error log, if the child died without the
 * parent signalling it.
 * @param pid The child that has died
 * @param status The status returned from ap_wait_or_timeout
 * @return 0 on success, APEXIT_CHILDFATAL if MPM should terminate
 */
#ifdef AP_MPM_WANT_PROCESS_CHILD_STATUS
int ap_process_child_status(apr_proc_t *pid, apr_exit_why_e why, int status);
#endif

#if defined(TCP_NODELAY) && !defined(MPE) && !defined(TPF)
/**
 * Turn off the nagle algorithm for the specified socket.  The nagle algorithm
 * says that we should delay sending partial packets in the hopes of getting
 * more data.  There are bad interactions between persistent connections and
 * Nagle's algorithm that have severe performance penalties.
 * @param s The socket to disable nagle for.
 */
void ap_sock_disable_nagle(apr_socket_t *s);
#else
#define ap_sock_disable_nagle(s)        /* NOOP */
#endif

#ifdef HAVE_GETPWNAM
/**
 * Convert a username to a numeric ID
 * @param name The name to convert
 * @return The user id corresponding to a name
 * @deffunc uid_t ap_uname2id(const char *name)
 */
AP_DECLARE(uid_t) ap_uname2id(const char *name);
#endif

#ifdef HAVE_GETGRNAM
/**
 * Convert a group name to a numeric ID
 * @param name The name to convert
 * @return The group id corresponding to a name
 * @deffunc gid_t ap_gname2id(const char *name)
 */
AP_DECLARE(gid_t) ap_gname2id(const char *name);
#endif

#define AP_MPM_HARD_LIMITS_FILE APACHE_MPM_DIR "/mpm_default.h"

#ifdef AP_MPM_USES_POD

typedef struct ap_pod_t ap_pod_t;

struct ap_pod_t {
    apr_file_t *pod_in;
    apr_file_t *pod_out;
    apr_pool_t *p;
};

/**
 * Open the pipe-of-death.  The pipe of death is used to tell all child
 * processes that it is time to die gracefully.
 * @param p The pool to use for allocating the pipe
 */
AP_DECLARE(apr_status_t) ap_mpm_pod_open(apr_pool_t *p, ap_pod_t **pod);

/**
 * Check the pipe to determine if the process has been signalled to die.
 */
AP_DECLARE(apr_status_t) ap_mpm_pod_check(ap_pod_t *pod);

/**
 * Close the pipe-of-death
 */
AP_DECLARE(apr_status_t) ap_mpm_pod_close(ap_pod_t *pod);

/**
 * Write data to the pipe-of-death, signalling that one child process
 * should die.
 * @param p The pool to use when allocating any required structures.
 */
AP_DECLARE(apr_status_t) ap_mpm_pod_signal(ap_pod_t *pod);

/**
 * Write data to the pipe-of-death, signalling that all child process
 * should die.
 * @param p The pool to use when allocating any required structures.
 * @param num The number of child processes to kill
 */
AP_DECLARE(void) ap_mpm_pod_killpg(ap_pod_t *pod, int num);
#endif

/*
 * These data members are common to all mpms. Each new mpm
 * should either use the appropriate ap_mpm_set_* function
 * in their command table or create their own for custom or
 * OS specific needs. These should work for most.
 */

/**
 * The maximum number of requests each child thread or
 * process handles before dying off
 */
#ifdef AP_MPM_WANT_SET_MAX_REQUESTS
extern int ap_max_requests_per_child;
const char *ap_mpm_set_max_requests(cmd_parms *cmd, void *dummy,
                                    const char *arg);
#endif

/**
 * The filename used to store the process id.
 */
#ifdef AP_MPM_WANT_SET_PIDFILE
extern const char *ap_pid_fname;
const char *ap_mpm_set_pidfile(cmd_parms *cmd, void *dummy,
                               const char *arg);
#endif

/**
 * The name of lockfile used when Apache needs to lock the accept() call.
 */
#ifdef AP_MPM_WANT_SET_LOCKFILE
extern const char *ap_lock_fname;
const char *ap_mpm_set_lockfile(cmd_parms *cmd, void *dummy,
                                const char *arg);
#endif

/**
 * The system mutex implementation to use for the accept mutex.
 */
#ifdef AP_MPM_WANT_SET_ACCEPT_LOCK_MECH
extern apr_lockmech_e ap_accept_lock_mech;
extern const char ap_valid_accept_mutex_string[];
const char *ap_mpm_set_accept_lock_mech(cmd_parms *cmd, void *dummy,
                                        const char *arg);
#endif

/*
 * Set the scorboard file.
 */
#ifdef AP_MPM_WANT_SET_SCOREBOARD
const char *ap_mpm_set_scoreboard(cmd_parms *cmd, void *dummy,
                                  const char *arg);
#endif

/*
 * The directory that the server changes directory to dump core.
 */
#ifdef AP_MPM_WANT_SET_COREDUMPDIR
extern char ap_coredump_dir[MAX_STRING_LEN];
extern int ap_coredumpdir_configured;
const char *ap_mpm_set_coredumpdir(cmd_parms *cmd, void *dummy,
                                   const char *arg);
#endif

/**
 * Set the timeout period for a graceful shutdown.
 */
#ifdef AP_MPM_WANT_SET_GRACEFUL_SHUTDOWN
extern int ap_graceful_shutdown_timeout;
const char *ap_mpm_set_graceful_shutdown(cmd_parms *cmd, void *dummy,
                                         const char *arg);
#define AP_GRACEFUL_SHUTDOWN_TIMEOUT_COMMAND \
AP_INIT_TAKE1("GracefulShutdownTimeout", ap_mpm_set_graceful_shutdown, NULL, \
              RSRC_CONF, "Maximum time in seconds to wait for child "        \
              "processes to complete transactions during shutdown")
#endif


#ifdef AP_MPM_WANT_SIGNAL_SERVER
int ap_signal_server(int *, apr_pool_t *);
void ap_mpm_rewrite_args(process_rec *);
#endif

#ifdef AP_MPM_WANT_SET_MAX_MEM_FREE
extern apr_uint32_t ap_max_mem_free;
extern const char *ap_mpm_set_max_mem_free(cmd_parms *cmd, void *dummy,
                                           const char *arg);
#endif

#ifdef AP_MPM_WANT_SET_STACKSIZE
extern apr_size_t ap_thread_stacksize;
extern const char *ap_mpm_set_thread_stacksize(cmd_parms *cmd, void *dummy,
                                               const char *arg);
#endif

#ifdef AP_MPM_WANT_FATAL_SIGNAL_HANDLER
extern apr_status_t ap_fatal_signal_setup(server_rec *s, apr_pool_t *pconf);
extern apr_status_t ap_fatal_signal_child_setup(server_rec *s);
#endif

#if AP_ENABLE_EXCEPTION_HOOK
extern const char *ap_mpm_set_exception_hook(cmd_parms *cmd, void *dummy,
                                             const char *arg);
#endif

AP_DECLARE_HOOK(int,monitor,(apr_pool_t *p))

#ifdef __cplusplus
}
#endif

#endif /* !APACHE_MPM_COMMON_H */
/** @} */
