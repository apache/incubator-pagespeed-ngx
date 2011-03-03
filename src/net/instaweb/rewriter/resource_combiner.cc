/*
 * Copyright 2010 Google Inc.
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

#include "net/instaweb/rewriter/public/resource_combiner.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/stl_util.h"
#include <string>
#include "net/instaweb/util/public/string_writer.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

// Another approach is to put a CHECK that the final URL with the
// resource naming does not exceed the limit.
//
// Another option too is to just instantiate a ResourceNamer and a
// hasher put in the correct ID and EXT and leave the name blank and
// take size of that.

ResourceCombiner::ResourceCombiner(RewriteDriver* driver,
                                   const StringPiece& filter_prefix,
                                   const StringPiece& extension)
    : resource_manager_(driver->resource_manager()),
      rewrite_driver_(driver),
      partnership_(driver->options(), GURL()),
      prev_num_components_(0),
      accumulated_leaf_size_(0),
      // TODO(jmarantz): The URL overhead computation is arguably fragile.
      url_overhead_(filter_prefix.size() + ResourceNamer::kOverhead +
                    extension.size()) {
  // This CHECK is here because RewriteDriver is constructed with its
  // resource_manager_ == NULL.
  // TODO(sligocki): Construct RewriteDriver with a ResourceManager.
  CHECK(resource_manager_ != NULL);
  filter_prefix.CopyToString(&filter_prefix_);
}

ResourceCombiner::~ResourceCombiner() {
  Clear();
}

bool ResourceCombiner::AddResource(const StringPiece& url,
                                   MessageHandler* handler) {
  // Assert the sanity of three parallel vectors.
  CHECK_EQ(num_urls(), static_cast<int>(resources_.size()));
  CHECK_EQ(num_urls(), static_cast<int>(multipart_encoder_.num_urls()));
  if (num_urls() == 0) {
    // Make sure to initialize the base URL.
    Reset();
  }

  // See if we have the source loaded, or start loading it
  // TODO(morlovich) this may not always be desirable.
  //    we want to do this if we can't combine due to URL limits,
  //    as we will eventually need the data, but not when it's
  //    disabled due to policy.

  scoped_ptr<Resource> resource(
      rewrite_driver_->CreateInputResource(rewrite_driver_->base_url().gurl(),
                                           url));
  if (resource.get() == NULL) {
    return false;
  }

  if (!(rewrite_driver_->ReadIfCached(resource.get()) &&
        resource->ContentsValid())) {
    return false;
  }

  // Make sure the specific filter is OK with the data --- it may be
  // unable to combine it safely
  if (!ResourceCombinable(resource.get(), handler)) {
    return false;
  }

  // Now manage the URL and policy.
  bool added = partnership_.AddUrl(url, handler);

  if (added) {
    int index = num_urls() - 1;

    if (partnership_.NumCommonComponents() != prev_num_components_) {
      UpdateResolvedBase();
    }
    const std::string relative_path = partnership_.RelativePath(index);
    multipart_encoder_.AddUrl(relative_path);

    if (accumulated_leaf_size_ == 0) {
      ComputeLeafSize();
    } else {
      AccumulateLeafSize(relative_path);
    }

    resources_.push_back(resource.release());
    if (UrlTooBig()) {
      RemoveLastResource();
      added = false;
    }
  }
  return added;
}

void ResourceCombiner::RemoveLastResource() {
  partnership_.RemoveLast();
  delete resources_.back();
  resources_.pop_back();
  multipart_encoder_.pop_back();
  if (partnership_.NumCommonComponents() != prev_num_components_) {
    UpdateResolvedBase();
  }
}

std::string ResourceCombiner::UrlSafeId() const {
  UrlEscaper* escaper = resource_manager_->url_escaper();
  std::string segment;
  escaper->EncodeToUrlSegment(multipart_encoder_.Encode(), &segment);
  return segment;
}

void ResourceCombiner::ComputeLeafSize() {
  std::string segment = UrlSafeId();
  accumulated_leaf_size_ = segment.size() + url_overhead_
      + resource_manager_->hasher()->HashSizeInChars();
}

void ResourceCombiner::AccumulateLeafSize(const StringPiece& url) {
  std::string segment;
  UrlEscaper* escaper = resource_manager_->url_escaper();
  escaper->EncodeToUrlSegment(url, &segment);
  const int kMultipartOverhead = 1;  // for the '+'
  accumulated_leaf_size_ += segment.size() + kMultipartOverhead;
}

bool ResourceCombiner::UrlTooBig() {
  // Note: We include kUrlSlack in our computations so that other filters,
  // which might add to URL length, can run after ours
  int expanded_size = accumulated_leaf_size_ + ResourceCombiner::kUrlSlack;

  if (expanded_size > rewrite_driver_->options()->max_url_segment_size()) {
    return true;
  }

  if ((expanded_size + static_cast<int>(resolved_base_.size())) >
      rewrite_driver_->options()->max_url_size()) {
    return true;
  }
  return false;
}

bool ResourceCombiner::ResourceCombinable(Resource* /*resource*/,
    MessageHandler* /*handler*/) {
  return true;
}

