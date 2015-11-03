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
      wait_called_(false),
      handle_error_(true),
      squelch_output_(false),
      status_ok_(false),
      is_proxy_(false),
      buffered_(true),
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

// Called by other threads unless buffered=false.
void ApacheFetch::HandleHeadersComplete() {
  if (buffered_) {
    // Do nothing on this thread right now.  When done waiting we'll deal with
    // headers on the request thread.
    return;
  }
  {
    ScopedMutex lock(mutex_.get());
    CHECK(!abandoned_);  // Can't be abandoned in unbuffered mode.
    SendOutHeaders();
  }
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

      if (buffered_) {
        // We've not been abandoned; let our owner on the apache request thread
        // know we're done and they will send out anything that still needs
        // sending and then delete us.
        condvar_->Signal();
      }
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

// Called by other threads, unless buffered=false.
bool ApacheFetch::HandleWrite(const StringPiece& sp, MessageHandler* handler) {
  {
    ScopedMutex lock(mutex_.get());
    if (!abandoned_) {
      if (squelch_output_) {
        return true;  // Suppressing further output after writing error message.
      } else if (buffered_) {
        sp.AppendToString(&output_bytes_);
        return true;
      } else {
        return apache_writer_->Write(sp, handler);
      }
    }
  }
  handler->Message(kWarning,
                   "Write of %zu bytes for url %s received after "
                   "being abandoned for timing out.",
                   sp.size(), mapped_url_.c_str());
  return false;  // Drop the write.
}

// Called by other threads, unless buffered=false.
bool ApacheFetch::HandleFlush(MessageHandler* handler) {
  if (buffered_) {
    return true;  // Don't pass flushes through.
  }
  {
    ScopedMutex lock(mutex_.get());
    CHECK(!abandoned_);  // Can't be abandoned in unbuffered mode.
    return apache_writer_->Flush(handler);
  }
}

// Called on the apache request thread.
bool ApacheFetch::Wait(const RewriteDriver* rewrite_driver) {
  if (wait_called_) {
    return true;  // Nothing to do.
  }
  if (!buffered_) {
    // If we're not buffered Wait() is only being called on us because it's a
    // code path shared with buffered code.  We should always be done at this
    // point.
    //
    // It's also not a performance issue to take the lock, as there should never
    // be contention.
    ScopedMutex lock(mutex_.get());
    CHECK(done_);
    return true;
  }
  wait_called_ = true;
  int64 start_ms = timer_->NowMs();
  mutex_->Lock();
  while (!done_) {
    condvar_->TimedWait(blocking_fetch_timeout_ms_);
    if (!done_) {
      int64 elapsed_ms = timer_->NowMs() - start_ms;
      if (elapsed_ms > max_wait_ms_) {
        abandoned_ = true;
        message_handler_->Message(
            kWarning, "Abandoned URL %s after %g sec (debug=%s, driver=%s).",
            mapped_url_.c_str(), elapsed_ms / 1000.0, debug_info_.c_str(),
            (rewrite_driver == NULL
                 ? "null rewrite driver, should only be used in tests"
                 : rewrite_driver->ToString(true /* show_detached_contexts */)
                       .c_str()));
        // Now that we're abandoned the instaweb context needs to drop its
        // pointer to us and not free us.
        //
        // Once we've abandoned and unlocked, we could be deleted at any moment
        // by a call to Done().  So make no member accesses after unlocking.
        mutex_->Unlock();
        return false;  // Abandoned with nothing written (because we're a
                       // buffered fetch), caller can DECLINE to handle request.
      }
      message_handler_->Message(
          kWarning, "Waiting for completion of URL %s for %g sec.",
          mapped_url_.c_str(), elapsed_ms / 1000.0);
    }
  }
  SendOutHeaders();
  if (!output_bytes_.empty()) {
    apache_writer_->Write(output_bytes_, message_handler_);
  }
  mutex_->Unlock();
  return true;  // handled
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
