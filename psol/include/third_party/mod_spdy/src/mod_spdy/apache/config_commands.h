// Copyright 2010 Google Inc.
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

#ifndef MOD_SPDY_APACHE_CONFIG_COMMANDS_H_
#define MOD_SPDY_APACHE_CONFIG_COMMANDS_H_

#include "httpd.h"
#include "http_config.h"

namespace mod_spdy {

// An array of configuration command objects, to be placed into an Apache
// module object.  See TAMB 9.4.
extern const command_rec kSpdyConfigCommands[];

// A function to create new server config objects, with a function signature
// appropriate to be placed into an Apache module object.  See TAMB 9.3.1.
void* CreateSpdyServerConfig(apr_pool_t* pool, server_rec* server);

// A function to merge existing server config objects, with a signature
// appropriate to be placed into an Apache module object.  See TAMB 9.5.
void* MergeSpdyServerConfigs(apr_pool_t* pool, void* base, void* add);

}  // namespace mod_spdy

#endif  // MOD_SPDY_APACHE_CONFIG_COMMANDS_H_
