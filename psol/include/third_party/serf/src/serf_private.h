/* Copyright 2002-2004 Justin Erenkrantz and Greg Stein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SERF_PRIVATE_H_
#define _SERF_PRIVATE_H_

/* ### what the hell? why does the APR interface have a "size" ??
   ### the implication is that, if we bust this limit, we'd need to
   ### stop, rebuild a pollset, and repopulate it. what suckage.  */
#define MAX_CONN 16

/* Windows does not define IOV_MAX, so we need to ensure it is defined. */
#ifndef IOV_MAX
#define IOV_MAX 16
#endif

#define SERF_IO_CLIENT (1)
#define SERF_IO_CONN (2)
#define SERF_IO_LISTENER (3)

typedef struct serf__authn_scheme_t serf__authn_scheme_t;

typedef struct serf_io_baton_t {
    int type;
    union {
        serf_incoming_t *client;
        serf_connection_t *conn;
        serf_listener_t *listener;
    } u;
} serf_io_baton_t;

/* Holds all the information corresponding to a request/response pair. */
struct serf_request_t {
    serf_connection_t *conn;

    apr_pool_t *respool;
    serf_bucket_alloc_t *allocator;

    /* The bucket corresponding to the request. Will be NULL once the
     * bucket has been emptied (for delivery into the socket).
     */
    serf_bucket_t *req_bkt;

    serf_request_setup_t setup;
    void *setup_baton;

    serf_response_acceptor_t acceptor;
    void *acceptor_baton;

    serf_response_handler_t handler;
    void *handler_baton;

    serf_bucket_t *resp_bkt;

    int written;
    int priority;

    struct serf_request_t *next;
};

typedef struct serf_pollset_t {
    /* the set of connections to poll */
    apr_pollset_t *pollset;
} serf_pollset_t;

typedef struct serf__authn_info_t {
    const char *realm;

    const serf__authn_scheme_t *scheme;

    void *baton;
} serf__authn_info_t;

struct serf_context_t {
    /* the pool used for self and for other allocations */
    apr_pool_t *pool;

    void *pollset_baton;
    serf_socket_add_t pollset_add;
    serf_socket_remove_t pollset_rm;

    /* one of our connections has a dirty pollset state. */
    int dirty_pollset;

    /* the list of active connections */
    apr_array_header_t *conns;
#define GET_CONN(ctx, i) (((serf_connection_t **)(ctx)->conns->elts)[i])

    /* Proxy server address */
    apr_sockaddr_t *proxy_address;

    /* Progress callback */
    serf_progress_t progress_func;
    void *progress_baton;
    apr_off_t progress_read;
    apr_off_t progress_written;

    /* authentication info for this context, shared by all connections. */
    serf__authn_info_t authn_info;
    serf__authn_info_t proxy_authn_info;

    /* List of authn types supported by the client.*/
    int authn_types;
    /* Callback function used to get credentials for a realm. */
    serf_credentials_callback_t cred_cb; 
};

struct serf_listener_t {
    serf_context_t *ctx;
    serf_io_baton_t baton;
    apr_socket_t *skt;
    apr_pool_t *pool;
    apr_pollfd_t desc;
    void *accept_baton;
    serf_accept_client_t accept_func;
};

struct serf_incoming_t {
    serf_context_t *ctx;
    serf_io_baton_t baton;
    void *request_baton;
    serf_incoming_request_cb_t request;
    apr_socket_t *skt;
    apr_pollfd_t desc;
};

/* States for the different stages in the lifecyle of a connection. */
typedef enum {
    SERF_CONN_INIT,             /* no socket created yet */
    SERF_CONN_SETUP_SSLTUNNEL,  /* ssl tunnel being setup, no requests sent */
    SERF_CONN_CONNECTED,        /* conn is ready to send requests */
    SERF_CONN_CLOSING,          /* conn is closing, no more requests,
                                   start a new socket */
} serf__connection_state_t;

struct serf_connection_t {
    serf_context_t *ctx;

    apr_status_t status;
    serf_io_baton_t baton;

    apr_pool_t *pool;
    serf_bucket_alloc_t *allocator;

    apr_sockaddr_t *address;

    apr_socket_t *skt;
    apr_pool_t *skt_pool;

    /* the last reqevents we gave to pollset_add */
    apr_int16_t reqevents;

    /* the events we've seen for this connection in our returned pollset */
    apr_int16_t seen_in_pollset;

    /* are we a dirty connection that needs its poll status updated? */
    int dirty_conn;

    /* number of completed requests we've sent */
    unsigned int completed_requests;

    /* number of completed responses we've got */
    unsigned int completed_responses;

    /* keepalive */
    unsigned int probable_keepalive_limit;

    /* Current state of the connection (whether or not it is connected). */
    serf__connection_state_t state;

    /* This connection may have responses without a request! */
    int async_responses;
    serf_bucket_t *current_async_response;
    serf_response_acceptor_t async_acceptor;
    void *async_acceptor_baton;
    serf_response_handler_t async_handler;
    void *async_handler_baton;

