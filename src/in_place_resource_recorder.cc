// Copyright 2013 Google Inc.
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
// Author: sligocki@google.com (Shawn Ligocki)

// Copied from net/instaweb/apache/in_place_resource_recorder.cc
// The next release of PSOL will include that file, at which point this file can
// be removed.
// TODO(jefftk): delete this when the next PSOL release after 1.6.29.3 is ready.

#include "in_place_resource_recorder.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

namespace {

const char kNumResources[] = "ipro_recorder_resources";
const char kNumInsertedIntoCache[] = "ipro_recorder_inserted_into_cache";
const char kNumNotCacheable[] = "ipro_recorder_not_cacheable";
const char kNumFailed[] = "ipro_recorder_failed";
const char kNumTooMany[] = "ipro_recorder_too_many";
const char kNumTooLarge[] = "ipro_recorder_too_large";

int64 kIproMaxResponseBytesDefault = 1024 * 1024 * 10;
int64 kIproMaxConcurrentRecordingsDefault = 10;
int64 IproMaxResponseBytes = kIproMaxResponseBytesDefault;
int64 IproMaxConcurrentRecordings = kIproMaxConcurrentRecordingsDefault;

}  // namespace


AtomicInt32 NgxInPlaceResourceRecorder::num_recordings_in_progress_(0);

NgxInPlaceResourceRecorder::NgxInPlaceResourceRecorder(
    StringPiece url, RequestHeaders* request_headers, bool respect_vary,
    HTTPCache* cache, Statistics* stats, MessageHandler* handler)
    : url_(url.data(), url.size()), request_headers_(request_headers),
      respect_vary_(respect_vary), cache_(cache), handler_(handler),
      num_resources_(stats->GetVariable(kNumResources)),
      num_inserted_into_cache_(stats->GetVariable(kNumInsertedIntoCache)),
      num_not_cacheable_(stats->GetVariable(kNumNotCacheable)),
      num_failed_(stats->GetVariable(kNumFailed)),
      num_too_many_(stats->GetVariable(kNumTooMany)),
      num_too_large_(stats->GetVariable(kNumTooLarge)),
      headers_considered_(false),
      response_headers_(NULL),
      too_large_stat_incremented_(false),
      success_(true),
      needs_to_decrement_in_progress_(false) {
  num_resources_->Add(1);
}

NgxInPlaceResourceRecorder::~NgxInPlaceResourceRecorder() {
  if (response_headers_ != NULL) {
    delete response_headers_;
    response_headers_ = NULL;
  }
  if (needs_to_decrement_in_progress_) {
    num_recordings_in_progress_.BarrierIncrement(-1);
    needs_to_decrement_in_progress_ = false;
  }
}

void NgxInPlaceResourceRecorder::InitStats(Statistics* statistics) {
  statistics->AddVariable(kNumResources);
  statistics->AddVariable(kNumInsertedIntoCache);
  statistics->AddVariable(kNumNotCacheable);
  statistics->AddVariable(kNumFailed);
  statistics->AddVariable(kNumTooMany);
  statistics->AddVariable(kNumTooLarge);
}

void NgxInPlaceResourceRecorder::InitLimits(
    int ipro_max_response_bytes,
    int ipro_max_concurrent_recordings) {
  CHECK_GE(ipro_max_response_bytes, -1);
  CHECK_GE(ipro_max_concurrent_recordings, -1);
  IproMaxResponseBytes = ipro_max_response_bytes == -1 ?
      kIproMaxResponseBytesDefault : ipro_max_response_bytes;
  IproMaxConcurrentRecordings = ipro_max_concurrent_recordings == -1 ?
      kIproMaxConcurrentRecordingsDefault : ipro_max_concurrent_recordings;
}

bool NgxInPlaceResourceRecorder::Write(const StringPiece& contents,
                                    MessageHandler* handler) {
  if (too_large_stat_incremented_) {
    return false;
  }
  if (IproMaxResponseBytes == 0 ||
      static_cast<int64>(resource_value_.size()) < IproMaxResponseBytes) {
    return resource_value_.Write(contents, handler);
  } else {
    too_large_stat_incremented_ = true;
    num_too_large_->Add(1);
    cache_->RememberNotCacheable(url_, response_headers_->status_code() == 200,
                                 handler_);
    VLOG(1) << "IPRO: MaxResponseBytes exceeded while recording [" <<
        url_ << "]";
  }
  return false;
}

