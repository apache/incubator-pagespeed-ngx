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

#include <apr_pools.h>
#include <apr_poll.h>
#include <apr_version.h>

#include "serf.h"
#include "serf_bucket_util.h"


/* ### what the hell? why does the APR interface have a "size" ??
   ### the implication is that, if we bust this limit, we'd need to
   ### stop, rebuild a pollset, and repopulate it. what suckage.  */
#define MAX_CONN 16

/* Windows does not define IOV_MAX, so we need to ensure it is defined. */
#ifndef IOV_MAX
#define IOV_MAX 16
#endif

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

    struct serf_request_t *next;
};

typedef struct serf_pollset_t {
    /* the set of connections to poll */
    apr_pollset_t *pollset;
} serf_pollset_t;

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
};

struct serf_connection_t {
    serf_context_t *ctx;

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

    /* someone has told us that the connection is closing
     * so, let's start a new socket.
     */
    int closing;

    /* A bucket wrapped around our socket (for reading responses). */
    serf_bucket_t *stream;

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

    /* Host info. */
    const char *host_url;
    apr_uri_t host_info;
};

/* cleanup for sockets */
static apr_status_t clean_skt(void *data)
{
    serf_connection_t *conn = data;
    apr_status_t status = APR_SUCCESS;

    if (conn->skt) {
        status = apr_socket_close(conn->skt);
        conn->skt = NULL;
    }

    return status;
}

static apr_status_t clean_resp(void *data)
{
    serf_request_t *req = data;

    /* This pool just got cleared/destroyed. Don't try to destroy the pool
     * (again) when the request is canceled.
     */
    req->respool = NULL;

    return APR_SUCCESS;
}

/* cleanup for conns */
static apr_status_t clean_conn(void *data)
{
    serf_connection_t *conn = data;

    serf_connection_close(conn);

    return APR_SUCCESS;
}

/* Update the pollset for this connection. We tweak the pollset based on
 * whether we want to read and/or write, given conditions within the
 * connection. If the connection is not (yet) in the pollset, then it
 * will be added.
 */
static apr_status_t update_pollset(serf_connection_t *conn)
{
    serf_context_t *ctx = conn->ctx;
    apr_status_t status;
    apr_pollfd_t desc = { 0 };

    if (!conn->skt) {
        return APR_SUCCESS;
    }

    /* Remove the socket from the poll set. */
    desc.desc_type = APR_POLL_SOCKET;
    desc.desc.s = conn->skt;
    desc.reqevents = conn->reqevents;

    status = ctx->pollset_rm(ctx->pollset_baton,
                              &desc, conn);
    if (status && !APR_STATUS_IS_NOTFOUND(status))
        return status;

    /* Now put it back in with the correct read/write values. */
    desc.reqevents = APR_POLLHUP | APR_POLLERR;
    if (conn->requests) {
        /* If there are any outstanding events, then we want to read. */
        /* ### not true. we only want to read IF we have sent some data */
        desc.reqevents |= APR_POLLIN;

        /* If the connection has unwritten data, or there are any requests
         * that still have buckets to write out, then we want to write.
         */
        if (conn->vec_len)
            desc.reqevents |= APR_POLLOUT;
        else {
            serf_request_t *request = conn->requests;

            if ((conn->probable_keepalive_limit &&
                 conn->completed_requests > conn->probable_keepalive_limit) ||
                (conn->max_outstanding_requests &&
                 conn->completed_requests - conn->completed_responses >=
                     conn->max_outstanding_requests)) {
                /* we wouldn't try to write any way right now. */
            }
            else {
                while (request != NULL && request->req_bkt == NULL &&
                       request->setup == NULL)
                    request = request->next;
                if (request != NULL)
                    desc.reqevents |= APR_POLLOUT;
            }
        }
    }

    /* save our reqevents, so we can pass it in to remove later. */
    conn->reqevents = desc.reqevents;

    /* Note: even if we don't want to read/write this socket, we still
     * want to poll it for hangups and errors.
     */
    return ctx->pollset_add(ctx->pollset_baton,
                            &desc, conn);
}

#ifdef SERF_DEBUG_BUCKET_USE

/* Make sure all response buckets were drained. */
static void check_buckets_drained(serf_connection_t *conn)
{
    serf_request_t *request = conn->requests;

    for ( ; request ; request = request->next ) {
        if (request->resp_bkt != NULL) {
            /* ### crap. can't do this. this allocator may have un-drained
             * ### REQUEST buckets.
             */
            /* serf_debug__entered_loop(request->resp_bkt->allocator); */
            /* ### for now, pretend we closed the conn (resets the tracking) */
            serf_debug__closed_conn(request->resp_bkt->allocator);
        }
    }
}

#endif

/* Create and connect sockets for any connections which don't have them
 * yet. This is the core of our lazy-connect behavior.
 */
