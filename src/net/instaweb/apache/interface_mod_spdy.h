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

#include "httpd.h"  // NOLINT

namespace net_instaweb {

// Needs to be called from a ap_hook_optional_fn_retrieve hook.
void attach_mod_spdy();

// If the connection is using SPDY with mod_spdy, returns the protocol
// version. Otherwise, returns 0.
int mod_spdy_get_spdy_version(conn_rec* conn);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_INTERFACE_MOD_SPDY_H_

