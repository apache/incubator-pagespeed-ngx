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

#include "serf_private.h"

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
apr_status_t serf__conn_update_pollset(serf_connection_t *conn)
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
                       request->written)
                    request = request->next;
                if (request != NULL)
                    desc.reqevents |= APR_POLLOUT;
            }
        }
    }

    /* If we can have async responses, always look for something to read. */
    if (conn->async_responses) {
        desc.reqevents |= APR_POLLIN;
    }

    /* save our reqevents, so we can pass it in to remove later. */
    conn->reqevents = desc.reqevents;

    /* Note: even if we don't want to read/write this socket, we still
     * want to poll it for hangups and errors.
     */
    return ctx->pollset_add(ctx->pollset_baton,
                            &desc, &conn->baton);
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
apr_status_t serf__open_connections(serf_context_t *ctx)
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
                                         APR_TCP_NODELAY, 1)) != APR_SUCCESS)
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

        /* If the authentication was already started on another connection,
           prepare this connection (it might be possible to skip some
           part of the handshaking). */
        if (ctx->proxy_address) {
            if (conn->ctx->proxy_authn_info.scheme)
                conn->ctx->proxy_authn_info.scheme->init_conn_func(407, conn,
                                                                   conn->pool);
        }

        if (conn->ctx->authn_info.scheme)
            conn->ctx->authn_info.scheme->init_conn_func(401, conn,
                                                         conn->pool);
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

static apr_status_t destroy_request(serf_request_t *request)
{
    serf_connection_t *conn = request->conn;

    /* The request and response buckets are no longer needed,
       nor is the request's pool.  */
    if (request->resp_bkt) {
        serf_debug__closed_conn(request->resp_bkt->allocator);
        serf_bucket_destroy(request->resp_bkt);
    }
    if (request->req_bkt) {
        serf_debug__closed_conn(request->req_bkt->allocator);
        serf_bucket_destroy(request->req_bkt);
    }

    serf_debug__bucket_alloc_check(request->allocator);
    if (request->respool) {
        apr_pool_destroy(request->respool);
    }

    serf_bucket_mem_free(conn->allocator, request);

    return APR_SUCCESS;
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

    return destroy_request(request);
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

static void destroy_ostream(serf_connection_t *conn)
{
    if (conn->ostream_head != NULL) {
        serf_bucket_destroy(conn->ostream_head);
        conn->ostream_head = NULL;
        conn->ostream_tail = NULL;
    }
}

/* A socket was closed, inform the application. */
static void handle_conn_closed(serf_connection_t *conn, apr_status_t status)
{
    (*conn->closed)(conn, conn->closed_baton, status,
                    conn->pool);
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
        if (requeue_requests && !old_reqs->written) {
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
            handle_conn_closed(conn, status);
        }
        conn->skt = NULL;
    }

    if (conn->stream != NULL) {
        serf_bucket_destroy(conn->stream);
        conn->stream = NULL;
    }

    destroy_ostream(conn);

    /* Don't try to resume any writes */
    conn->vec_len = 0;

    conn->dirty_conn = 1;
    conn->ctx->dirty_pollset = 1;

    conn->status = APR_SUCCESS;

    /* Let our context know that we've 'reset' the socket already. */
    conn->seen_in_pollset |= APR_POLLHUP;

    /* Found the connection. Closed it. All done. */
    return APR_SUCCESS;
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
                conn->vec[0].iov_base = (char *)conn->vec[0].iov_base + (conn->vec[0].iov_len - (len - written));
                conn->vec[0].iov_len = len - written;
                break;
            }
        }
        if (len == written) {
            conn->vec_len = 0;
        }

        /* Log progress information */
        serf__context_progress_delta(conn->ctx, 0, written);
    }

    return status;
}

static apr_status_t detect_eof(void *baton, serf_bucket_t *aggregate_bucket)
{
    serf_connection_t *conn = baton;
    conn->hit_eof = 1;
    return APR_EAGAIN;
}