static apr_status_t open_connections(serf_context_t *ctx)
{
    int i;

    for (i = ctx->conns->nelts; i--; ) {
        serf_connection_t *conn = GET_CONN(ctx, i);
        apr_status_t status;
        apr_socket_t *skt;
        apr_sockaddr_t *serv_addr;

        conn->seen_in_pollset = 0;

        if (conn->skt != NULL) {
#ifdef SERF_DEBUG_BUCKET_USE
            check_buckets_drained(conn);
#endif
            continue;
        }

        /* Delay opening until we have something to deliver! */
        if (conn->requests == NULL) {
            continue;
        }

        apr_pool_clear(conn->skt_pool);
        apr_pool_cleanup_register(conn->skt_pool, conn, clean_skt, clean_skt);

        /* Do we have to connect to a proxy server? */
        if (ctx->proxy_address)
            serv_addr = ctx->proxy_address;
        else
            serv_addr = conn->address;

        if ((status = apr_socket_create(&skt, serv_addr->family,
                                        SOCK_STREAM,
#if APR_MAJOR_VERSION > 0
                                        APR_PROTO_TCP,
#endif
                                        conn->skt_pool)) != APR_SUCCESS)
            return status;

        /* Set the socket to be non-blocking */
        if ((status = apr_socket_timeout_set(skt, 0)) != APR_SUCCESS)
            return status;

        /* Disable Nagle's algorithm */
        if ((status = apr_socket_opt_set(skt,
                                         APR_TCP_NODELAY, 0)) != APR_SUCCESS)
            return status;

        /* Configured. Store it into the connection now. */
        conn->skt = skt;

        /* Now that the socket is set up, let's connect it. This should
         * return immediately.
         */
        if ((status = apr_socket_connect(skt,
                                         serv_addr)) != APR_SUCCESS) {
            if (!APR_STATUS_IS_EINPROGRESS(status))
                return status;
        }

        /* Flag our pollset as dirty now that we have a new socket. */
        conn->dirty_conn = 1;
        ctx->dirty_pollset = 1;
    }

    return APR_SUCCESS;
}

static apr_status_t no_more_writes(serf_connection_t *conn,
                                   serf_request_t *request)
{
  /* Note that we should hold new requests until we open our new socket. */
  conn->closing = 1;

  /* We can take the *next* request in our list and assume it hasn't
   * been written yet and 'save' it for the new socket.
   */
  conn->hold_requests = request->next;
  conn->hold_requests_tail = conn->requests_tail;
  request->next = NULL;
  conn->requests_tail = request;

  /* Clear our iovec. */
  conn->vec_len = 0;

  /* Update the pollset to know we don't want to write on this socket any
   * more.
   */
  conn->dirty_conn = 1;
  conn->ctx->dirty_pollset = 1;
  return APR_SUCCESS;
}

/* Read the 'Connection' header from the response. Return SERF_ERROR_CLOSING if
 * the header contains value 'close' indicating the server is closing the
 * connection right after this response.
 * Otherwise returns APR_SUCCESS.
 */
static apr_status_t is_conn_closing(serf_bucket_t *response)
{
  serf_bucket_t *hdrs;
  const char *val;

  hdrs = serf_bucket_response_get_headers(response);
  val = serf_bucket_headers_get(hdrs, "Connection");
  if (val && strcasecmp("close", val) == 0)
    {
      return SERF_ERROR_CLOSING;
    }

  return APR_SUCCESS;
}

static void link_requests(serf_request_t **list, serf_request_t **tail,
                          serf_request_t *request)
{
    if (*list == NULL) {
        *list = request;
        *tail = request;
    }
    else {
        (*tail)->next = request;
        *tail = request;
    }
}

static apr_status_t cancel_request(serf_request_t *request,
                                   serf_request_t **list,
                                   int notify_request)
{
    /* If we haven't run setup, then we won't have a handler to call. */
    if (request->handler && notify_request) {
        /* We actually don't care what the handler returns.
         * We have bigger matters at hand.
         */
        (*request->handler)(request, NULL, request->handler_baton,
                            request->respool);
    }

    if (*list == request) {
        *list = request->next;
    }
    else {
        serf_request_t *scan = *list;

        while (scan->next && scan->next != request)
            scan = scan->next;

        if (scan->next) {
            scan->next = scan->next->next;
        }
    }

    if (request->resp_bkt) {
        serf_debug__closed_conn(request->resp_bkt->allocator);
        serf_bucket_destroy(request->resp_bkt);
    }
    if (request->req_bkt) {
        serf_debug__closed_conn(request->req_bkt->allocator);
        serf_bucket_destroy(request->req_bkt);
    }

    if (request->respool) {
        apr_pool_destroy(request->respool);
    }

    serf_bucket_mem_free(request->conn->allocator, request);

    return APR_SUCCESS;
}

static apr_status_t remove_connection(serf_context_t *ctx,
                                      serf_connection_t *conn)
{
    apr_pollfd_t desc = { 0 };

    desc.desc_type = APR_POLL_SOCKET;
    desc.desc.s = conn->skt;
    desc.reqevents = conn->reqevents;

    return ctx->pollset_rm(ctx->pollset_baton,
                           &desc, conn);
}

