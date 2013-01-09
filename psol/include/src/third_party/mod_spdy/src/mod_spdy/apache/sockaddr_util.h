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

#ifndef MOD_SPDY_APACHE_SOCKADDR_UTIL_H_
#define MOD_SPDY_APACHE_SOCKADDR_UTIL_H_

#include "apr_pools.h"
#include "apr_network_io.h"

namespace mod_spdy {

// Deep-copies the apr_sockaddr_t 'in', with the result being allocated in the
// pool 'pool'.
apr_sockaddr_t* DeepCopySockAddr(const apr_sockaddr_t* in, apr_pool_t* pool);

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_SOCKADDR_UTIL_H_
