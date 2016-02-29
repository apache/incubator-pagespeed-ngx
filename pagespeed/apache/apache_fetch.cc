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

#include "pagespeed/apache/apache_fetch.h"

#include <algorithm>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/thread/scheduler_sequence.h"

namespace net_instaweb {

ApacheFetch::ApacheFetch(const GoogleString& mapped_url, StringPiece debug_info,
                         RewriteDriver* driver, ApacheWriter* apache_writer,
                         RequestHeaders* request_headers,
                         const RequestContextPtr& request_context,
                         const RewriteOptions* options, MessageHandler* handler)
    : AsyncFetch(request_context),
      mapped_url_(mapped_url),
      apache_writer_(apache_writer),
      options_(options),
      message_handler_(handler),
      done_(false),
      wait_called_(false),
      handle_error_(true),
      squelch_output_(false),
      status_ok_(false),
      is_proxy_(false),
      buffered_(true),
      driver_(driver),
      scheduler_(nullptr) {
  // We are proxying content, and the caching in the http configuration
  // should not apply; we want to use the caching from the proxy.
  apache_writer_->set_disable_downstream_header_filters(true);
  // TODO(jefftk): ApacheWriter has a bug where it doesn't actually strip the
  // cookies when we ask it to.  This is hard to fix because we're not sure
  // which uses depend on cookies being passed through.
  apache_writer_->set_strip_cookies(true);

  request_headers->RemoveAll(HttpAttributes::kCookie);
  request_headers->RemoveAll(HttpAttributes::kCookie2);
  SetRequestHeadersTakingOwnership(request_headers);

  debug_info.CopyToString(&debug_info_);

  driver_->SetRequestHeaders(*request_headers);
  driver_->IncrementAsyncEventsCount();
  driver_->RunTasksOnRequestThread();
  scheduler_ = driver_->scheduler();
}

ApacheFetch::~ApacheFetch() {
  driver_->DecrementAsyncEventsCount();
}

// Called by other threads unless buffered=false.
void ApacheFetch::HandleHeadersComplete() {
  if (buffered_) {
    // Do nothing on this thread right now.  When done waiting we'll deal with
    // headers on the request thread.
    return;
  }
  SendOutHeaders();
}

// Called on the request thread.
void ApacheFetch::SendOutHeaders() {
  int status_code = response_headers()->status_code();

  // Setting status_ok = true tells InstawebHandler that we've handled this
  // request and sent out the response.  If we leave it as false InstawebHandler
  // will DECLINE the request and another handler will deal with it.
  status_ok_ = (status_code != 0) && (status_code < 400);

  if (handle_error_ || status_ok_) {
    StringPiece error_message = "";  // No error by default.
    // 304 and 204 responses shouldn't have content lengths and aren't expected
    // to have Content-Types.  All other responses should.
    if ((status_code != HttpStatus::kNotModified) &&
        (status_code != HttpStatus::kNoContent) &&
        !response_headers()->Has(HttpAttributes::kContentType)) {
      status_code = HttpStatus::kForbidden;
      status_ok_ = false;
      response_headers()->SetStatusAndReason(HttpStatus::kForbidden);
      response_headers()->Add(HttpAttributes::kContentType, "text/html");
      response_headers()->RemoveAll(HttpAttributes::kCacheControl);
      error_message = "Missing Content-Type required for proxied resource";
    }

    int64 now_ms = scheduler_->timer()->NowMs();
    response_headers()->SetDate(now_ms);

    // http://msdn.microsoft.com/en-us/library/ie/gg622941(v=vs.85).aspx
    // Script and styleSheet elements will reject responses with
    // incorrect MIME types if the server sends the response header
    // "X-Content-Type-Options: nosniff". This is a security feature
    // that helps prevent attacks based on MIME-type confusion.
    if (!is_proxy_) {
      response_headers()->Add("X-Content-Type-Options", "nosniff");
    }

    // TODO(sligocki): Add X-Mod-Pagespeed header.

    // Default cache-control to nocache.
    if (!response_headers()->Has(HttpAttributes::kCacheControl)) {
      response_headers()->Add(HttpAttributes::kCacheControl,
                              HttpAttributes::kNoCacheMaxAge0);
    }
    response_headers()->ComputeCaching();

    if (content_length_known() && error_message.empty()) {
      apache_writer_->set_content_length(content_length());
    }
    apache_writer_->OutputHeaders(response_headers());
    if (!error_message.empty()) {
      if (buffered_) {
        error_message.CopyToString(&output_bytes_);
      } else {
        apache_writer_->Write(error_message, message_handler_);
      }
      squelch_output_ = true;
    }
  }
}

// Called by other threads.
void ApacheFetch::HandleDone(bool success) {
  {
    ScopedMutex lock(scheduler_->mutex());
    done_ = true;

    if (status_ok_ && !success) {
      message_handler_->Message(
          kWarning,
          "Response for url %s issued with status %d %s but "
          "failed to complete.",
          mapped_url_.c_str(), response_headers()->status_code(),
          response_headers()->reason_phrase());
    }

    if (buffered_) {
      // Let our owner on the apache request thread know we're done and they
      // will send out anything that still needs sending and then delete us.
      scheduler_->Signal();
    }
  }
}

bool ApacheFetch::HandleWrite(const StringPiece& sp, MessageHandler* handler) {
  if (squelch_output_) {
    return true;  // Suppressing further output after writing error message.
  } else if (buffered_) {
    sp.AppendToString(&output_bytes_);
    return true;
  }
  return apache_writer_->Write(sp, handler);
}

bool ApacheFetch::HandleFlush(MessageHandler* handler) {
  if (buffered_) {
    return true;  // Don't pass flushes through.
  }
  return apache_writer_->Flush(handler);
}

// Called on the apache request thread.  Blocks until the request is retired.
void ApacheFetch::Wait() {
  if (wait_called_) {
    return;
  }
  wait_called_ = true;
  Timer* timer = scheduler_->timer();
  int64 start_ms = timer->NowMs();

  {
    ScopedMutex lock(scheduler_->mutex());

    // Compute the time we want to block on each call to RunTasksUntil
    // below, based on the in-place rewrite deadline and the
    // configured FetcherTimeoutMs.
    //
    // The role of this timeout here is to dictate how often we'll log
    // "Waiting for completion" messages.  The loop will not actually exit
    // until the request is completed, and we are dependent on timeouts
    // configured elsewhere in the code to guarantee that a completion will
    // come at some point.
    //
    // We pick the 'max' of those two values because that will ensure we don't
    // get a large number of spurious messages as a result of (say) configuring
    // the in-place rewrite deadline to be much higher than the fetcher timeout.
    int64 fetch_timeout_ms = std::max(
        options_->blocking_fetch_timeout_ms(),
        static_cast<int64>(options_->in_place_rewrite_deadline_ms()));
    Scheduler::Sequence* scheduler_sequence = driver_->scheduler_sequence();
    while (!scheduler_sequence->RunTasksUntil(fetch_timeout_ms, &done_)) {
      int64 elapsed_ms = timer->NowMs() - start_ms;
      message_handler_->Message(
          kWarning, "Waiting for completion of URL %s for %g sec.",
          mapped_url_.c_str(), elapsed_ms / 1000.0);
    }
    CHECK(done_);

    // A 'true' return from RunTasksUntil means done_==true, but it
    // does not mean all tasks are exhausted.  For example, an
    // in-place rewrite deadline timeout will successfully break out
    // of RunTasksUntil, and we'll want to continue processing even
    // though we are going to retire the request.
    driver_->SwitchToQueuedWorkerPool();
  }
  if (buffered_) {
    SendOutHeaders();
    if (!output_bytes_.empty()) {
      apache_writer_->Write(output_bytes_, message_handler_);
    }
  }
}

bool ApacheFetch::IsCachedResultValid(const ResponseHeaders& headers) {
  ScopedMutex lock(scheduler_->mutex());
  return OptionsAwareHTTPCacheCallback::IsCacheValid(
      mapped_url_, *options_, request_context(), headers);
}

}  // namespace net_instaweb