static apr_status_t reset_connection(serf_connection_t *conn,
                                     int requeue_requests)
{
    serf_context_t *ctx = conn->ctx;
    apr_status_t status;
    serf_request_t *old_reqs, *held_reqs, *held_reqs_tail;

    conn->probable_keepalive_limit = conn->completed_responses;
    conn->completed_requests = 0;
    conn->completed_responses = 0;

    old_reqs = conn->requests;
    held_reqs = conn->hold_requests;
    held_reqs_tail = conn->hold_requests_tail;

    if (conn->closing) {
        conn->hold_requests = NULL;
        conn->hold_requests_tail = NULL;
        conn->closing = 0;
    }

    conn->requests = NULL;
    conn->requests_tail = NULL;

    while (old_reqs) {
        /* If we haven't started to write the connection, bring it over
         * unchanged to our new socket.  Otherwise, call the cancel function.
         */
        if (requeue_requests && old_reqs->setup) {
            serf_request_t *req = old_reqs;
            old_reqs = old_reqs->next;
            req->next = NULL;
            link_requests(&conn->requests, &conn->requests_tail, req);
        }
        else {
            cancel_request(old_reqs, &old_reqs, requeue_requests);
        }
    }

    if (conn->requests_tail) {
        conn->requests_tail->next = held_reqs;
    }
    else {
        conn->requests = held_reqs;
    }
    if (held_reqs_tail) {
        conn->requests_tail = held_reqs_tail;
    }

    if (conn->skt != NULL) {
        remove_connection(ctx, conn);
        status = apr_socket_close(conn->skt);
        if (conn->closed != NULL) {
            (*conn->closed)(conn, conn->closed_baton, status,
                            conn->pool);
        }
        conn->skt = NULL;
    }

    if (conn->stream != NULL) {
        serf_bucket_destroy(conn->stream);
        conn->stream = NULL;
    }

    /* Don't try to resume any writes */
    conn->vec_len = 0;

    conn->dirty_conn = 1;
    conn->ctx->dirty_pollset = 1;

    /* Let our context know that we've 'reset' the socket already. */
    conn->seen_in_pollset |= APR_POLLHUP;

    /* Found the connection. Closed it. All done. */
    return APR_SUCCESS;
}

/**
 * Callback function (implements serf_progress_t). Takes a number of bytes
 * read @a read and bytes written @a written, adds those to the total for this
 * context and notifies an interested party (if any).
 */
static void serf_context_progress_delta(
    void *progress_baton,
    apr_off_t read,
    apr_off_t written)
{
    serf_context_t *ctx = progress_baton;

    ctx->progress_read += read;
    ctx->progress_written += written;

    if (ctx->progress_func)
        ctx->progress_func(ctx->progress_baton,
                           ctx->progress_read,
                           ctx->progress_written);
}

static apr_status_t socket_writev(serf_connection_t *conn)
{
    apr_size_t written;
    apr_status_t status;

    status = apr_socket_sendv(conn->skt, conn->vec,
                              conn->vec_len, &written);

    /* did we write everything? */
    if (written) {
        apr_size_t len = 0;
        int i;

        for (i = 0; i < conn->vec_len; i++) {
            len += conn->vec[i].iov_len;
            if (written < len) {
                if (i) {
                    memmove(conn->vec, &conn->vec[i],
                            sizeof(struct iovec) * (conn->vec_len - i));
                    conn->vec_len -= i;
                }
                conn->vec[0].iov_base += conn->vec[0].iov_len - (len - written);
                conn->vec[0].iov_len = len - written;
                break;
            }
        }
        if (len == written) {
            conn->vec_len = 0;
        }

        /* Log progress information */
        serf_context_progress_delta(conn->ctx, 0, written);
    }

    return status;
}

