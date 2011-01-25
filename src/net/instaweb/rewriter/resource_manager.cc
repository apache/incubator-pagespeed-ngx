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

#include "net/instaweb/rewriter/public/resource_manager.h"

#include "base/scoped_ptr.h"
#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/file_input_resource.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "net/instaweb/util/public/content_type.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/statistics.h"
#include <string>
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/time_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_escaper.h"

namespace net_instaweb {

namespace {

// resource_url_domain_rejections counts the number of urls on a page that we
// could have rewritten, except that they lay in a domain that did not
// permit resource rewriting relative to the current page.
const char kResourceUrlDomainRejections[] = "resource_url_domain_rejections";

const int64 kGeneratedMaxAgeMs = Timer::kYearMs;
const int64 kGeneratedMaxAgeSec = Timer::kYearMs / Timer::kSecondMs;
const int64 kRefreshExpirePercent = 75;

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
//
// Whenever we change the hashing function we can bust caches by
// changing this prefix.
//
// TODO(jmarantz): inject the SVN version number here to automatically bust
// caches whenever pagespeed is upgraded.
const char kCacheKeyPrefix[] = "rname/";

// In the case when we want to remember that it was not beneficial to produce
// a certain resource we include this header in the metadata of the entry
// in the above cache.
const char kCacheUnoptimizableHeader[] = "X-ModPagespeed-Unoptimizable";

}  // namespace

const int ResourceManager::kNotSharded = -1;

// We set etags for our output resources to "W/0".  The "W" means
// that this etag indicates a functional consistency, but is not
// guaranteeing byte-consistency.  This distinction is important because
// we serve different bytes for clients that do not accept gzip.
//
// This value is a shared constant so that it can also be used in
// the Apache-specific code that repairs headers after mod_headers
// alters them.
const char ResourceManager::kResourceEtagValue[] = "W/0";

ResourceManager::ResourceManager(const StringPiece& file_prefix,
                                 FileSystem* file_system,
                                 FilenameEncoder* filename_encoder,
                                 UrlAsyncFetcher* url_async_fetcher,
                                 Hasher* hasher,
                                 HTTPCache* http_cache,
                                 NamedLockManager* lock_manager)
    : file_prefix_(file_prefix.data(), file_prefix.size()),
      resource_id_(0),
      file_system_(file_system),
      filename_encoder_(filename_encoder),
      url_async_fetcher_(url_async_fetcher),
      hasher_(hasher),
      statistics_(NULL),
      resource_url_domain_rejections_(NULL),
      http_cache_(http_cache),
      url_escaper_(new UrlEscaper()),
      relative_path_(false),
      store_outputs_in_file_system_(true),
      lock_manager_(lock_manager),
      max_age_string_(StringPrintf("max-age=%d",
                                   static_cast<int>(kGeneratedMaxAgeSec))) {
}

ResourceManager::~ResourceManager() {
}

void ResourceManager::Initialize(Statistics* statistics) {
  statistics->AddVariable(kResourceUrlDomainRejections);
}

// TODO(jmarantz): consider moving this method to ResponseHeaders
void ResourceManager::SetDefaultHeaders(const ContentType* content_type,
                                        ResponseHeaders* header) const {
  CHECK(!header->has_major_version());
  CHECK_EQ(0, header->NumAttributes());
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);
  if (content_type != NULL) {
    header->Add(HttpAttributes::kContentType, content_type->mime_type());
  }
  int64 now_ms = http_cache_->timer()->NowMs();
  header->Add(HttpAttributes::kCacheControl, max_age_string_);
  std::string expires_string;
  if (ConvertTimeToString(now_ms + kGeneratedMaxAgeMs, &expires_string)) {
    header->Add(HttpAttributes::kExpires, expires_string);
  }

  // While PageSpeed claims the "Vary" header is needed to avoid proxy cache
  // issues for clients where some accept gzipped content and some don't, it
  // should not be done here.  It should instead be done by whatever code is
  // conditionally gzipping the content based on user-agent, e.g. mod_deflate.
  // header->Add(HttpAttributes::kVary, HttpAttributes::kAcceptEncoding);

  // ETag is superfluous for mod_pagespeed as we sign the URL with the
  // content hash.  However, we have seen evidence that IE8 will not
  // serve images from its cache when the image lacks an ETag.  Since
  // we sign URLs, there is no reason to have a unique signature in
  // the ETag.
  header->Add(HttpAttributes::kEtag, kResourceEtagValue);