bool NgxInPlaceResourceRecorder::Flush(MessageHandler* handler) {
  return true;
}

bool NgxInPlaceResourceRecorder::ConsiderResponseHeaders(
    ResponseHeaders* response_headers) {
  CHECK(response_headers != NULL) << "Response headers cannot be NULL";
  headers_considered_ = true;

  // First, check if IPRO applies considering the content type.
  if (!IsIproContentType(response_headers)) {
    cache_->RememberNotCacheable(url_, response_headers->status_code() == 200,
                                 handler_);
    return false;
  }

  bool is_cacheable =
      response_headers->IsProxyCacheableGivenRequest(*request_headers_);
  if (is_cacheable && respect_vary_) {
    is_cacheable = response_headers->VaryCacheable(
        request_headers_->Has(HttpAttributes::kCookie));
  }
  if (!is_cacheable) {
    cache_->RememberNotCacheable(url_, response_headers->status_code() == 200,
                                 handler_);
    num_not_cacheable_->Add(1);
    return false;
  }

  // Shortcut for bailing out early when the response will be too large
  int64 content_length;
  if (response_headers->FindContentLength(&content_length)) {
    if (IproMaxResponseBytes > 0 && content_length > IproMaxResponseBytes) {
      VLOG(1) << "IPRO: Content-Length header indicates that [" <<
          url_ << "] is too large to record (" << content_length  << " bytes)";
      cache_->RememberNotCacheable(url_, response_headers->status_code() == 200,
                                   handler_);
      num_too_large_->Add(1);
      return false;
    }
  }

  // Copy the response headers, we might need them when Done()
  if (IproMaxConcurrentRecordings == 0) {
    response_headers_ = new ResponseHeaders();
    response_headers_->CopyFrom(*response_headers);
    return true;
  }
  if (num_recordings_in_progress_.BarrierIncrement(1) <=
             IproMaxConcurrentRecordings) {
    response_headers_ = new ResponseHeaders();
    response_headers_->CopyFrom(*response_headers);
    needs_to_decrement_in_progress_ = true;
    return true;
  } else {
    VLOG(1) << "IPRO: too many recordings in progress, not recording";
    num_recordings_in_progress_.BarrierIncrement(-1);
    num_too_many_->Add(1);
    return false;
  }
}

void NgxInPlaceResourceRecorder::Done() {
  num_recordings_in_progress_.BarrierIncrement(-1);
  needs_to_decrement_in_progress_ = false;

  if (success_) {
    if (!too_large_stat_incremented_) {
      // If a content length was specified, perform a sanity check on it
      int64 content_length;
      if (response_headers_->FindContentLength(&content_length)) {
        if (static_cast<int64>(resource_value_.size()) != content_length) {
          handler_->Message(kWarning,
                            "IPRO: Mismatched content length for [%s]",
                            url_.c_str());
          num_failed_->Add(1);
          delete this;
          return;
        }
      }
      resource_value_.SetHeaders(response_headers_);
      cache_->Put(url_, &resource_value_, handler_);
      // TODO(sligocki): Start IPRO rewrite.
      num_inserted_into_cache_->Add(1);
    }
  } else {
    // TODO(sligocki): Should we RememberFetchFailed() if success == false?
    // We don't expect this to happen much, it should only happen on aborted
    // responses.
    num_failed_->Add(1);
  }

  delete this;
}

bool NgxInPlaceResourceRecorder::IsIproContentType(
    ResponseHeaders* response_headers) {
  bool is_ipro_content_type = false;
  const net_instaweb::ContentType* content_type =
      response_headers->DetermineContentType();

  if (content_type != NULL) {
    is_ipro_content_type = content_type->IsImage() || content_type->IsCss() ||
        content_type->type() == net_instaweb::ContentType::kJavascript;
  }

  return is_ipro_content_type;
}


}  // namespace net_instaweb
