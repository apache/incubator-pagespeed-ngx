// Copyright 2015 Google Inc.
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
// Author: morlovich@google.com (Maksim Orlovich)
// (Based on apache_fetch.h)

#ifndef PAGESPEED_SIMPLE_BUFFERED_APACHE_FETCH_H_
#define PAGESPEED_SIMPLE_BUFFERED_APACHE_FETCH_H_

#include <memory>
#include <queue>
#include <utility>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/apache/apache_writer.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

struct request_rec;

namespace net_instaweb {

// Links an apache request_rec* to an AsyncFetch, adding the ability to
// block based on a condition variable. Unlike ApacheFetch this always
// buffers and implements no policy, nor does it try to use Apache thread
// for any rewriting --- a scheduler thread should be used along with this.
class SimpleBufferedApacheFetch : public AsyncFetch {
 public:
  // Takes ownership of request_headers. req is expected to survive at least
  // until Wait() returns.
  SimpleBufferedApacheFetch(
      const RequestContextPtr& request_context,
      RequestHeaders* request_headers,
      ThreadSystem* thread_system,
      request_rec* req,
      MessageHandler* handler);
  ~SimpleBufferedApacheFetch() override;

  // Blocks waiting for the fetch to complete.
  void Wait() LOCKS_EXCLUDED(mutex_);

  bool IsCachedResultValid(const ResponseHeaders& headers) override
      LOCKS_EXCLUDED(mutex_);

 protected:
  void HandleHeadersComplete() override LOCKS_EXCLUDED(mutex_);
  void HandleDone(bool success) override LOCKS_EXCLUDED(mutex_);
  bool HandleFlush(MessageHandler* handler) override LOCKS_EXCLUDED(mutex_);
  bool HandleWrite(const StringPiece& sp, MessageHandler* handler) override
      LOCKS_EXCLUDED(mutex_);

 private:
  enum Op {
    kOpHeadersComplete,
    kOpWrite,
    kOpFlush,
    kOpDone
  };

  typedef std::pair<Op, GoogleString> OpInfo;

  // Blocks until there is an operation in the queue, and move it to *out.
  void WaitForOp(OpInfo* out) LOCKS_EXCLUDED(mutex_);

  void SendOutHeaders();

  std::unique_ptr<ApacheWriter> apache_writer_;
  MessageHandler* message_handler_;

  std::unique_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  std::unique_ptr<ThreadSystem::Condvar> notify_;
  std::queue<OpInfo> queue_;

  bool wait_called_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(SimpleBufferedApacheFetch);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_SIMPLE_BUFFERED_APACHE_FETCH_H_
