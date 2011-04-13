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
#include "net/instaweb/util/public/string.h"
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

}  // namespace

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
const char ResourceManager::kCacheKeyResourceNamePrefix[] = "rname/";
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
                                 CacheInterface* metadata_cache,
                                 NamedLockManager* lock_manager,
                                 MessageHandler* handler)
    : file_prefix_(file_prefix.data(), file_prefix.size()),
      resource_id_(0),
      file_system_(file_system),
      filename_encoder_(filename_encoder),
      url_async_fetcher_(url_async_fetcher),
      hasher_(hasher),
      statistics_(NULL),
      resource_url_domain_rejections_(NULL),
      http_cache_(http_cache),
      metadata_cache_(metadata_cache),
      relative_path_(false),
      store_outputs_in_file_system_(true),
      lock_manager_(lock_manager),
      max_age_string_(StringPrintf("max-age=%d",
                                   static_cast<int>(kGeneratedMaxAgeSec))),
      message_handler_(handler) {
}

ResourceManager::~ResourceManager() {
}

void ResourceManager::Initialize(Statistics* statistics) {
  statistics->AddVariable(kResourceUrlDomainRejections);
}

// TODO(jmarantz): consider moving this method to ResponseHeaders
void ResourceManager::SetDefaultHeaders(const ContentType* content_type,
                                        ResponseHeaders* header) const {
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);
  header->RemoveAll(HttpAttributes::kContentType);
  if (content_type != NULL) {
    header->Add(HttpAttributes::kContentType, content_type->mime_type());
  }
  int64 now_ms = http_cache_->timer()->NowMs();
  header->Replace(HttpAttributes::kCacheControl, max_age_string_);
  GoogleString expires_string;
  header->RemoveAll(HttpAttributes::kExpires);
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
  header->Replace(HttpAttributes::kEtag, kResourceEtagValue);

  // TODO(jmarantz): add date/last-modified headers by default.
  StringStarVector v;
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
  header->Replace(HttpAttributes::kContentType, content_type->mime_type());
  header->ComputeCaching();
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

    if (output->kind() != OutputResource::kOnTheFlyResource) {
      http_cache_->Put(output->url(), &output->value_, handler);
    }

    // If our URL is derived from some pre-existing URL (and not invented by
    // us due to something like outlining), cache the mapping from original URL
    // to the constructed one.
    if (output->kind() != OutputResource::kOutlinedResource) {
      output->EnsureCachedResultCreated()->set_optimizable(true);
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
  output->EnsureCachedResultCreated()->set_optimizable(false);
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
  GoogleString name_key = StrCat(kCacheKeyResourceNamePrefix,
                                 output->name_key());
  CachedResult* cached = output->EnsureCachedResultCreated();
  if (cached->optimizable()) {
    cached->set_url(output->url());
  }
  cached->set_origin_expiration_time_ms(origin_expire_time_ms);
  output->SaveCachedResult(name_key, handler);
}

bool ResourceManager::IsImminentlyExpiring(int64 start_date_ms,
                                           int64 expire_ms) const {
  // Consider a resource with 5 minute expiration time (the default
  // assumed by mod_pagespeed when a potentialy cacheable resource
  // lacks a cache control header, which happens a lot).  If the
  // origin TTL was 5 minutes and 4 minutes have expired, then we want
  // to re-fetch it so that we can avoid expiring the data.
  //
  // If we don't do this, then every 5 minutes, someone will see
  // this page unoptimized.  In a site with very low QPS, including
  // test instances of a site, this can happen quite often.
  int64 now_ms = timer()->NowMs();
  int64 ttl_ms = expire_ms - start_date_ms;
  // Only proactively refresh resources that have at least our
  // default expiration of 5 minutes.
  //
  // TODO(jmaessen): Lower threshold when If-Modified-Since checking is in
  // place; consider making this settable.
  if (ttl_ms >= ResponseHeaders::kImplicitCacheTtlMs) {
    int64 elapsed_ms = now_ms - start_date_ms;
    if ((elapsed_ms * 100) >= (kRefreshExpirePercent * ttl_ms)) {
      return true;
    }
  }
  return false;
}

void ResourceManager::RefreshIfImminentlyExpiring(
    Resource* resource, MessageHandler* handler) const {
  if (!http_cache_->force_caching() && resource->IsCacheable()) {
    const ResponseHeaders* headers = resource->metadata();
    int64 start_date_ms = headers->timestamp_ms();
    int64 expire_ms = headers->CacheExpirationTimeMs();
    if (IsImminentlyExpiring(start_date_ms, expire_ms)) {
      resource->Freshen(handler);
    }
  }
}

}  // namespace net_instaweb