/* write data out to the connection */
static apr_status_t write_to_connection(serf_connection_t *conn)
{
    serf_request_t *request = conn->requests;

    if (conn->probable_keepalive_limit &&
        conn->completed_requests > conn->probable_keepalive_limit) {
        /* backoff for now. */
        return APR_SUCCESS;
    }

    /* Find a request that has data which needs to be delivered. */
    while (request != NULL &&
           request->req_bkt == NULL && request->setup == NULL)
        request = request->next;

    /* assert: request != NULL || conn->vec_len */

    /* Keep reading and sending until we run out of stuff to read, or
     * writing would block.
     */
    while (1) {
        int stop_reading = 0;
        apr_status_t status;
        apr_status_t read_status;

        if (conn->max_outstanding_requests &&
            conn->completed_requests -
                conn->completed_responses >= conn->max_outstanding_requests) {
            /* backoff for now. */
            return APR_SUCCESS;
        }

        /* If we have unwritten data, then write what we can. */
        while (conn->vec_len) {
            status = socket_writev(conn);

            /* If the write would have blocked, then we're done. Don't try
             * to write anything else to the socket.
             */
            if (APR_STATUS_IS_EAGAIN(status))
                return APR_SUCCESS;
            if (APR_STATUS_IS_EPIPE(status))
                return no_more_writes(conn, request);
            if (status)
                return status;
        }
        /* ### can we have a short write, yet no EAGAIN? a short write
           ### would imply unwritten_len > 0 ... */
        /* assert: unwritten_len == 0. */

        /* We may need to move forward to a request which has something
         * to write.
         */
        while (request != NULL &&
               request->req_bkt == NULL && request->setup == NULL)
            request = request->next;

        if (request == NULL) {
            /* No more requests (with data) are registered with the
             * connection. Let's update the pollset so that we don't
             * try to write to this socket again.
             */
            conn->dirty_conn = 1;
            conn->ctx->dirty_pollset = 1;
            return APR_SUCCESS;
        }

        /* If the connection does not have an associated bucket, then
         * call the setup callback to get one.
         */
        if (conn->stream == NULL) {
            conn->stream = (*conn->setup)(conn->skt,
                                          conn->setup_baton,
                                          conn->pool);
        }

        if (request->req_bkt == NULL) {
            /* Now that we are about to serve the request, allocate a pool. */
            apr_pool_create(&request->respool, conn->pool);
            request->allocator = serf_bucket_allocator_create(request->respool,
                                                              NULL, NULL);
            apr_pool_cleanup_register(request->respool, request,
                                      clean_resp, clean_resp);

            /* Fill in the rest of the values for the request. */
            read_status = request->setup(request, request->setup_baton,
                                         &request->req_bkt,
                                         &request->acceptor,
                                         &request->acceptor_baton,
                                         &request->handler,
                                         &request->handler_baton,
                                         request->respool);

            if (read_status) {
                /* Something bad happened. Propagate any errors. */
                return read_status;
            }

            request->setup = NULL;
        }

        /* ### optimize at some point by using read_for_sendfile */
        read_status = serf_bucket_read_iovec(request->req_bkt,
                                             SERF_READ_ALL_AVAIL,
                                             IOV_MAX,
                                             conn->vec,
                                             &conn->vec_len);

        if (APR_STATUS_IS_EAGAIN(read_status)) {
            /* We read some stuff, but should not try to read again. */
            stop_reading = 1;

            /* ### we should avoid looking for writability for a while so
               ### that (hopefully) something will appear in the bucket so
               ### we can actually write something. otherwise, we could
               ### end up in a CPU spin: socket wants something, but we
               ### don't have anything (and keep returning EAGAIN)
            */
        }
        else if (read_status && !APR_STATUS_IS_EOF(read_status)) {
            /* Something bad happened. Propagate any errors. */
            return read_status;
        }

        /* If we got some data, then deliver it. */
        /* ### what to do if we got no data?? is that a problem? */
        if (conn->vec_len > 0) {
            status = socket_writev(conn);

            /* If we can't write any more, or an error occurred, then
             * we're done here.
             */
            if (APR_STATUS_IS_EAGAIN(status))
                return APR_SUCCESS;
            if (APR_STATUS_IS_EPIPE(status))
                return no_more_writes(conn, request);
            if (APR_STATUS_IS_ECONNRESET(status)) {
                return no_more_writes(conn, request);
            }
            if (status)
                return status;
        }

        if (APR_STATUS_IS_EOF(read_status) &&
            conn->vec_len == 0) {
            /* If we hit the end of the request bucket and all of its data has
             * been written, then clear it out to signify that we're done
             * sending the request. On the next iteration through this loop:
             * - if there are remaining bytes they will be written, and as the
             * request bucket will be completely read it will be destroyed then.
             * - we'll see if there are other requests that need to be sent
             * ("pipelining").
             */
            serf_bucket_destroy(request->req_bkt);
            request->req_bkt = NULL;

            conn->completed_requests++;

            if (conn->probable_keepalive_limit &&
                conn->completed_requests > conn->probable_keepalive_limit) {
                /* backoff for now. */
                stop_reading = 1;
            }
        }

        if (stop_reading) {
            return APR_SUCCESS;
        }
    }
    /* NOTREACHED */
}

