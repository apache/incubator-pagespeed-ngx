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

#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/file_input_resource.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
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
#include "net/instaweb/util/public/url_escaper.h"

namespace {

const char kCacheControl[] = "Cache-control";

// Our HTTP cache mostly stores full URLs, including the http: prefix,
// mapping them into the URL contents and HTTP headers.  However, we
// also put name->hash mappings into the HTTP cache, and we prefix
// these with "ResourceName:" to disambiguate them.
//
// Cache entries prefixed this way map the base name of a resource
// into the hash-code of the contents.  This mapping has a TTL based
// on the minimum TTL of the input resources used to construct the
// resource.  After that TTL has expired, we will need to re-fetch the
// resources from their origin, and recompute the hash.
const char kFilenameCacheKeyPrefix[] = "ResourceName:";

}  // namespace

namespace net_instaweb {

const int ResourceManager::kNotSharded = -1;

ResourceManager::ResourceManager(const StringPiece& file_prefix,
                                 const StringPiece& url_prefix_pattern,
                                 const int num_shards,
                                 FileSystem* file_system,
                                 FilenameEncoder* filename_encoder,
                                 UrlAsyncFetcher* url_async_fetcher,
                                 Hasher* hasher,
                                 HTTPCache* http_cache,
                                 DomainLawyer* domain_lawyer)
    : num_shards_(num_shards),
      resource_id_(0),
      file_system_(file_system),
      filename_encoder_(filename_encoder),
      url_async_fetcher_(url_async_fetcher),
      hasher_(hasher),
      statistics_(NULL),
      http_cache_(http_cache),
      url_escaper_(new UrlEscaper()),
      relative_path_(false),
      store_outputs_in_file_system_(true),
      domain_lawyer_(domain_lawyer) {
  file_prefix.CopyToString(&file_prefix_);
  SetUrlPrefixPattern(url_prefix_pattern);
}

ResourceManager::~ResourceManager() {
}

void ResourceManager::SetUrlPrefixPattern(const StringPiece& pattern) {
  pattern.CopyToString(&url_prefix_pattern_);
  ValidateShardsAgainstUrlPrefixPattern();
}

std::string ResourceManager::UrlPrefixFor(
    const ResourceNamer& namer) const {
  CHECK(!namer.hash().empty());
  std::string url_prefix;
  if (num_shards_ == 0) {
    url_prefix = url_prefix_pattern_;
  } else {
    size_t hash = namer.Hash();
    int shard = hash % num_shards_;
    CHECK_NE(std::string::npos, url_prefix_pattern_.find("%d"));
    url_prefix = StringPrintf(url_prefix_pattern_.c_str(), shard);
  }
  return url_prefix;
}

bool ResourceManager::UrlToResourceNamer(
    const StringPiece& url, int* shard, ResourceNamer* resource) const {
  StringPiece suffix;
  if (num_shards_ == 0) {
    CHECK_EQ(std::string::npos, url_prefix_pattern_.find("%d"));
    if (url.starts_with(url_prefix_pattern_)) {
      suffix = url.substr(url_prefix_pattern_.size());
      *shard = kNotSharded;
    }
  } else {
    CHECK_NE(std::string::npos, url_prefix_pattern_.find("%d"));
    // TODO(jmaessen): Ugh.  Lint hates this sscanf call and so do I.  Can parse
    // based on the results of the above find.
    if (sscanf(url.as_string().c_str(),
               url_prefix_pattern_.c_str(), shard) == 1) {
      // Get the actual prefix length by re-generating the URL.
      std::string prefix = StringPrintf(url_prefix_pattern_.c_str(), *shard);
      suffix = url.substr(prefix.size());
    }
  }
  return (!suffix.empty() && resource->Decode(this, suffix));
}

void ResourceManager::ValidateShardsAgainstUrlPrefixPattern() {
  std::string::size_type pos = url_prefix_pattern_.find('%');
  if (num_shards_ == 0) {
    CHECK(pos == StringPiece::npos) << "URL prefix should not have a percent "
                                    << "when num_shards==0";
  } else {
    // Ensure that the % is followed by a 'd'.  But be careful because
    // the percent may have appeared at the end of the string, which
    // is not necessarily null-terminated.
    if ((pos == std::string::npos) ||
        ((pos + 1) == url_prefix_pattern_.size()) ||
        (url_prefix_pattern_.substr(pos + 1, 1) != "d")) {
      CHECK(false) << "url_prefix must contain exactly one %d";
    } else {
      // make sure there is not another percent
      pos = url_prefix_pattern_.find('%', pos + 2);
      CHECK(pos == std::string::npos) << "Extra % found in url_prefix_pattern";
    }
  }
}

// TODO(jmarantz): consider moving this method to MetaData
void ResourceManager::SetDefaultHeaders(const ContentType* content_type,
                                        MetaData* header) const {
  CHECK_EQ(0, header->major_version());
  CHECK_EQ(0, header->NumAttributes());
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);
  if (content_type != NULL) {
    header->Add(HttpAttributes::kContentType, content_type->mime_type());
  }
  header->Add(kCacheControl, "public, max-age=31536000");

