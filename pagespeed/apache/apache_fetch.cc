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

namespace net_instaweb {

namespace {

// How long to try waiting before giving up.
const int kDefaultMaxWaitMs = 2 * Timer::kMinuteMs;

}  // namespace

ApacheFetch::ApacheFetch(const GoogleString& mapped_url, StringPiece debug_info,
                         ThreadSystem* thread_system, Timer* timer,
                         ApacheWriter* apache_writer,
                         RequestHeaders* request_headers,
                         const RequestContextPtr& request_context,
                         const RewriteOptions* options, MessageHandler* handler)
    : AsyncFetch(request_context),
      mapped_url_(mapped_url),
      apache_writer_(apache_writer),
      options_(options),
      timer_(timer),
      message_handler_(handler),
      mutex_(thread_system->NewMutex()),
      condvar_(mutex_->NewCondvar()),
      abandoned_(false),
      done_(false),
      headers_sent_(false),
      handle_error_(true),
      status_ok_(false),
      is_proxy_(false),
      blocking_fetch_timeout_ms_(options_->blocking_fetch_timeout_ms()),
      max_wait_ms_(kDefaultMaxWaitMs) {
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
}

ApacheFetch::~ApacheFetch() { }

void ApacheFetch::HandleHeadersComplete() {
  int status_code = response_headers()->status_code();
  status_ok_ = (status_code != 0) && (status_code < 400);

  if (handle_error_ || status_ok_) {
    bool inject_error_message = false;

    // 304 and 204 responses aren't expected to have Content-Types.
    // All other responses should.
    if ((status_code != HttpStatus::kNotModified) &&
        (status_code != HttpStatus::kNoContent) &&
        !response_headers()->Has(HttpAttributes::kContentType)) {
      status_code = HttpStatus::kForbidden;
      status_ok_ = false;
      response_headers()->SetStatusAndReason(HttpStatus::kForbidden);
      response_headers()->Add(HttpAttributes::kContentType, "text/html");
      response_headers()->RemoveAll(HttpAttributes::kCacheControl);
      inject_error_message = true;
    }

    int64 now_ms = timer_->NowMs();
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

    {
      ScopedMutex lock(mutex_.get());
      if (!abandoned_) {
        if (content_length_known() && !inject_error_message) {
          apache_writer_->set_content_length(content_length());
        }

        apache_writer_->OutputHeaders(response_headers());
        headers_sent_ = true;

        if (inject_error_message) {
          apache_writer_->Write(
              "Missing Content-Type required for proxied "
              "resource",
              message_handler_);
          apache_writer_->set_squelch_output(true);
        }
        return;
      }
    }
    message_handler_->Message(kWarning,
                              "HeadersComplete for url %s received after "
                              "being abandoned for timing out.",
                              mapped_url_.c_str());
    return;  // Don't do anything.
  }
}

void ApacheFetch::HandleDone(bool success) {
  {
    ScopedMutex lock(mutex_.get());
    done_ = true;

    if (!abandoned_) {
      if (status_ok_ && !success) {
        message_handler_->Message(
            kWarning,
            "Response for url %s issued with status %d %s but "
            "failed to complete.",
            mapped_url_.c_str(), response_headers()->status_code(),
            response_headers()->reason_phrase());
      }
      // We've not been abandoned; let our owner know we're done and they will
      // delete us.
      condvar_->Signal();
      return;
    }
    message_handler_->Message(
        kWarning,
        "Response for url %s completed with status %d %s after "
        "being abandoned for timing out.",
        mapped_url_.c_str(), response_headers()->status_code(),
        response_headers()->reason_phrase());
  }
  // We've been abandoned, so we have ownership of ourself.
  delete this;
}

bool ApacheFetch::HandleWrite(const StringPiece& sp, MessageHandler* handler) {
  {
    ScopedMutex lock(mutex_.get());
    if (!abandoned_) {
      return apache_writer_->Write(sp, handler);
    }
  }
  handler->Message(kWarning,
                   "Write of %ld bytes for url %s received after "
                   "being abandoned for timing out.",
                   sp.size(), mapped_url_.c_str());
  return false;  // Drop the write.
}

bool ApacheFetch::HandleFlush(MessageHandler* handler) {
  {
    ScopedMutex lock(mutex_.get());
    if (!abandoned_) {
      return apache_writer_->Flush(handler);
    }
  }
  handler->Message(kWarning,
                   "Flush for url %s received after "
                   "being abandoned for timing out.",
                   mapped_url_.c_str());
  return false;  // Drop the flush.
}

ApacheFetch::WaitResult ApacheFetch::Wait(const RewriteDriver* rewrite_driver) {
  int64 start_ms = timer_->NowMs();
  WaitResult result = kWaitSuccess;
  mutex_->Lock();
  while (!done_ && (result == kWaitSuccess)) {
    condvar_->TimedWait(blocking_fetch_timeout_ms_);
    if (!done_) {
      int64 elapsed_ms = timer_->NowMs() - start_ms;
      if (elapsed_ms > max_wait_ms_) {
        abandoned_ = true;
        // Now that we're abandoned the instaweb context needs to drop its
        // pointer to us and not free us.  We tell it to do this by returning
        // kAbandonedAndHandled / kAbandonedAndDeclined.
        //
        // If we never sent headers out then it's safe to DECLINE this request
        // and pass it to a different content handler.  Otherwise some data was
        // sent out and we have to accept this as it is.
        result = headers_sent_ ? kAbandonedAndHandled : kAbandonedAndDeclined;
      }

      // Unlock briefly so we can write messages without holding the lock.
      mutex_->Unlock();
      if (result != kWaitSuccess) {
        message_handler_->Message(
            kWarning, "Abandoned URL %s after %g sec (debug=%s, driver=%s).",
            mapped_url_.c_str(), elapsed_ms / 1000.0, debug_info_.c_str(),
            (rewrite_driver == NULL
                 ? "null rewrite driver, should only be used in tests"
                 : rewrite_driver->ToString(true /* show_detached_contexts */)
                       .c_str()));
      } else {
        message_handler_->Message(
            kWarning, "Waiting for completion of URL %s for %g sec.",
            mapped_url_.c_str(), elapsed_ms / 1000.0);
      }
      mutex_->Lock();
    }
  }
  mutex_->Unlock();
  return result;
}

bool ApacheFetch::IsCachedResultValid(const ResponseHeaders& headers) {
  ScopedMutex lock(mutex_.get());
  // options_ isn't valid after we're abandoned, so make sure we haven't been.
  if (abandoned_) {
    return false;
  }
  return OptionsAwareHTTPCacheCallback::IsCacheValid(
      mapped_url_, *options_, request_context(), headers);
}

}  // namespace net_instaweb
