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

#ifndef NET_INSTAWEB_APACHE_LOG_MESSAGE_HANDLER_H_
#define NET_INSTAWEB_APACHE_LOG_MESSAGE_HANDLER_H_

#include "apr_pools.h"

namespace net_instaweb {

// Install a log message handler that routes LOG() messages to the
// apache error log. Should be called once at startup.
void InstallLogMessageHandler(apr_pool_t* pool);

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_APACHE_LOG_MESSAGE_HANDLER_H_
