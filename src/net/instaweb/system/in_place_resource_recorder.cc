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
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"

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
    const RequestContextPtr& request_context,
    StringPiece url, StringPiece fragment,
    const RequestHeaders::Properties request_properties, bool respect_vary,
    int max_response_bytes, int max_concurrent_recordings,
    int64 implicit_cache_ttl_ms, HTTPCache* cache, Statistics* stats,
    MessageHandler* handler)
    : url_(url.data(), url.size()),
      fragment_(fragment.data(), fragment.size()),
      request_properties_(request_properties),
      respect_vary_(ResponseHeaders::GetVaryOption(respect_vary)),
      max_response_bytes_(max_response_bytes),
      max_concurrent_recordings_(max_concurrent_recordings),
      implicit_cache_ttl_ms_(implicit_cache_ttl_ms),
      write_to_resource_value_(request_context, &resource_value_),
      inflating_fetch_(&write_to_resource_value_),
      cache_(cache), handler_(handler),
      num_resources_(stats->GetVariable(kNumResources)),
      num_inserted_into_cache_(stats->GetVariable(kNumInsertedIntoCache)),
      num_not_cacheable_(stats->GetVariable(kNumNotCacheable)),
      num_failed_(stats->GetVariable(kNumFailed)),
      num_dropped_due_to_load_(stats->GetVariable(kNumDroppedDueToLoad)),
      num_dropped_due_to_size_(stats->GetVariable(kNumDroppedDueToSize)),
      status_code_(-1),
      failure_(false),
      full_response_headers_considered_(false),
      consider_response_headers_called_(false) {
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
  DCHECK(consider_response_headers_called_);
  if (failure_) {
    return false;
  }

  // Write into resource_value_ decompressing if needed.
  failure_ = !inflating_fetch_.Write(contents, handler_);
  if (max_response_bytes_ == 0 ||
      resource_value_.contents_size() < max_response_bytes_) {
    return !failure_;
  } else {
    DroppedDueToSize();
    VLOG(1) << "IPRO: MaxResponseBytes exceeded while recording " << url_;
    return false;
  }
}

void InPlaceResourceRecorder::ConsiderResponseHeaders(
    HeadersKind headers_kind,
    ResponseHeaders* response_headers) {
  CHECK(response_headers != NULL) << "Response headers cannot be NULL";
  DCHECK(!full_response_headers_considered_);

  if (!consider_response_headers_called_) {
    consider_response_headers_called_ = true;
    // In first call, set up headers for potential deflating. We basically only
    // care about Content-Encoding, plus AsyncFetch gets unhappy with 0
    // status code.
    inflating_fetch_.response_headers()->CopyFrom(*response_headers);
    write_to_resource_value_.response_headers()->set_status_code(
        HttpStatus::kOK);
  }

  if (headers_kind != kFullHeaders) {
    return;
  }
  full_response_headers_considered_ = true;

  status_code_ = response_headers->status_code();

  // For 4xx and 5xx we can't IPRO, but we can also cache the failure so we
  // don't retry recording for a bit.
  if (response_headers->IsErrorStatus()) {
    cache_->RememberFetchFailed(url_, fragment_, handler_);
    failure_ = true;
    return;
  }

  // We can't optimize anything that's not a 200, so say recording failed
  // for such statuses. However, we don't cache the failure here: for statuses
  // like 304 and 206 an another response is likely to be a 200 soon. We group
  // the other stuff with them here since it's the conservative default.
  if (status_code_ != HttpStatus::kOK) {
    failure_ = true;
    return;
  }

  // First, check if IPRO applies considering the content type.
  // Note: in a proxy setup it might be desireable to cache HTML and
  // non-rewritable Content-Types to avoid re-fetching from the origin server.
  const ContentType* content_type =
      response_headers->DetermineContentType();
  if (content_type == NULL ||
      !(content_type->IsImage() ||
        content_type->IsCss() ||
        content_type->type() == ContentType::kJavascript)) {
    cache_->RememberNotCacheable(
        url_, fragment_, status_code_ == 200, handler_);
    failure_ = true;
    return;
  }
  bool is_cacheable = response_headers->IsProxyCacheable(
      request_properties_, respect_vary_,
      ResponseHeaders::kNoValidator);
  if (!is_cacheable) {
    cache_->RememberNotCacheable(
        url_, fragment_, status_code_ == 200, handler_);
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
    DroppedDueToSize();
    return;
  }
}

void InPlaceResourceRecorder::DroppedDueToSize() {
  cache_->RememberNotCacheable(url_, fragment_, status_code_ == 200, handler_);
  num_dropped_due_to_size_->Add(1);
  failure_ = true;
}

void InPlaceResourceRecorder::DoneAndSetHeaders(
    ResponseHeaders* response_headers) {
  if (!failure_ && !full_response_headers_considered_) {
    ConsiderResponseHeaders(kFullHeaders, response_headers);
  }

  if (failure_) {
    num_failed_->Add(1);
  } else {
    // We don't consider content-encoding to be valid here, since it can
    // be captured post-mod_deflate with pre-deflate content. Also note
    // that content-length doesn't have to be accurate either, since it can be
    // due to compression; we do still use it for quickly reject since
    // if gzip'd is too large uncompressed is likely too large, too.
    response_headers->RemoveAll(HttpAttributes::kContentEncoding);
    response_headers->RemoveAll(HttpAttributes::kContentLength);
    resource_value_.SetHeaders(response_headers);
    cache_->Put(url_, fragment_, request_properties_, respect_vary_,
                &resource_value_, handler_);
    // TODO(sligocki): Start IPRO rewrite.
    num_inserted_into_cache_->Add(1);
  }
  delete this;
}

}  // namespace net_instaweb
