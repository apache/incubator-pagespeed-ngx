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

// Author: jmarantz@google.com (Joshua Marantz)
//
// Captures the Apache request details in our request context, including
// the port (used for loopback fetches).

#ifndef PAGESPEED_APACHE_APACHE_REQUEST_CONTEXT_H_
#define PAGESPEED_APACHE_APACHE_REQUEST_CONTEXT_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/system/system_request_context.h"

struct request_rec;

namespace net_instaweb {

class AbstractMutex;
class RequestContext;
class Timer;

class ApacheRequestContext : public SystemRequestContext {
 public:
  ApacheRequestContext(
      AbstractMutex* logging_mutex, Timer* timer, request_rec* req);

  // Returns rc as an ApacheRequestContext* if it is one and CHECK
  // fails if it is not. Returns NULL if rc is NULL.
  static ApacheRequestContext* DynamicCast(RequestContext* rc);

 protected:
  virtual ~ApacheRequestContext();

 private:
  DISALLOW_COPY_AND_ASSIGN(ApacheRequestContext);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_APACHE_REQUEST_CONTEXT_H_
