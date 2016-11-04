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
#include "net/instaweb/http/public/request_timing_info.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

// TODO(piatek): Remove this default constructor that permits NULL request
// contexts.
AsyncFetch::AsyncFetch()
    : request_headers_(NULL),
      response_headers_(NULL),
      extra_response_headers_(NULL),
      request_ctx_(NULL),
      owns_request_headers_(false),
      owns_response_headers_(false),
      owns_extra_response_headers_(false),
      headers_complete_(false),
      content_length_(kContentLengthUnknown) {
}

AsyncFetch::AsyncFetch(const RequestContextPtr& request_ctx)
    : request_headers_(NULL),
      response_headers_(NULL),
      extra_response_headers_(NULL),
      request_ctx_(request_ctx),
      owns_request_headers_(false),
      owns_response_headers_(false),
      owns_extra_response_headers_(false),
      headers_complete_(false),
      content_length_(kContentLengthUnknown) {
  DCHECK(request_ctx_.get() != NULL);
}

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
}

AbstractLogRecord* AsyncFetch::log_record() {
  CHECK(request_context().get() != NULL);
  return request_context()->log_record();
}

bool AsyncFetch::Write(const StringPiece& sp, MessageHandler* handler) {
  bool ret = true;
  if (!sp.empty()) {  // empty-writes should be no-ops.
    if (!headers_complete_) {
      HeadersComplete();
    }
    if (request_headers()->method() == RequestHeaders::kHead) {
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
    if (!success) {
      if (response_headers()->status_code() == 0) {
        // Failing fetches might not set status codes but we expect
        // successful ones to.
        response_headers()->set_status_code(HttpStatus::kNotFound);
      } else if (response_headers()->status_code() == HttpStatus::kOK) {
        // Our API here is not ideal when Done is called with success=false.
        // The problem is that we need to call HeadersComplete before we call
        // HandleDone, but there's no way to tell HeadersComplete that we're in
        // a bad state and it should throw away anything it has and return an
        // error to the visitor.  This was a problem with ngx_pagespeed, because
        // it sends out the headers immediately on HeadersComplete.
        //
        // Since this can't be handled by the callee, handle it here.
        response_headers()->Clear();
        response_headers()->set_status_code(HttpStatus::kInternalServerError);
      }
    }
    response_headers()->ComputeCaching();
    HeadersComplete();
  }
  // You might think we could put DCHECK here that either !success or
  // response_headers()->IsErrorStatus(), right?  The way to abort a request is
  // either to report a failure in your response headers, or to call
  // Done(success=false) and have the code for !success above set failing
  // response headers for you.  (The first one is better, because Done(false)
  // doesn't know whether it should be reporting a 4xx or 5xx error and
  // sysadmins generally care about things like this.)
  //
  // The problem is, what about errors we don't discover until after we've sent
  // out headers?  For example, say we're streaming gzipped data from somewhere,
  // ungzipping it as we go, and we hit a gzip decoding error.  Whoops!  Nothing
  // we can really do there, except abort the request, and currently we call
  // Done(false) when we do that.
  //
  // TODO(jefftk): remove Done(bool) and require everyone to just set failing
  // response headers if that's what they want.
  HandleDone(success);
}

// Sets the request-headers to the specified pointer.  The caller must
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

void AsyncFetch::SetRequestHeadersTakingOwnership(RequestHeaders* headers) {
  set_request_headers(headers);
  owns_request_headers_ = true;
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
    response_headers_ = new ResponseHeaders(request_ctx_->options());
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
    extra_response_headers_ = new ResponseHeaders(request_ctx_->options());
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

GoogleString AsyncFetch::LoggingString() {
  GoogleString logging_info_str;

  if (NULL == request_ctx_.get()) {
    return logging_info_str;
  }

  int64 latency;
  const RequestTimingInfo& timing_info = request_ctx_->timing_info();
  if (timing_info.GetHTTPCacheLatencyMs(&latency)) {
    StrAppend(&logging_info_str, "c1:", Integer64ToString(latency), ";");
  }
  if (timing_info.GetL2HTTPCacheLatencyMs(&latency)) {
    StrAppend(&logging_info_str, "c2:", Integer64ToString(latency), ";");
  }
  if (timing_info.GetFetchHeaderLatencyMs(&latency)) {
    StrAppend(&logging_info_str, "hf:", Integer64ToString(latency), ";");
  }
  if (timing_info.GetFetchLatencyMs(&latency)) {
    StrAppend(&logging_info_str, "f:", Integer64ToString(latency), ";");
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
    : AsyncFetch(base_fetch->request_context()),
      base_fetch_(base_fetch) {
  set_response_headers(base_fetch->response_headers());
  set_extra_response_headers(base_fetch->extra_response_headers());
  set_request_headers(base_fetch->request_headers());
}

SharedAsyncFetch::~SharedAsyncFetch() {
}

void SharedAsyncFetch::PropagateContentLength() {
  if (content_length_known()) {
    base_fetch_->set_content_length(content_length());
  }
}

void SharedAsyncFetch::HandleHeadersComplete() {
  PropagateContentLength();
  base_fetch_->HeadersComplete();
}

void AsyncFetch::FixCacheControlForGoogleCache() {
  ConstStringStarVector values;
  if (request_headers()->Lookup(HttpAttributes::kVia, &values)) {
    for (int i = 0, n = values.size(); i < n; ++i) {
      // See https://github.com/pagespeed/ngx_pagespeed/issues/1149
      // In general, it is not necessary to include a specific 'public'
      // in a cache-control entry to allow a proxy-cache to cache it;
      // it's sufficient to specify a max-age, as long as 'private'
      // is not present.
      //
      // However, the Google Cloud CDN cache requires it, with some
      // controversy.  See this discussion with Mark Nottingham:
      // https://groups.google.com/forum/#!searchin/pagespeed-insights-discuss/nottingham/pagespeed-insights-discuss/NWwrz1By36c/9RN9sHdj9EIJ
      //
      // Mark's comment about Firefox treating public differently is
      // no longer current.
      //
      // However the Google Cloud CDN help page on caching unambiguously
      // requires that 'public' be included in Cache-Control to enable
      // the Google Cache.
      // https://cloud.google.com/cdn/docs/caching#cacheability
      //
      // We only need to put in the 'public" header if there's
      // a "Via: 1.1 google" header.  As there's only a small cost
      // in bytes, we'll just scan the via value for 'google', so
      // that a future HTTP rev won't break caching.
      if (IsGoogleCacheVia(*values[i])) {
        response_headers()->SetCacheControlPublic();
        break;
      }
    }
  }
}

// static
bool AsyncFetch::IsGoogleCacheVia(StringPiece via_value) {
  // The value of the Google Via header as of this writing
  // is "1.1 google".  In an attempt to be future proof, we'll
  // match "X.Y google", and be case-insensitive matching "google".
  StringPieceVector tokens;
  SplitStringPieceToVector(via_value, " ", &tokens, true);
  double version;
  return
      (tokens.size() == 2) &&
      StringCaseEqual(tokens[1], "google") &&
      StringToDouble(tokens[0], &version);
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
    StringPiece contents;
    fallback_.ExtractContents(&contents);
    set_content_length(contents.size());
    SharedAsyncFetch::HandleHeadersComplete();
    SharedAsyncFetch::HandleWrite(contents, handler_);
    SharedAsyncFetch::HandleFlush(handler_);
    if (fallback_responses_served_ != NULL) {
      fallback_responses_served_->Add(1);
    }
    // Do not call Done() on the base fetch yet since it could delete shared
    // pointers.
  } else {
    SharedAsyncFetch::HandleHeadersComplete();
  }
}

bool FallbackSharedAsyncFetch::HandleWrite(const StringPiece& content,
                                           MessageHandler* handler) {
  if (serving_fallback_) {
    return true;
  }
  return SharedAsyncFetch::HandleWrite(content, handler);
}

bool FallbackSharedAsyncFetch::HandleFlush(MessageHandler* handler) {
  if (serving_fallback_) {
    return true;
  }
  return SharedAsyncFetch::HandleFlush(handler);
}

void FallbackSharedAsyncFetch::HandleDone(bool success) {
  SharedAsyncFetch::HandleDone(serving_fallback_ || success);
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
      ResponseHeaders cached_response_headers(request_context()->options());
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
    SharedAsyncFetch::HandleHeadersComplete();
    StringPiece contents;
    cached_value_.ExtractContents(&contents);
    SharedAsyncFetch::HandleWrite(contents, handler_);
    SharedAsyncFetch::HandleFlush(handler_);
    // Do not call Done() on the base fetch yet since it could delete shared
    // pointers.
    if (num_conditional_refreshes_ != NULL) {
      num_conditional_refreshes_->Add(1);
    }
  } else {
    SharedAsyncFetch::HandleHeadersComplete();
  }
}

bool ConditionalSharedAsyncFetch::HandleWrite(const StringPiece& content,
                                              MessageHandler* handler) {
  if (serving_cached_value_) {
    return true;
  }
  return SharedAsyncFetch::HandleWrite(content, handler);
}

bool ConditionalSharedAsyncFetch::HandleFlush(MessageHandler* handler) {
  if (serving_cached_value_) {
    return true;
  }
  return SharedAsyncFetch::HandleFlush(handler);
}

void ConditionalSharedAsyncFetch::HandleDone(bool success) {
  SharedAsyncFetch::HandleDone(serving_cached_value_ || success);
  delete this;
}

}  // namespace net_instaweb
