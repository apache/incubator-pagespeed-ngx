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
 * @file httpd.h
 * @brief HTTP Daemon routines
 *
 * @defgroup APACHE Apache
 *
 * Top level group of which all other groups are a member
 * @{
 *
 * @defgroup APACHE_MODS Apache Modules
 *           Top level group for Apache Modules
 * @defgroup APACHE_OS Operating System Specific
 * @defgroup APACHE_CORE Apache Core
 * @{
 * @defgroup APACHE_CORE_DAEMON HTTP Daemon Routine
 * @{
 */

#ifndef APACHE_HTTPD_H
#define APACHE_HTTPD_H

/* XXX - We need to push more stuff to other .h files, or even .c files, to
 * make this file smaller
 */

/* Headers in which EVERYONE has an interest... */
#include "ap_config.h"
#include "ap_mmn.h"

#include "ap_release.h"

#include "apr.h"
#include "apr_general.h"
#include "apr_tables.h"
#include "apr_pools.h"
#include "apr_time.h"
#include "apr_network_io.h"
#include "apr_buckets.h"
#include "apr_poll.h"

#include "os.h"

#include "ap_regex.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* Note: apr_uri.h is also included, see below */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CORE_PRIVATE

/* ----------------------------- config dir ------------------------------ */

/** Define this to be the default server home dir. Most things later in this
 * file with a relative pathname will have this added.
 */
#ifndef HTTPD_ROOT
#ifdef OS2
/** Set default for OS/2 file system */
#define HTTPD_ROOT "/os2httpd"
#elif defined(WIN32)
/** Set default for Windows file system */
#define HTTPD_ROOT "/apache"
#elif defined (BEOS)
/** Set the default for BeOS */
#define HTTPD_ROOT "/boot/home/apache"
#elif defined (NETWARE)
/** Set the default for NetWare */
#define HTTPD_ROOT "/apache"
#else
/** Set for all other OSs */
#define HTTPD_ROOT "/usr/local/apache"
#endif
#endif /* HTTPD_ROOT */

/* 
 * --------- You shouldn't have to edit anything below this line ----------
 *
 * Any modifications to any defaults not defined above should be done in the 
 * respective configuration file. 
 *
 */

/** 
 * Default location of documents.  Can be overridden by the DocumentRoot
 * directive.
 */
#ifndef DOCUMENT_LOCATION
#ifdef OS2
/* Set default for OS/2 file system */
#define DOCUMENT_LOCATION  HTTPD_ROOT "/docs"
#else
/* Set default for non OS/2 file system */
#define DOCUMENT_LOCATION  HTTPD_ROOT "/htdocs"
#endif
#endif /* DOCUMENT_LOCATION */

/** Maximum number of dynamically loaded modules */
#ifndef DYNAMIC_MODULE_LIMIT
#define DYNAMIC_MODULE_LIMIT 128
#endif

/** Default administrator's address */
#define DEFAULT_ADMIN "[no address given]"

/** The name of the log files */
#ifndef DEFAULT_ERRORLOG
#if defined(OS2) || defined(WIN32)
#define DEFAULT_ERRORLOG "logs/error.log"
#else
#define DEFAULT_ERRORLOG "logs/error_log"
#endif
#endif /* DEFAULT_ERRORLOG */

/** Define this to be what your per-directory security files are called */
#ifndef DEFAULT_ACCESS_FNAME
#ifdef OS2
/* Set default for OS/2 file system */
#define DEFAULT_ACCESS_FNAME "htaccess"
#else
#define DEFAULT_ACCESS_FNAME ".htaccess"
#endif
#endif /* DEFAULT_ACCESS_FNAME */

/** The name of the server config file */
#ifndef SERVER_CONFIG_FILE
#define SERVER_CONFIG_FILE "conf/httpd.conf"
#endif

/** The default path for CGI scripts if none is currently set */
#ifndef DEFAULT_PATH
#define DEFAULT_PATH "/bin:/usr/bin:/usr/ucb:/usr/bsd:/usr/local/bin"
#endif

/** The path to the suExec wrapper, can be overridden in Configuration */
#ifndef SUEXEC_BIN
#define SUEXEC_BIN  HTTPD_ROOT "/bin/suexec"
#endif

/** The timeout for waiting for messages */
#ifndef DEFAULT_TIMEOUT
#define DEFAULT_TIMEOUT 300 
#endif

/** The timeout for waiting for keepalive timeout until next request */
#ifndef DEFAULT_KEEPALIVE_TIMEOUT
#define DEFAULT_KEEPALIVE_TIMEOUT 5
#endif

/** The number of requests to entertain per connection */
#ifndef DEFAULT_KEEPALIVE
#define DEFAULT_KEEPALIVE 100
#endif

/*
 * Limits on the size of various request items.  These limits primarily
 * exist to prevent simple denial-of-service attacks on a server based
 * on misuse of the protocol.  The recommended values will depend on the
 * nature of the server resources -- CGI scripts and database backends
 * might require large values, but most servers could get by with much
 * smaller limits than we use below.  The request message body size can
 * be limited by the per-dir config directive LimitRequestBody.
 *
 * Internal buffer sizes are two bytes more than the DEFAULT_LIMIT_REQUEST_LINE
 * and DEFAULT_LIMIT_REQUEST_FIELDSIZE below, which explains the 8190.
 * These two limits can be lowered (but not raised) by the server config
 * directives LimitRequestLine and LimitRequestFieldsize, respectively.
 *
 * DEFAULT_LIMIT_REQUEST_FIELDS can be modified or disabled (set = 0) by
 * the server config directive LimitRequestFields.
 */

/** default limit on bytes in Request-Line (Method+URI+HTTP-version) */
#ifndef DEFAULT_LIMIT_REQUEST_LINE
#define DEFAULT_LIMIT_REQUEST_LINE 8190
#endif 
/** default limit on bytes in any one header field  */
#ifndef DEFAULT_LIMIT_REQUEST_FIELDSIZE
#define DEFAULT_LIMIT_REQUEST_FIELDSIZE 8190
#endif 
/** default limit on number of request header fields */
#ifndef DEFAULT_LIMIT_REQUEST_FIELDS
#define DEFAULT_LIMIT_REQUEST_FIELDS 100
#endif 

/**
 * The default default character set name to add if AddDefaultCharset is
 * enabled.  Overridden with AddDefaultCharsetName.
 */
#define DEFAULT_ADD_DEFAULT_CHARSET_NAME "iso-8859-1"

#endif /* CORE_PRIVATE */

/** default HTTP Server protocol */
#define AP_SERVER_PROTOCOL "HTTP/1.1"


/* ------------------ stuff that modules are allowed to look at ----------- */

/** Define this to be what your HTML directory content files are called */
#ifndef AP_DEFAULT_INDEX
#define AP_DEFAULT_INDEX "index.html"
#endif


/** 
 * Define this to be what type you'd like returned for files with unknown 
 * suffixes.  
 * @warning MUST be all lower case. 
 */
#ifndef DEFAULT_CONTENT_TYPE
#define DEFAULT_CONTENT_TYPE "text/plain"
#endif

/**
 * NO_CONTENT_TYPE is an alternative DefaultType value that suppresses
 * setting any default type when there's no information (e.g. a proxy).
 */
#ifndef NO_CONTENT_TYPE
#define NO_CONTENT_TYPE "none"
#endif

/** The name of the MIME types file */
#ifndef AP_TYPES_CONFIG_FILE
#define AP_TYPES_CONFIG_FILE "conf/mime.types"
#endif

/*
 * Define the HTML doctype strings centrally.
 */
/** HTML 2.0 Doctype */
#define DOCTYPE_HTML_2_0  "<!DOCTYPE HTML PUBLIC \"-//IETF//" \
                          "DTD HTML 2.0//EN\">\n"
/** HTML 3.2 Doctype */
#define DOCTYPE_HTML_3_2  "<!DOCTYPE HTML PUBLIC \"-//W3C//" \
                          "DTD HTML 3.2 Final//EN\">\n"
/** HTML 4.0 Strict Doctype */
#define DOCTYPE_HTML_4_0S "<!DOCTYPE HTML PUBLIC \"-//W3C//" \
                          "DTD HTML 4.0//EN\"\n" \
                          "\"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
/** HTML 4.0 Transitional Doctype */
#define DOCTYPE_HTML_4_0T "<!DOCTYPE HTML PUBLIC \"-//W3C//" \
                          "DTD HTML 4.0 Transitional//EN\"\n" \
                          "\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n"
/** HTML 4.0 Frameset Doctype */
#define DOCTYPE_HTML_4_0F "<!DOCTYPE HTML PUBLIC \"-//W3C//" \
                          "DTD HTML 4.0 Frameset//EN\"\n" \
                          "\"http://www.w3.org/TR/REC-html40/frameset.dtd\">\n"
/** XHTML 1.0 Strict Doctype */
#define DOCTYPE_XHTML_1_0S "<!DOCTYPE html PUBLIC \"-//W3C//" \
                           "DTD XHTML 1.0 Strict//EN\"\n" \
                           "\"http://www.w3.org/TR/xhtml1/DTD/" \
                           "xhtml1-strict.dtd\">\n"
/** XHTML 1.0 Transitional Doctype */
#define DOCTYPE_XHTML_1_0T "<!DOCTYPE html PUBLIC \"-//W3C//" \
                           "DTD XHTML 1.0 Transitional//EN\"\n" \
                           "\"http://www.w3.org/TR/xhtml1/DTD/" \
                           "xhtml1-transitional.dtd\">\n"
/** XHTML 1.0 Frameset Doctype */
#define DOCTYPE_XHTML_1_0F "<!DOCTYPE html PUBLIC \"-//W3C//" \
                           "DTD XHTML 1.0 Frameset//EN\"\n" \
                           "\"http://www.w3.org/TR/xhtml1/DTD/" \
                           "xhtml1-frameset.dtd\">"

/** Internal representation for a HTTP protocol number, e.g., HTTP/1.1 */
#define HTTP_VERSION(major,minor) (1000*(major)+(minor))
/** Major part of HTTP protocol */
#define HTTP_VERSION_MAJOR(number) ((number)/1000)
/** Minor part of HTTP protocol */
#define HTTP_VERSION_MINOR(number) ((number)%1000)

/* -------------- Port number for server running standalone --------------- */

/** default HTTP Port */
#define DEFAULT_HTTP_PORT	80
/** default HTTPS Port */
#define DEFAULT_HTTPS_PORT	443
/**
 * Check whether @a port is the default port for the request @a r.
 * @param port The port number
 * @param r The request
 * @see #ap_default_port
 */
#define ap_is_default_port(port,r)	((port) == ap_default_port(r))
/**
 * Get the default port for a request (which depends on the scheme).
 * @param r The request
 */
#define ap_default_port(r)	ap_run_default_port(r)
/**
 * Get the scheme for a request.
 * @param r The request
 */
#define ap_http_scheme(r)	ap_run_http_scheme(r)

/** The default string length */
#define MAX_STRING_LEN HUGE_STRING_LEN

/** The length of a Huge string */
#define HUGE_STRING_LEN 8192

/** The size of the server's internal read-write buffers */
#define AP_IOBUFSIZE 8192

/** The max number of regex captures that can be expanded by ap_pregsub */
#define AP_MAX_REG_MATCH 10

/**
 * APR_HAS_LARGE_FILES introduces the problem of spliting sendfile into 
 * mutiple buckets, no greater than MAX(apr_size_t), and more granular 
 * than that in case the brigade code/filters attempt to read it directly.
 * ### 16mb is an invention, no idea if it is reasonable.
 */
#define AP_MAX_SENDFILE 16777216  /* 2^24 */

/**
 * Special Apache error codes. These are basically used
 *  in http_main.c so we can keep track of various errors.
 *        
 */
/** a normal exit */
#define APEXIT_OK		0x0
/** A fatal error arising during the server's init sequence */
#define APEXIT_INIT		0x2
/**  The child died during its init sequence */
#define APEXIT_CHILDINIT	0x3
/**  
 *   The child exited due to a resource shortage.
 *   The parent should limit the rate of forking until
 *   the situation is resolved.
 */
#define APEXIT_CHILDSICK        0x7
/** 
 *     A fatal error, resulting in the whole server aborting.
 *     If a child exits with this error, the parent process
 *     considers this a server-wide fatal error and aborts.
 */
#define APEXIT_CHILDFATAL	0xf

#ifndef AP_DECLARE
/**
 * Stuff marked #AP_DECLARE is part of the API, and intended for use
 * by modules. Its purpose is to allow us to add attributes that
 * particular platforms or compilers require to every exported function.
 */
# define AP_DECLARE(type)    type
#endif

#ifndef AP_DECLARE_NONSTD
/**
 * Stuff marked #AP_DECLARE_NONSTD is part of the API, and intended for
 * use by modules.  The difference between #AP_DECLARE and
 * #AP_DECLARE_NONSTD is that the latter is required for any functions
 * which use varargs or are used via indirect function call.  This
 * is to accomodate the two calling conventions in windows dlls.
 */
# define AP_DECLARE_NONSTD(type)    type
#endif
#ifndef AP_DECLARE_DATA
# define AP_DECLARE_DATA
#endif

#ifndef AP_MODULE_DECLARE
# define AP_MODULE_DECLARE(type)    type
#endif
#ifndef AP_MODULE_DECLARE_NONSTD
# define AP_MODULE_DECLARE_NONSTD(type)  type
#endif
#ifndef AP_MODULE_DECLARE_DATA
# define AP_MODULE_DECLARE_DATA
#endif

/**
 * @internal
 * modules should not use functions marked AP_CORE_DECLARE
 */
#ifndef AP_CORE_DECLARE
# define AP_CORE_DECLARE	AP_DECLARE
#endif

/**
 * @internal
 * modules should not use functions marked AP_CORE_DECLARE_NONSTD
 */

#ifndef AP_CORE_DECLARE_NONSTD
# define AP_CORE_DECLARE_NONSTD	AP_DECLARE_NONSTD
#endif

/** 
 * @brief The numeric version information is broken out into fields within this 
 * structure. 
 */
typedef struct {
    int major;              /**< major number */
    int minor;              /**< minor number */
    int patch;              /**< patch number */
    const char *add_string; /**< additional string like "-dev" */
} ap_version_t;

/**
 * Return httpd's version information in a numeric form.
 *
 *  @param version Pointer to a version structure for returning the version
 *                 information.
 */
AP_DECLARE(void) ap_get_server_revision(ap_version_t *version);

/**
 * Get the server version string, as controlled by the ServerTokens directive
 * @return The server version string
 * @deprecated @see ap_get_server_banner() and ap_get_server_description()
 */
AP_DECLARE(const char *) ap_get_server_version(void);

/**
 * Get the server banner in a form suitable for sending over the
 * network, with the level of information controlled by the
 * ServerTokens directive.
 * @return The server banner
 */
AP_DECLARE(const char *) ap_get_server_banner(void);

/**
 * Get the server description in a form suitable for local displays,
 * status reports, or logging.  This includes the detailed server
 * version and information about some modules.  It is not affected
 * by the ServerTokens directive.
 * @return The server description
 */
AP_DECLARE(const char *) ap_get_server_description(void);

/**
 * Add a component to the server description and banner strings
 * (The latter is returned by the deprecated function
 * ap_get_server_version().)
 * @param pconf The pool to allocate the component from
 * @param component The string to add
 */
AP_DECLARE(void) ap_add_version_component(apr_pool_t *pconf, const char *component);

/**
 * Get the date a time that the server was built
 * @return The server build time string
 */
AP_DECLARE(const char *) ap_get_server_built(void);

#define DECLINED -1		/**< Module declines to handle */
#define DONE -2			/**< Module has served the response completely 
				 *  - it's safe to die() with no more output
				 */
#define OK 0			/**< Module has handled this stage. */


/**
 * @defgroup HTTP_Status HTTP Status Codes
 * @{
 */
/**
 * The size of the static array in http_protocol.c for storing
 * all of the potential response status-lines (a sparse table).
 * A future version should dynamically generate the apr_table_t at startup.
 */
#define RESPONSE_CODES 57

#define HTTP_CONTINUE                      100
#define HTTP_SWITCHING_PROTOCOLS           101
#define HTTP_PROCESSING                    102
#define HTTP_OK                            200
#define HTTP_CREATED                       201
#define HTTP_ACCEPTED                      202
#define HTTP_NON_AUTHORITATIVE             203
#define HTTP_NO_CONTENT                    204
#define HTTP_RESET_CONTENT                 205
#define HTTP_PARTIAL_CONTENT               206
#define HTTP_MULTI_STATUS                  207
#define HTTP_MULTIPLE_CHOICES              300
#define HTTP_MOVED_PERMANENTLY             301
#define HTTP_MOVED_TEMPORARILY             302
#define HTTP_SEE_OTHER                     303
#define HTTP_NOT_MODIFIED                  304
#define HTTP_USE_PROXY                     305
#define HTTP_TEMPORARY_REDIRECT            307
#define HTTP_BAD_REQUEST                   400
#define HTTP_UNAUTHORIZED                  401
#define HTTP_PAYMENT_REQUIRED              402
#define HTTP_FORBIDDEN                     403
#define HTTP_NOT_FOUND                     404
#define HTTP_METHOD_NOT_ALLOWED            405
#define HTTP_NOT_ACCEPTABLE                406
#define HTTP_PROXY_AUTHENTICATION_REQUIRED 407
#define HTTP_REQUEST_TIME_OUT              408
#define HTTP_CONFLICT                      409
#define HTTP_GONE                          410
#define HTTP_LENGTH_REQUIRED               411
#define HTTP_PRECONDITION_FAILED           412
#define HTTP_REQUEST_ENTITY_TOO_LARGE      413
#define HTTP_REQUEST_URI_TOO_LARGE         414
#define HTTP_UNSUPPORTED_MEDIA_TYPE        415
#define HTTP_RANGE_NOT_SATISFIABLE         416
#define HTTP_EXPECTATION_FAILED            417
#define HTTP_UNPROCESSABLE_ENTITY          422
#define HTTP_LOCKED                        423
#define HTTP_FAILED_DEPENDENCY             424
#define HTTP_UPGRADE_REQUIRED              426
#define HTTP_INTERNAL_SERVER_ERROR         500
#define HTTP_NOT_IMPLEMENTED               501
#define HTTP_BAD_GATEWAY                   502
#define HTTP_SERVICE_UNAVAILABLE           503
#define HTTP_GATEWAY_TIME_OUT              504
#define HTTP_VERSION_NOT_SUPPORTED         505
#define HTTP_VARIANT_ALSO_VARIES           506
#define HTTP_INSUFFICIENT_STORAGE          507
#define HTTP_NOT_EXTENDED                  510

/** is the status code informational */
#define ap_is_HTTP_INFO(x)         (((x) >= 100)&&((x) < 200))
/** is the status code OK ?*/
#define ap_is_HTTP_SUCCESS(x)      (((x) >= 200)&&((x) < 300))
/** is the status code a redirect */
#define ap_is_HTTP_REDIRECT(x)     (((x) >= 300)&&((x) < 400))
/** is the status code a error (client or server) */
#define ap_is_HTTP_ERROR(x)        (((x) >= 400)&&((x) < 600))
/** is the status code a client error  */
#define ap_is_HTTP_CLIENT_ERROR(x) (((x) >= 400)&&((x) < 500))
/** is the status code a server error  */
#define ap_is_HTTP_SERVER_ERROR(x) (((x) >= 500)&&((x) < 600))
/** is the status code a (potentially) valid response code?  */
#define ap_is_HTTP_VALID_RESPONSE(x) (((x) >= 100)&&((x) < 600))

/** should the status code drop the connection */
#define ap_status_drops_connection(x) \
                                   (((x) == HTTP_BAD_REQUEST)           || \
                                    ((x) == HTTP_REQUEST_TIME_OUT)      || \
                                    ((x) == HTTP_LENGTH_REQUIRED)       || \
                                    ((x) == HTTP_REQUEST_ENTITY_TOO_LARGE) || \
                                    ((x) == HTTP_REQUEST_URI_TOO_LARGE) || \
                                    ((x) == HTTP_INTERNAL_SERVER_ERROR) || \
                                    ((x) == HTTP_SERVICE_UNAVAILABLE) || \
				    ((x) == HTTP_NOT_IMPLEMENTED))
/** @} */

/**
 * @defgroup Methods List of Methods recognized by the server
 * @ingroup APACHE_CORE_DAEMON
 * @{
 *
 * @brief Methods recognized (but not necessarily handled) by the server.
 *
 * These constants are used in bit shifting masks of size int, so it is
 * unsafe to have more methods than bits in an int.  HEAD == M_GET.
 * This list must be tracked by the list in http_protocol.c in routine
 * ap_method_name_of().
 *
 */

#define M_GET                   0       /** RFC 2616: HTTP */
#define M_PUT                   1       /*  :             */
#define M_POST                  2
#define M_DELETE                3
#define M_CONNECT               4
#define M_OPTIONS               5
#define M_TRACE                 6       /** RFC 2616: HTTP */
#define M_PATCH                 7       /** no rfc(!)  ### remove this one? */
#define M_PROPFIND              8       /** RFC 2518: WebDAV */
#define M_PROPPATCH             9       /*  :               */
#define M_MKCOL                 10
#define M_COPY                  11
#define M_MOVE                  12
#define M_LOCK                  13
#define M_UNLOCK                14      /** RFC 2518: WebDAV */
#define M_VERSION_CONTROL       15      /** RFC 3253: WebDAV Versioning */
#define M_CHECKOUT              16      /*  :                          */
#define M_UNCHECKOUT            17
#define M_CHECKIN               18
#define M_UPDATE                19
#define M_LABEL                 20
#define M_REPORT                21
#define M_MKWORKSPACE           22
#define M_MKACTIVITY            23
#define M_BASELINE_CONTROL      24
#define M_MERGE                 25
#define M_INVALID               26      /** RFC 3253: WebDAV Versioning */

/**
 * METHODS needs to be equal to the number of bits
 * we are using for limit masks.
 */
#define METHODS     64

/**
 * The method mask bit to shift for anding with a bitmask.
 */
#define AP_METHOD_BIT ((apr_int64_t)1)
/** @} */


/** @see ap_method_list_t */
typedef struct ap_method_list_t ap_method_list_t;

/**
 * @struct ap_method_list_t
 * @brief  Structure for handling HTTP methods.  
 *
 * Methods known to the server are accessed via a bitmask shortcut; 
 * extension methods are handled by an array.
 */
struct ap_method_list_t {
    /** The bitmask used for known methods */
    apr_int64_t method_mask;
    /** the array used for extension methods */
    apr_array_header_t *method_list;
};

/**
 * @defgroup module_magic Module Magic mime types
 * @{
 */
/** Magic for mod_cgi[d] */
#define CGI_MAGIC_TYPE "application/x-httpd-cgi"
/** Magic for mod_include */
#define INCLUDES_MAGIC_TYPE "text/x-server-parsed-html"
/** Magic for mod_include */
#define INCLUDES_MAGIC_TYPE3 "text/x-server-parsed-html3"
/** Magic for mod_dir */
#define DIR_MAGIC_TYPE "httpd/unix-directory"

/** @} */
/* Just in case your linefeed isn't the one the other end is expecting. */
#if !APR_CHARSET_EBCDIC
/** linefeed */
#define LF 10
/** carrige return */
#define CR 13
/** carrige return /Line Feed Combo */
#define CRLF "\015\012"
#else /* APR_CHARSET_EBCDIC */
/* For platforms using the EBCDIC charset, the transition ASCII->EBCDIC is done
 * in the buff package (bread/bputs/bwrite).  Everywhere else, we use
 * "native EBCDIC" CR and NL characters. These are therefore
 * defined as
 * '\r' and '\n'.
 */
#define CR '\r'
#define LF '\n'
#define CRLF "\r\n"
#endif /* APR_CHARSET_EBCDIC */                                   
/** Useful for common code with either platform charset. */
#define CRLF_ASCII "\015\012"

/**
 * @defgroup values_request_rec_body Possible values for request_rec.read_body 
 * @{
 * Possible values for request_rec.read_body (set by handling module):
 */

/** Send 413 error if message has any body */
#define REQUEST_NO_BODY          0
/** Send 411 error if body without Content-Length */
#define REQUEST_CHUNKED_ERROR    1
/** If chunked, remove the chunks for me. */
#define REQUEST_CHUNKED_DECHUNK  2
/** @} // values_request_rec_body */

/**
 * @defgroup values_request_rec_used_path_info Possible values for request_rec.used_path_info 
 * @ingroup APACHE_CORE_DAEMON
 * @{
 * Possible values for request_rec.used_path_info:
 */

/** Accept the path_info from the request */
#define AP_REQ_ACCEPT_PATH_INFO    0
/** Return a 404 error if path_info was given */
#define AP_REQ_REJECT_PATH_INFO    1
/** Module may chose to use the given path_info */
#define AP_REQ_DEFAULT_PATH_INFO   2

/** @} // values_request_rec_used_path_info */


/*
 * Things which may vary per file-lookup WITHIN a request ---
 * e.g., state of MIME config.  Basically, the name of an object, info
 * about the object, and any other info we may ahve which may need to
 * change as we go poking around looking for it (e.g., overridden by
 * .htaccess files).
 *
 * Note how the default state of almost all these things is properly
 * zero, so that allocating it with pcalloc does the right thing without
 * a whole lot of hairy initialization... so long as we are willing to
 * make the (fairly) portable assumption that the bit pattern of a NULL
 * pointer is, in fact, zero.
 */

/**
 * @brief This represents the result of calling htaccess; these are cached for
 * each request.
 */
struct htaccess_result {
    /** the directory to which this applies */
    const char *dir;
    /** the overrides allowed for the .htaccess file */
    int override;
    /** the override options allowed for the .htaccess file */
    int override_opts;
    /** the configuration directives */
    struct ap_conf_vector_t *htaccess;
    /** the next one, or NULL if no more; N.B. never change this */
    const struct htaccess_result *next;
};

/* The following four types define a hierarchy of activities, so that
 * given a request_rec r you can write r->connection->server->process
 * to get to the process_rec.  While this reduces substantially the
 * number of arguments that various hooks require beware that in
 * threaded versions of the server you must consider multiplexing
 * issues.  */


/** A structure that represents one process */
typedef struct process_rec process_rec;
/** A structure that represents a virtual server */
typedef struct server_rec server_rec;
/** A structure that represents one connection */
typedef struct conn_rec conn_rec;
/** A structure that represents the current request */
typedef struct request_rec request_rec;
/** A structure that represents the status of the current connection */
typedef struct conn_state_t conn_state_t;

/* ### would be nice to not include this from httpd.h ... */
/* This comes after we have defined the request_rec type */
#include "apr_uri.h"

/** 
 * @brief A structure that represents one process 
 */
struct process_rec {
    /** Global pool. Cleared upon normal exit */
    apr_pool_t *pool;
    /** Configuration pool. Cleared upon restart */
    apr_pool_t *pconf;
    /** Number of command line arguments passed to the program */
    int argc;
    /** The command line arguments */
    const char * const *argv;
    /** The program name used to execute the program */
    const char *short_name;
};

/** 
 * @brief A structure that represents the current request 
 */
struct request_rec {
    /** The pool associated with the request */
    apr_pool_t *pool;
    /** The connection to the client */
    conn_rec *connection;
    /** The virtual host for this request */
    server_rec *server;

    /** Pointer to the redirected request if this is an external redirect */
    request_rec *next;
    /** Pointer to the previous request if this is an internal redirect */
    request_rec *prev;

    /** Pointer to the main request if this is a sub-request
     * (see http_request.h) */
    request_rec *main;

    /* Info about the request itself... we begin with stuff that only
     * protocol.c should ever touch...
     */
    /** First line of request */
    char *the_request;
    /** HTTP/0.9, "simple" request (e.g. GET /foo\n w/no headers) */
    int assbackwards;
    /** A proxy request (calculated during post_read_request/translate_name)
     *  possible values PROXYREQ_NONE, PROXYREQ_PROXY, PROXYREQ_REVERSE,
     *                  PROXYREQ_RESPONSE
     */
    int proxyreq;
    /** HEAD request, as opposed to GET */
    int header_only;
    /** Protocol string, as given to us, or HTTP/0.9 */
    char *protocol;
    /** Protocol version number of protocol; 1.1 = 1001 */
    int proto_num;
    /** Host, as set by full URI or Host: */
    const char *hostname;

    /** Time when the request started */
    apr_time_t request_time;

    /** Status line, if set by script */
    const char *status_line;
    /** Status line */
    int status;

    /* Request method, two ways; also, protocol, etc..  Outside of protocol.c,
     * look, but don't touch.
     */

    /** Request method (eg. GET, HEAD, POST, etc.) */
    const char *method;
    /** M_GET, M_POST, etc. */
    int method_number;

    /**
     *  'allowed' is a bitvector of the allowed methods.
     *
     *  A handler must ensure that the request method is one that
     *  it is capable of handling.  Generally modules should DECLINE
     *  any request methods they do not handle.  Prior to aborting the
     *  handler like this the handler should set r->allowed to the list
     *  of methods that it is willing to handle.  This bitvector is used
     *  to construct the "Allow:" header required for OPTIONS requests,
     *  and HTTP_METHOD_NOT_ALLOWED and HTTP_NOT_IMPLEMENTED status codes.
     *
     *  Since the default_handler deals with OPTIONS, all modules can
     *  usually decline to deal with OPTIONS.  TRACE is always allowed,
     *  modules don't need to set it explicitly.
     *
     *  Since the default_handler will always handle a GET, a
     *  module which does *not* implement GET should probably return
     *  HTTP_METHOD_NOT_ALLOWED.  Unfortunately this means that a Script GET
     *  handler can't be installed by mod_actions.
     */
    apr_int64_t allowed;
    /** Array of extension methods */
    apr_array_header_t *allowed_xmethods; 
    /** List of allowed methods */
    ap_method_list_t *allowed_methods; 

    /** byte count in stream is for body */
    apr_off_t sent_bodyct;
    /** body byte count, for easy access */
    apr_off_t bytes_sent;
    /** Last modified time of the requested resource */
    apr_time_t mtime;

    /* HTTP/1.1 connection-level features */

    /** sending chunked transfer-coding */
    int chunked;
    /** The Range: header */
    const char *range;
    /** The "real" content length */
    apr_off_t clength;

    /** Remaining bytes left to read from the request body */
    apr_off_t remaining;
    /** Number of bytes that have been read  from the request body */
    apr_off_t read_length;
    /** Method for reading the request body
     * (eg. REQUEST_CHUNKED_ERROR, REQUEST_NO_BODY,
     *  REQUEST_CHUNKED_DECHUNK, etc...) */
    int read_body;
    /** reading chunked transfer-coding */
    int read_chunked;
    /** is client waiting for a 100 response? */
    unsigned expecting_100;

    /* MIME header environments, in and out.  Also, an array containing
     * environment variables to be passed to subprocesses, so people can
     * write modules to add to that environment.
     *
     * The difference between headers_out and err_headers_out is that the
     * latter are printed even on error, and persist across internal redirects
     * (so the headers printed for ErrorDocument handlers will have them).
     *
     * The 'notes' apr_table_t is for notes from one module to another, with no
     * other set purpose in mind...
     */

    /** MIME header environment from the request */
    apr_table_t *headers_in;
    /** MIME header environment for the response */
    apr_table_t *headers_out;
    /** MIME header environment for the response, printed even on errors and
     * persist across internal redirects */
    apr_table_t *err_headers_out;
    /** Array of environment variables to be used for sub processes */
    apr_table_t *subprocess_env;
    /** Notes from one module to another */
    apr_table_t *notes;

    /* content_type, handler, content_encoding, and all content_languages 
     * MUST be lowercased strings.  They may be pointers to static strings;
     * they should not be modified in place.
     */
    /** The content-type for the current request */
    const char *content_type;	/* Break these out --- we dispatch on 'em */
    /** The handler string that we use to call a handler function */
    const char *handler;	/* What we *really* dispatch on */

    /** How to encode the data */
    const char *content_encoding;
    /** Array of strings representing the content languages */
    apr_array_header_t *content_languages;

    /** variant list validator (if negotiated) */
    char *vlist_validator;
    
    /** If an authentication check was made, this gets set to the user name. */
    char *user;	
    /** If an authentication check was made, this gets set to the auth type. */
    char *ap_auth_type;

    /** This response can not be cached */
    int no_cache;
    /** There is no local copy of this response */
    int no_local_copy;

    /* What object is being requested (either directly, or via include
     * or content-negotiation mapping).
     */

    /** The URI without any parsing performed */
    char *unparsed_uri;	
    /** The path portion of the URI */
    char *uri;
    /** The filename on disk corresponding to this response */
    char *filename;
    /* XXX: What does this mean? Please define "canonicalize" -aaron */
    /** The true filename, we canonicalize r->filename if these don't match */
    char *canonical_filename;
    /** The PATH_INFO extracted from this request */
    char *path_info;
    /** The QUERY_ARGS extracted from this request */
    char *args;	
    /**  finfo.protection (st_mode) set to zero if no such file */
    apr_finfo_t finfo;
    /** A struct containing the components of URI */
    apr_uri_t parsed_uri;

    /**
     * Flag for the handler to accept or reject path_info on 
     * the current request.  All modules should respect the
     * AP_REQ_ACCEPT_PATH_INFO and AP_REQ_REJECT_PATH_INFO 
     * values, while AP_REQ_DEFAULT_PATH_INFO indicates they
     * may follow existing conventions.  This is set to the
     * user's preference upon HOOK_VERY_FIRST of the fixups.
     */
    int used_path_info;

    /* Various other config info which may change with .htaccess files
     * These are config vectors, with one void* pointer for each module
     * (the thing pointed to being the module's business).
     */

    /** Options set in config files, etc. */
    struct ap_conf_vector_t *per_dir_config;
    /** Notes on *this* request */
    struct ap_conf_vector_t *request_config;

    /**
     * A linked list of the .htaccess configuration directives
     * accessed by this request.
     * N.B. always add to the head of the list, _never_ to the end.
     * that way, a sub request's list can (temporarily) point to a parent's list
     */
    const struct htaccess_result *htaccess;

    /** A list of output filters to be used for this request */
    struct ap_filter_t *output_filters;
    /** A list of input filters to be used for this request */
    struct ap_filter_t *input_filters;

    /** A list of protocol level output filters to be used for this
     *  request */
    struct ap_filter_t *proto_output_filters;
    /** A list of protocol level input filters to be used for this
     *  request */
    struct ap_filter_t *proto_input_filters;

    /** A flag to determine if the eos bucket has been sent yet */
    int eos_sent;

/* Things placed at the end of the record to avoid breaking binary
 * compatibility.  It would be nice to remember to reorder the entire
 * record to improve 64bit alignment the next time we need to break
 * binary compatibility for some other reason.
 */
};

/**
 * @defgroup ProxyReq Proxy request types
 *
 * Possible values of request_rec->proxyreq. A request could be normal,
 *  proxied or reverse proxied. Normally proxied and reverse proxied are
 *  grouped together as just "proxied", but sometimes it's necessary to
 *  tell the difference between the two, such as for authentication.
 * @{
 */

#define PROXYREQ_NONE 0		/**< No proxy */
#define PROXYREQ_PROXY 1	/**< Standard proxy */
#define PROXYREQ_REVERSE 2	/**< Reverse proxy */
#define PROXYREQ_RESPONSE 3 /**< Origin response */

/* @} */

/**
 * @brief Enumeration of connection keepalive options
 */
typedef enum {
    AP_CONN_UNKNOWN,
    AP_CONN_CLOSE,
    AP_CONN_KEEPALIVE
} ap_conn_keepalive_e;

/** 
 * @brief Structure to store things which are per connection 
 */
struct conn_rec {
    /** Pool associated with this connection */
    apr_pool_t *pool;
    /** Physical vhost this conn came in on */
    server_rec *base_server;
    /** used by http_vhost.c */
    void *vhost_lookup_data;

    /* Information about the connection itself */
    /** local address */
    apr_sockaddr_t *local_addr;
    /** remote address */
    apr_sockaddr_t *remote_addr;

    /** Client's IP address */
    char *remote_ip;
    /** Client's DNS name, if known.  NULL if DNS hasn't been checked,
     *  "" if it has and no address was found.  N.B. Only access this though
     * get_remote_host() */
    char *remote_host;
    /** Only ever set if doing rfc1413 lookups.  N.B. Only access this through
     *  get_remote_logname() */
    char *remote_logname;

    /** Are we still talking? */
    unsigned aborted:1;

    /** Are we going to keep the connection alive for another request?
     * @see ap_conn_keepalive_e */
    ap_conn_keepalive_e keepalive;

    /** have we done double-reverse DNS? -1 yes/failure, 0 not yet, 
     *  1 yes/success */
    signed int double_reverse:2;

    /** How many times have we used it? */
    int keepalives;
    /** server IP address */
    char *local_ip;
    /** used for ap_get_server_name when UseCanonicalName is set to DNS
     *  (ignores setting of HostnameLookups) */
    char *local_host;

    /** ID of this connection; unique at any point in time */
    long id; 
    /** Config vector containing pointers to connections per-server
     *  config structures. */
    struct ap_conf_vector_t *conn_config;
    /** Notes on *this* connection: send note from one module to
     *  another. must remain valid for all requests on this conn */
    apr_table_t *notes;
    /** A list of input filters to be used for this connection */
    struct ap_filter_t *input_filters;
    /** A list of output filters to be used for this connection */
    struct ap_filter_t *output_filters;
    /** handle to scoreboard information for this connection */
    void *sbh;
    /** The bucket allocator to use for all bucket/brigade creations */
    struct apr_bucket_alloc_t *bucket_alloc;
    /** The current state of this connection */
    conn_state_t *cs;
    /** Is there data pending in the input filters? */ 
    int data_in_input_filters;
    
    /** Are there any filters that clogg/buffer the input stream, breaking
     *  the event mpm.
     */
    int clogging_input_filters;
};

/** 
 * Enumeration of connection states 
 */
typedef enum  {
    CONN_STATE_CHECK_REQUEST_LINE_READABLE,
    CONN_STATE_READ_REQUEST_LINE,
    CONN_STATE_LINGER
} conn_state_e;

/** 
 * @brief A structure to contain connection state information 
 */
struct conn_state_t {
    /** APR_RING of expiration timeouts */
    APR_RING_ENTRY(conn_state_t) timeout_list;
    /** the expiration time of the next keepalive timeout */
    apr_time_t expiration_time;
    /** Current state of the connection */
    conn_state_e state;
    /** connection record this struct refers to */
    conn_rec *c;
    /** memory pool to allocate from */
    apr_pool_t *p;
    /** bucket allocator */
    apr_bucket_alloc_t *bucket_alloc;
    /** poll file decriptor information */
    apr_pollfd_t pfd;
};

/* Per-vhost config... */

/**
 * The address 255.255.255.255, when used as a virtualhost address,
 * will become the "default" server when the ip doesn't match other vhosts.
 */
#define DEFAULT_VHOST_ADDR 0xfffffffful


/**
 * @struct server_addr_rec
 * @brief  A structure to be used for Per-vhost config 
 */
typedef struct server_addr_rec server_addr_rec;
struct server_addr_rec {
    /** The next server in the list */
    server_addr_rec *next;
    /** The bound address, for this server */
    apr_sockaddr_t *host_addr;
    /** The bound port, for this server */
    apr_port_t host_port;
    /** The name given in "<VirtualHost>" */
    char *virthost;
};

/** 
 * @brief A structure to store information for each virtual server 
 */
struct server_rec {
    /** The process this server is running in */
    process_rec *process;
    /** The next server in the list */
    server_rec *next;

    /** The name of the server */
    const char *defn_name;
    /** The line of the config file that the server was defined on */
    unsigned defn_line_number;

    /* Contact information */

    /** The admin's contact information */
    char *server_admin;
    /** The server hostname */
    char *server_hostname;
    /** for redirects, etc. */
    apr_port_t port;

    /* Log files --- note that transfer log is now in the modules... */

    /** The name of the error log */
    char *error_fname;
    /** A file descriptor that references the error log */
    apr_file_t *error_log;
    /** The log level for this server */
    int loglevel;

    /* Module-specific configuration for server, and defaults... */

    /** true if this is the virtual server */
    int is_virtual;
    /** Config vector containing pointers to modules' per-server config 
     *  structures. */
    struct ap_conf_vector_t *module_config; 
    /** MIME type info, etc., before we start checking per-directory info */
    struct ap_conf_vector_t *lookup_defaults;

    /* Transaction handling */

    /** I haven't got a clue */
    server_addr_rec *addrs;
    /** Timeout, as an apr interval, before we give up */
    apr_interval_time_t timeout;
    /** The apr interval we will wait for another request */
    apr_interval_time_t keep_alive_timeout;
    /** Maximum requests per connection */
    int keep_alive_max;
    /** Use persistent connections? */
    int keep_alive;

    /** Pathname for ServerPath */
    const char *path;
    /** Length of path */
    int pathlen;

    /** Normal names for ServerAlias servers */
    apr_array_header_t *names;
    /** Wildcarded names for ServerAlias servers */
    apr_array_header_t *wild_names;

    /** limit on size of the HTTP request line    */
    int limit_req_line;
    /** limit on size of any request header field */
    int limit_req_fieldsize;
    /** limit on number of request header fields  */
    int limit_req_fields; 

    /** The server request scheme for redirect responses */
    const char *server_scheme;
};

typedef struct core_output_filter_ctx {
    apr_bucket_brigade *b;
    /** subpool of c->pool used for resources 
     * which may outlive the request
     */
    apr_pool_t *deferred_write_pool;
} core_output_filter_ctx_t;
 
typedef struct core_filter_ctx {
    apr_bucket_brigade *b;
    apr_bucket_brigade *tmpbb;
} core_ctx_t;
 
typedef struct core_net_rec {
    /** Connection to the client */
    apr_socket_t *client_socket;

    /** connection record */
    conn_rec *c;
 
    core_output_filter_ctx_t *out_ctx;
    core_ctx_t *in_ctx;
} core_net_rec;

/**
 * Examine a field value (such as a media-/content-type) string and return
 * it sans any parameters; e.g., strip off any ';charset=foo' and the like.
 * @param p Pool to allocate memory from
 * @param intype The field to examine
 * @return A copy of the field minus any parameters
 */
AP_DECLARE(char *) ap_field_noparam(apr_pool_t *p, const char *intype);

/**
 * Convert a time from an integer into a string in a specified format
 * @param p The pool to allocate memory from
 * @param t The time to convert
 * @param fmt The format to use for the conversion
 * @param gmt Convert the time for GMT?
 * @return The string that represents the specified time
 */
AP_DECLARE(char *) ap_ht_time(apr_pool_t *p, apr_time_t t, const char *fmt, int gmt);

/* String handling. The *_nc variants allow you to use non-const char **s as
   arguments (unfortunately C won't automatically convert a char ** to a const
   char **) */

/**
 * Get the characters until the first occurance of a specified character
 * @param p The pool to allocate memory from
 * @param line The string to get the characters from
 * @param stop The character to stop at
 * @return A copy of the characters up to the first stop character
 */
AP_DECLARE(char *) ap_getword(apr_pool_t *p, const char **line, char stop);

/**
 * Get the characters until the first occurance of a specified character
 * @param p The pool to allocate memory from
 * @param line The string to get the characters from
 * @param stop The character to stop at
 * @return A copy of the characters up to the first stop character
 * @note This is the same as ap_getword(), except it doesn't use const char **.
 */
AP_DECLARE(char *) ap_getword_nc(apr_pool_t *p, char **line, char stop);

/**
 * Get the first word from a given string.  A word is defined as all characters
 * up to the first whitespace.
 * @param p The pool to allocate memory from
 * @param line The string to traverse
 * @return The first word in the line
 */
AP_DECLARE(char *) ap_getword_white(apr_pool_t *p, const char **line);

/**
 * Get the first word from a given string.  A word is defined as all characters
 * up to the first whitespace.
 * @param p The pool to allocate memory from
 * @param line The string to traverse
 * @return The first word in the line
 * @note The same as ap_getword_white(), except it doesn't use const char**
 */
AP_DECLARE(char *) ap_getword_white_nc(apr_pool_t *p, char **line);

/**
 * Get all characters from the first occurance of @a stop to the first "\0"
 * @param p The pool to allocate memory from
 * @param line The line to traverse
 * @param stop The character to start at
 * @return A copy of all caracters after the first occurance of the specified
 *         character
 */
AP_DECLARE(char *) ap_getword_nulls(apr_pool_t *p, const char **line,
				    char stop);

/**
 * Get all characters from the first occurance of @a stop to the first "\0"
 * @param p The pool to allocate memory from
 * @param line The line to traverse
 * @param stop The character to start at
 * @return A copy of all caracters after the first occurance of the specified
 *         character
 * @note The same as ap_getword_nulls(), except it doesn't use const char **.
 */
AP_DECLARE(char *) ap_getword_nulls_nc(apr_pool_t *p, char **line, char stop);

/**
 * Get the second word in the string paying attention to quoting
 * @param p The pool to allocate from
 * @param line The line to traverse
 * @return A copy of the string
 */
AP_DECLARE(char *) ap_getword_conf(apr_pool_t *p, const char **line);

/**
 * Get the second word in the string paying attention to quoting
 * @param p The pool to allocate from
 * @param line The line to traverse
 * @return A copy of the string
 * @note The same as ap_getword_conf(), except it doesn't use const char **.
 */
AP_DECLARE(char *) ap_getword_conf_nc(apr_pool_t *p, char **line);

/**
 * Check a string for any ${ENV} environment variable construct and replace 
 * each them by the value of that environment variable, if it exists. If the 
 * environment value does not exist, leave the ${ENV} construct alone; it 
 * means something else.
 * @param p The pool to allocate from
 * @param word The string to check
 * @return The string with the replaced environment variables
 */
AP_DECLARE(const char *) ap_resolve_env(apr_pool_t *p, const char * word); 

/**
 * Size an HTTP header field list item, as separated by a comma.
 * @param field The field to size
 * @param len The length of the field
 * @return The return value is a pointer to the beginning of the non-empty 
 * list item within the original string (or NULL if there is none) and the 
 * address of field is shifted to the next non-comma, non-whitespace 
 * character.  len is the length of the item excluding any beginning whitespace.
 */
AP_DECLARE(const char *) ap_size_list_item(const char **field, int *len);

/**
 * Retrieve an HTTP header field list item, as separated by a comma,
 * while stripping insignificant whitespace and lowercasing anything not in
 * a quoted string or comment.  
 * @param p The pool to allocate from
 * @param field The field to retrieve
 * @return The return value is a new string containing the converted list 
 *         item (or NULL if none) and the address pointed to by field is 
 *         shifted to the next non-comma, non-whitespace.
 */
AP_DECLARE(char *) ap_get_list_item(apr_pool_t *p, const char **field);

/**
 * Find an item in canonical form (lowercase, no extra spaces) within
 * an HTTP field value list.  
 * @param p The pool to allocate from
 * @param line The field value list to search
 * @param tok The token to search for
 * @return 1 if found, 0 if not found.
 */
AP_DECLARE(int) ap_find_list_item(apr_pool_t *p, const char *line, const char *tok);

/**
 * Retrieve a token, spacing over it and adjusting the pointer to
 * the first non-white byte afterwards.  Note that these tokens
 * are delimited by semis and commas and can also be delimited
 * by whitespace at the caller's option.
 * @param p The pool to allocate from
 * @param accept_line The line to retrieve the token from (adjusted afterwards)
 * @param accept_white Is it delimited by whitespace
 * @return the token
 */
AP_DECLARE(char *) ap_get_token(apr_pool_t *p, const char **accept_line, int accept_white);

/**
 * Find http tokens, see the definition of token from RFC2068 
 * @param p The pool to allocate from
 * @param line The line to find the token
 * @param tok The token to find
 * @return 1 if the token is found, 0 otherwise
 */
AP_DECLARE(int) ap_find_token(apr_pool_t *p, const char *line, const char *tok);

/**
 * find http tokens from the end of the line
 * @param p The pool to allocate from
 * @param line The line to find the token
 * @param tok The token to find
 * @return 1 if the token is found, 0 otherwise
 */
AP_DECLARE(int) ap_find_last_token(apr_pool_t *p, const char *line, const char *tok);

/**
 * Check for an Absolute URI syntax
 * @param u The string to check
 * @return 1 if URI, 0 otherwise
 */
AP_DECLARE(int) ap_is_url(const char *u);

/**
 * Unescape a URL
 * @param url The url to unescape
 * @return 0 on success, non-zero otherwise
 */
AP_DECLARE(int) ap_unescape_url(char *url);

/**
 * Unescape a URL, but leaving %2f (slashes) escaped
 * @param url The url to unescape
 * @return 0 on success, non-zero otherwise
 */
AP_DECLARE(int) ap_unescape_url_keep2f(char *url);

/**
 * Convert all double slashes to single slashes
 * @param name The string to convert
 */
AP_DECLARE(void) ap_no2slash(char *name);

/**
 * Remove all ./ and xx/../ substrings from a file name. Also remove
 * any leading ../ or /../ substrings.
 * @param name the file name to parse
 */
AP_DECLARE(void) ap_getparents(char *name);

/**
 * Escape a path segment, as defined in RFC 1808
 * @param p The pool to allocate from
 * @param s The path to convert
 * @return The converted URL
 */
AP_DECLARE(char *) ap_escape_path_segment(apr_pool_t *p, const char *s);

/**
 * convert an OS path to a URL in an OS dependant way.
 * @param p The pool to allocate from
 * @param path The path to convert
 * @param partial if set, assume that the path will be appended to something
 *        with a '/' in it (and thus does not prefix "./")
 * @return The converted URL
 */
AP_DECLARE(char *) ap_os_escape_path(apr_pool_t *p, const char *path, int partial);

/** @see ap_os_escape_path */
#define ap_escape_uri(ppool,path) ap_os_escape_path(ppool,path,1)

/**
 * Escape an html string
 * @param p The pool to allocate from
 * @param s The html to escape
 * @return The escaped string
 */
AP_DECLARE(char *) ap_escape_html(apr_pool_t *p, const char *s);
/**
 * Escape an html string
 * @param p The pool to allocate from
 * @param s The html to escape
 * @param toasc Whether to escape all non-ASCII chars to &#nnn;
 * @return The escaped string
 */
AP_DECLARE(char *) ap_escape_html2(apr_pool_t *p, const char *s, int toasc);

/**
 * Escape a string for logging
 * @param p The pool to allocate from
 * @param str The string to escape
 * @return The escaped string
 */
AP_DECLARE(char *) ap_escape_logitem(apr_pool_t *p, const char *str);

/**
 * Escape a string for logging into the error log (without a pool)
 * @param dest The buffer to write to
 * @param source The string to escape
 * @param buflen The buffer size for the escaped string (including "\0")
 * @return The len of the escaped string (always < maxlen)
 */
AP_DECLARE(apr_size_t) ap_escape_errorlog_item(char *dest, const char *source,
                                               apr_size_t buflen);

/**
 * Construct a full hostname
 * @param p The pool to allocate from
 * @param hostname The hostname of the server
 * @param port The port the server is running on
 * @param r The current request
 * @return The server's hostname
 */
AP_DECLARE(char *) ap_construct_server(apr_pool_t *p, const char *hostname,
				    apr_port_t port, const request_rec *r);

/**
 * Escape a shell command
 * @param p The pool to allocate from
 * @param s The command to escape
 * @return The escaped shell command
 */
AP_DECLARE(char *) ap_escape_shell_cmd(apr_pool_t *p, const char *s);

/**
 * Count the number of directories in a path
 * @param path The path to count
 * @return The number of directories
 */
AP_DECLARE(int) ap_count_dirs(const char *path);

/**
 * Copy at most @a n leading directories of @a s into @a d. @a d
 * should be at least as large as @a s plus 1 extra byte
 *
 * @param d The location to copy to
 * @param s The location to copy from
 * @param n The number of directories to copy
 * @return value is the ever useful pointer to the trailing "\0" of d
 * @note on platforms with drive letters, n = 0 returns the "/" root, 
 * whereas n = 1 returns the "d:/" root.  On all other platforms, n = 0
 * returns the empty string.  */
AP_DECLARE(char *) ap_make_dirstr_prefix(char *d, const char *s, int n);

/**
 * Return the parent directory name (including trailing /) of the file
 * @a s
 * @param p The pool to allocate from
 * @param s The file to get the parent of
 * @return A copy of the file's parent directory
 */
AP_DECLARE(char *) ap_make_dirstr_parent(apr_pool_t *p, const char *s);

/**
 * Given a directory and filename, create a single path from them.  This
 * function is smart enough to ensure that there is a sinlge '/' between the
 * directory and file names
 * @param a The pool to allocate from
 * @param dir The directory name
 * @param f The filename
 * @return A copy of the full path
 * @note Never consider using this function if you are dealing with filesystem
 * names that need to remain canonical, unless you are merging an apr_dir_read
 * path and returned filename.  Otherwise, the result is not canonical.
 */
AP_DECLARE(char *) ap_make_full_path(apr_pool_t *a, const char *dir, const char *f);

/**
 * Test if the given path has an an absolute path.
 * @param p The pool to allocate from
 * @param dir The directory name
 * @note The converse is not necessarily true, some OS's (Win32/OS2/Netware) have
 * multiple forms of absolute paths.  This only reports if the path is absolute
 * in a canonical sense.
 */
AP_DECLARE(int) ap_os_is_path_absolute(apr_pool_t *p, const char *dir);

/**
 * Does the provided string contain wildcard characters?  This is useful
 * for determining if the string should be passed to strcmp_match or to strcmp.
 * The only wildcard characters recognized are '?' and '*'
 * @param str The string to check
 * @return 1 if the string has wildcards, 0 otherwise
 */
AP_DECLARE(int) ap_is_matchexp(const char *str);

/**
 * Determine if a string matches a patterm containing the wildcards '?' or '*'
 * @param str The string to check
 * @param expected The pattern to match against
 * @return 1 if the two strings match, 0 otherwise
 */
AP_DECLARE(int) ap_strcmp_match(const char *str, const char *expected);

/**
 * Determine if a string matches a patterm containing the wildcards '?' or '*',
 * ignoring case
 * @param str The string to check
 * @param expected The pattern to match against
 * @return 1 if the two strings match, 0 otherwise
 */
AP_DECLARE(int) ap_strcasecmp_match(const char *str, const char *expected);

/**
 * Find the first occurrence of the substring s2 in s1, regardless of case
 * @param s1 The string to search
 * @param s2 The substring to search for
 * @return A pointer to the beginning of the substring
 * @remark See apr_strmatch() for a faster alternative
 */
AP_DECLARE(char *) ap_strcasestr(const char *s1, const char *s2);

/**
 * Return a pointer to the location inside of bigstring immediately after prefix
 * @param bigstring The input string
 * @param prefix The prefix to strip away
 * @return A pointer relative to bigstring after prefix
 */
AP_DECLARE(const char *) ap_stripprefix(const char *bigstring,
                                        const char *prefix);

/**
 * Decode a base64 encoded string into memory allocated from a pool
 * @param p The pool to allocate from
 * @param bufcoded The encoded string
 * @return The decoded string
 */
AP_DECLARE(char *) ap_pbase64decode(apr_pool_t *p, const char *bufcoded);

/**
 * Encode a string into memory allocated from a pool in base 64 format
 * @param p The pool to allocate from
 * @param string The plaintext string
 * @return The encoded string
 */
AP_DECLARE(char *) ap_pbase64encode(apr_pool_t *p, char *string); 

/**
 * Compile a regular expression to be used later
 * @param p The pool to allocate from
 * @param pattern the regular expression to compile
 * @param cflags The bitwise or of one or more of the following:
 *   @li REG_EXTENDED - Use POSIX extended Regular Expressions
 *   @li REG_ICASE    - Ignore case
 *   @li REG_NOSUB    - Support for substring addressing of matches
 *       not required
 *   @li REG_NEWLINE  - Match-any-character operators don't match new-line
 * @return The compiled regular expression
 */
AP_DECLARE(ap_regex_t *) ap_pregcomp(apr_pool_t *p, const char *pattern,
                                     int cflags);

/**
 * Free the memory associated with a compiled regular expression
 * @param p The pool the regex was allocated from
 * @param reg The regular expression to free
 */
AP_DECLARE(void) ap_pregfree(apr_pool_t *p, ap_regex_t *reg);

/**
 * After performing a successful regex match, you may use this function to 
 * perform a series of string substitutions based on subexpressions that were
 * matched during the call to ap_regexec
 * @param p The pool to allocate from
 * @param input An arbitrary string containing $1 through $9.  These are 
 *              replaced with the corresponding matched sub-expressions
 * @param source The string that was originally matched to the regex
 * @param nmatch the nmatch returned from ap_pregex
 * @param pmatch the pmatch array returned from ap_pregex
 */
AP_DECLARE(char *) ap_pregsub(apr_pool_t *p, const char *input, const char *source,
                              size_t nmatch, ap_regmatch_t pmatch[]);

/**
 * We want to downcase the type/subtype for comparison purposes
 * but nothing else because ;parameter=foo values are case sensitive.
 * @param s The content-type to convert to lowercase
 */
AP_DECLARE(void) ap_content_type_tolower(char *s);

/**
 * convert a string to all lowercase
 * @param s The string to convert to lowercase 
 */
AP_DECLARE(void) ap_str_tolower(char *s);

/**
 * Search a string from left to right for the first occurrence of a 
 * specific character
 * @param str The string to search
 * @param c The character to search for
 * @return The index of the first occurrence of c in str
 */
AP_DECLARE(int) ap_ind(const char *str, char c);	/* Sigh... */

/**
 * Search a string from right to left for the first occurrence of a 
 * specific character
 * @param str The string to search
 * @param c The character to search for
 * @return The index of the first occurrence of c in str
 */
AP_DECLARE(int) ap_rind(const char *str, char c);

/**
 * Given a string, replace any bare " with \" .
 * @param p The pool to allocate memory from
 * @param instring The string to search for "
 * @return A copy of the string with escaped quotes 
 */
AP_DECLARE(char *) ap_escape_quotes(apr_pool_t *p, const char *instring);

/**
 * Given a string, append the PID deliminated by delim.
 * Usually used to create a pid-appended filepath name
 * (eg: /a/b/foo -> /a/b/foo.6726). A function, and not
 * a macro, to avoid unistd.h dependency
 * @param p The pool to allocate memory from
 * @param string The string to append the PID to
 * @param delim The string to use to deliminate the string from the PID
 * @return A copy of the string with the PID appended 
 */
AP_DECLARE(char *) ap_append_pid(apr_pool_t *p, const char *string,
                                 const char *delim);

/**
 * Parse a given timeout parameter string into an apr_interval_time_t value.
 * The unit of the time interval is given as postfix string to the numeric
 * string. Currently the following units are understood:
 *
 * ms    : milliseconds
 * s     : seconds
 * mi[n] : minutes
 * h     : hours
 *
 * If no unit is contained in the given timeout parameter the default_time_unit
 * will be used instead.
 * @param timeout_parameter The string containing the timeout parameter.
 * @param timeout The timeout value to be returned.
 * @param default_time_unit The default time unit to use if none is specified
 * in timeout_parameter.
 * @return Status value indicating whether the parsing was successful or not.
 */
AP_DECLARE(apr_status_t) ap_timeout_parameter_parse(
                                               const char *timeout_parameter,
                                               apr_interval_time_t *timeout,
                                               const char *default_time_unit);

/* Misc system hackery */
/**
 * Given the name of an object in the file system determine if it is a directory
 * @param p The pool to allocate from 
 * @param name The name of the object to check
 * @return 1 if it is a directory, 0 otherwise
 */
AP_DECLARE(int) ap_is_rdirectory(apr_pool_t *p, const char *name);

/**
 * Given the name of an object in the file system determine if it is a directory - this version is symlink aware
 * @param p The pool to allocate from 
 * @param name The name of the object to check
 * @return 1 if it is a directory, 0 otherwise
 */
AP_DECLARE(int) ap_is_directory(apr_pool_t *p, const char *name);

#ifdef _OSD_POSIX
extern int os_init_job_environment(server_rec *s, const char *user_name, int one_process);
#endif /* _OSD_POSIX */

/**
 * Determine the local host name for the current machine
 * @param p The pool to allocate from
 * @return A copy of the local host name
 */
char *ap_get_local_host(apr_pool_t *p);

/**
 * Log an assertion to the error log
 * @param szExp The assertion that failed
 * @param szFile The file the assertion is in
 * @param nLine The line the assertion is defined on
 */
AP_DECLARE(void) ap_log_assert(const char *szExp, const char *szFile, int nLine)
			    __attribute__((noreturn));

/** 
 * @internal Internal Assert function
 */
#define ap_assert(exp) ((exp) ? (void)0 : ap_log_assert(#exp,__FILE__,__LINE__))

/**
 * Redefine assert() to something more useful for an Apache...
 *
 * Use ap_assert() if the condition should always be checked.
 * Use AP_DEBUG_ASSERT() if the condition should only be checked when AP_DEBUG
 * is defined.
 */
#ifdef AP_DEBUG
#define AP_DEBUG_ASSERT(exp) ap_assert(exp)
#else
#define AP_DEBUG_ASSERT(exp) ((void)0)
#endif

/**
 * @defgroup stopsignal Flags which indicate places where the sever should stop for debugging.
 * @{
 * A set of flags which indicate places where the server should raise(SIGSTOP).
 * This is useful for debugging, because you can then attach to that process
 * with gdb and continue.  This is important in cases where one_process
 * debugging isn't possible.
 */
/** stop on a Detach */
#define SIGSTOP_DETACH			1
/** stop making a child process */
#define SIGSTOP_MAKE_CHILD		2
/** stop spawning a child process */
#define SIGSTOP_SPAWN_CHILD		4
/** stop spawning a child process with a piped log */
#define SIGSTOP_PIPED_LOG_SPAWN		8
/** stop spawning a CGI child process */
#define SIGSTOP_CGI_CHILD		16

/** Macro to get GDB started */
#ifdef DEBUG_SIGSTOP
extern int raise_sigstop_flags;
#define RAISE_SIGSTOP(x)	do { \
	if (raise_sigstop_flags & SIGSTOP_##x) raise(SIGSTOP);\
    } while (0)
#else
#define RAISE_SIGSTOP(x)
#endif
/** @} */
/**
 * Get HTML describing the address and (optionally) admin of the server.
 * @param prefix Text which is prepended to the return value
 * @param r The request_rec
 * @return HTML describing the server, allocated in @a r's pool.
 */
AP_DECLARE(const char *) ap_psignature(const char *prefix, request_rec *r);

/** strtoul does not exist on sunos4. */
#ifdef strtoul
#undef strtoul
#endif
#define strtoul strtoul_is_not_a_portable_function_use_strtol_instead

  /* The C library has functions that allow const to be silently dropped ...
     these macros detect the drop in maintainer mode, but use the native
     methods for normal builds

     Note that on some platforms (e.g., AIX with gcc, Solaris with gcc), string.h needs 
     to be included before the macros are defined or compilation will fail.
  */
#include <string.h>

AP_DECLARE(char *) ap_strchr(char *s, int c);
AP_DECLARE(const char *) ap_strchr_c(const char *s, int c);
AP_DECLARE(char *) ap_strrchr(char *s, int c);
AP_DECLARE(const char *) ap_strrchr_c(const char *s, int c);
AP_DECLARE(char *) ap_strstr(char *s, const char *c);
AP_DECLARE(const char *) ap_strstr_c(const char *s, const char *c);

#ifdef AP_DEBUG

#undef strchr
# define strchr(s, c)	ap_strchr(s,c)
#undef strrchr
# define strrchr(s, c)  ap_strrchr(s,c)
#undef strstr
# define strstr(s, c)  ap_strstr(s,c)

#else

/** use this instead of strchr */
# define ap_strchr(s, c)	strchr(s, c)
/** use this instead of strchr */
# define ap_strchr_c(s, c)	strchr(s, c)
/** use this instead of strrchr */
# define ap_strrchr(s, c)	strrchr(s, c)
/** use this instead of strrchr */
# define ap_strrchr_c(s, c)	strrchr(s, c)
/** use this instead of strrstr*/
# define ap_strstr(s, c)	strstr(s, c)
/** use this instead of strrstr*/
# define ap_strstr_c(s, c)	strstr(s, c)

#endif

#define AP_NORESTART		APR_OS_START_USEERR + 1

#ifdef __cplusplus
}
#endif

#endif	/* !APACHE_HTTPD_H */

/** @} //APACHE Daemon      */
/** @} //APACHE Core        */
/** @} //APACHE super group */