  // PageSpeed claims the "Vary" header is needed to avoid proxy cache
  // issues for clients where some accept gzipped content and some don't.
  header->Add("Vary", HttpAttributes::kAcceptEncoding);

  // TODO(jmarantz): add date/last-modified headers by default.
  int64 now_ms = http_cache_->timer()->NowMs();
  CharStarVector v;
  if (!header->Lookup("Date", &v)) {
    header->SetDate(now_ms);
  }
  if (!header->Lookup(HttpAttributes::kLastModified, &v)) {
    header->SetLastModified(now_ms);
  }

  // TODO(jmarantz): Page-speed suggested adding a "Last-Modified" header
  // for cache validation.  To do this we must track the max of all
  // Last-Modified values for all input resources that are used to
  // create this output resource.  For now we are using the current
  // time.

  header->ComputeCaching();
}

// TODO(jmarantz): consider moving this method to MetaData
void ResourceManager::SetContentType(const ContentType* content_type,
                                     MetaData* header) {
  CHECK(content_type != NULL);
  header->RemoveAll(HttpAttributes::kContentType);
  header->Add(HttpAttributes::kContentType, content_type->mime_type());
  header->ComputeCaching();
}

OutputResource* ResourceManager::CreateGeneratedOutputResource(
    const StringPiece& filter_prefix,
    const ContentType* content_type,
    MessageHandler* handler) {
  CHECK(content_type != NULL);
  ResourceNamer full_name;
  full_name.set_id(filter_prefix);
  full_name.set_name("_");
  // TODO(jmaessen): The addition of 1 below avoids the leading ".";
  // make this convention consistent and fix all code.
  full_name.set_ext(content_type->file_extension() + 1);
  OutputResource* resource = new OutputResource(this, full_name, content_type);
  resource->set_generated(true);
  return resource;
}

// Constructs a name key to help map all the parts of a resource name,
// excluding the hash, to the hash.  In other words, the full name of
// a resource is of the form
//    prefix.encoded_resource_name.hash.extension
// we know prefix and name, but not the hash, and we don't always even
// have the extension, which might have changes as the result of, for
// example image optimization (e.g. gif->png).  But We can "remember"
// the hash/extension for as long as the origin URL was cacheable.  So we
// construct this as a key:
//    ResourceName:prefix.encoded_resource_name
// and use that to map to the hash-code and extension.  If we know the
// hash-code then we may also be able to look up the contents in the same
// cache.
std::string ResourceManager::ConstructNameKey(
    const OutputResource& output) const {
  ResourceNamer full_name;
  full_name.set_id(output.filter_prefix());
  full_name.set_name(output.name());
  return full_name.EncodeIdName();
}

// Constructs an output resource corresponding to the specified input resource
// and encoded using the provided encoder.
OutputResource* ResourceManager::CreateOutputResourceFromResource(
    const StringPiece& filter_prefix,
    const ContentType* content_type,
    UrlSegmentEncoder* encoder,
    Resource* input_resource,
    MessageHandler* handler) {
  // TODO: use prefix and suffix here, which ought to be stored in resource.
  std::string name;
  encoder->EncodeToUrlSegment(input_resource->url(), &name);
  return CreateNamedOutputResource(filter_prefix, name, content_type, handler);
}

