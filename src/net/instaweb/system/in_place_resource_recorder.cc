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

#include "net/instaweb/system/public/in_place_resource_recorder.h"

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

namespace {

const char kNumResources[] = "ipro_recorder_resources";
const char kNumInsertedIntoCache[] = "ipro_recorder_inserted_into_cache";
const char kNumNotCacheable[] = "ipro_recorder_not_cacheable";
const char kNumFailed[] = "ipro_recorder_failed";
const char kNumDroppedDueToLoad[] = "ipro_recorder_dropped_due_to_load";
const char kNumDroppedDueToSize[] = "ipro_recorder_dropped_due_to_size";

}

AtomicInt32 InPlaceResourceRecorder::active_recordings_(0);

InPlaceResourceRecorder::InPlaceResourceRecorder(
    StringPiece url, RequestHeaders* request_headers, bool respect_vary,
    int max_response_bytes, int max_concurrent_recordings,
    HTTPCache* cache, Statistics* stats, MessageHandler* handler)
    : url_(url.data(), url.size()), request_headers_(request_headers),
      respect_vary_(respect_vary),
      max_response_bytes_(max_response_bytes),
      max_concurrent_recordings_(max_concurrent_recordings),
      cache_(cache), handler_(handler),
      num_resources_(stats->GetVariable(kNumResources)),
      num_inserted_into_cache_(stats->GetVariable(kNumInsertedIntoCache)),
      num_not_cacheable_(stats->GetVariable(kNumNotCacheable)),
      num_failed_(stats->GetVariable(kNumFailed)),
      num_dropped_due_to_load_(stats->GetVariable(kNumDroppedDueToLoad)),
      num_dropped_due_to_size_(stats->GetVariable(kNumDroppedDueToSize)),
      status_code_(-1),
      failure_(false),
      response_headers_considered_(false) {
  num_resources_->Add(1);
  if (limit_active_recordings() &&
      active_recordings_.BarrierIncrement(1) > max_concurrent_recordings_) {
    VLOG(1) << "IPRO: too many recordings in progress, not recording";
    num_dropped_due_to_load_->Add(1);
    failure_ = true;
  }
}

InPlaceResourceRecorder::~InPlaceResourceRecorder() {
  if (limit_active_recordings()) {
    active_recordings_.BarrierIncrement(-1);
  }
}

void InPlaceResourceRecorder::InitStats(Statistics* statistics) {
  statistics->AddVariable(kNumResources);
  statistics->AddVariable(kNumInsertedIntoCache);
  statistics->AddVariable(kNumNotCacheable);
  statistics->AddVariable(kNumFailed);
  statistics->AddVariable(kNumDroppedDueToLoad);
  statistics->AddVariable(kNumDroppedDueToSize);
}

bool InPlaceResourceRecorder::Write(const StringPiece& contents,
                                    MessageHandler* handler) {
  if (failure_) {
    return false;
  }
  if (max_response_bytes_ == 0 ||
      resource_value_.size() + contents.size() < max_response_bytes_) {
    return resource_value_.Write(contents, handler_);
  } else {
    failure_ = true;
    num_dropped_due_to_size_->Add(1);
    cache_->RememberNotCacheable(url_, status_code_ == 200, handler_);
    VLOG(1) << "IPRO: MaxResponseBytes exceeded while recording " << url_;
    return false;
  }
}

void InPlaceResourceRecorder::ConsiderResponseHeaders(
    ResponseHeaders* response_headers) {
  CHECK(response_headers != NULL) << "Response headers cannot be NULL";
  DCHECK(!response_headers_considered_);
  response_headers_considered_ = true;
  status_code_ = response_headers->status_code();
  // First, check if IPRO applies considering the content type.
  // Note: in a proxy setup it might be desireable to cache HTML and
  // non-rewritable Content-Types to avoid re-fetching from the origin server.
  const ContentType* content_type =
      response_headers->DetermineContentType();
  if (content_type == NULL ||
      !(content_type->IsImage() ||
        content_type->IsCss() ||
        content_type->type() == ContentType::kJavascript)) {
    cache_->RememberNotCacheable(url_, status_code_ == 200, handler_);
    failure_ = true;
    return;
  }
  bool is_cacheable =
      response_headers->IsProxyCacheableGivenRequest(*request_headers_);
  // TODO(jefftk): could IsProxyCacheableGivenRequest handle the cookie check?
  if (is_cacheable && respect_vary_) {
    is_cacheable = response_headers->VaryCacheable(
        request_headers_->Has(HttpAttributes::kCookie));
  }
  if (!is_cacheable) {
    cache_->RememberNotCacheable(url_, status_code_ == 200, handler_);
    num_not_cacheable_->Add(1);
    failure_ = true;
    return;
  }
  // Shortcut for bailing out early when the response will be too large
  int64 content_length;
  if (max_response_bytes_ != 0 &&
      response_headers->FindContentLength(&content_length) &&
      content_length > max_response_bytes_) {
    VLOG(1) << "IPRO: Content-Length header indicates that ["
            << url_ << "] is too large to record (" << content_length
            << " bytes)";
    cache_->RememberNotCacheable(url_, status_code_ == 200, handler_);
    num_dropped_due_to_size_->Add(1);
    failure_ = true;
    return;
  }
}

void InPlaceResourceRecorder::DoneAndSetHeaders(
    ResponseHeaders* response_headers) {
  if (!failure_ && !response_headers_considered_) {
    ConsiderResponseHeaders(response_headers);
  }
  if (failure_) {
    num_failed_->Add(1);
  } else {
    // If a content length was specified, sanity check it.
    int64 content_length;
    if (response_headers->FindContentLength(&content_length) &&
        static_cast<int64>(resource_value_.contents_size()) != content_length) {
      handler_->Message(
          kWarning, "IPRO: Mismatched content length for [%s]", url_.c_str());
      num_failed_->Add(1);
    } else {
      resource_value_.SetHeaders(response_headers);
      cache_->Put(url_, &resource_value_, handler_);
      // TODO(sligocki): Start IPRO rewrite.
      num_inserted_into_cache_->Add(1);
    }
  }
  delete this;
}

}  // namespace net_instaweb