void ResourceCombiner::UpdateResolvedBase() {
  // If the addition of this URL changes the base path,
  // then we will have to recompute the multi-part encoding.
  // This is n^2 in the pathological case and if this code
  // gets used for image spriting then this
  // algorithm should be revisited.  For CSS and JS we expect N to
  // be relatively small.
  prev_num_components_ = partnership_.NumCommonComponents();
  resolved_base_ = ResolvedBase();
  multipart_encoder_.clear();
  for (size_t i = 0; i < resources_.size(); ++i) {
    multipart_encoder_.AddUrl(partnership_.RelativePath(i));
  }

  accumulated_leaf_size_ = 0;
}

OutputResource* ResourceCombiner::Combine(const ContentType& content_type,
                                          MessageHandler* handler) {
  if (resources_.size() <= 1) {
    // No point in combining.
    return NULL;
  }
  // First, compute the name of the new resource based on the names of
  // the old resources.
  std::string url_safe_id = UrlSafeId();
  // Start building up the combination.  At this point we are still
  // not committed to the combination, because the 'write' can fail.
  // TODO(jmaessen, jmarantz): encode based on partnership
  scoped_ptr<OutputResource> combination(
      rewrite_driver_->CreateOutputResourceWithPath(
          ResolvedBase(), filter_prefix_, url_safe_id, &content_type,
          RewriteDriver::kRewrittenResource));
  if (combination->cached_result() != NULL &&
      combination->cached_result()->optimizable()) {
    // If the combination has a Url set on it we have cached information
    // on what the output would be, so we'll just use that.
    return combination.release();
  }
  if (WriteCombination(resources_, combination.get(), handler)
      && combination->IsWritten()) {
    // Otherwise, we have to compute it.
    return combination.release();
  }
  // No dice.
  return NULL;
}

bool ResourceCombiner::WriteCombination(
    const ResourceVector& combine_resources,
    OutputResource* combination,
    MessageHandler* handler) {
  bool written = true;
  // TODO(sligocki): Write directly to a temp file rather than doing the extra
  // string copy.
  std::string combined_contents;
  StringWriter writer(&combined_contents);
  int64 min_origin_expiration_time_ms = 0;

  for (int i = 0, n = combine_resources.size(); written && (i < n); ++i) {
    Resource* input = combine_resources[i];
    StringPiece contents = input->contents();
    int64 input_expire_time_ms = input->CacheExpirationTimeMs();
    if ((min_origin_expiration_time_ms == 0) ||
        (input_expire_time_ms < min_origin_expiration_time_ms)) {
      min_origin_expiration_time_ms = input_expire_time_ms;
    }

    written = WritePiece(input, combination, &writer, handler);
  }
  if (written) {
    written =
        resource_manager_->Write(
            HttpStatus::kOK, combined_contents, combination,
            min_origin_expiration_time_ms, handler);
  }
  return written;
}

bool ResourceCombiner::WritePiece(Resource* input,
                                  OutputResource* /*combination*/,
                                  Writer* writer,
                                  MessageHandler* handler) {
  return writer->Write(input->contents(), handler);
}

// Callback to run whenever a resource is collected.  This keeps a
// count of the resources collected so far.  When the last one is collected,
// it aggregates the results and calls the final callback with the result.
class CombinerCallback : public Resource::AsyncCallback {
 public:
  CombinerCallback(ResourceCombiner* combiner,
                   MessageHandler* handler,
                   UrlAsyncFetcher::Callback* callback,
                   OutputResource* combination,
                   Writer* writer,
                   ResponseHeaders* response_headers) :
      enable_completion_(false),
      emit_done_(true),
      done_count_(0),
      fail_count_(0),
      combiner_(combiner),
      message_handler_(handler),
      callback_(callback),
      combination_(combination),
      writer_(writer),
      response_headers_(response_headers) {
  }

  virtual ~CombinerCallback() {
    STLDeleteContainerPointers(combine_resources_.begin(),
                               combine_resources_.end());
  }

  // Note that the passed-in resource might be NULL; this gives us a chance
  // to note failure.
  bool AddResource(Resource* resource) {
    bool ret = false;
    if (resource == NULL) {
      // Whoops, we've failed to even obtain a resource.
      ++fail_count_;
    } else if (fail_count_ > 0) {
      // Another of the resource fetches failed.  Drop this resource
      // and don't fetch it.
      delete resource;
    } else {
      combine_resources_.push_back(resource);
      ret = true;
    }
    return ret;
  }

