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

// Author: jmarantz@google.com (Joshua Marantz)
//         sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/http/public/async_fetch.h"

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/statistics.h"

namespace net_instaweb {

AsyncFetch::~AsyncFetch() {
  if (owns_response_headers_) {
    delete response_headers_;
  }
  if (owns_extra_response_headers_) {
    delete extra_response_headers_;
  }
  if (owns_request_headers_) {
    delete request_headers_;
  }
  if (owns_log_record_) {
    delete log_record_;
  }
}

bool AsyncFetch::Write(const StringPiece& sp, MessageHandler* handler) {
  bool ret = true;
  if (!sp.empty()) {  // empty-writes should be no-ops.
    if (!headers_complete_) {
      HeadersComplete();
    } else if (request_headers()->method() == RequestHeaders::kHead) {
      // If the request is a head request, then don't write the contents of
      // body.
      return ret;
    }
    ret = HandleWrite(sp, handler);
  }
  return ret;
}

bool AsyncFetch::Flush(MessageHandler* handler) {
  if (!headers_complete_) {
    HeadersComplete();
  }
  return HandleFlush(handler);
}

void AsyncFetch::HeadersComplete() {
  DCHECK_NE(0, response_headers()->status_code());
  if (headers_complete_) {
    LOG(DFATAL) << "AsyncFetch::HeadersComplete() called twice.";
  } else {
    headers_complete_ = true;
    HandleHeadersComplete();
  }
}

void AsyncFetch::Done(bool success) {
  if (!headers_complete_) {
    if (!success &&
        (response_headers()->status_code() == 0)) {
      // Failing fetches might not set status codes but we expect
      // successful ones to.
      response_headers()->set_status_code(HttpStatus::kNotFound);
    }
    HeadersComplete();
  }
  HandleDone(success);
}

// Sets the request-headers to the specifid pointer.  The caller must
// guarantee that the pointed-to headers remain valid as long as the
// AsyncFetch is running.
void AsyncFetch::set_request_headers(RequestHeaders* headers) {
  DCHECK(!owns_request_headers_);
  if (owns_request_headers_) {
    delete request_headers_;
  }
  request_headers_ = headers;
  owns_request_headers_ = false;
}

RequestHeaders* AsyncFetch::request_headers() {
  // TODO(jmarantz): Consider DCHECKing that only const reads occur
  // after headers_complete_.
  if (request_headers_ == NULL) {
    request_headers_ = new RequestHeaders;
    owns_request_headers_ = true;
  }
  return request_headers_;
}

const RequestHeaders* AsyncFetch::request_headers() const {
  CHECK(request_headers_ != NULL);
  return request_headers_;
}

ResponseHeaders* AsyncFetch::response_headers() {
  if (response_headers_ == NULL) {
    response_headers_ = new ResponseHeaders;
    owns_response_headers_ = true;
  }
  return response_headers_;
}

void AsyncFetch::set_response_headers(ResponseHeaders* headers) {
  DCHECK(!owns_response_headers_);
  if (owns_response_headers_) {
    delete response_headers_;
  }
  response_headers_ = headers;
  owns_response_headers_ = false;
}

ResponseHeaders* AsyncFetch::extra_response_headers() {
  if (extra_response_headers_ == NULL) {
    extra_response_headers_ = new ResponseHeaders;
    owns_extra_response_headers_ = true;
  }
  return extra_response_headers_;
}

void AsyncFetch::set_extra_response_headers(ResponseHeaders* headers) {
  DCHECK(!owns_extra_response_headers_);
  if (owns_extra_response_headers_) {
    delete extra_response_headers_;
  }
  extra_response_headers_ = headers;
  owns_extra_response_headers_ = false;
}

LoggingInfo* AsyncFetch::logging_info() {
  return log_record()->logging_info();
}

LogRecord* AsyncFetch::log_record() {
  if (log_record_ == NULL) {
    log_record_ = new LogRecord();
    owns_log_record_ = true;
  }
  return log_record_;
}

void AsyncFetch::set_log_record(LogRecord* log_record) {
  DCHECK(!owns_log_record_);
  if (owns_log_record_) {
    delete log_record_;
  }
  log_record_ = log_record;
  owns_log_record_ = false;
}

void AsyncFetch::set_owned_log_record(LogRecord* log_record) {
  if (owns_log_record_) {
    delete log_record_;
  }
  log_record_ = log_record;
  owns_log_record_ = true;
}

GoogleString AsyncFetch::LoggingString() {
  GoogleString logging_info_str;
  if (logging_info() != NULL) {
    if (logging_info()->has_timing_info()) {
      const TimingInfo timing_info = logging_info()->timing_info();
      if (timing_info.has_cache1_ms()) {
        StrAppend(&logging_info_str, "c1:",
                  Integer64ToString(timing_info.cache1_ms()), ";");
      }
      if (timing_info.has_cache2_ms()) {
        StrAppend(&logging_info_str, "c2:",
                  Integer64ToString(timing_info.cache2_ms()), ";");
      }
      if (timing_info.has_header_fetch_ms()) {
        StrAppend(&logging_info_str, "hf:",
                  Integer64ToString(timing_info.header_fetch_ms()), ";");
      }
      if (timing_info.has_fetch_ms()) {
        StrAppend(&logging_info_str, "f:",
                  Integer64ToString(timing_info.fetch_ms()), ";");
      }
    }
  }
  return logging_info_str;
}

StringAsyncFetch::~StringAsyncFetch() {
}

bool AsyncFetchUsingWriter::HandleWrite(const StringPiece& sp,
                                        MessageHandler* handler) {
  return writer_->Write(sp, handler);
}

bool AsyncFetchUsingWriter::HandleFlush(MessageHandler* handler) {
  return writer_->Flush(handler);
}

AsyncFetchUsingWriter::~AsyncFetchUsingWriter() {
}

SharedAsyncFetch::SharedAsyncFetch(AsyncFetch* base_fetch)
    : base_fetch_(base_fetch) {
  set_response_headers(base_fetch->response_headers());
  set_extra_response_headers(base_fetch->extra_response_headers());
  set_request_headers(base_fetch->request_headers());
  set_log_record(base_fetch->log_record());
}

SharedAsyncFetch::~SharedAsyncFetch() {
}

const char FallbackSharedAsyncFetch::kStaleWarningHeaderValue[] =
    "110 Response is stale";

FallbackSharedAsyncFetch::FallbackSharedAsyncFetch(AsyncFetch* base_fetch,
                                                   HTTPValue* fallback,
                                                   MessageHandler* handler)
    : SharedAsyncFetch(base_fetch),
      handler_(handler),
      serving_fallback_(false),
      fallback_responses_served_(NULL) {
  if (fallback != NULL && !fallback->Empty()) {
    fallback_.Link(fallback);
  }
}

FallbackSharedAsyncFetch::~FallbackSharedAsyncFetch() {}

void FallbackSharedAsyncFetch::HandleHeadersComplete() {
  if (response_headers()->IsServerErrorStatus() && !fallback_.Empty()) {
    // If the fetch resulted in a server side error from the origin, stop
    // passing any events through to the base fetch until HandleDone().
    serving_fallback_ = true;
    response_headers()->Clear();
    fallback_.ExtractHeaders(response_headers(), handler_);
    // Add a warning header indicating that the response is stale.
    response_headers()->Add(HttpAttributes::kWarning, kStaleWarningHeaderValue);
    response_headers()->ComputeCaching();
    base_fetch()->HeadersComplete();
    StringPiece contents;
    fallback_.ExtractContents(&contents);
    base_fetch()->Write(contents, handler_);
    base_fetch()->Flush(handler_);
    if (fallback_responses_served_ != NULL) {
      fallback_responses_served_->Add(1);
    }
    // Do not call Done() on the base fetch yet since it could delete shared
    // pointers.
  } else {
    base_fetch()->HeadersComplete();
  }
}

bool FallbackSharedAsyncFetch::HandleWrite(const StringPiece& content,
                                           MessageHandler* handler) {
  if (serving_fallback_) {
    return true;
  }
  return base_fetch()->Write(content, handler);
}

bool FallbackSharedAsyncFetch::HandleFlush(MessageHandler* handler) {
  if (serving_fallback_) {
    return true;
  }
  return base_fetch()->Flush(handler);
}

void FallbackSharedAsyncFetch::HandleDone(bool success) {
  base_fetch()->Done(serving_fallback_ || success);
  delete this;
}

ConditionalSharedAsyncFetch::ConditionalSharedAsyncFetch(
    AsyncFetch* base_fetch,
    HTTPValue* cached_value,
    MessageHandler* handler)
    : SharedAsyncFetch(base_fetch),
      handler_(handler),
      serving_cached_value_(false),
      added_conditional_headers_to_request_(false),
      num_conditional_refreshes_(NULL) {
  if (cached_value != NULL && !cached_value->Empty()) {
    // Only do our own conditional fetch if the original request wasn't
    // conditional.
    if (!request_headers()->Has(HttpAttributes::kIfModifiedSince) &&
        !request_headers()->Has(HttpAttributes::kIfNoneMatch)) {
      ResponseHeaders cached_response_headers;
      cached_value->ExtractHeaders(&cached_response_headers, handler_);
      // Check that the cached response is a 200.
      if (cached_response_headers.status_code() == HttpStatus::kOK) {
        // Copy the Etag and Last-Modified if any into the If-None-Match and
        // If-Modified-Since request headers. Also, ensure that the Etag wasn't
        // added by us.
        const char* etag = cached_response_headers.Lookup1(
            HttpAttributes::kEtag);
        if (etag != NULL && !StringCaseStartsWith(etag,
                                                  HTTPCache::kEtagPrefix)) {
          request_headers()->Add(HttpAttributes::kIfNoneMatch, etag);
          added_conditional_headers_to_request_ = true;
        }
        const char* last_modified = cached_response_headers.Lookup1(
            HttpAttributes::kLastModified);
        if (last_modified != NULL) {
          request_headers()->Add(HttpAttributes::kIfModifiedSince,
                                 last_modified);
          added_conditional_headers_to_request_ = true;
        }
      }
      if (added_conditional_headers_to_request_) {
        cached_value_.Link(cached_value);
      }
    }
  }
}

ConditionalSharedAsyncFetch::~ConditionalSharedAsyncFetch() {}

void ConditionalSharedAsyncFetch::HandleHeadersComplete() {
  if (added_conditional_headers_to_request_ &&
      response_headers()->status_code() == HttpStatus::kNotModified) {
    // If the fetch resulted in a 304 from the server, serve the cached response
    // and stop passing any events through to the base fetch.
    serving_cached_value_ = true;
    int64 implicit_cache_ttl_ms = response_headers()->implicit_cache_ttl_ms();
    response_headers()->Clear();
    cached_value_.ExtractHeaders(response_headers(), handler_);
    if (response_headers()->is_implicitly_cacheable()) {
      response_headers()->SetCacheControlMaxAge(implicit_cache_ttl_ms);
      response_headers()->ComputeCaching();
    }
    base_fetch()->HeadersComplete();
    StringPiece contents;
    cached_value_.ExtractContents(&contents);
    base_fetch()->Write(contents, handler_);
    base_fetch()->Flush(handler_);
    // Do not call Done() on the base fetch yet since it could delete shared
    // pointers.
    if (num_conditional_refreshes_ != NULL) {
      num_conditional_refreshes_->Add(1);
    }
  } else {
    base_fetch()->HeadersComplete();
  }
}

bool ConditionalSharedAsyncFetch::HandleWrite(const StringPiece& content,
                                              MessageHandler* handler) {
  if (serving_cached_value_) {
    return true;
  }
  return base_fetch()->Write(content, handler);
}

bool ConditionalSharedAsyncFetch::HandleFlush(MessageHandler* handler) {
  if (serving_cached_value_) {
    return true;
  }
  return base_fetch()->Flush(handler);
}

void ConditionalSharedAsyncFetch::HandleDone(bool success) {
  base_fetch()->Done(serving_cached_value_ || success);
  delete this;
}

}  // namespace net_instaweb