  // TODO(jmarantz): add date/last-modified headers by default.
  CharStarVector v;
  if (!header->Lookup(HttpAttributes::kDate, &v)) {
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

// TODO(jmarantz): consider moving this method to ResponseHeaders
void ResourceManager::SetContentType(const ContentType* content_type,
                                     ResponseHeaders* header) {
  CHECK(content_type != NULL);
  header->RemoveAll(HttpAttributes::kContentType);
  header->Add(HttpAttributes::kContentType, content_type->mime_type());
  header->ComputeCaching();
}

// Constructs an output resource corresponding to the specified input resource
// and encoded using the provided encoder.
OutputResource* ResourceManager::CreateOutputResourceFromResource(
    const StringPiece& filter_prefix,
    const ContentType* content_type,
    UrlSegmentEncoder* encoder,
    Resource* input_resource,
    const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  OutputResource* result = NULL;
  if (input_resource != NULL) {
    std::string url = input_resource->url();
    GURL input_gurl(url);
    CHECK(input_gurl.is_valid());  // or input_resource should have been NULL.
    std::string name;
    encoder->EncodeToUrlSegment(GoogleUrl::LeafWithQuery(input_gurl), &name);
    result = CreateOutputResourceWithPath(
        GoogleUrl::AllExceptLeaf(input_gurl),
        filter_prefix, name, content_type, rewrite_options, handler);
  }
  return result;
}

OutputResource* ResourceManager::CreateOutputResourceForRewrittenUrl(
    const GURL& document_gurl,
    const StringPiece& filter_prefix,
    const StringPiece& resource_url,
    const ContentType* content_type,
    UrlSegmentEncoder* encoder,
    const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  OutputResource* output_resource = NULL;
  UrlPartnership partnership(rewrite_options, document_gurl);
  if (partnership.AddUrl(resource_url, handler)) {
    std::string base = partnership.ResolvedBase();
    std::string relative_url = partnership.RelativePath(0);
    std::string name;
    encoder->EncodeToUrlSegment(relative_url, &name);
    output_resource = CreateOutputResourceWithPath(
        base, filter_prefix, name, content_type, rewrite_options, handler);
  }
  return output_resource;
}

OutputResource* ResourceManager::CreateOutputResourceWithPath(
    const StringPiece& path,
    const StringPiece& filter_prefix,
    const StringPiece& name,
    const ContentType* content_type,
    const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  CHECK(content_type != NULL);
  ResourceNamer full_name;
  full_name.set_id(filter_prefix);
  full_name.set_name(name);
  // TODO(jmaessen): The addition of 1 below avoids the leading ".";
  // make this convention consistent and fix all code.
  full_name.set_ext(content_type->file_extension() + 1);
  OutputResource* resource =
      new OutputResource(this, path, full_name, content_type, rewrite_options);

  // Determine whether this output resource is still valid by looking
  // up by hash in the http cache.  Note that this cache entry will
  // expire when any of the origin resources expire.
  ResponseHeaders meta_data;
  StringPiece hash_extension;
  HTTPValue value;
  std::string name_key = StrCat(kCacheKeyPrefix, resource->name_key());
  if ((http_cache_->Find(name_key, &value, &meta_data, handler)
       == HTTPCache::kFound) &&
      value.ExtractContents(&hash_extension)) {
    CharStarVector dummy;
    if (meta_data.Lookup(kCacheUnoptimizableHeader, &dummy)) {
      resource->set_optimizable(false);
    } else {
      ResourceNamer hash_ext;
      if (hash_ext.DecodeHashExt(hash_extension)) {
        resource->SetHash(hash_ext.hash());
        // Note that the '.' must be included in the suffix
        // TODO(jmarantz): remove this from the suffix.
        resource->set_suffix(StrCat(".", hash_ext.ext()));
      }
    }
  }
  return resource;
}

OutputResource* ResourceManager::CreateOutputResourceForFetch(
    const StringPiece& url) {
  OutputResource* resource = NULL;
  std::string url_string(url.data(), url.size());
  GURL gurl(url_string);
  if (gurl.is_valid()) {
    std::string name = GoogleUrl::LeafSansQuery(gurl);
    ResourceNamer namer;
    if (namer.Decode(name)) {
      std::string base = GoogleUrl::AllExceptLeaf(gurl);
      // The RewriteOptions* is not supplied when creating an output-resource
      // on behalf of a fetch.  This is because that field is only used for
      // domain sharding, which is a rewriting activity, not a fetching
      // activity.
      resource = new OutputResource(this, base, namer, NULL, NULL);
    }
  }
  return resource;
}

void ResourceManager::set_filename_prefix(const StringPiece& file_prefix) {
  file_prefix.CopyToString(&file_prefix_);
}

// Implements lazy initialization of resource_url_domain_rejections_,
// necessitated by the fact that we can set_statistics before
// Initialize(...) has been called and thus can't safely look
// for the variable until first use.
void ResourceManager::IncrementResourceUrlDomainRejections() {
  if (resource_url_domain_rejections_ == NULL) {
    if (statistics_ == NULL) {
      return;
    }
    resource_url_domain_rejections_ =
        statistics_->GetVariable(kResourceUrlDomainRejections);
  }
  resource_url_domain_rejections_->Add(1);
}

Resource* ResourceManager::CreateInputResource(
    const GURL& base_gurl,
    const StringPiece& input_url,
    const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  UrlPartnership partnership(rewrite_options, base_gurl);
  Resource* resource = NULL;
  if (partnership.AddUrl(input_url, handler)) {
    const GURL* input_gurl = partnership.FullPath(0);
    resource = CreateInputResourceUnchecked(*input_gurl, rewrite_options,
                                            handler);
  } else {
    handler->Message(kInfo, "Invalid resource url '%s' relative to '%s'",
                     input_url.as_string().c_str(), base_gurl.spec().c_str());
    IncrementResourceUrlDomainRejections();
    resource = NULL;
  }
  return resource;
}

Resource* ResourceManager::CreateInputResourceAndReadIfCached(
    const GURL& base_gurl, const StringPiece& input_url,
    const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  Resource* input_resource = CreateInputResource(
      base_gurl, input_url, rewrite_options, handler);
  if (input_resource != NULL &&
      (!input_resource->IsCacheable() ||
       !ReadIfCached(input_resource, handler))) {
    handler->Message(kInfo,
                     "%s: Couldn't fetch resource %s to rewrite.",
                     base_gurl.spec().c_str(), input_url.as_string().c_str());
    delete input_resource;
    input_resource = NULL;
  }
  return input_resource;
}

Resource* ResourceManager::CreateInputResourceFromOutputResource(
    UrlSegmentEncoder* encoder,
    OutputResource* output_resource,
    const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  Resource* input_resource = NULL;
  std::string input_name;
  if (encoder->DecodeFromUrlSegment(output_resource->name(), &input_name)) {
    GURL base_gurl(output_resource->resolved_base());
    input_resource = CreateInputResource(base_gurl, input_name,
                                         rewrite_options, handler);
  }
  return input_resource;
}

Resource* ResourceManager::CreateInputResourceAbsolute(
    const StringPiece& absolute_url, const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  std::string url_string(absolute_url.data(), absolute_url.size());
  GURL url(url_string);
  return CreateInputResourceUnchecked(url, rewrite_options, handler);
}

Resource* ResourceManager::CreateInputResourceUnchecked(
    const GURL& url, const RewriteOptions* rewrite_options,
    MessageHandler* handler) {
  if (!url.is_valid()) {
    // Note: Bad user-content can leave us here.  But it's really hard
    // to concatenate a valid protocol and domain onto an arbitrary string
    // and end up with an invalid GURL.
    handler->Message(kWarning, "Invalid resource url '%s'",
                     url.possibly_invalid_spec().c_str());
    return NULL;
  }
  std::string url_string = GoogleUrl::Spec(url);

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
    resource = new UrlInputResource(this, rewrite_options, type, url_string);
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
    Writer* writer, ResponseHeaders* response_headers,
    MessageHandler* handler, BlockingBehavior blocking) const {
  if (output_resource == NULL) {
    return false;
  }

  // TODO(jmarantz): we are making lots of copies of the data.  We should
  // retrieve the data from the cache without copying it.

  // The http_cache is shared between multiple different classes in Instaweb.
  // To avoid colliding hash keys, we use a class-specific prefix.
  //
  // TODO(jmarantz): consider formalizing this in the HTTPCache API and
  // doing the StrCat inside.
  bool ret = false;
  StringPiece content;
  ResponseHeaders* meta_data = output_resource->metadata();
  if (output_resource->IsWritten()) {
    ret = ((writer == NULL) ||
           ((output_resource->value_.ExtractContents(&content)) &&
            writer->Write(content, handler)));
  } else if (output_resource->has_hash()) {
    std::string url = output_resource->url();
    // Check cache once without lock, then if that fails try again with lock.
    // Note that it would be *correct* to lock up front and only check once.
    // However, the common case here is that the resource is present (because
    // this path mostly happens during resource fetch).  We want to avoid
    // unnecessarily serializing resource fetch on a lock.
    for (int i = 0; !ret && i < 2; ++i) {
      if ((http_cache_->Find(url, &output_resource->value_, meta_data, handler)
           == HTTPCache::kFound) &&
          ((writer == NULL) ||
           output_resource->value_.ExtractContents(&content)) &&
          ((writer == NULL) || writer->Write(content, handler))) {
        output_resource->set_written(true);
        ret = true;
      } else if (ReadIfCached(output_resource, handler)) {
        content = output_resource->contents();
        http_cache_->Put(url, meta_data, content, handler);
        ret = ((writer == NULL) || writer->Write(content, handler));
      }
      // On the first iteration, obtain the lock if we don't have data.
      if (!ret && i == 0 && !output_resource->LockForCreation(this, blocking)) {
        // We didn't get the lock; we need to abandon ship.  The caller should
        // see this as a successful fetch for which IsWritten() remains false.
        CHECK(!output_resource->IsWritten());
        ret = true;
      }
    }
  } else {
    // TODO(jmaessen): This path should also re-try fetching the resource after
    // obtaining the lock.  However, in this case we need to look for the hash
    // in the cache first, which duplicates logic from creation time and makes
    // life generally complicated.
    ret = !output_resource->LockForCreation(this, blocking);
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
  ResponseHeaders* meta_data = output->metadata();
  SetDefaultHeaders(output->type(), meta_data);
  meta_data->SetStatusAndReason(status_code);

  // The URL for any resource we will write includes the hash of contents,
  // so it can can live, essentially, forever. So compute this hash,
  // and cache the output using meta_data's default headers which are to cache
  // forever.
  scoped_ptr<OutputResource::OutputWriter> writer(output->BeginWrite(handler));
  bool ret = (writer != NULL);
  if (ret) {
    ret = writer->Write(contents, handler);
    ret &= output->EndWrite(writer.get(), handler);
    http_cache_->Put(output->url(), &output->value_, handler);

    // If our URL is derived from some pre-existing URL (and not invented by
    // us due to something like outlining), cache the mapping from original URL
    // to the constructed one.
    if (!output->generated()) {
      CacheComputedResourceMapping(output, origin_expire_time_ms, handler);
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

void ResourceManager::WriteUnoptimizable(OutputResource* output,
                                         int64 origin_expire_time_ms,
                                         MessageHandler* handler) {
  output->set_optimizable(false);
  CacheComputedResourceMapping(output, origin_expire_time_ms, handler);
}

// Map the name of this resource to information on its contents:
// either the fully expanded filename, or the fact that we don't want
// to make this resource (!optimizable()).
//
// The name of the output resource is usually a function of how it is
// constructed from input resources.  For example, with combine_css,
// output->name() encodes all the component CSS filenames.  The filename
// this maps to includes the hash of the content.
//
// The name->filename map expires when any of the origin files expire.
// When that occurs, fresh content must be read, and the output must
// be recomputed and re-hashed. We'll hence mutate meta_data to expire when the
// origin expires
//
// TODO(morlovich) We should consider caching based on the input hash, too,
// so we don't end redoing work when input resources don't change but have
// short expiration.
void ResourceManager::CacheComputedResourceMapping(OutputResource* output,
    int64 origin_expire_time_ms, MessageHandler* handler) {
  int64 delta_ms = origin_expire_time_ms - http_cache_->timer()->NowMs();
  int64 delta_sec = delta_ms / 1000;
  if ((delta_sec > 0) || http_cache_->force_caching()) {
    ResponseHeaders origin_meta_data;
    SetDefaultHeaders(output->type(), &origin_meta_data);
    std::string cache_control = StringPrintf(
        "max-age=%ld",
        static_cast<long>(delta_sec));  // NOLINT
    origin_meta_data.RemoveAll(HttpAttributes::kCacheControl);
    origin_meta_data.Add(HttpAttributes::kCacheControl, cache_control);
    if (!output->optimizable()) {
      origin_meta_data.Add(kCacheUnoptimizableHeader, "true");
    }
    origin_meta_data.ComputeCaching();

    std::string name_key = StrCat(kCacheKeyPrefix, output->name_key());
    std::string file_mapping;
    if (output->optimizable()) {
      file_mapping = output->hash_ext();
    }
    http_cache_->Put(name_key, &origin_meta_data, file_mapping, handler);
  }
}

void ResourceManager::RefreshImminentlyExpiringResource(
    Resource* resource, MessageHandler* handler) const {
  // Consider a resource with 5 minute expiration time (the default
  // assumed by mod_pagespeed when a potentialy cacheable resource
  // lacks a cache control header, which happens a lot).  If the
  // origin TTL was 5 minutes and 4 minutes have expired, then re-fetch
  // it so that we can avoid expiring the data.
  //
  // If we don't do this, then every 5 minutes, someone will see
  // this page unoptimized.  In a site with very low QPS, including
  // test instances of a site, this can happen quite often.
  if (!http_cache_->force_caching() && resource->IsCacheable()) {
    int64 now_ms = timer()->NowMs();
    const ResponseHeaders* headers = resource->metadata();
    int64 start_date_ms = headers->timestamp_ms();
    int64 expire_ms = headers->CacheExpirationTimeMs();
    int64 ttl_ms = expire_ms - start_date_ms;

    // Only proactively refresh resources that have at least our
    // default expiration of 5 minutes.
    //
    // TODO(jmaessen): Lower threshold when If-Modified-Since checking is in
    // place; consider making this settable.
    if (ttl_ms >= ResponseHeaders::kImplicitCacheTtlMs) {
      int64 elapsed_ms = now_ms - start_date_ms;
      if ((elapsed_ms * 100) >= (kRefreshExpirePercent * ttl_ms)) {
        resource->Freshen(handler);
      }
    }
  }
}

void ResourceManager::ReadAsync(Resource* resource,
                                Resource::AsyncCallback* callback,
                                MessageHandler* handler) {
  HTTPCache::FindResult result = HTTPCache::kNotFound;

  // If the resource is not already loaded, and this type of resource (e.g.
  // URL vs File vs Data) is cacheable, then try to load it.
  if (resource->loaded()) {
    result = HTTPCache::kFound;
  } else if (resource->IsCacheable()) {
    result = http_cache_->Find(resource->url(), &resource->value_,
                               resource->metadata(), handler);
  }

  switch (result) {
    case HTTPCache::kFound:
      RefreshImminentlyExpiringResource(resource, handler);
      callback->Done(true, resource);
      break;
    case HTTPCache::kRecentFetchFailedDoNotRefetch:
      // TODO(jmarantz): in this path, should we try to fetch again
      // sooner than 5 minutes?  The issue is that in this path we are
      // serving for the user, not for a rewrite.  This could get
      // frustrating, even if the software is functioning as intended,
      // because a missing resource that is put in place by a site
      // admin will not be checked again for 5 minutes.
      //
      // The "good" news is that if the admin is willing to crank up
      // logging to 'info' then http_cache.cc will log the
      // 'remembered' failure.
      callback->Done(false, resource);
      break;
    case HTTPCache::kNotFound:
      // If not, load it asynchronously.
      resource->LoadAndCallback(callback, handler);
      break;
  }
  // TODO(sligocki): Do we need to call DetermineContentType like below?
}

bool ResourceManager::ReadIfCached(Resource* resource,
                                   MessageHandler* handler) const {
  HTTPCache::FindResult result = HTTPCache::kNotFound;

  // If the resource is not already loaded, and this type of resource (e.g.
  // URL vs File vs Data) is cacheable, then try to load it.
  if (resource->loaded()) {
    result = HTTPCache::kFound;
  } else if (resource->IsCacheable()) {
    result = http_cache_->Find(resource->url(), &resource->value_,
                               resource->metadata(), handler);
  }
  if ((result == HTTPCache::kNotFound) && resource->Load(handler)) {
    result = HTTPCache::kFound;
  }
  if (result == HTTPCache::kFound) {
    resource->DetermineContentType();
    RefreshImminentlyExpiringResource(resource, handler);
    return true;
  }
  return false;
}

}  // namespace net_instaweb
