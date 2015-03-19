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

// Author: jefftk@google.com (Jeff Kaufman)
//
// Captures the request details in our request context, including
// the port and ip (used for loopback fetches).

#ifndef PAGESPEED_SYSTEM_SYSTEM_REQUEST_CONTEXT_H_
#define PAGESPEED_SYSTEM_SYSTEM_REQUEST_CONTEXT_H_

#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AbstractMutex;
class Timer;

class SystemRequestContext : public RequestContext {
 public:
  // There are two ways a request may specify the hostname: with the Host
  // header or on the initial request line.  Callers need to check both places.
  SystemRequestContext(AbstractMutex* logging_mutex,
                       Timer* timer,
                       StringPiece hostname,
                       int local_port,
                       StringPiece local_ip);

  // Captures the original URL of the request, which is used to help
  // authorize domains for fetches we do on behalf of that request.
  void set_url(StringPiece url) { url.CopyToString(&url_); }

  // Returns rc as a SystemRequestContext* if it is one and CHECK fails if it is
  // not. Returns NULL if rc is NULL.
  static SystemRequestContext* DynamicCast(RequestContext* rc);

  int local_port() const { return local_port_; }
  const GoogleString& local_ip() const { return local_ip_; }
  StringPiece url() const { return url_; }

 protected:
  virtual ~SystemRequestContext() {}

 private:
  int local_port_;
  GoogleString local_ip_;
  GoogleString url_;

  DISALLOW_COPY_AND_ASSIGN(SystemRequestContext);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SYSTEM_SYSTEM_REQUEST_CONTEXT_H_
