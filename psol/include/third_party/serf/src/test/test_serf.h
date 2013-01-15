/* Copyright 2002-2007 Justin Erenkrantz and Greg Stein
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

#ifndef TEST_SERF_H
#define TEST_SERF_H

#include "CuTest.h"

#include <apr.h>
#include <apr_pools.h>
#include <apr_uri.h>

#include "serf.h"
#include "server/test_server.h"

/** These macros are provided by APR itself from version 1.3.
 * Definitions are provided here for when using older versions of APR.
 */

/** index into an apr_array_header_t */
#ifndef APR_ARRAY_IDX
#define APR_ARRAY_IDX(ary,i,type) (((type *)(ary)->elts)[i])
#endif

/** easier array-pushing syntax */
#ifndef APR_ARRAY_PUSH
#define APR_ARRAY_PUSH(ary,type) (*((type *)apr_array_push(ary)))
#endif

/* CuTest declarations */
CuSuite *getsuite(void);

CuSuite *test_context(void);
CuSuite *test_buckets(void);
CuSuite *test_ssl(void);

/* Test setup declarations */

#define CRLF "\r\n"

#define CHUNKED_REQUEST(len, body)\
        "GET / HTTP/1.1" CRLF\
        "Host: localhost:12345" CRLF\
        "Transfer-Encoding: chunked" CRLF\
        CRLF\
        #len CRLF\
        body CRLF\
        "0" CRLF\
        CRLF

#define CHUNKED_RESPONSE(len, body)\
        "HTTP/1.1 200 OK" CRLF\
        "Transfer-Encoding: chunked" CRLF\
        CRLF\
        #len CRLF\
        body CRLF\
        "0" CRLF\
        CRLF

#define CHUNKED_EMPTY_RESPONSE\
        "HTTP/1.1 200 OK" CRLF\
        "Transfer-Encoding: chunked" CRLF\
        CRLF\
        "0" CRLF\
        CRLF

typedef struct {
    /* Pool for resource allocation. */
    apr_pool_t *pool;

    serf_context_t *context;
    serf_connection_t *connection;
    serf_bucket_alloc_t *bkt_alloc;

    serv_ctx_t *serv_ctx;
    apr_sockaddr_t *serv_addr;

    serv_ctx_t *proxy_ctx;
    apr_sockaddr_t *proxy_addr;

    /* An extra baton which can be freely used by tests. */
    void *user_baton;

} test_baton_t;

apr_status_t test_server_setup(test_baton_t **tb_p,
                               test_server_message_t *message_list,
                               apr_size_t message_count,
                               test_server_action_t *action_list,
                               apr_size_t action_count,
                               apr_int32_t options,
                               serf_connection_setup_t conn_setup,
                               apr_pool_t *pool);

apr_status_t test_server_proxy_setup(
                 test_baton_t **tb_p,
                 test_server_message_t *serv_message_list,
                 apr_size_t serv_message_count,
                 test_server_action_t *serv_action_list,
                 apr_size_t serv_action_count,
                 test_server_message_t *proxy_message_list,
                 apr_size_t proxy_message_count,
                 test_server_action_t *proxy_action_list,
                 apr_size_t proxy_action_count,
                 apr_int32_t options,
                 serf_connection_setup_t conn_setup,
                 apr_pool_t *pool);

apr_status_t test_server_teardown(test_baton_t *tb, apr_pool_t *pool);

apr_pool_t *test_setup(void);
void test_teardown(apr_pool_t *test_pool);

#endif /* TEST_SERF_H */
