// Copyright 2012 Google Inc
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

#ifndef MOD_SPDY_APACHE_SSL_UTIL_H_
#define MOD_SPDY_APACHE_SSL_UTIL_H_

// This file contains some utility functions for communicating to mod_ssl.

struct conn_rec;

namespace mod_spdy {

// This must be called from optional_fn_retrieve hook at startup before
// using any of the methods below.
void RetrieveModSslFunctions();

// Disables SSL on a connection. Returns true if successful, false if failed
// for some reason (such as the optional function not being available, or
// mod_ssl indicating a problem).
bool DisableSslForConnection(conn_rec* connection);

// Returns true if the given connection is using SSL. Note that this answers
// whether the connection is really using SSL rather than whether we should tell
// others that it is. This distinction matters for slave connection belonging to
// an SSL master --- we're not really using SSL for them (so this method
// will return false), but will claim we are (since they'll be encrypted once
// going to other outside world).
bool IsUsingSslForConnection(conn_rec* connection);

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_SOCKADDR_UTIL_H_
