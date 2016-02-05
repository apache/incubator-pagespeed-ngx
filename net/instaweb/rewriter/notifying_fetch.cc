/*
 * Copyright 2011 Google Inc.
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

// Author: nikhilmadan@google.com (Nikhil Madan)

#include "net/instaweb/rewriter/public/notifying_fetch.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

NotifyingFetch::NotifyingFetch(const RequestContextPtr& request_context,
                               RewriteOptions* options,
                               const GoogleString& url,
                               WorkerTestBase::SyncPoint* sync,
                               ResponseHeaders* response_headers)
    : AsyncFetch(request_context),
      done_(false),
      success_(false),
      options_(options),
      url_(url),
      sync_(sync) {
  if (response_headers != nullptr) {
    set_response_headers(response_headers);
  }
}

NotifyingFetch::~NotifyingFetch() {
}

bool NotifyingFetch::IsCachedResultValid(const ResponseHeaders& headers) {
  return OptionsAwareHTTPCacheCallback::IsCacheValid(
      url_, *options_, request_context(), headers);
}

bool NotifyingFetch::HandleWrite(const StringPiece& content,
                                 MessageHandler* handler) {
  content.AppendToString(&content_);
  return true;
}

bool NotifyingFetch::HandleFlush(MessageHandler* handler) {
  return true;
}

void NotifyingFetch::HandleDone(bool success) {
  response_headers()->ComputeCaching();
  done_ = true;
  success_ = success;
  sync_->Notify();
}

}  // namespace net_instaweb
