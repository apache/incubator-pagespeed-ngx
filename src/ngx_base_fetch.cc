/*
 * Copyright 2012 Google Inc.
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

// Author: jefftk@google.com (Jeff Kaufman)
#include <unistd.h> //for usleep

#include "ngx_base_fetch.h"
#include "ngx_event_connection.h"
#include "ngx_list_iterator.h"

#include "ngx_pagespeed.h"

#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

const char kHeadersComplete = 'H';
const char kFlush = 'F';
const char kDone = 'D';

NgxEventConnection* NgxBaseFetch::event_connection = NULL;
int NgxBaseFetch::active_base_fetches = 0;

NgxBaseFetch::NgxBaseFetch(ngx_http_request_t* r,
                           NgxServerContext* server_context,
                           const RequestContextPtr& request_ctx,
                           PreserveCachingHeaders preserve_caching_headers,
                           NgxBaseFetchType base_fetch_type)
    : AsyncFetch(request_ctx),
      request_(r),
      server_context_(server_context),
      done_called_(false),
      last_buf_sent_(false),
      references_(2),
      base_fetch_type_(base_fetch_type),
      preserve_caching_headers_(preserve_caching_headers),
      detached_(false),
      suppress_(false) {
  if (pthread_mutex_init(&mutex_, NULL)) CHECK(0);
  __sync_add_and_fetch(&NgxBaseFetch::active_base_fetches, 1);
}

NgxBaseFetch::~NgxBaseFetch() {
  pthread_mutex_destroy(&mutex_);
  __sync_add_and_fetch(&NgxBaseFetch::active_base_fetches, -1);
}

bool NgxBaseFetch::Initialize(ngx_cycle_t* cycle) {
  CHECK(event_connection == NULL) << "event connection already set";
  event_connection = new NgxEventConnection(ReadCallback);
  return event_connection->Init(cycle);
}

void NgxBaseFetch::Terminate() {
  if (event_connection != NULL) {
    GoogleMessageHandler handler;
    PosixTimer timer;
    int64 timeout_us = Timer::kSecondUs * 30;
    int64 end_us = timer.NowUs() + timeout_us;
    static unsigned int sleep_microseconds = 100;

    handler.Message(
        kInfo,"NgxBaseFetch::Terminate rounding up %d active base fetches.",
        NgxBaseFetch::active_base_fetches);

    // Try to continue processing and get the active base fetch count to 0
    // untill the timeout expires.
    // TODO(oschaaf): This needs more work.
    while (NgxBaseFetch::active_base_fetches > 0 && end_us > timer.NowUs()) {
      event_connection->Drain();
      usleep(sleep_microseconds);
    }

    if (NgxBaseFetch::active_base_fetches != 0) {
      handler.Message(
          kWarning,"NgxBaseFetch::Terminate timed out with %d active base fetches.",
          NgxBaseFetch::active_base_fetches);
    }

    // Close down the named pipe.
    event_connection->Shutdown();
    delete event_connection;
    event_connection = NULL;
  }
}

const char* BaseFetchTypeToCStr(NgxBaseFetchType type) {
  switch(type) {
    case kPageSpeedResource:
      return "ps resource";
    case kHtmlTransform:
      return "html transform";
    case kAdminPage:
      return "admin page";
    case kIproLookup:
      return "ipro lookup";
  }
  CHECK(false);
  return "can't get here";
}

void NgxBaseFetch::ReadCallback(const ps_event_data& data) {
  NgxBaseFetch* base_fetch = reinterpret_cast<NgxBaseFetch*>(data.sender);
  ngx_http_request_t* r = base_fetch->request();
  bool detached = base_fetch->detached();
  const char* type = BaseFetchTypeToCStr(base_fetch->base_fetch_type_);
  int refcount = base_fetch->DecrementRefCount();

  #if (NGX_DEBUG)
  ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
     "pagespeed [%p] event: %c. bf:%p (%s) - refcnt:%d - det: %c", r,
     data.type, base_fetch, type, refcount, detached ? 'Y': 'N');
  #endif

  // If we ended up destructing the base fetch, or the request context is
  // detached, skip this event.
  if (refcount == 0 || detached) {
    return;
  }
  ps_request_ctx_t* ctx = ps_get_request_context(r);

  CHECK(data.sender == ctx->base_fetch);
  CHECK(r->count > 0) << "r->count: " << r->count;

  int rc;
  // If we are unlucky enough to have our connection finalized mid-ipro-lookup,
  // we must enter a different flow. Also see ps_in_place_check_header_filter().
  if ((ctx->base_fetch->base_fetch_type_ != kIproLookup)
      && r->connection->error) {
    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
      "pagespeed [%p] request already finalized", r);
    rc = NGX_ERROR;
  } else {
    rc = ps_base_fetch::ps_base_fetch_handler(r);
  }

#if (NGX_DEBUG)
  ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0,
                "pagespeed [%p] ps_base_fetch_handler() returned %d for %c",
                r, rc, data.type);
#endif

  ngx_connection_t* c = r->connection;
  ngx_http_finalize_request(r, rc);
  // See http://forum.nginx.org/read.php?2,253006,253061
  ngx_http_run_posted_requests(c);
}

void NgxBaseFetch::Lock() {
  pthread_mutex_lock(&mutex_);
}

void NgxBaseFetch::Unlock() {
  pthread_mutex_unlock(&mutex_);
}

bool NgxBaseFetch::HandleWrite(const StringPiece& sp,
                               MessageHandler* handler) {
  Lock();
  buffer_.append(sp.data(), sp.size());
  Unlock();
  return true;
}

// should only be called in nginx thread
ngx_int_t NgxBaseFetch::CopyBufferToNginx(ngx_chain_t** link_ptr) {
  CHECK(!(done_called_ && last_buf_sent_))
        << "CopyBufferToNginx() was called after the last buffer was sent";

  // there is no buffer to send
  if (!done_called_ && buffer_.empty()) {
    *link_ptr = NULL;
    return NGX_AGAIN;
  }

  int rc = string_piece_to_buffer_chain(
      request_->pool, buffer_, link_ptr, done_called_ /* send_last_buf */);
  if (rc != NGX_OK) {
    return rc;
  }

  // Done with buffer contents now.
  buffer_.clear();

  if (done_called_) {
    last_buf_sent_ = true;
    return NGX_OK;
  }

  return NGX_AGAIN;
}

