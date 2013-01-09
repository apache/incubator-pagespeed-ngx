/* Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is a public header file, to be used by other Apache modules.  So,
 * identifiers declared here should follow Apache module naming conventions
 * (specifically, identifiers should be lowercase_with_underscores, and our
 * identifiers should start with the spdy_ prefix), and this header file must
 * be valid in old-school C (not just C++). */

#ifndef MOD_SPDY_APACHE_SLAVE_CONNECTION_API_H_
#define MOD_SPDY_APACHE_SLAVE_CONNECTION_API_H_

#include "httpd.h"
#include "apr_optional.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ap_filter_rec_t;

struct spdy_slave_connection_factory;
struct spdy_slave_connection;

/** Creates a factory object that can be used to make in-process pseudo-fetches
 * with the same origin and target hosts as in master_connection
 */
APR_DECLARE_OPTIONAL_FN(
    struct spdy_slave_connection_factory*,
    spdy_create_slave_connection_factory, (conn_rec* master_connection));

/** Destroys a factory object. */
APR_DECLARE_OPTIONAL_FN(
    void, spdy_destroy_slave_connection_factory,
        (struct spdy_slave_connection_factory* factory));

/** Asks mod_spdy to help with fetching a request on a slave connection.
 * The input_filter must produce the request, and output_filter must
 * handle the response. May return NULL if functionality is not available.
 * The request will not be run until spdy_run_slave_connection() is invoked.
 */
APR_DECLARE_OPTIONAL_FN(
    struct spdy_slave_connection*,
    spdy_create_slave_connection, (
        struct spdy_slave_connection_factory* factory,
        struct ap_filter_rec_t* input_filter,
        void* input_filter_ctx,
        struct ap_filter_rec_t* output_filter,
        void* output_filter_ctx));

/** Actually performs the fetch on the object. Blocks, perhaps for a significant
 *  amount of time. */
APR_DECLARE_OPTIONAL_FN(
    void, spdy_run_slave_connection, (struct spdy_slave_connection* conn));

/** Cleans up the connection object. Must not be in active use. */
APR_DECLARE_OPTIONAL_FN(
    void, spdy_destroy_slave_connection, (struct spdy_slave_connection*));

/* Used by mod_spdy to setup the exports. Not exported itself */
void ModSpdyExportSlaveConnectionFunctions(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* MOD_SPDY_APACHE_SLAVE_CONNECTION_API_H_ */
