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

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/resource_manager.h"

#include "net/instaweb/rewriter/public/file_input_resource.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/http_cache.h"
#include "net/instaweb/util/public/http_value.h"
#include "net/instaweb/util/public/message_handler.h"
#include <string>
#include "net/instaweb/util/public/timer.h"

namespace {

const char kCacheControl[] = "Cache-control";

// These two constants are used to segragate the keys contained in
// the HTTP cache.  We want to store two distinct mappings in the
// http cache.
//
// We want to store a mapping from the base name of a resource into
// the hash.  This mapping has a TTL based the minimum TTL of the
// input resources used to construct the resource.  After that TTL has
// expired, we will need to re-fetch the resources from their origin,
// and recompute the hash.
const char kFilenameCacheKeyPrefix[] = "ResourceName:";

// We also store a mapping from the hashed name to the resource
// contents.  This can have an arbitrarily long TTL since the hash
// of the content is in the key.
const char kContentsCacheKeyPrefix[] = "ResourceContents:";

}  // namespace

namespace net_instaweb {

ResourceManager::ResourceManager(const StringPiece& file_prefix,
                                 const StringPiece& url_prefix,
                                 const int num_shards,
                                 FileSystem* file_system,
                                 FilenameEncoder* filename_encoder,
                                 UrlFetcher* url_fetcher,
                                 Hasher* hasher,
                                 HTTPCache* http_cache)
    : num_shards_(num_shards),
      resource_id_(0),
      file_system_(file_system),
      filename_encoder_(filename_encoder),
      url_fetcher_(url_fetcher),
      hasher_(hasher),
      statistics_(NULL),
      http_cache_(http_cache),
      relative_path_(false) {
  file_prefix.CopyToString(&file_prefix_);
  url_prefix.CopyToString(&url_prefix_);
}

ResourceManager::~ResourceManager() {
}

// TODO(jmarantz): consider moving this method to MetaData
void ResourceManager::SetDefaultHeaders(const ContentType* content_type,
                                        MetaData* header) {
  CHECK(header->major_version() == 0);
  header->set_major_version(1);
  header->set_minor_version(1);
  header->set_status_code(HttpStatus::OK);
  header->set_reason_phrase("OK");
  if (content_type != NULL) {
    header->Add("Content-Type", content_type->mime_type());
  }
  header->Add(kCacheControl, "public, max-age=31536000");
  header->Add("Vary", "Accept-Encoding");

  // TODO(jmarantz): Page-speed suggested adding a "Last-Modified",header
  // for cache validation.  To do this we must track the max of all
  // Last-Modified values for all input resources that are used to
  // create this output resource.

  header->ComputeCaching();
}

// TODO(jmarantz): consider moving this method to MetaData
void ResourceManager::SetContentType(const ContentType* content_type,
                                     MetaData* header) {
  CHECK(header->major_version() != 0);
  CHECK(content_type != NULL);
  header->RemoveAll("Content-Type");
  header->Add("Content-Type", content_type->mime_type());
  header->ComputeCaching();
}

OutputResource* ResourceManager::CreateGeneratedOutputResource(
    const StringPiece& filter_prefix,
    const ContentType* content_type,
    MessageHandler* handler) {
  int id = resource_id_++;
  std::string id_string = IntegerToString(id);
  return CreateNamedOutputResource(filter_prefix, id_string, content_type,
                                   handler);
}

OutputResource* ResourceManager::CreateNamedOutputResource(
    const StringPiece& filter_prefix,
    const StringPiece& name,
    const ContentType* content_type,
    MessageHandler* handler) {
  OutputResource* resource = new OutputResource(
      this, content_type, filter_prefix, name);

  // Determine whether this output resource is still valid by looking
  // up by hash in the http cache.  Note that this cache entry will
  // expire when any of the origin resources expire.
  std::string separator = RewriteFilter::prefix_separator();
  std::string prefix_name = StrCat(filter_prefix, separator, name);
  SimpleMetaData meta_data;
  StringPiece hash;
  HTTPValue value;
  std::string name_key = StrCat(kFilenameCacheKeyPrefix, prefix_name);
  if (http_cache_->Get(name_key.c_str(), &value, handler) &&
      value.ExtractContents(&hash)) {
    resource->SetHash(hash);
  }
  return resource;
}

OutputResource* ResourceManager::CreateUrlOutputResource(
    const StringPiece& filter_prefix, const StringPiece& name,
    const StringPiece& hash, const ContentType* content_type) {
  OutputResource* resource = new OutputResource(
      this, content_type, filter_prefix, name);
  resource->SetHash(hash);
  return resource;
}

void ResourceManager::set_filename_prefix(const StringPiece& file_prefix) {
  file_prefix.CopyToString(&file_prefix_);
}

void ResourceManager::set_url_prefix(const StringPiece& url_prefix) {
  url_prefix.CopyToString(&url_prefix_);
}

void ResourceManager::set_base_url(const StringPiece& url) {
  // TODO(sligocki): Is there any way to init GURL w/o alloc a whole new string?
  base_url_.reset(new GURL(url.as_string()));
}

std::string ResourceManager::base_url() const {
  CHECK(base_url_->is_valid());
  return base_url_->spec();
}

Resource* ResourceManager::CreateInputResource(
    const StringPiece& input_url, MessageHandler* handler) {
  Resource* resource = NULL;

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
  std::string url_string;
  GURL url;
  if (base_url_ == NULL) {
    GURL input_gurl(input_url_string);
    url = input_gurl;
    if (!url.is_valid()) {
      handler->Message(kError,
                       "CreateInputResource called before base_url set.");
      return NULL;
    }
  } else if (relative_path_) {
    url_string = base_url_->scheme();
    url_string += ":";
    url_string += input_url_string;
    url = GURL(url_string);
  } else {
    // Get absolute url based on the (possibly relative) input_url.
    url = base_url_->Resolve(input_url_string);
    url_string.clear();
    url_string.append(url.spec().data(), url.spec().size());
  }

  const ContentType* type = NameExtensionToContentType(input_url);
  // Note that the type may be null if, for example, an image has an
  // unexpected extension.  We will have to figure out the image type
  // from the content, but we will not be able to do that until it's
  // been read in.

  if (url.SchemeIs("http")) {
    // TODO(sligocki): Figure out if these are actually local by
    // seing if the serving path matches url_prefix_, in which case
    // we can do a local file read.
    // TODO(jmaessen): In order to permit url loading from a context
    // where the base url isn't set, we must keep the normalized url
    // in the UrlInputResource rather than the original input_url.
    // This is ugly and yields unnecessarily verbose rewritten urls.
    resource = new UrlInputResource(this, type, url_string);
    // TODO(sligocki): Probably shouldn't support file:// scheme.
    // (but it's used extensively in eg rewriter_test.)
  } else if (url.SchemeIsFile()) {
    // NOTE: This is raw filesystem access, no filename-encoding, etc.
    if (relative_path_) {
      resource = new FileInputResource(this, type, url_string,
                                       input_url_string);
    } else {
      const std::string& filename = url.path();
      resource = new FileInputResource(this, type, url_string,
                                       filename);
    }
  } else {
    handler->Message(kError, "Unsupported scheme '%s' for url '%s'",
                     url.scheme().c_str(), url_string.c_str());
  }
  return resource;
}

bool ResourceManager::FetchOutputResource(
    OutputResource* output_resource,
    Writer* writer, MetaData* response_headers,
    MessageHandler* handler) const {
  // TODO(jmarantz): we are making lots of copies of the data.  We should
  // retrieve the data from the cache without copying it.

  // The http_cache is shared between multiple different classes in Instaweb.
  // To avoid colliding hash keys, we use a class-specific prefix.
  //
  // TODO(jmarantz): consider formalizing this in the HTTPCache API and
  // doing the StrCat inside.
  std::string content_key = StrCat(kContentsCacheKeyPrefix,
                                    output_resource->filename());
  bool ret = false;
  HTTPValue value;
  StringPiece content;
  if (http_cache_->Get(content_key.c_str(), &value, handler) &&
      value.ExtractContents(&content) &&
      value.ExtractHeaders(response_headers, handler) &&
      writer->Write(content, handler)) {
    ret = true;
  } else {
    if (output_resource->Read(handler)) {
      StringPiece contents = output_resource->contents();
      MetaData* meta_data = output_resource->metadata();
      http_cache_->Put(content_key.c_str(), *meta_data, contents, handler);
      ret = writer->Write(contents, handler);
      response_headers->CopyFrom(*meta_data);
    }
  }
  return ret;
}

bool ResourceManager::Write(const StringPiece& contents,
                            OutputResource* output,
                            int64 origin_expire_time_ms,
                            MessageHandler* handler) {
  MetaData* meta_data = output->metadata();
  SetDefaultHeaders(output->type(), meta_data);

  OutputResource::OutputWriter* writer = output->BeginWrite(handler);
  bool ret = (writer != NULL);
  if (ret) {
    ret = writer->Write(contents, handler);
    ret &= output->EndWrite(writer, handler);

    // Map the name of this resource to the fully expanded filename.  The
    // name of the output resource is usually a function of how it is
    // constructed from input resources.  For example, with combine_css,
    // output->name() encodes all the component CSS filenames.  The filename
    // this maps to includes the hash of the content.  Thus the two mappings
    // have different lifetimes.
    //
    // The name->filename map expires when any of the origin files expire.
    // When that occurs, fresh content must be read, and the output must
    // be recomputed and re-hashed.
    //
    // However, the hashed output filename can live, essentially, forever.
    // This is what we'll hash first as meta_data's default headers are
    // to cache forever.
    std::string content_key = StrCat(kContentsCacheKeyPrefix,
                                      output->filename());
    if (ret) {
      http_cache_->Put(content_key.c_str(), *meta_data, contents, handler);
    } else {
      meta_data->set_status_code(HttpStatus::NOT_FOUND);
      meta_data->set_reason_phrase("Not-Found");
    }

    // Now we'll mutate meta_data to expire when the origin expires, and
    // map the name to the filename.
    int64 delta_ms = origin_expire_time_ms - http_cache_->timer()->NowMs();
    int64 delta_sec = delta_ms / 1000;
    if (delta_sec > 0) {
      SimpleMetaData origin_meta_data;
      SetDefaultHeaders(output->type(), &origin_meta_data);
      std::string cache_control = StringPrintf(
          "public, max-age=%ld",
          static_cast<long>(delta_sec));  // NOLINT
      origin_meta_data.RemoveAll(kCacheControl);
      origin_meta_data.Add(kCacheControl, cache_control.c_str());
      origin_meta_data.ComputeCaching();
      std::string name_key = StrCat(kFilenameCacheKeyPrefix, output->name());
      http_cache_->Put(name_key.c_str(), origin_meta_data, output->hash(),
                       handler);
    }
  }
  return ret;
}

}  // namespace net_instaweb