  virtual void Done(bool success, Resource* resource) {
    if (!success) {
      ++fail_count_;
    }
    ++done_count_;

    if (Ready()) {
      DoCombination();
    }
  }

  bool Ready() {
    return (enable_completion_ &&
            (done_count_ == combine_resources_.size()));
  }

  void EnableCompletion() {
    enable_completion_ = true;
    if (Ready()) {
      DoCombination();
    }
  }

  void DoCombination() {
    bool ok = fail_count_ == 0;
    for (size_t i = 0; ok && (i < combine_resources_.size()); ++i) {
      Resource* resource = combine_resources_[i];
      ok = resource->ContentsValid();
    }
    if (ok) {
      ok = (combiner_->WriteCombination(combine_resources_, combination_,
                                        message_handler_) &&
            combination_->IsWritten() &&
            ((writer_ == NULL) ||
             writer_->Write(combination_->contents(), message_handler_)));
      // Above code fills in combination_->metadata(); now propagate to
      // response_headers_.
    }
    if (ok) {
      response_headers_->CopyFrom(*combination_->metadata());
    } else if (emit_done_) {
      // we can only safely touch response_headers_ if we can emit
      // Done() safely.
      response_headers_->SetStatusAndReason(HttpStatus::kNotFound);
    }

    if (emit_done_) {
      callback_->Done(ok);
    }

    delete this;
  }

  void set_emit_done(bool emit_done) {
    emit_done_ = emit_done;
  }

 private:
  bool enable_completion_;
  bool emit_done_;
  size_t done_count_;
  size_t fail_count_;
  ResourceCombiner* combiner_;
  MessageHandler* message_handler_;
  UrlAsyncFetcher::Callback* callback_;
  OutputResource* combination_;
  ResourceCombiner::ResourceVector combine_resources_;
  Writer* writer_;
  ResponseHeaders* response_headers_;

  DISALLOW_COPY_AND_ASSIGN(CombinerCallback);
};

bool ResourceCombiner::Fetch(OutputResource* combination,
                             Writer* writer,
                             const RequestHeaders& request_header,
                             ResponseHeaders* response_headers,
                             MessageHandler* message_handler,
                             UrlAsyncFetcher::Callback* callback) {
  CHECK(response_headers != NULL);
  bool ret = false;
  StringPiece url_safe_id = combination->name();
  UrlMultipartEncoder multipart_encoder;
  UrlEscaper* escaper = resource_manager_->url_escaper();
  std::string multipart_encoding;
  GoogleUrl gurl(combination->url());
  if (gurl.is_valid() &&
      escaper->DecodeFromUrlSegment(url_safe_id, &multipart_encoding) &&
      multipart_encoder.Decode(multipart_encoding, message_handler)) {
    std::string url, decoded_resource;
    ret = true;
    CombinerCallback* combiner = new CombinerCallback(
        this, message_handler, callback, combination, writer, response_headers);

    StringPiece root = gurl.AllExceptLeaf();
    for (int i = 0; ret && (i < multipart_encoder.num_urls()); ++i)  {
      std::string url = StrCat(root, multipart_encoder.url(i));
      // Safe since we use StrCat to absolutize the URL rather than
      // full resolve, so it will always be a subpath of root.
      Resource* resource =
          rewrite_driver_->CreateInputResourceAbsoluteUnchecked(url);
      ret = combiner->AddResource(resource);
      if (ret) {
        rewrite_driver_->ReadAsync(resource, combiner, message_handler);
      }
    }

    // If we're about to return false, we do not want the combiner to emit
    // Done as RewriteDriver::FetchResource will do it as well.
    combiner->set_emit_done(ret);

    // In the case where the first input file is already cached,
    // ReadAsync will directly call the CombineCallback, which, if
    // already enabled, would think it was complete and run DoCombination
    // prematurely.  So we wait until the resources are all added before
    // enabling the callback to complete.
    combiner->EnableCompletion();
  } else {
    message_handler->Error(url_safe_id.as_string().c_str(), 0,
                           "Unable to decode resource string");
  }
  return ret;
}

void ResourceCombiner::Clear() {
  STLDeleteElements(&resources_);
  resources_.clear();
  multipart_encoder_.clear();
}

void ResourceCombiner::Reset() {
  Clear();
  partnership_.Reset(rewrite_driver_->base_url().gurl());
  prev_num_components_ = 0;
  accumulated_leaf_size_ = 0;
  resolved_base_.clear();
}

}  // namespace net_instaweb