OutputResource* ResourceManager::CreateNamedOutputResource(
    const StringPiece& filter_prefix,
    const StringPiece& name,
    const ContentType* content_type,
    MessageHandler* handler) {
  assert(content_type != NULL);
  ResourceNamer full_name;
  full_name.set_id(filter_prefix);
  full_name.set_name(name);
  // TODO(jmaessen): The addition of 1 below avoids the leading ".";
  // make this convention consistent and fix all code.
  full_name.set_ext(content_type->file_extension() + 1);
  OutputResource* resource = new OutputResource(this, full_name, content_type);

  // Determine whether this output resource is still valid by looking
  // up by hash in the http cache.  Note that this cache entry will
  // expire when any of the origin resources expire.
  SimpleMetaData meta_data;
  StringPiece hash_extension;
  HTTPValue value;

  if (http_cache_->Get(
          full_name.EncodeIdName(), &value, &meta_data, handler) &&
      value.ExtractContents(&hash_extension)) {
    ResourceNamer hash_ext;
    if (hash_ext.DecodeHashExt(hash_extension)) {
      resource->SetHash(hash_ext.hash());
      // Note that the '.' must be included in the suffix
      // TODO(jmarantz): remove this from the suffix.
      resource->set_suffix(StrCat(".", hash_ext.ext()));
    }
  }
  return resource;
}

OutputResource* ResourceManager::CreateUrlOutputResource(
    const ResourceNamer& resource_id, const ContentType* type) {
  CHECK(!resource_id.hash().empty());
  OutputResource* resource = new OutputResource(this, resource_id, type);
  return resource;
}

void ResourceManager::set_filename_prefix(const StringPiece& file_prefix) {
  file_prefix.CopyToString(&file_prefix_);
}

Resource* ResourceManager::CreateInputResource(const GURL& base_gurl,
                                               const StringPiece& input_url,
                                               MessageHandler* handler) {
  CHECK(base_gurl.is_valid());
  std::string input_url_string(input_url.data(), input_url.size());
  GURL url = base_gurl.Resolve(input_url_string);
  if (!url.is_valid()) {
    // Note: Bad user-content can leave us here.
    handler->Message(kWarning, "Invalid url '%s' relative to base '%s'",
                     input_url_string.c_str(), base_gurl.spec().c_str());
    return NULL;
  }

  return CreateInputResourceGURL(url, handler);
}

Resource* ResourceManager::CreateInputResourceAndReadIfCached(
    const GURL& base_gurl, const StringPiece& input_url,
    MessageHandler* handler) {
  Resource* input_resource =
      CreateInputResource(base_gurl, input_url, handler);
  if (input_resource == NULL ||
      !input_resource->IsCacheable() ||
      !ReadIfCached(input_resource, handler)) {
    delete input_resource;
    input_resource = NULL;
  }
  return input_resource;
}

Resource* ResourceManager::CreateInputResourceFromOutputResource(
    UrlSegmentEncoder* encoder,
    OutputResource* output_resource,
    MessageHandler* handler) {
  // TODO(jmaessen): do lawyer checking here, and preferably call
  // CreateInputResourceGURL instead.
  Resource* input_resource = NULL;
  std::string input_url;
  if (encoder->DecodeFromUrlSegment(output_resource->name(), &input_url)) {
    input_resource = CreateInputResourceAbsolute(input_url, handler);
  }
  return input_resource;
}

Resource* ResourceManager::CreateInputResourceAbsolute(
    const StringPiece& absolute_url, MessageHandler* handler) {
  std::string url_string(absolute_url.data(), absolute_url.size());
  GURL url(url_string);
  if (!url.is_valid()) {
    // Note Bad user-content can leave us here.
    handler->Message(kWarning, "Invalid url '%s'", url_string.c_str());
    return NULL;
  }

  return CreateInputResourceGURL(url, handler);
}

Resource* ResourceManager::CreateInputResourceGURL(const GURL& url,
                                                   MessageHandler* handler) {
  CHECK(url.is_valid());
  const std::string& url_string = url.spec();

  Resource* resource = NULL;

  if (url.SchemeIs("data")) {
    resource = DataUrlInputResource::Make(url_string, this);
    if (resource == NULL) {
      // Note: Bad user-content can leave us here.
      handler->Message(kWarning, "Badly formatted data url '%s'",
                       url_string.c_str());
    }
  } else if (url.SchemeIs("http")) {
    // TODO(sligocki): Figure out if these are actually local, in
    // which case we can do a local file read.

    // Note: type may be NULL if url has an unexpected or malformed extension.
    const ContentType* type = NameExtensionToContentType(url_string);
    resource = new UrlInputResource(this, type, url_string);
  } else {
    // Note: Bad user-content can leave us here.
    handler->Message(kWarning, "Unsupported scheme '%s' for url '%s'",
                     url.scheme().c_str(), url_string.c_str());
  }
  return resource;
}

