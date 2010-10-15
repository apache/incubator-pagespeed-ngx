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
#include "net/instaweb/rewriter/public/resource_encoder.h"
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
#include "net/instaweb/util/public/string_hash.h"
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
                                 HTTPCache* http_cache)
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
      store_outputs_in_file_system_(true) {
  file_prefix.CopyToString(&file_prefix_);
  SetUrlPrefixPattern(url_prefix_pattern);
}

ResourceManager::~ResourceManager() {
}

void ResourceManager::SetUrlPrefixPattern(const StringPiece& pattern) {
  pattern.CopyToString(&url_prefix_pattern_);
  ValidateShardsAgainstUrlPrefixPattern();
}

std::string ResourceManager::GenerateUrl(const StringPiece& name) const {
  std::string url_prefix;
  if (num_shards_ == 0) {
    url_prefix = url_prefix_pattern_;
  } else {
    size_t hash = HashString(name.data(), name.size());
    int shard = hash % num_shards_;
    CHECK_NE(std::string::npos, url_prefix_pattern_.find("%d"));
    url_prefix = StringPrintf(url_prefix_pattern_.c_str(), shard);
  }
  return StrCat(url_prefix, name);
}

const char* ResourceManager::SplitUrl(const char* url, int* shard) const {
  const char* resource = NULL;
  if (num_shards_ == 0) {
    CHECK_EQ(std::string::npos, url_prefix_pattern_.find("%d"));
    if (strncmp(url, url_prefix_pattern_.data(),
                url_prefix_pattern_.size()) == 0) {
      resource = url + url_prefix_pattern_.size();
      *shard = kNotSharded;
    }
  } else {
    CHECK_NE(std::string::npos, url_prefix_pattern_.find("%d"));
    if (sscanf(url, url_prefix_pattern_.c_str(), shard) == 1) {
      // Get the actual prefix length by re-generating the URL.
      std::string prefix = StringPrintf(url_prefix_pattern_.c_str(), *shard);
      size_t prefix_length = prefix.size();
      resource = url + prefix_length;
    }
  }
  return resource;
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
  CHECK(header->major_version() == 0);
  CHECK(header->NumAttributes() == 0);
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
  OutputResource* resource = new OutputResource(
      this, content_type, filter_prefix, "_");
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
    const OutputResource* output) const {
  ResourceEncoder encoder;
  encoder.set_id(output->filter_prefix());
  encoder.set_name(output->name());
  return encoder.EncodeNameKey();
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
  SimpleMetaData meta_data;
  StringPiece hash_extension;
  HTTPValue value;

  if (http_cache_->Get(ConstructNameKey(resource), &value, &meta_data,
                       handler) &&
      value.ExtractContents(&hash_extension)) {
    ResourceEncoder encoder;
    if (encoder.DecodeHashExt(hash_extension)) {
      resource->SetHash(encoder.hash());
      // Note that the '.' must be included in the suffix
      // TODO(jmarantz): remove this from the suffix.
      resource->set_suffix(StrCat(".", encoder.ext()));
    }
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
  std::string url_buffer;
  GURL url;
  StringPiece actual_url;
  if (base_url_ == NULL) {
    input_url.CopyToString(&url_buffer);
    url = GURL(url_buffer);
    if (!url.is_valid()) {
      handler->Message(kError,
                       "CreateInputResource called before base_url set.");
      return NULL;
    }
    actual_url.set(url_buffer.data(), url_buffer.size());
  } else if (relative_path_) {
    url_buffer = base_url_->scheme();
    url_buffer += ":";
    input_url.AppendToString(&url_buffer);
    url = GURL(url_buffer);
    actual_url.set(url_buffer.data(), url_buffer.size());
  } else {
    // Get absolute url based on the (possibly relative) input_url.
    input_url.CopyToString(&url_buffer);
    url = base_url_->Resolve(url_buffer);
    actual_url.set(url.spec().data(), url.spec().size());
  }

  if (url.SchemeIs("data")) {
    resource = DataUrlInputResource::Make(actual_url, this);
    if (resource == NULL) {
      handler->Message(kError, "Badly formatted data url '%s'",
                       actual_url.as_string().c_str());
    }
  } else {
    const ContentType* type = NameExtensionToContentType(input_url);
    // Note that the type may be null if, for example, an image has an
    // unexpected extension.  We will have to figure out the image type
    // from the content, but we will not be able to do that until it's
    // been read in.

    if (url.SchemeIs("http")) {
      // TODO(sligocki): Figure out if these are actually local by
      // seeing if the serving path matches url_prefix_pattern_, in
      // which case we can do a local file read.
      // TODO(jmaessen): In order to permit url loading from a context
      // where the base url isn't set, we must keep the normalized url
      // in the UrlInputResource rather than the original input_url.
      // This is ugly and yields unnecessarily verbose rewritten urls.
      resource = new UrlInputResource(this, type, actual_url);
      // TODO(sligocki): Probably shouldn't support file:// scheme.
      // (but it's used extensively in eg rewriter_test.)
    } else if (url.SchemeIsFile()) {
      // NOTE: This is raw filesystem access, no filename-encoding, etc.
      if (relative_path_) {
        resource = new FileInputResource(this, type, actual_url,
                                         input_url);
      } else {
        const std::string& filename = url.path();
        resource = new FileInputResource(this, type, actual_url,
                                         filename);
      }
    } else {
      handler->Message(kError, "Unsupported scheme '%s' for url '%s'",
                       url.scheme().c_str(), actual_url.as_string().c_str());
    }
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

        ResourceEncoder encoder;
        encoder.set_hash(output->hash());
        encoder.set_ext(output->suffix().substr(1));  // skip the "."
        http_cache_->Put(ConstructNameKey(output), origin_meta_data,
                         encoder.EncodeHashExt(), handler);
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
  bool ret =
      (resource->loaded() ||
       (resource->IsCacheable() &&
        http_cache_->Get(resource->url(), &resource->value_,
                         resource->metadata(), handler)) ||
       resource->ReadIfCached(handler));
  if (ret) {
    resource->DetermineContentType();
  }
  return ret;
}

}  // namespace net_instaweb
