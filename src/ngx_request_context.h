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

// Author: oschaaf@google.com (Otto van der Schaaf)
//
// I tried to keep this as close to ApacheRequestContext as possible.
// Captures the NGINX request details in our request context, including
// the port (used for loopback fetches).

#ifndef NGX_REQUEST_CONTEXT_H_
#define NGX_REQUEST_CONTEXT_H_

#include "ngx_pagespeed.h"

#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"


namespace net_instaweb {

class AbstractMutex;
class Timer;

class NgxRequestContext : public RequestContext {
 public:
  NgxRequestContext(AbstractMutex* logging_mutex,
                    Timer* timer,
                    ngx_http_request_t* ps_request_context);

  // Returns rc as an NgxRequestContext* if it is one and CHECK
  // fails if it is not. Returns NULL if rc is NULL.
  static NgxRequestContext* DynamicCast(RequestContext* rc);

  int local_port() const { return local_port_; }
  const GoogleString& local_ip() const { return local_ip_; }

 protected:
  virtual ~NgxRequestContext();

 private:
  int local_port_;
  GoogleString local_ip_;

  DISALLOW_COPY_AND_ASSIGN(NgxRequestContext);
};

}  // namespace net_instaweb

#endif  // NGX_REQUEST_CONTEXT_H_