/* read data from the connection */
static apr_status_t read_from_connection(serf_connection_t *conn)
{
    apr_status_t status;
    apr_pool_t *tmppool;
    int close_connection = FALSE;

    /* Whatever is coming in on the socket corresponds to the first request
     * on our chain.
     */
    serf_request_t *request = conn->requests;

    /* assert: request != NULL */

    if ((status = apr_pool_create(&tmppool, conn->pool)) != APR_SUCCESS)
        goto error;

    /* Invoke response handlers until we have no more work. */
    while (1) {
        apr_pool_clear(tmppool);

        /* If the connection does not have an associated bucket, then
         * call the setup callback to get one.
         */
        if (conn->stream == NULL) {
            conn->stream = (*conn->setup)(conn->skt,
                                          conn->setup_baton,
                                          conn->pool);
        }

        /* We are reading a response for a request we haven't
         * written yet!
         *
         * This shouldn't normally happen EXCEPT:
         *
         * 1) when the other end has closed the socket and we're
         *    pending an EOF return.
         * 2) Doing the initial SSL handshake - we'll get EAGAIN
         *    as the SSL buckets will hide the handshake from us
         *    but not return any data.
         *
         * In these cases, we should not receive any actual user data.
         *
         * If we see an EOF (due to an expired timeout), we'll reset the
         * connection and open a new one.
         */
        if (request->req_bkt || request->setup) {
            const char *data;
            apr_size_t len;

            status = serf_bucket_read(conn->stream, SERF_READ_ALL_AVAIL,
                                      &data, &len);

            if (!status && len) {
                status = APR_EGENERAL;
            }
            else if (APR_STATUS_IS_EOF(status)) {
                reset_connection(conn, 1);
                status = APR_SUCCESS;
            }
            else if (APR_STATUS_IS_EAGAIN(status)) {
                status = APR_SUCCESS;
            }

            goto error;
        }

        /* If the request doesn't have a response bucket, then call the
         * acceptor to get one created.
         */
        if (request->resp_bkt == NULL) {
            request->resp_bkt = (*request->acceptor)(request, conn->stream,
                                                     request->acceptor_baton,
                                                     tmppool);
            apr_pool_clear(tmppool);
        }

        status = (*request->handler)(request,
                                     request->resp_bkt,
                                     request->handler_baton,
                                     tmppool);

        /* Some systems will not generate a HUP poll event so we have to
         * handle the ECONNRESET issue here.
         */
        if (APR_STATUS_IS_ECONNRESET(status) ||
            status == SERF_ERROR_REQUEST_LOST) {
            reset_connection(conn, 1);
            status = APR_SUCCESS;
            goto error;
        }

        /* If our response handler says it can't do anything more, we now
         * treat that as a success.
         */
        if (APR_STATUS_IS_EAGAIN(status)) {
            status = APR_SUCCESS;
            goto error;
        }

        /* If we received APR_SUCCESS, run this loop again. */
        if (!status) {
            continue;
        }

        close_connection = is_conn_closing(request->resp_bkt);

        if (!APR_STATUS_IS_EOF(status) &&
            close_connection != SERF_ERROR_CLOSING) {
            /* Whether success, or an error, there is no more to do unless
             * this request has been completed.
             */
            goto error;
        }

        /* The request has been fully-delivered, and the response has
         * been fully-read. Remove it from our queue and loop to read
         * another response.
         */
        conn->requests = request->next;

        /* The bucket is no longer needed, nor is the request's pool. */
        serf_bucket_destroy(request->resp_bkt);
        if (request->req_bkt) {
            serf_bucket_destroy(request->req_bkt);
        }

        serf_debug__bucket_alloc_check(request->allocator);
        apr_pool_destroy(request->respool);
        serf_bucket_mem_free(conn->allocator, request);

        request = conn->requests;

        /* If we're truly empty, update our tail. */
        if (request == NULL) {
            conn->requests_tail = NULL;
        }

        conn->completed_responses++;

        /* This means that we're being advised that the connection is done. */
        if (close_connection == SERF_ERROR_CLOSING) {
            reset_connection(conn, 1);
            if (APR_STATUS_IS_EOF(status))
                status = APR_SUCCESS;
            goto error;
        }

        /* The server is suddenly deciding to serve more responses than we've
         * seen before.
         *
         * Let our requests go.
         */
        if (conn->probable_keepalive_limit &&
            conn->completed_responses > conn->probable_keepalive_limit) {
            conn->probable_keepalive_limit = 0;
        }

        /* If we just ran out of requests or have unwritten requests, then
         * update the pollset. We don't want to read from this socket any
         * more. We are definitely done with this loop, too.
         */
        if (request == NULL || request->setup) {
            conn->dirty_conn = 1;
            conn->ctx->dirty_pollset = 1;
            status = APR_SUCCESS;
            goto error;
        }
    }

  error:
    apr_pool_destroy(tmppool);
    return status;
}

/* process all events on the connection */
static apr_status_t process_connection(serf_connection_t *conn,
                                       apr_int16_t events)
{
    apr_status_t status;

    /* POLLHUP/ERR should come after POLLIN so if there's an error message or
     * the like sitting on the connection, we give the app a chance to read
     * it before we trigger a reset condition.
     */
    if ((events & APR_POLLIN) != 0) {
        if ((status = read_from_connection(conn)) != APR_SUCCESS)
            return status;

        /* If we decided to reset our connection, return now as we don't
         * want to write.
         */
        if ((conn->seen_in_pollset & APR_POLLHUP) != 0) {
            return APR_SUCCESS;
        }
    }
    if ((events & APR_POLLHUP) != 0) {
        return APR_ECONNRESET;
    }
    if ((events & APR_POLLERR) != 0) {
        /* We might be talking to a buggy HTTP server that doesn't
         * do lingering-close.  (httpd < 2.1.8 does this.)
         *
         * See:
         *
         * http://issues.apache.org/bugzilla/show_bug.cgi?id=35292
         */
        if (!conn->probable_keepalive_limit) {
            return reset_connection(conn, 1);
        }
        return APR_EGENERAL;
    }
    if ((events & APR_POLLOUT) != 0) {
        if ((status = write_to_connection(conn)) != APR_SUCCESS)
            return status;
    }
    return APR_SUCCESS;
}