static apr_status_t do_conn_setup(serf_connection_t *conn)
{
    apr_status_t status;
    serf_bucket_t *ostream;

    if (conn->ostream_head == NULL) {
        conn->ostream_head = serf_bucket_aggregate_create(conn->allocator);
    }

    if (conn->ostream_tail == NULL) {
        conn->ostream_tail = serf__bucket_stream_create(conn->allocator,
                                                        detect_eof,
                                                        conn);
    }

    ostream = conn->ostream_tail;

    status = (*conn->setup)(conn->skt,
                            &conn->stream,
                            &ostream,
                            conn->setup_baton,
                            conn->pool);
    if (status) {
        /* extra destroy here since it wasn't added to the head bucket yet. */
        serf_bucket_destroy(conn->ostream_tail);
        destroy_ostream(conn);
        return status;
    }

    serf_bucket_aggregate_append(conn->ostream_head,
                                 ostream);

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
           request->req_bkt == NULL && request->written)
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
               request->req_bkt == NULL && request->written)
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
            status = do_conn_setup(conn);
            if (status) {
                return status;
            }
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

            request->written = 1;
            serf_bucket_aggregate_append(conn->ostream_tail, request->req_bkt);
        }

        /* ### optimize at some point by using read_for_sendfile */
        read_status = serf_bucket_read_iovec(conn->ostream_head,
                                             SERF_READ_ALL_AVAIL,
                                             IOV_MAX,
                                             conn->vec,
                                             &conn->vec_len);

        if (!conn->hit_eof) {
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

        if (conn->hit_eof &&
            conn->vec_len == 0) {
            /* If we hit the end of the request bucket and all of its data has
             * been written, then clear it out to signify that we're done
             * sending the request. On the next iteration through this loop:
             * - if there are remaining bytes they will be written, and as the
             * request bucket will be completely read it will be destroyed then.
             * - we'll see if there are other requests that need to be sent
             * ("pipelining").
             */
            conn->hit_eof = 0;
            request->req_bkt = NULL;

            /* If our connection has async responses enabled, we're not
             * going to get a reply back, so kill the request.
             */
            if (conn->async_responses) {
                conn->requests = request->next;
                destroy_request(request);
            }

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

/* A response message was received from the server, so call
   the handler as specified on the original request. */
static apr_status_t handle_response(serf_request_t *request,
                                    apr_pool_t *pool)
{
    apr_status_t status = APR_EGENERAL;
    int consumed_response = 0;

    /* Only enable the new authentication framework if the program has
     * registered an authentication credential callback.
     *
     * This permits older Serf apps to still handle authentication
     * themselves by not registering credential callbacks.
     */
#if 0 /* This disables authentication support for now */
    if (request->conn->ctx->cred_cb) {
      status = serf__handle_auth_response(&consumed_response,
                                          request,
                                          request->resp_bkt,
                                          request->handler_baton,
                                          pool);

      /* If there was an error reading the response (maybe there wasn't
         enough data available), don't bother passing the response to the
         application.

         If the authentication was tried, but failed, pass the response
         to the application, maybe it can do better. */
      if (APR_STATUS_IS_EOF(status) ||
          APR_STATUS_IS_EAGAIN(status)) {
          return status;
      }
    }
#endif

    if (!consumed_response) {
        return (*request->handler)(request,
                                   request->resp_bkt,
                                   request->handler_baton,
                                   pool);
    }

    return status;
}

/* An async response message was received from the server. */
static apr_status_t handle_async_response(serf_connection_t *conn,
                                          apr_pool_t *pool)
{
    apr_status_t status;

    if (conn->current_async_response == NULL) {
        conn->current_async_response =
            (*conn->async_acceptor)(NULL, conn->stream,
                                    conn->async_acceptor_baton, pool);
    }

    status = (*conn->async_handler)(NULL, conn->current_async_response,
                                    conn->async_handler_baton, pool);

    if (APR_STATUS_IS_EOF(status)) {
        serf_bucket_destroy(conn->current_async_response);
        conn->current_async_response = NULL;
        status = APR_SUCCESS;
    }

    return status;
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
            status = do_conn_setup(conn);
            if (status) {
                goto error;
            }
        }

        /* We have a different codepath when we can have async responses. */
        if (conn->async_responses) {
            /* TODO What about socket errors? */
            status = handle_async_response(conn, tmppool);
            if (APR_STATUS_IS_EAGAIN(status)) {
                status = APR_SUCCESS;
                goto error;
            }
            if (status) {
                goto error;
            }
            continue;
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
        if (request->req_bkt || !request->written) {
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

        status = handle_response(request, tmppool);

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

        destroy_request(request);

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
        if (request == NULL || !request->written) {
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
apr_status_t serf__process_connection(serf_connection_t *conn,
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

serf_connection_t *serf_connection_create(
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
    conn->status = APR_SUCCESS;
    conn->address = address;
    conn->setup = setup;
    conn->setup_baton = setup_baton;
    conn->closed = closed;
    conn->closed_baton = closed_baton;
    conn->pool = pool;
    conn->allocator = serf_bucket_allocator_create(pool, NULL, NULL);
    conn->stream = NULL;
    conn->ostream_head = NULL;
    conn->ostream_tail = NULL;
    conn->baton.type = SERF_IO_CONN;
    conn->baton.u.conn = conn;
    conn->hit_eof = 0;

    /* Create a subpool for our connection. */
    apr_pool_create(&conn->skt_pool, conn->pool);

    /* register a cleanup */
    apr_pool_cleanup_register(conn->pool, conn, clean_conn, apr_pool_cleanup_null);

    /* Add the connection to the context. */
    *(serf_connection_t **)apr_array_push(ctx->conns) = conn;

    return conn;
}

apr_status_t serf_connection_create2(
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

    /* Support for HTTPS proxies is not implemented yet. */
    if (ctx->proxy_address && strcmp(host_info.scheme, "https") == 0)
        return APR_ENOTIMPL;

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

    if (status)
        return status;

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

apr_status_t serf_connection_reset(
    serf_connection_t *conn)
{
    return reset_connection(conn, 0);
}


apr_status_t serf_connection_close(
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
                    handle_conn_closed(conn, status);
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

/*
 Returns true if this connection has had error events reported during the last
 call to serf_context_run. It should be called after serf_context_run
 invocation, and not within callbacks.

 Return value is conceptually bool, but Serf implementation language is C.

 google-added.
*/
int serf_connection_is_in_error_state(serf_connection_t* conn)
{
  return ((conn->seen_in_pollset & (APR_POLLERR | APR_POLLHUP)) != 0);
}

void serf_connection_set_max_outstanding_requests(
    serf_connection_t *conn,
    unsigned int max_requests)
{
    conn->max_outstanding_requests = max_requests;
}


void serf_connection_set_async_responses(
    serf_connection_t *conn,
    serf_response_acceptor_t acceptor,
    void *acceptor_baton,
    serf_response_handler_t handler,
    void *handler_baton)
{
    conn->async_responses = 1;
    conn->async_acceptor = acceptor;
    conn->async_acceptor_baton = acceptor_baton;
    conn->async_handler = handler;
    conn->async_handler_baton = handler_baton;
}


serf_request_t *serf_connection_request_create(
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
    request->written = 0;
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


serf_request_t *serf_connection_priority_request_create(
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
    request->written = 0;
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
    while (iter != NULL && iter->req_bkt == NULL && iter->written) {
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


apr_status_t serf_request_cancel(serf_request_t *request)
{
    return cancel_request(request, &request->conn->requests, 0);
}


apr_pool_t *serf_request_get_pool(const serf_request_t *request)
{
    return request->respool;
}


serf_bucket_alloc_t *serf_request_get_alloc(
    const serf_request_t *request)
{
    return request->allocator;
}


serf_connection_t *serf_request_get_conn(
    const serf_request_t *request)
{
    return request->conn;
}


void serf_request_set_handler(
    serf_request_t *request,
    const serf_response_handler_t handler,
    const void **handler_baton)
{
    request->handler = handler;
    request->handler_baton = handler_baton;
}


serf_bucket_t *serf_request_bucket_request_create_for_host(
    serf_request_t *request,
    const char *method,
    const char *uri,
    serf_bucket_t *body,
    serf_bucket_alloc_t *allocator,
    const char* host)
{
    serf_bucket_t *req_bkt, *hdrs_bkt;
    serf_connection_t *conn = request->conn;
    serf_context_t *ctx = conn->ctx;

    req_bkt = serf_bucket_request_create(method, uri, body, allocator);
    hdrs_bkt = serf_bucket_request_get_headers(req_bkt);

    /* Proxy? */
    if (ctx->proxy_address && conn->host_url)
        serf_bucket_request_set_root(req_bkt, conn->host_url);

    if (host == NULL)
        host = request->conn->host_info.hostname;
    if (host)
        serf_bucket_headers_setn(hdrs_bkt, "Host", host);

    /* Setup server authorization headers */
    if (ctx->authn_info.scheme)
        ctx->authn_info.scheme->setup_request_func(401, conn, method, uri,
                                                   hdrs_bkt);

    /* Setup proxy authorization headers */
    if (ctx->proxy_authn_info.scheme)
        ctx->proxy_authn_info.scheme->setup_request_func(407, conn, method,
                                                         uri, hdrs_bkt);

    return req_bkt;
}

serf_bucket_t *serf_request_bucket_request_create(
    serf_request_t *request,
    const char *method,
    const char *uri,
    serf_bucket_t *body,
    serf_bucket_alloc_t *allocator)
{
    return serf_request_bucket_request_create_for_host(
        request, method, uri, body, allocator, NULL);
}
