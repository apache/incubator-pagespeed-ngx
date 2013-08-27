// Copyright 2013 Google Inc.
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
// Author: sligocki@google.com (Shawn Ligocki)
//
// Content handlers that are usable by any implementation of PSOL.

#ifndef NET_INSTAWEB_SYSTEM_PUBLIC_HANDLERS_H_
#define NET_INSTAWEB_SYSTEM_PUBLIC_HANDLERS_H_

#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class ContentType;
class MessageHandler;
class SystemRewriteDriverFactory;
class SystemRewriteOptions;
class SystemServerContext;
class Writer;

// Handler which serves PSOL console.
// Note: ConsoleHandler always succeeds.
void ConsoleHandler(SystemServerContext* server_context,
                    SystemRewriteOptions* options,
                    Writer* writer,
                    MessageHandler* handler);

// Deprecated handler for graphs in the PSOL console.
void StatisticsGraphsHandler(SystemRewriteOptions* options,
                             Writer* writer,
                             MessageHandler* message_handler);

// Handler for /mod_pagespeed_statistics and /ngx_pagespeed_statistics, as well
// as /...pagespeed__global_statistics.  If the latter, is_global_request should
// be true.
//
// Returns NULL on success, otherwise the returned error string should be passed
// along to the user and the contents of writer and content_type should be
// ignored.
//
// In systems without a spdy-specific config, spdy_config should be null.
const char* StatisticsHandler(
    SystemRewriteDriverFactory* factory,
    SystemServerContext* server_context,
    SystemRewriteOptions* spdy_config,
    bool is_global_request,
    StringPiece query_params,
    ContentType* content_type,
    Writer* writer,
    MessageHandler* message_handler);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_SYSTEM_PUBLIC_HANDLERS_H_
