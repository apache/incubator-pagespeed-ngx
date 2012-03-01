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

#include "apr_optional.h"

extern "C" {
  APR_DECLARE_OPTIONAL_FN(int, spdy_get_version, (conn_rec*));
}

namespace net_instaweb {

namespace {

typedef int SpdyGetVersionType(conn_rec*);
SpdyGetVersionType* spdy_get_version_ptr = NULL;

}  // namespace

void attach_mod_spdy() {
  spdy_get_version_ptr = APR_RETRIEVE_OPTIONAL_FN(spdy_get_version);
}

int mod_spdy_get_spdy_version(conn_rec* conn) {
  if (spdy_get_version_ptr == NULL) {
    return 0;
  } else {
    return spdy_get_version_ptr(conn);
  }
}

}  // namespace net_instaweb
