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
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/apache/apache_writer.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/thread/scheduler.h"

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
              RewriteDriver* driver, ApacheWriter* apache_writer,
              RequestHeaders* request_headers,
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

  // Blocks waiting for the fetch to complete.
  void Wait() LOCKS_EXCLUDED(scheduler_->mutex());

  bool status_ok() const { return status_ok_; }

  virtual bool IsCachedResultValid(const ResponseHeaders& headers)
      LOCKS_EXCLUDED(scheduler_->mutex());

  // By default ApacheFetch is not intended for proxying third party content.
  // When it is to be used for proxying third party content, we must avoid
  // sending a 'nosniff' header.
  void set_is_proxy(bool x) { is_proxy_ = x; }

 protected:
  virtual void HandleHeadersComplete() LOCKS_EXCLUDED(scheduler_->mutex());
  virtual void HandleDone(bool success) LOCKS_EXCLUDED(scheduler_->mutex());
  virtual bool HandleFlush(MessageHandler* handler)
      LOCKS_EXCLUDED(scheduler_->mutex());
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler)
      LOCKS_EXCLUDED(scheduler_->mutex());

 private:
  void SendOutHeaders();

  GoogleString mapped_url_;
  scoped_ptr<ApacheWriter> apache_writer_;
  const RewriteOptions* options_ GUARDED_BY(scheduler_->mutex());

  MessageHandler* message_handler_;

  bool done_ GUARDED_BY(scheduler_->mutex());
  bool wait_called_;
  bool handle_error_;
  bool squelch_output_;
  bool status_ok_;
  bool is_proxy_;
  bool buffered_;
  GoogleString debug_info_;
  GoogleString output_bytes_;
  RewriteDriver* driver_;
  Scheduler* scheduler_;

  DISALLOW_COPY_AND_ASSIGN(ApacheFetch);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_APACHE_FETCH_H_