/* Check for dirty connections and update their pollsets accordingly. */
static apr_status_t check_dirty_pollsets(serf_context_t *ctx)
{
    int i;

    /* if we're not dirty, return now. */
    if (!ctx->dirty_pollset) {
        return APR_SUCCESS;
    }

    for (i = ctx->conns->nelts; i--; ) {
        serf_connection_t *conn = GET_CONN(ctx, i);
        apr_status_t status;

        /* if this connection isn't dirty, skip it. */
        if (!conn->dirty_conn) {
            continue;
        }

        /* reset this connection's flag before we update. */
        conn->dirty_conn = 0;

        if ((status = update_pollset(conn)) != APR_SUCCESS)
            return status;
    }

    /* reset our context flag now */
    ctx->dirty_pollset = 0;

    return APR_SUCCESS;
}


static apr_status_t pollset_add(void *user_baton,
                                apr_pollfd_t *pfd,
                                void *serf_baton)
{
    serf_pollset_t *s = (serf_pollset_t*)user_baton;
    pfd->client_data = serf_baton;
    return apr_pollset_add(s->pollset, pfd);
}

static apr_status_t pollset_rm(void *user_baton,
                               apr_pollfd_t *pfd,
                               void *serf_baton)
{
    serf_pollset_t *s = (serf_pollset_t*)user_baton;
    pfd->client_data = serf_baton;
    return apr_pollset_remove(s->pollset, pfd);
}


SERF_DECLARE(void) serf_config_proxy(serf_context_t *ctx,
                                     apr_sockaddr_t *address)
{
    ctx->proxy_address = address;
}

SERF_DECLARE(serf_context_t *) serf_context_create_ex(void *user_baton,
                                                      serf_socket_add_t addf,
                                                      serf_socket_remove_t rmf,
                                                      apr_pool_t *pool)
{
    serf_context_t *ctx = apr_pcalloc(pool, sizeof(*ctx));

    ctx->pool = pool;

    if (user_baton != NULL) {
        ctx->pollset_baton = user_baton;
        ctx->pollset_add = addf;
        ctx->pollset_rm = rmf;
    }
    else {
        /* build the pollset with a (default) number of connections */
        serf_pollset_t *ps = apr_pcalloc(pool, sizeof(*ps));
        (void) apr_pollset_create(&ps->pollset, MAX_CONN, pool, 0);
        ctx->pollset_baton = ps;
        ctx->pollset_add = pollset_add;
        ctx->pollset_rm = pollset_rm;
    }

    /* default to a single connection since that is the typical case */
    ctx->conns = apr_array_make(pool, 1, sizeof(serf_connection_t *));

    /* Initialize progress status */
    ctx->progress_read = 0;
    ctx->progress_written = 0;

    return ctx;
}

SERF_DECLARE(serf_context_t *) serf_context_create(apr_pool_t *pool)
{
    return serf_context_create_ex(NULL, NULL, NULL, pool);
}

SERF_DECLARE(apr_status_t) serf_context_prerun(serf_context_t *ctx)
{
    apr_status_t status = APR_SUCCESS;
    if ((status = open_connections(ctx)) != APR_SUCCESS)
        return status;

    if ((status = check_dirty_pollsets(ctx)) != APR_SUCCESS)
        return status;
    return status;
}

SERF_DECLARE(apr_status_t) serf_event_trigger(serf_context_t *s,
                                              void *serf_baton,
                                              const apr_pollfd_t *desc)
{
    apr_status_t status = APR_SUCCESS;

    serf_connection_t *conn = serf_baton;

    /* apr_pollset_poll() can return a conn multiple times... */
    if ((conn->seen_in_pollset & desc->rtnevents) != 0 ||
        (conn->seen_in_pollset & APR_POLLHUP) != 0) {
        return APR_SUCCESS;
    }

    conn->seen_in_pollset |= desc->rtnevents;

    if ((status = process_connection(conn,
                                     desc->rtnevents)) != APR_SUCCESS) {
        return status;
    }

    return status;
}

