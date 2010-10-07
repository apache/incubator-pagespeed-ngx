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


#ifndef MOD_INSTAWEB_MOD_INSTAWEB_H_
#define MOD_INSTAWEB_MOD_INSTAWEB_H_

#include "base/basictypes.h"

// Forward declaration.
struct server_rec;

namespace net_instaweb {

struct PageSpeedConfig;
class PageSpeedServerContext;

PageSpeedConfig* mod_pagespeed_get_server_config(server_rec* server);
PageSpeedServerContext* mod_pagespeed_get_config_server_context(
    server_rec* server);
}  // namespace net_instaweb

#endif  // MOD_INSTAWEB_MOD_INSTAWEB_H_
