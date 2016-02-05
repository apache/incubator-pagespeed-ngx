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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_NOTIFYING_FETCH_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_NOTIFYING_FETCH_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/thread/worker_test_base.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

class MessageHandler;
class RewriteOptions;

// Implements an AsyncFetch subclass that notifies a WorkerTestBase::SyncPoint
// upon completion.  This is intended for testing.
class NotifyingFetch : public AsyncFetch {
 public:
  NotifyingFetch(const RequestContextPtr& request_context,
                 RewriteOptions* options,
                 const GoogleString& url,
                 WorkerTestBase::SyncPoint* sync,
                 ResponseHeaders* response_headers);
  virtual ~NotifyingFetch();

  StringPiece content() { return content_; }
  bool done() { return done_; }
  bool success() { return success_; }

 protected:
  void HandleHeadersComplete() override {}
  bool HandleWrite(const StringPiece& content, MessageHandler* handler)
      override;
  bool HandleFlush(MessageHandler* handler) override;
  void HandleDone(bool success) override;
  bool IsCachedResultValid(const ResponseHeaders& headers) override;

 private:
  GoogleString content_;
  bool done_;
  bool success_;
  const RewriteOptions* options_;
  GoogleString url_;
  WorkerTestBase::SyncPoint* sync_;

  DISALLOW_COPY_AND_ASSIGN(NotifyingFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_NOTIFYING_FETCH_H_
