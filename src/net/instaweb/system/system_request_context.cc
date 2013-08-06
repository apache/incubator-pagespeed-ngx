/*
 * Copyright 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "net/instaweb/system/public/system_request_context.h"

#include "base/logging.h"

namespace net_instaweb {

SystemRequestContext::SystemRequestContext(
    AbstractMutex* logging_mutex, Timer* timer,
    int local_port, StringPiece local_ip)
    : RequestContext(logging_mutex, timer),
      local_port_(local_port),
      local_ip_(local_ip.as_string()) {}

SystemRequestContext* SystemRequestContext::DynamicCast(RequestContext* rc) {
  if (rc == NULL) {
    return NULL;
  }
  SystemRequestContext* out = dynamic_cast<SystemRequestContext*>(rc);
  DCHECK(out != NULL) << "Invalid request conversion. Do not rely on RTTI for "
                      << "functional behavior. System handling flows must use "
                      << "SystemRequestContexts or a subclass.";
  return out;
}

}  // namespace net_instaweb
