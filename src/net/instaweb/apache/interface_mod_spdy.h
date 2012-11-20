// Copyright 2012 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: morlovich@google.com (Maksim Orlovich)
//
// Interfaces with mod_spdy's exported functions.

#ifndef NET_INSTAWEB_APACHE_INTERFACE_MOD_SPDY_H_
#define NET_INSTAWEB_APACHE_INTERFACE_MOD_SPDY_H_

#include "util_filter.h"

#include "third_party/mod_spdy/src/mod_spdy/apache/slave_connection_api.h"

struct conn_rec;

namespace net_instaweb {

// Needs to be called from a ap_hook_optional_fn_retrieve hook.
void attach_mod_spdy();

// If the connection is using SPDY with mod_spdy, returns the protocol
// version. Otherwise, returns 0.
int mod_spdy_get_spdy_version(conn_rec* conn);

// See mod_spdy's slave_connection_api.h for description of the methods below.
// These are merely forwarding wrappers with some CHECKS.
// Note that this method will return NULL if the relevant mod_spdy methods
// weren't found registered with Apache. Others, however, will CHECK-fail
// (since there is no sensible way to call them if this method failed);
// except you can always safely mod_spdy_destroy_slave_connection_factory(NULL).
spdy_slave_connection_factory* mod_spdy_create_slave_connection_factory(
    conn_rec* master_connection);
void mod_spdy_destroy_slave_connection_factory(
    spdy_slave_connection_factory* factory);

spdy_slave_connection* mod_spdy_create_slave_connection(
    spdy_slave_connection_factory* factory,
    ap_filter_rec_t* input_filter,
    void* input_filter_ctx,
    ap_filter_rec_t* output_filter,
    void* output_filter_ctx);

void mod_spdy_run_slave_connection(spdy_slave_connection* conn);
void mod_spdy_destroy_slave_connection(spdy_slave_connection* conn);

// Returns true if given connection is using HTTPS.
// (This is actually a mod_ssl function).
bool mod_ssl_is_https(conn_rec* conn);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_INTERFACE_MOD_SPDY_H_
