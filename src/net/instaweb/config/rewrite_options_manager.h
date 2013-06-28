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

// Author: gee@google.com (Adam Gee)

#ifndef NET_INSTAWEB_CONFIG_REWRITE_OPTIONS_MANAGER_H_
#define NET_INSTAWEB_CONFIG_REWRITE_OPTIONS_MANAGER_H_

#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/string.h"


namespace net_instaweb {

class GoogleUrl;
class RewriteOptions;
class RequestHeaders;

class RewriteOptionsManager {
 public:
  RewriteOptionsManager() {}
  virtual ~RewriteOptionsManager() {}

  // Given the request url and request headers, generate the rewrite options.
  typedef Callback1<RewriteOptions*> OptionsCallback;
  virtual void GetRewriteOptions(const GoogleUrl& url,
                                 const RequestHeaders& headers,
                                 OptionsCallback* done);

  // Modifies the request prior to dispatch to the underlying fetcher.  Invokes
  // "done" once preparation has finished with a boolean argument
  // representing success.  "url" may be modified by PrepareRequest, but should
  // be owned by the caller.
  typedef Callback1<bool> BoolCallback;
  virtual void PrepareRequest(const RewriteOptions* rewrite_options,
                              GoogleString* url,
                              RequestHeaders* request_headers,
                              BoolCallback* done);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_CONFIG_REWRITE_OPTIONS_MANAGER_H_
