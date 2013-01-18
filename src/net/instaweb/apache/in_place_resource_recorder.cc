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

#include "net/instaweb/apache/in_place_resource_recorder.h"

#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

class MessageHandler;

namespace {

const char kNumResources[] = "ipro_recorder_resources";
const char kNumInsertedIntoCache[] = "ipro_recorder_inserted_into_cache";
const char kNumNotCacheable[] = "ipro_recorder_not_cacheable";
const char kNumFailed[] = "ipro_recorder_failed";

}

InPlaceResourceRecorder::InPlaceResourceRecorder(
    StringPiece url, RequestHeaders* request_headers, bool respect_vary,
    HTTPCache* cache, Statistics* stats, MessageHandler* handler)
    : url_(url.data(), url.size()), request_headers_(request_headers),
      respect_vary_(respect_vary), success_(true), cache_(cache),
      handler_(handler),
      num_resources_(stats->GetVariable(kNumResources)),
      num_inserted_into_cache_(stats->GetVariable(kNumInsertedIntoCache)),
      num_not_cacheable_(stats->GetVariable(kNumNotCacheable)),
      num_failed_(stats->GetVariable(kNumFailed)) {
  num_resources_->Add(1);
}

InPlaceResourceRecorder::~InPlaceResourceRecorder() {
}

void InPlaceResourceRecorder::InitStats(Statistics* statistics) {
  statistics->AddVariable(kNumResources);
  statistics->AddVariable(kNumInsertedIntoCache);
  statistics->AddVariable(kNumNotCacheable);
  statistics->AddVariable(kNumFailed);
}


bool InPlaceResourceRecorder::Write(const StringPiece& contents,
                                    MessageHandler* handler) {
  return resource_value_.Write(contents, handler);
}

bool InPlaceResourceRecorder::Flush(MessageHandler* handler) {
  return true;
}

void InPlaceResourceRecorder::DoneAndSetHeaders(
    ResponseHeaders* response_headers) {
  if (success_) {
    // TODO(sligocki): Currently this will cache HTML and other Content-Types
    // that cannot be rewritten in-place. It seems like we should only cache
    // resources that have a hope of being rewritten in-place, and store a
    // failure in the IPRO meta-data lookup for all others.

    bool is_cacheable =
        response_headers->IsProxyCacheableGivenRequest(*request_headers_);
    if (is_cacheable && respect_vary_) {
      is_cacheable = response_headers->VaryCacheable(
          request_headers_->Has(HttpAttributes::kCookie));
    }

    // Put resource_value_ in cache.
    if (is_cacheable) {
      resource_value_.SetHeaders(response_headers);
      cache_->Put(url_, &resource_value_, handler_);
      // TODO(sligocki): Start IPRO rewrite.
      num_inserted_into_cache_->Add(1);
    } else {
      cache_->RememberNotCacheable(url_, response_headers->status_code() == 200,
                                   handler_);
      num_not_cacheable_->Add(1);
    }
  } else {
    // TODO(sligocki): Should we RememberFetchFailed() if success_ == false?
    // We don't expect this to happen much, it seems to only happen with Apache
    // Bucket Brigade issues.
    num_failed_->Add(1);
  }

  delete this;
}

}  // namespace net_instaweb
