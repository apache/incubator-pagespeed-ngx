/* include/ap_config_auto.h.  Generated automatically by configure.  */
/* include/ap_config_auto.h.in.  Generated automatically from configure.in by autoheader 2.13.  */

/* Define if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* #undef _ALL_SOURCE */
#endif

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if on MINIX.  */
/* #undef _MINIX */

/* Define if the system does not provide POSIX.1 features except
   with this defined.  */
/* #undef _POSIX_1_SOURCE */

/* Define if you need to in order for stat and other things to work.  */
/* #undef _POSIX_SOURCE */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you have the ENGINE_init function.  */
/* #undef HAVE_ENGINE_INIT */

/* Define if you have the ENGINE_load_builtin_engines function.  */
/* #undef HAVE_ENGINE_LOAD_BUILTIN_ENGINES */

/* Define if you have the SSLC_library_version function.  */
/* #undef HAVE_SSLC_LIBRARY_VERSION */

/* Define if you have the SSL_CTX_new function.  */
/* #undef HAVE_SSL_CTX_NEW */

/* Define if you have the SSL_set_cert_store function.  */
/* #undef HAVE_SSL_SET_CERT_STORE */

/* Define if you have the SSL_set_state function.  */
/* #undef HAVE_SSL_SET_STATE */

/* Define if you have the SSLeay_version function.  */
/* #undef HAVE_SSLEAY_VERSION */

/* Define if you have the bindprocessor function.  */
/* #undef HAVE_BINDPROCESSOR */

/* Define if you have the getgrnam function.  */
#define HAVE_GETGRNAM 1

/* Define if you have the getpgid function.  */
#define HAVE_GETPGID 1

/* Define if you have the getpwnam function.  */
#define HAVE_GETPWNAM 1

/* Define if you have the initgroups function.  */
#define HAVE_INITGROUPS 1

/* Define if you have the killpg function.  */
#define HAVE_KILLPG 1

/* Define if you have the prctl function.  */
#define HAVE_PRCTL 1

/* Define if you have the pthread_kill function.  */
/* #undef HAVE_PTHREAD_KILL */

/* Define if you have the setsid function.  */
#define HAVE_SETSID 1

/* Define if you have the syslog function.  */
/* #undef HAVE_SYSLOG */

/* Define if you have the timegm function.  */
#define HAVE_TIMEGM 1

/* Define if you have the times function.  */
#define HAVE_TIMES 1

/* Define if you have the <bstring.h> header file.  */
/* #undef HAVE_BSTRING_H */

/* Define if you have the <grp.h> header file.  */
#define HAVE_GRP_H 1

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <openssl/engine.h> header file.  */
/* #undef HAVE_OPENSSL_ENGINE_H */

/* Define if you have the <openssl/opensslv.h> header file.  */
/* #undef HAVE_OPENSSL_OPENSSLV_H */

/* Define if you have the <openssl/ssl.h> header file.  */
/* #undef HAVE_OPENSSL_SSL_H */

/* Define if you have the <pwd.h> header file.  */
#define HAVE_PWD_H 1

/* Define if you have the <sslc.h> header file.  */
/* #undef HAVE_SSLC_H */

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/ipc.h> header file.  */
#define HAVE_SYS_IPC_H 1

/* Define if you have the <sys/prctl.h> header file.  */
#define HAVE_SYS_PRCTL_H 1

/* Define if you have the <sys/processor.h> header file.  */
/* #undef HAVE_SYS_PROCESSOR_H */

/* Define if you have the <sys/resource.h> header file.  */
#define HAVE_SYS_RESOURCE_H 1

/* Define if you have the <sys/sem.h> header file.  */
#define HAVE_SYS_SEM_H 1

/* Define if you have the <sys/socket.h> header file.  */
#define HAVE_SYS_SOCKET_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <sys/times.h> header file.  */
#define HAVE_SYS_TIMES_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if struct tm has a tm_gmtoff field */
#define HAVE_GMTOFF 1

/* Allow IPv4 connections on IPv6 listening sockets */
#define AP_ENABLE_V4_MAPPED 1

/* Allow modules to run hook after a fatal exception */
/* #undef AP_ENABLE_EXCEPTION_HOOK */

/* Define if SSL is supported using OpenSSL */
/* #undef HAVE_OPENSSL */

/* Define if SSL is supported using SSL-C */
/* #undef HAVE_SSLC */

/* Define if distcache support is enabled */
/* #undef HAVE_DISTCACHE */

/* Define to 'int' if <sys/resource.h> doesn't define it for us */
/* #undef rlim_t */

/* Path to suexec binary  */
/* #undef SUEXEC_BIN */

/* User allowed to call SuExec  */
/* #undef AP_HTTPD_USER */

/* User subdirectory  */
/* #undef AP_USERDIR_SUFFIX */

/* SuExec root directory  */
/* #undef AP_DOC_ROOT */

/* Minimum allowed UID  */
/* #undef AP_UID_MIN */

/* Minimum allowed GID  */
/* #undef AP_GID_MIN */

/* SuExec log file  */
/* #undef AP_LOG_EXEC */

/* safe shell path for SuExec  */
/* #undef AP_SAFE_PATH */

/* umask for suexec'd process  */
/* #undef AP_SUEXEC_UMASK */

/* Using autoconf to configure Apache */
#define AP_USING_AUTOCONF 1

/* This platform doesn't suffer from the thundering herd problem */
#define SINGLE_LISTEN_UNSERIALIZED_ACCEPT 1

/* Listening sockets are non-blocking when there are more than 1 */
#define AP_NONBLOCK_WHEN_MULTI_LISTEN 1

/* Root directory of the Apache install area */
/* #define HTTPD_ROOT "/usr/local/apache2" */

/* Location of the config file, relative to the Apache root directory */
#define SERVER_CONFIG_FILE "conf/httpd.conf"

/* Location of the MIME types config file, relative to the Apache root directory */
#define AP_TYPES_CONFIG_FILE "conf/mime.types"

/* Location of the source for the current MPM */
#define APACHE_MPM_DIR "server/mpm/prefork"

