/* Copyright 2011 Justin Erenkrantz and Greg Stein
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

#ifndef TEST_SERVER_H
#define TEST_SERVER_H

typedef struct serv_ctx_t serv_ctx_t;

#define TEST_SERVER_DUMP 1

/* Default port for our test server. */
#define SERV_PORT 12345
#define SERV_PORT_STR "12345"

#define PROXY_PORT 23456

typedef struct
{
    enum {
        SERVER_RECV,
        SERVER_SEND,
        SERVER_RESPOND,
        SERVER_IGNORE_AND_KILL_CONNECTION,
        SERVER_KILL_CONNECTION
    } kind;

    const char *text;
} test_server_action_t;

typedef struct
{
    const char *text;
} test_server_message_t;

apr_status_t test_start_server(serv_ctx_t **servctx_p,
                               apr_sockaddr_t *address,
                               test_server_message_t *message_list,
                               apr_size_t message_count,
                               test_server_action_t *action_list,
                               apr_size_t action_count,
                               apr_int32_t options,
                               apr_pool_t *pool);

apr_status_t test_server_run(serv_ctx_t *servctx,
                             apr_short_interval_time_t duration,
                             apr_pool_t *pool);

apr_status_t test_server_destroy(serv_ctx_t *servctx, apr_pool_t *pool);

#ifndef APR_VERSION_AT_LEAST /* Introduced in APR 1.3.0 */
#define APR_VERSION_AT_LEAST(major,minor,patch)                  \
(((major) < APR_MAJOR_VERSION)                                       \
 || ((major) == APR_MAJOR_VERSION && (minor) < APR_MINOR_VERSION)    \
 || ((major) == APR_MAJOR_VERSION && (minor) == APR_MINOR_VERSION && \
     (patch) <= APR_PATCH_VERSION))
#endif /* APR_VERSION_AT_LEAST */

#endif /* TEST_SERVER_H */
