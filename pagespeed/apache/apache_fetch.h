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
// Author: jmarantz@google.com (Joshua Marantz)
//         jefftk@google.com (Jeff Kaufman)

#ifndef PAGESPEED_APACHE_FETCH_H_
#define PAGESPEED_APACHE_FETCH_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/apache/apache_writer.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

// Links an apache request_rec* to an AsyncFetch, adding the ability to
// block based on a condition variable.
//
// If you need this to stream writes and flushes out to apache, call
// set_buffered(false) and make sure all calls are on the request thread.
// ApacheWriter will DCHECK-fail if it's called on another thread.
class ApacheFetch : public AsyncFetch {
 public:
  // Takes ownership of apache_writer, but the underlying request_rec needs to
  // stay around until Wait returns false or the caller finishes handling a true
  // return and deletes the ApacheFetch.
  // Also takes ownership of request_headers.
  ApacheFetch(const GoogleString& mapped_url, StringPiece debug_info,
              ThreadSystem* thread_system, Timer* timer,
              ApacheWriter* apache_writer, RequestHeaders* request_headers,
              const RequestContextPtr& request_context,
              const RewriteOptions* options, MessageHandler* handler);
  virtual ~ApacheFetch();

  // When used for in-place resource optimization in mod_pagespeed, we have
  // disabled fetching resources that are not in cache, otherwise we may wind
  // up doing a loopback fetch to the same Apache server.  So the
  // CacheUrlAsyncFetcher will return a 501 or 404 but we do not want to
  // send that to the client.  So for ipro we suppress reporting errors
  // in this flow.
  //
  // TODO(jmarantz): consider allowing serf fetches in ipro when running as
  // a reverse-proxy.
  void set_handle_error(bool x) { handle_error_ = x; }

  // If all calls to an ApacheFetch are on the apache request thread it is safe
  // to set buffered to false.  All calls Write()/Flush() will be passed through
  // to the underlying writer immediately.  Otherwise, leave it as true and they
  // will be delayed until Wait().
  void set_buffered(bool x) { buffered_ = x; }

  // Blocks waiting for the fetch to complete, then returns kWaitSuccess.  Every
  // 'blocking_fetch_timeout_ms', log a message so that if we get stuck there's
  // noise in the logs, but we don't expect this to happen because underlying
  // fetch/cache timeouts should fire.
  //
  // In some cases, however, we've seen a bug where underlying timeouts take a
  // very long time to fire or may not ever fire.  Instead of tying up an Apache
  // thread indefinitely, after kMaxWaitMs (2 minutes by default) of waiting we
  // give up and transition to the abandoned state.  This doesn't fix the
  // underlying issue of the delay, but it makes it far less destructive.  Even
  // after this bug is resolved, however, it would be good to keep this timeout
  // behavior because it's good defensive programming that protects us from
  // future similar bugs.  See github.com/pagespeed/mod_pagespeed/issues/1048
  //
  // If rewrite_driver is not NULL then on abandonment it's serialized and
  // logged for debugging.
  //
  // Returns false if we abandon the request.
  //
  // TODO(jefftk): find the underlying issue in the code we're waiting for.
  bool Wait(const RewriteDriver* rewrite_driver) LOCKS_EXCLUDED(mutex_);

  bool status_ok() const { return status_ok_; }

  virtual bool IsCachedResultValid(const ResponseHeaders& headers)
      LOCKS_EXCLUDED(mutex_);

  // By default ApacheFetch is not intended for proxying third party content.
  // When it is to be used for proxying third party content, we must avoid
  // sending a 'nosniff' header.
  void set_is_proxy(bool x) { is_proxy_ = x; }

  ThreadSystem::Condvar* CondvarForTesting() { return condvar_.get(); }

  void set_max_wait_ms(int x) { max_wait_ms_ = x; }

 protected:
  virtual void HandleHeadersComplete() LOCKS_EXCLUDED(mutex_);
  virtual void HandleDone(bool success) LOCKS_EXCLUDED(mutex_);
  virtual bool HandleFlush(MessageHandler* handler) LOCKS_EXCLUDED(mutex_);
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler)
      LOCKS_EXCLUDED(mutex_);

 private:
  void SendOutHeaders() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  GoogleString mapped_url_;

  // All uses of apache_writer_ and options_ must be protected with a check
  // under mutex_ that we've not been abandoned.  They reference memory that may
  // be freed on abandonment.
  scoped_ptr<ApacheWriter> apache_writer_ GUARDED_BY(mutex_);
  const RewriteOptions* options_ GUARDED_BY(mutex_);

  Timer* timer_;
  MessageHandler* message_handler_;

  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;

  // If we're abandoned we ignore all writes and delete ourself when Done() is
  // called.  All fetches start with abandoned_ = false and then if Wait() times
  // out they transition into abandoned_ = true.
  bool abandoned_ GUARDED_BY(mutex_);
  bool done_ GUARDED_BY(mutex_);
  bool wait_called_;
  bool handle_error_;
  bool squelch_output_;
  bool status_ok_;
  bool is_proxy_;
  bool buffered_;
  int64 blocking_fetch_timeout_ms_;  // Need in Wait()
  int max_wait_ms_;                  // Need in Wait()
  GoogleString debug_info_;
  GoogleString output_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ApacheFetch);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_FETCH_H_
