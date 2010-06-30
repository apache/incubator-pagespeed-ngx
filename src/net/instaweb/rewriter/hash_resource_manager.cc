/**
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

// Author: sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/hash_resource_manager.h"

#include "net/instaweb/rewriter/public/file_input_resource.h"
#include "net/instaweb/rewriter/public/hash_output_resource.h"
#include "net/instaweb/rewriter/public/input_resource.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include <string>

namespace net_instaweb {

HashResourceManager::HashResourceManager(const StringPiece& file_prefix,
                                         const StringPiece& url_prefix,
                                         const int num_shards,
                                         FileSystem* file_system,
                                         FilenameEncoder* filename_encoder,
                                         UrlFetcher* url_fetcher,
                                         Hasher* hasher)
    : num_shards_(num_shards),
      resource_id_(0),
      file_system_(file_system),
      filename_encoder_(filename_encoder),
      url_fetcher_(url_fetcher),
      hasher_(hasher),
      statistics_(NULL) {
  file_prefix.CopyToString(&file_prefix_);
  url_prefix.CopyToString(&url_prefix_);
}

HashResourceManager::~HashResourceManager() {
  CleanupResources();
}

void HashResourceManager::CleanupResources() {
  for (size_t i = 0; i < input_resources_.size(); ++i) {
    delete input_resources_[i];
  }
  input_resources_.clear();
  for (size_t i = 0; i < output_resources_.size(); ++i) {
    delete output_resources_[i];
  }
  output_resources_.clear();
}

void HashResourceManager::SetDefaultHeaders(const ContentType& content_type,
                                            MetaData* header) {
  header->set_major_version(1);
  header->set_minor_version(1);
  header->set_status_code(HttpStatus::OK);
  header->set_reason_phrase("OK");
  header->Add("Content-Type", content_type.mime_type());
  header->Add("Cache-control", "public, max-age=31536000");
  header->ComputeCaching();
}

OutputResource* HashResourceManager::GenerateOutputResource(
    const StringPiece& filter_prefix,
    const ContentType& content_type) {
  int id = resource_id_++;
  std::string id_string = IntegerToString(id);
  return NamedOutputResource(filter_prefix, id_string, content_type);
}

OutputResource* HashResourceManager::FindNamedOutputResourceInternal(
    const StringPiece& filter_prefix,
    const StringPiece& name,
    const StringPiece& ext,
    std::string* resource_key) const {
  *resource_key = StrCat(filter_prefix, ":", name, ":", ext);

  // TODO(jmarantz): this "cache" is not ideal.  Its memory usage is not
  // bounded.  It doesn't get invalidated when resources are updated.
  // Consider changing the model so that we always create & destroy
  // OutputResource* objects during the filter, and rely on an underlying
  // http cache to manage what needs to be recomputed.  The downside
  // is that during every HTML rewrite we will re-hash all resources,
  // even those that have not changed, which wastes server resources
  // and potentially increases latency, so we might need to have a
  // different cache-like mechanism to avoid re-hashing the same content.
  ResourceMap::const_iterator p = resource_map_.find(*resource_key);
  OutputResource* resource = (p != resource_map_.end()) ? p->second : NULL;
  return resource;
}

OutputResource* HashResourceManager::FindNamedOutputResource(
      const StringPiece& filter_prefix,
      const StringPiece& name,
      const StringPiece& ext) const {
  std::string key;
  return FindNamedOutputResourceInternal(filter_prefix, name, ext, &key);
}

OutputResource* HashResourceManager::NamedOutputResource(
    const StringPiece& filter_prefix,
    const StringPiece& name,
    const ContentType& content_type) {
  std::string resource_key;
  const char* ext = content_type.file_extension() + 1;
  OutputResource* resource = FindNamedOutputResourceInternal(
      filter_prefix, name, ext, &resource_key);
  if (resource == NULL) {
    resource = new HashOutputResource(
        url_prefix_, file_prefix_, filter_prefix, name,
        content_type.file_extension(),
        file_system_, filename_encoder_, hasher_);
    SetDefaultHeaders(content_type, resource->metadata());
    // Note: resource will be deleted by ~HashResourceManager destructor.
    output_resources_.push_back(resource);
    resource_map_[resource_key] = resource;
  }
  return resource;
}

void HashResourceManager::set_file_prefix(const StringPiece& file_prefix) {
  file_prefix.CopyToString(&file_prefix_);
}

void HashResourceManager::set_url_prefix(const StringPiece& url_prefix) {
  url_prefix.CopyToString(&url_prefix_);
}

void HashResourceManager::set_base_url(const StringPiece& url) {
  // TODO(sligocki): Is there any way to init GURL w/o alloc a whole new string?
  base_url_.reset(new GURL(url.as_string()));
}

std::string HashResourceManager::base_url() const {
  assert(base_url_->is_valid());
  return base_url_->spec();
}

InputResource* HashResourceManager::CreateInputResource(
    const StringPiece& input_url, MessageHandler* handler) {
  // We must deal robustly with calls to CreateInputResource on absolute urls
  // even when base_url_ has not been set, since in some contexts we can only
  // set base_url_ in response to an html page request, but we may need to
  // satisfy requests for rewritten for resources before any html has been
  // rewritten, or which don't come from the most-recently-rewritten html.

  // TODO(jmaessen): rewrite idiomatically.  We want a
  // GURL that is constructed in one of two different ways.
  // Calling new is clearly wrong, so we end up constructing and copying
  // down each branch which is ugly albeit ever so slightly less wrong.
  const std::string input_url_string = input_url.as_string();
  GURL url;
  if (base_url_ == NULL) {
    GURL input_gurl(input_url_string);
    url = input_gurl;
    if (!url.is_valid()) {
      handler->Message(kError,
                       "CreateInputResource called before base_url set.");
      return NULL;
    }
  } else {
    // Get absolute url based on the (possibly relative) input_url.
    url = base_url_->Resolve(input_url_string);
  }

  InputResource* resource = NULL;
  const StringPiece url_string(url.spec().data(), url.spec().size());
  if (url.SchemeIs("http")) {
    // TODO(sligocki): Figure out if these are actually local by
    // seing if the serving path matches url_prefix_, in which case
    // we can do a local file read.
    // TODO(jmaessen): In order to permit url loading from a context
    // where the base url isn't set, we must keep the normalized url
    // in the UrlInputResource rather than the original input_url.
    // This is ugly and yields unnecessarily verbose rewritten urls.
    resource = new UrlInputResource(input_url, url_string, url_fetcher_);
  // TODO(sligocki): Probably shouldn't support file:// scheme.
  // (but it's used extensively in eg rewriter_test.)
  } else if (url.SchemeIsFile()) {
    const std::string& filename = url.path();
    // NOTE: This is raw filesystem access, no filename-encoding, etc.
    resource =
        new FileInputResource(input_url, url_string, filename, file_system_);
  } else {
    handler->Message(kError, "Unsupported scheme '%s' for url '%s'",
                     url.scheme().c_str(), url.spec().c_str());
  }
  if (resource != NULL) {
    input_resources_.push_back(resource);
  }
  return resource;
}

}  // namespace net_instaweb