    /* A bucket wrapped around our socket (for reading responses). */
    serf_bucket_t *stream;
    /* A reference to the aggregate bucket that provides the boundary between
     * request level buckets and connection level buckets.
     */
    serf_bucket_t *ostream_head;
    serf_bucket_t *ostream_tail;

    /* Aggregate bucket used to send the CONNECT request. */
    serf_bucket_t *ssltunnel_ostream;

    /* The list of active requests. */
    serf_request_t *requests;
    serf_request_t *requests_tail;

    /* The list of requests we're holding on to because we're going to
     * reset the connection soon.
     */
    serf_request_t *hold_requests;
    serf_request_t *hold_requests_tail;

    struct iovec vec[IOV_MAX];
    int vec_len;

    serf_connection_setup_t setup;
    void *setup_baton;
    serf_connection_closed_t closed;
    void *closed_baton;

    /* Max. number of outstanding requests. */
    unsigned int max_outstanding_requests;

    int hit_eof;
    /* Host info. */
    const char *host_url;
    apr_uri_t host_info;

    /* connection and authentication scheme specific information */ 
    void *authn_baton;
    void *proxy_authn_baton;
};

/*** Authentication handler declarations ***/

/**
 * For each authentication scheme we need a handler function of type
 * serf__auth_handler_func_t. This function will be called when an
 * authentication challenge is received in a session.
 */
typedef apr_status_t
(*serf__auth_handler_func_t)(int code,
                             serf_request_t *request,
                             serf_bucket_t *response,
                             const char *auth_hdr,
                             const char *auth_attr,
                             void *baton,
                             apr_pool_t *pool);

/**
 * For each authentication scheme we need an initialization function of type
 * serf__init_context_func_t. This function will be called the first time
 * serf tries a specific authentication scheme handler.
 */
typedef apr_status_t
(*serf__init_context_func_t)(int code,
                             serf_context_t *conn,
                             apr_pool_t *pool);

/**
 * For each authentication scheme we need an initialization function of type
 * serf__init_conn_func_t. This function will be called when a new
 * connection is opened.
 */
typedef apr_status_t
(*serf__init_conn_func_t)(int code,
                          serf_connection_t *conn,
                          apr_pool_t *pool);

/**
 * For each authentication scheme we need a setup_request function of type
 * serf__setup_request_func_t. This function will be called when a
 * new serf_request_t object is created and should fill in the correct
 * authentication headers (if needed).
 */
typedef apr_status_t
(*serf__setup_request_func_t)(int code,
                              serf_connection_t *conn,
                              const char *method,
                              const char *uri,
                              serf_bucket_t *hdrs_bkt);

/**
 * This function will be called when a response is received, so that the 
 * scheme handler can validate the Authentication related response headers
 * (if needed).
 */
typedef apr_status_t
(*serf__validate_response_func_t)(int code,
                                  serf_connection_t *conn,
                                  serf_request_t *request,
                                  serf_bucket_t *response,
                                  apr_pool_t *pool);

/**
 * serf__authn_scheme_t: vtable for an authn scheme provider.
 */
struct serf__authn_scheme_t {
    /* The http status code that's handled by this authentication scheme.
       Normal values are 401 for server authentication and 407 for proxy
       authentication */
    int code;

    /* The name of this authentication scheme. This should be a case
       sensitive match of the string sent in the HTTP authentication header. */
    const char *name;

    /* Internal code used for this authn type. */
    int type;

    /* The context initialization function if any; otherwise, NULL */
    serf__init_context_func_t init_ctx_func;

    /* The connection initialization function if any; otherwise, NULL */
    serf__init_conn_func_t init_conn_func;

    /* The authentication handler function */
    serf__auth_handler_func_t handle_func;

    /* Function to set up the authentication header of a request */
    serf__setup_request_func_t setup_request_func;

    /* Function to validate the authentication header of a response */
    serf__validate_response_func_t validate_response_func;
};

/**
 * Handles a 401 or 407 response, tries the different available authentication
 * handlers.
 */
apr_status_t serf__handle_auth_response(int *consumed_response,
                                        serf_request_t *request,
                                        serf_bucket_t *response,
                                        void *baton,
                                        apr_pool_t *pool);

/* fromt context.c */
void serf__context_progress_delta(void *progress_baton, apr_off_t read,
                                  apr_off_t written);

/* from incoming.c */
apr_status_t serf__process_client(serf_incoming_t *l, apr_int16_t events);
apr_status_t serf__process_listener(serf_listener_t *l);

/* from outgoing.c */
apr_status_t serf__open_connections(serf_context_t *ctx);
apr_status_t serf__process_connection(serf_connection_t *conn,
                                       apr_int16_t events);
apr_status_t serf__conn_update_pollset(serf_connection_t *conn);

/* from ssltunnel.c */
apr_status_t serf__ssltunnel_connect(serf_connection_t *conn);

#endif