SERF_DECLARE(apr_status_t) serf_context_run(serf_context_t *ctx,
                                            apr_short_interval_time_t duration,
                                            apr_pool_t *pool)
{
    apr_status_t status;
    apr_int32_t num;
    const apr_pollfd_t *desc;
    serf_pollset_t *ps = (serf_pollset_t*)ctx->pollset_baton;

    if ((status = serf_context_prerun(ctx)) != APR_SUCCESS) {
        return status;
    }

    if ((status = apr_pollset_poll(ps->pollset, duration, &num,
                                   &desc)) != APR_SUCCESS) {
        /* ### do we still need to dispatch stuff here?
           ### look at the potential return codes. map to our defined
           ### return values? ...
        */
        return status;
    }

    while (num--) {
        serf_connection_t *conn = desc->client_data;

        status = serf_event_trigger(ctx, conn, desc);
        if (status) {
            return status;
        }

        desc++;
    }

    return APR_SUCCESS;
}

SERF_DECLARE(void) serf_context_set_progress_cb(
    serf_context_t *ctx,
    const serf_progress_t progress_func,
    void *progress_baton)
{
    ctx->progress_func = progress_func;
    ctx->progress_baton = progress_baton;
}

SERF_DECLARE(serf_connection_t *) serf_connection_create(
    serf_context_t *ctx,
    apr_sockaddr_t *address,
    serf_connection_setup_t setup,
    void *setup_baton,
    serf_connection_closed_t closed,
    void *closed_baton,
    apr_pool_t *pool)
{
    serf_connection_t *conn = apr_pcalloc(pool, sizeof(*conn));

    conn->ctx = ctx;
    conn->address = address;
    conn->setup = setup;
    conn->setup_baton = setup_baton;
    conn->closed = closed;
    conn->closed_baton = closed_baton;
    conn->pool = pool;
    conn->allocator = serf_bucket_allocator_create(pool, NULL, NULL);
    conn->stream = NULL;

    /* Create a subpool for our connection. */
    apr_pool_create(&conn->skt_pool, conn->pool);

    /* register a cleanup */
    apr_pool_cleanup_register(conn->pool, conn, clean_conn, apr_pool_cleanup_null);

    /* Add the connection to the context. */
    *(serf_connection_t **)apr_array_push(ctx->conns) = conn;

    return conn;
}

SERF_DECLARE(apr_status_t) serf_connection_create2(
    serf_connection_t **conn,
    serf_context_t *ctx,
    apr_uri_t host_info,
    serf_connection_setup_t setup,
    void *setup_baton,
    serf_connection_closed_t closed,
    void *closed_baton,
    apr_pool_t *pool)
{
    apr_status_t status;
    serf_connection_t *c;
    apr_sockaddr_t *host_address;

    /* Parse the url, store the address of the server. */
    if (ctx->proxy_address) {
        host_address = ctx->proxy_address;
        status = APR_SUCCESS;
    } else {
        status = apr_sockaddr_info_get(&host_address,
                                       host_info.hostname,
                                       APR_UNSPEC, host_info.port, 0, pool);
        if (status)
            return status;
    }

    c = serf_connection_create(ctx, host_address, setup, setup_baton,
                               closed, closed_baton, pool);

    /* We're not interested in the path following the hostname. */
    c->host_url = apr_uri_unparse(c->pool,
                                  &host_info,
                                  APR_URI_UNP_OMITPATHINFO);
    c->host_info = host_info;

    *conn = c;

    return status;
}

SERF_DECLARE(apr_status_t) serf_connection_reset(
    serf_connection_t *conn)
{
    return reset_connection(conn, 0);
}

SERF_DECLARE(apr_status_t) serf_connection_close(
    serf_connection_t *conn)
{
    int i;
    serf_context_t *ctx = conn->ctx;
    apr_status_t status;

    for (i = ctx->conns->nelts; i--; ) {
        serf_connection_t *conn_seq = GET_CONN(ctx, i);

        if (conn_seq == conn) {
            while (conn->requests) {
                serf_request_cancel(conn->requests);
            }
            if (conn->skt != NULL) {
                remove_connection(ctx, conn);
                status = apr_socket_close(conn->skt);
                if (conn->closed != NULL) {
                    (*conn->closed)(conn, conn->closed_baton, status,
                                    conn->pool);
                }
                conn->skt = NULL;
            }
            if (conn->stream != NULL) {
                serf_bucket_destroy(conn->stream);
                conn->stream = NULL;
            }

            /* Remove the connection from the context. We don't want to
             * deal with it any more.
             */
            if (i < ctx->conns->nelts - 1) {
                /* move later connections over this one. */
                memmove(
                    &GET_CONN(ctx, i),
                    &GET_CONN(ctx, i + 1),
                    (ctx->conns->nelts - i - 1) * sizeof(serf_connection_t *));
            }
            --ctx->conns->nelts;

            /* Found the connection. Closed it. All done. */
            return APR_SUCCESS;
        }
    }

    /* We didn't find the specified connection. */
    /* ### doc talks about this w.r.t poll structures. use something else? */
    return APR_NOTFOUND;
}