// TODO(jmarantz): remove writer/response_headers args from this function
// and force caller to pull those directly from output_resource, as that will
// save the effort of copying the headers.
//
// It will also simplify this routine quite a bit.
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
  bool ret = false;
  StringPiece content;
  MetaData* meta_data = output_resource->metadata();
  if (output_resource->IsWritten()) {
    ret = ((writer == NULL) ||
           ((output_resource->value_.ExtractContents(&content)) &&
            writer->Write(content, handler)));
  } else if (output_resource->has_hash()) {
    std::string url = output_resource->url();
    if (http_cache_->Get(url, &output_resource->value_, meta_data, handler) &&
        ((writer == NULL) ||
         output_resource->value_.ExtractContents(&content)) &&
        ((writer == NULL) || writer->Write(content, handler))) {
      output_resource->set_written(true);
      ret = true;
    } else if (ReadIfCached(output_resource, handler)) {
      content = output_resource->contents();
      http_cache_->Put(url, *meta_data, content, handler);
      ret = ((writer == NULL) || writer->Write(content, handler));
    }
  }
  if (ret && (response_headers != NULL) && (response_headers != meta_data)) {
    response_headers->CopyFrom(*meta_data);
  }
  return ret;
}

bool ResourceManager::Write(HttpStatus::Code status_code,
                            const StringPiece& contents,
                            OutputResource* output,
                            int64 origin_expire_time_ms,
                            MessageHandler* handler) {
  MetaData* meta_data = output->metadata();
  SetDefaultHeaders(output->type(), meta_data);
  meta_data->SetStatusAndReason(status_code);

  OutputResource::OutputWriter* writer = output->BeginWrite(handler);
  bool ret = (writer != NULL);
  if (ret) {
    ret = writer->Write(contents, handler);
    ret &= output->EndWrite(writer, handler);
    http_cache_->Put(output->url(), &output->value_, handler);

    if (!output->generated()) {
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

      // Now we'll mutate meta_data to expire when the origin expires, and
      // map the name to the hash.
      int64 delta_ms = origin_expire_time_ms - http_cache_->timer()->NowMs();
      int64 delta_sec = delta_ms / 1000;
      if ((delta_sec > 0) || http_cache_->force_caching()) {
        SimpleMetaData origin_meta_data;
        SetDefaultHeaders(output->type(), &origin_meta_data);
        std::string cache_control = StringPrintf(
            "public, max-age=%ld",
            static_cast<long>(delta_sec));  // NOLINT
        origin_meta_data.RemoveAll(kCacheControl);
        origin_meta_data.Add(kCacheControl, cache_control.c_str());
        origin_meta_data.ComputeCaching();

        ResourceNamer full_name;
        full_name.set_hash(output->hash());
        full_name.set_ext(output->suffix().substr(1));  // skip the "."
        http_cache_->Put(ConstructNameKey(*output), origin_meta_data,
                         full_name.EncodeHashExt(), handler);
      }
    }
  } else {
    // Note that we've already gotten a "could not open file" message;
    // this just serves to explain why and suggest a remedy.
    handler->Message(kInfo, "Could not create output resource"
                     " (bad filename prefix '%s'?)",
                     file_prefix_.c_str());
  }
  return ret;
}

void ResourceManager::ReadAsync(Resource* resource,
                                Resource::AsyncCallback* callback,
                                MessageHandler* handler) {
  if (http_cache_->Get(resource->url(), &resource->value_,
                       resource->metadata(), handler)) {
    callback->Done(true, resource);
  } else {
    resource->ReadAsync(callback, handler);
  }
}

bool ResourceManager::ReadIfCached(Resource* resource,
                                   MessageHandler* handler) const {
  bool ret = resource->loaded();
  if (!ret && resource->IsCacheable()) {
    ret = http_cache_->Get(resource->url(), &resource->value_,
                           resource->metadata(), handler);
  }
  // TODO(sligocki): How is ReadIfCached different from http_cache_->Get?
  // It appears what's going on is that we check the cache first, then send out
  // and async fetch and if that fetch was actually synchronous, retrieve the
  // result from the cache.
  // TODO(sligocki): More thorough comments and analysis of this.
  if (!ret) {
    ret = resource->ReadIfCached(handler);
  }
  if (ret) {
    resource->DetermineContentType();
  }
  return ret;
}

}  // namespace net_instaweb