// There may also be a race condition if this is called between the last Write()
// and Done() such that we're sending an empty buffer with last_buf set, which I
// think nginx will reject.
ngx_int_t NgxBaseFetch::CollectAccumulatedWrites(ngx_chain_t** link_ptr) {
  ngx_int_t rc;
  Lock();
  rc = CopyBufferToNginx(link_ptr);
  Unlock();
  return rc;
}

ngx_int_t NgxBaseFetch::CollectHeaders(ngx_http_headers_out_t* headers_out) {
  const ResponseHeaders* pagespeed_headers = response_headers();

  if (content_length_known()) {
     headers_out->content_length = NULL;
     headers_out->content_length_n = content_length();
  }

  return copy_response_headers_to_ngx(request_, *pagespeed_headers,
                                      preserve_caching_headers_);
}

void NgxBaseFetch::RequestCollection(char type) {
  if (suppress_) {
    return;
  }

  // We must optimistically increment the refcount, and decrement it
  // when we conclude we failed. If we only increment on a successfull write,
  // there's a small chance that between writing and adding to the refcount
  // both pagespeed and nginx will release their refcount -- destructing
  // this NgxBaseFetch instance.
  IncrementRefCount();
  if (!event_connection->WriteEvent(type, this)) {
    DecrementRefCount();
  }
}

void NgxBaseFetch::HandleHeadersComplete() {
  int status_code = response_headers()->status_code();
  bool status_ok = (status_code != 0) && (status_code < 400);

  if ((base_fetch_type_ != kIproLookup) || status_ok) {
    // If this is a 404 response we need to count it in the stats.
    if (response_headers()->status_code() == HttpStatus::kNotFound) {
      server_context_->rewrite_stats()->resource_404_count()->Add(1);
    }
  }

  RequestCollection(kHeadersComplete);  // Headers available.

  // For the IPRO lookup, supress notification of the nginx side here.
  // If we send both the headerscomplete event and the one from done, nasty
  // stuff will happen if we loose the race with with the nginx side destructing
  // this base fetch instance.
  if (base_fetch_type_ == kIproLookup && !status_ok) {
    suppress_ = true;
  }
}

bool NgxBaseFetch::HandleFlush(MessageHandler* handler) {
  RequestCollection(kFlush);  // A new part of the response body is available
  return true;
}

int NgxBaseFetch::DecrementRefCount() {
  return DecrefAndDeleteIfUnreferenced();
}

int NgxBaseFetch::IncrementRefCount() {
  return __sync_add_and_fetch(&references_, 1);
}

int NgxBaseFetch::DecrefAndDeleteIfUnreferenced() {
  // Creates a full memory barrier.
  int r = __sync_add_and_fetch(&references_, -1);
  if (r == 0) {
    delete this;
  }
  return r;
}

void NgxBaseFetch::HandleDone(bool success) {
  // TODO(jefftk): it's possible that instead of locking here we can just modify
  // CopyBufferToNginx to only read done_called_ once.
  CHECK(!done_called_) << "Done already called!";
  Lock();
  done_called_ = true;
  Unlock();
  RequestCollection(kDone);
  DecrefAndDeleteIfUnreferenced();
}

}  // namespace net_instaweb
