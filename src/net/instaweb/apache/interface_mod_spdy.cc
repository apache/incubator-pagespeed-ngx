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
// Interfaces with mod_spdy

#include "net/instaweb/apache/interface_mod_spdy.h"

#include <cstddef>

#include "apr_optional.h"
#include "util_filter.h"

#include "base/logging.h"
#include "third_party/mod_spdy/src/mod_spdy/mod_spdy.h"

struct conn_rec;

extern "C" {
  // This is actually from mod_ssl, but we included it here due to
  // similarity.
  APR_DECLARE_OPTIONAL_FN(int, ssl_is_https, (conn_rec*));
}

namespace net_instaweb {

namespace {

int (*ssl_is_https_ptr)(conn_rec* conn) = NULL;

int (*spdy_get_version_ptr)(conn_rec* conn) = NULL;

spdy_slave_connection_factory* (*spdy_create_slave_connection_factory_ptr)(
    conn_rec* master_connection) = NULL;

void (*spdy_destroy_slave_connection_factory_ptr)(
    spdy_slave_connection_factory* factory) = NULL;

spdy_slave_connection* (*spdy_create_slave_connection_ptr)(
    spdy_slave_connection_factory* factory,
    ap_filter_rec_t* input_filter,
    void* input_filter_ctx,
    ap_filter_rec_t* output_filter,
    void* output_filter_ctx) = NULL;

void (*spdy_run_slave_connection_ptr)(spdy_slave_connection* conn) = NULL;
void (*spdy_destroy_slave_connection_ptr)(spdy_slave_connection* conn) = NULL;

}  // namespace

void attach_mod_spdy() {
  ssl_is_https_ptr = APR_RETRIEVE_OPTIONAL_FN(ssl_is_https);

  spdy_get_version_ptr = APR_RETRIEVE_OPTIONAL_FN(spdy_get_version);

  spdy_create_slave_connection_factory_ptr =
      APR_RETRIEVE_OPTIONAL_FN(spdy_create_slave_connection_factory);
  spdy_destroy_slave_connection_factory_ptr =
      APR_RETRIEVE_OPTIONAL_FN(spdy_destroy_slave_connection_factory);
  spdy_create_slave_connection_ptr =
      APR_RETRIEVE_OPTIONAL_FN(spdy_create_slave_connection);
  spdy_run_slave_connection_ptr =
      APR_RETRIEVE_OPTIONAL_FN(spdy_run_slave_connection);
  spdy_destroy_slave_connection_ptr =
      APR_RETRIEVE_OPTIONAL_FN(spdy_destroy_slave_connection);

  // For predictability, NULL out all of the slave_connection routine pointers
  // if at least one is NULL.
  if (spdy_create_slave_connection_factory_ptr == NULL ||
      spdy_destroy_slave_connection_factory_ptr == NULL ||
      spdy_create_slave_connection_ptr == NULL ||
      spdy_run_slave_connection_ptr == NULL ||
      spdy_destroy_slave_connection_ptr == NULL) {
    spdy_create_slave_connection_factory_ptr = NULL;
    spdy_destroy_slave_connection_factory_ptr = NULL;
    spdy_create_slave_connection_ptr = NULL;
    spdy_run_slave_connection_ptr = NULL;
    spdy_destroy_slave_connection_ptr = NULL;
  }
}

int mod_spdy_get_spdy_version(conn_rec* conn) {
  if (spdy_get_version_ptr == NULL) {
    return 0;
  } else {
    return spdy_get_version_ptr(conn);
  }
}

spdy_slave_connection_factory* mod_spdy_create_slave_connection_factory(
    conn_rec* master_connection) {
  if (spdy_create_slave_connection_factory_ptr == NULL) {
    // TODO(morlovich): We should warn when this happens, but only
    // in cases where ModPagespeedFetchFromModSpdy is on,
    // and not in a spammy way.
    return NULL;
  }
  return spdy_create_slave_connection_factory_ptr(master_connection);
}

void mod_spdy_destroy_slave_connection_factory(
    spdy_slave_connection_factory* factory) {
  if (factory == NULL) {
    return;
  }
  CHECK(spdy_destroy_slave_connection_factory_ptr != NULL);
  spdy_destroy_slave_connection_factory_ptr(factory);
}

spdy_slave_connection* mod_spdy_create_slave_connection(
    spdy_slave_connection_factory* factory,
    ap_filter_rec_t* input_filter,
    void* input_filter_ctx,
    ap_filter_rec_t* output_filter,
    void* output_filter_ctx) {
  CHECK(spdy_create_slave_connection_ptr != NULL);
  CHECK(factory != NULL);
  return spdy_create_slave_connection_ptr(
      factory, input_filter, input_filter_ctx,
      output_filter, output_filter_ctx);
}

void mod_spdy_run_slave_connection(spdy_slave_connection* conn) {
  CHECK(spdy_run_slave_connection_ptr != NULL);
  spdy_run_slave_connection_ptr(conn);
}

void mod_spdy_destroy_slave_connection(spdy_slave_connection* conn) {
  CHECK(spdy_destroy_slave_connection_ptr != NULL);
  spdy_destroy_slave_connection_ptr(conn);
}

bool mod_ssl_is_https(conn_rec* conn) {
  if (ssl_is_https_ptr == NULL) {
    return false;
  }

  return ssl_is_https_ptr(conn);
}

}  // namespace net_instaweb