SERF_DECLARE(void)
serf_connection_set_max_outstanding_requests(serf_connection_t *conn,
                                             unsigned int max_requests)
{
    conn->max_outstanding_requests = max_requests;
}

SERF_DECLARE(serf_request_t *) serf_connection_request_create(
    serf_connection_t *conn,
    serf_request_setup_t setup,
    void *setup_baton)
{
    serf_request_t *request;

    request = serf_bucket_mem_alloc(conn->allocator, sizeof(*request));
    request->conn = conn;
    request->setup = setup;
    request->setup_baton = setup_baton;
    request->handler = NULL;
    request->respool = NULL;
    request->req_bkt = NULL;
    request->resp_bkt = NULL;
    request->next = NULL;

    /* Link the request to the end of the request chain. */
    if (conn->closing) {
        link_requests(&conn->hold_requests, &conn->hold_requests_tail, request);
    }
    else {
        link_requests(&conn->requests, &conn->requests_tail, request);

        /* Ensure our pollset becomes writable in context run */
        conn->ctx->dirty_pollset = 1;
        conn->dirty_conn = 1;
    }

    return request;
}

SERF_DECLARE(serf_request_t *) serf_connection_priority_request_create(
    serf_connection_t *conn,
    serf_request_setup_t setup,
    void *setup_baton)
{
    serf_request_t *request;
    serf_request_t *iter, *prev;

    request = serf_bucket_mem_alloc(conn->allocator, sizeof(*request));
    request->conn = conn;
    request->setup = setup;
    request->setup_baton = setup_baton;
    request->handler = NULL;
    request->respool = NULL;
    request->req_bkt = NULL;
    request->resp_bkt = NULL;
    request->next = NULL;

    /* Link the new request after the last written request, but before all
       upcoming requests. */
    if (conn->closing) {
        iter = conn->hold_requests;
    }
    else {
        iter = conn->requests;
    }
    prev = NULL;

    /* Find a request that has data which needs to be delivered. */
    while (iter != NULL && iter->req_bkt == NULL && iter->setup == NULL) {
        prev = iter;
        iter = iter->next;
    }

    if (prev) {
        request->next = iter;
        prev->next = request;
    } else {
        request->next = iter;
        if (conn->closing) {
            conn->hold_requests = request;
        }
        else {
            conn->requests = request;
        }
    }

    if (! conn->closing) {
        /* Ensure our pollset becomes writable in context run */
        conn->ctx->dirty_pollset = 1;
        conn->dirty_conn = 1;
    }

    return request;
}

SERF_DECLARE(apr_status_t) serf_request_cancel(serf_request_t *request)
{
    return cancel_request(request, &request->conn->requests, 0);
}

SERF_DECLARE(apr_pool_t *) serf_request_get_pool(const serf_request_t *request)
{
    return request->respool;
}

SERF_DECLARE(serf_bucket_alloc_t *) serf_request_get_alloc(
    const serf_request_t *request)
{
    return request->allocator;
}

SERF_DECLARE(serf_connection_t *) serf_request_get_conn(
    const serf_request_t *request)
{
    return request->conn;
}

SERF_DECLARE(void) serf_request_set_handler(
    serf_request_t *request,
    const serf_response_handler_t handler,
    const void **handler_baton)
{
    request->handler = handler;
    request->handler_baton = handler_baton;
}

SERF_DECLARE(serf_bucket_t *) serf_context_bucket_socket_create(
    serf_context_t *ctx,
    apr_socket_t *skt,
    serf_bucket_alloc_t *allocator)
{
    serf_bucket_t *bucket = serf_bucket_socket_create(skt, allocator);

    /* Use serf's default bytes read/written callback */
    serf_bucket_socket_set_read_progress_cb(bucket,
                                            serf_context_progress_delta,
                                            ctx);

    return bucket;
}

SERF_DECLARE(serf_bucket_t *) serf_request_bucket_request_create_for_host(
    serf_request_t *request,
    const char *method,
    const char *uri,
    serf_bucket_t *body,
    serf_bucket_alloc_t *allocator, const char* host)
{
    serf_bucket_t *req_bkt, *hdrs_bkt;

    req_bkt = serf_bucket_request_create(method, uri, body, allocator);
    hdrs_bkt = serf_bucket_request_get_headers(req_bkt);

    /* Proxy? */
    if (request->conn->ctx->proxy_address && request->conn->host_url)
      serf_bucket_request_set_root(req_bkt, request->conn->host_url);

    if (host == NULL)
      host = request->conn->host_info.hostname;
    if (host)
      serf_bucket_headers_setn(hdrs_bkt, "Host", host);

    return req_bkt;
}

SERF_DECLARE(serf_bucket_t *) serf_request_bucket_request_create(
    serf_request_t *request,
    const char *method,
    const char *uri,
    serf_bucket_t *body,
    serf_bucket_alloc_t *allocator)
{
    return serf_request_bucket_request_create_for_host(
        request, method, uri, body, allocator, NULL);
}
