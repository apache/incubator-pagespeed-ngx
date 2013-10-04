/*
 * Copyright 2012 Google Inc.
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

// Manage pagespeed state across requests.  Compare to ApacheResourceManager.

#ifndef NGX_SERVER_CONTEXT_H_
#define NGX_SERVER_CONTEXT_H_

#include "ngx_message_handler.h"
#include "net/instaweb/system/public/system_server_context.h"

extern "C" {
#include <ngx_http.h>
}

namespace net_instaweb {

class NgxRewriteDriverFactory;
class NgxRewriteOptions;
class SystemRequestContext;

class NgxServerContext : public SystemServerContext {
 public:
  NgxServerContext(
      NgxRewriteDriverFactory* factory, StringPiece hostname, int port);
  virtual ~NgxServerContext();

  // We expect to use ProxyFetch with HTML.
  virtual bool ProxiesHtml() const { return true; }

  // Call only when you need an NgxRewriteOptions.  If you don't need
  // nginx-specific behavior, call global_options() instead which doesn't
  // downcast.
  NgxRewriteOptions* config();

  NgxRewriteDriverFactory* ngx_rewrite_driver_factory() { return ngx_factory_; }
  SystemRequestContext* NewRequestContext(ngx_http_request_t* r);

  NgxMessageHandler* ngx_message_handler() {
    return dynamic_cast<NgxMessageHandler*>(message_handler());
  }

 private:
  NgxRewriteDriverFactory* ngx_factory_;

  DISALLOW_COPY_AND_ASSIGN(NgxServerContext);
};

}  // namespace net_instaweb

#endif  // NGX_SERVER_CONTEXT_H_
