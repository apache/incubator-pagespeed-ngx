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

#include <algorithm>                   // for std::binary_search
#include <cstddef>                     // for size_t
#include <set>
#include <vector>
#include "base/logging.h"               // for operator<<, etc
#include "base/scoped_ptr.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"  // for HttpAttributes, etc
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"          // for STLDeleteElements
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class RewriteFilter;

namespace {

const int64 kGeneratedMaxAgeMs = Timer::kYearMs;
const int64 kRefreshExpirePercent = 75;

// Attributes that should not be automatically copied from inputs to outputs
const char* kExcludedAttributes[] = {
  HttpAttributes::kCacheControl,
  HttpAttributes::kContentEncoding,
  HttpAttributes::kContentLength,
  HttpAttributes::kContentType,
  HttpAttributes::kDate,
  HttpAttributes::kEtag,
  HttpAttributes::kExpires,
  HttpAttributes::kLastModified,
  // Rewritten resources are publicly cached, so we should avoid cookies
  // which are generally meant for private data.
  HttpAttributes::kSetCookie,
  HttpAttributes::kSetCookie2,
  HttpAttributes::kTransferEncoding,
  HttpAttributes::kVary
};

bool IsExcludedAttribute(const char* attribute) {
  const char** end = kExcludedAttributes + arraysize(kExcludedAttributes);
  return std::binary_search(kExcludedAttributes, end, attribute,
                            CharStarCompareInsensitive());
}

}  // namespace

// Statistics group names.
const char ResourceManager::kStatisticsGroup[] = "Statistics";

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

// We set etags for our output resources to "W/0".  The "W" means
// that this etag indicates a functional consistency, but is not
// guaranteeing byte-consistency.  This distinction is important because
// we serve different bytes for clients that do not accept gzip.
//
// This value is a shared constant so that it can also be used in
// the Apache-specific code that repairs headers after mod_headers
// alters them.
const char ResourceManager::kResourceEtagValue[] = "W/0";

class ResourceManagerHttpCallback : public OptionsAwareHTTPCacheCallback {
 public:
  ResourceManagerHttpCallback(Resource::AsyncCallback* resource_callback,
                              ResourceManager* resource_manager);
  virtual ~ResourceManagerHttpCallback();
  virtual void Done(HTTPCache::FindResult find_result);

 private:
  Resource::AsyncCallback* resource_callback_;
  ResourceManager* resource_manager_;
};

ResourceManager::ResourceManager(RewriteDriverFactory* factory)
    : thread_system_(factory->thread_system()),
      rewrite_stats_(NULL),
      file_system_(factory->file_system()),
      filename_encoder_(NULL),
      url_namer_(NULL),
      scheduler_(factory->scheduler()),
      url_async_fetcher_(NULL),
      hasher_(NULL),
      lock_hasher_(20),
      contents_hasher_(21),
      statistics_(NULL),
      http_cache_(NULL),
      metadata_cache_(NULL),
      relative_path_(false),
      store_outputs_in_file_system_(true),
      block_until_completion_in_render_(false),
      async_rewrites_(true),
      lock_manager_(NULL),
      message_handler_(NULL),
      trying_to_cleanup_rewrite_drivers_(false),
      factory_(factory),
      rewrite_drivers_mutex_(thread_system_->NewMutex()),
      html_workers_(NULL),
      rewrite_workers_(NULL),
      low_priority_rewrite_workers_(NULL) {
  // Make sure the excluded-attributes are in abc order so binary_search works.
  // Make sure to use the same comparator that we pass to the binary_search.
#ifndef NDEBUG
  for (int i = 1, n = arraysize(kExcludedAttributes); i < n; ++i) {
    DCHECK(CharStarCompareInsensitive()(kExcludedAttributes[i - 1],
                                        kExcludedAttributes[i]));
  }
#endif
}

ResourceManager::~ResourceManager() {
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());

    // Actually release anything that got deferred above.
    trying_to_cleanup_rewrite_drivers_ = false;
    for (RewriteDriverSet::iterator i =
             deferred_release_rewrite_drivers_.begin();
         i != deferred_release_rewrite_drivers_.end(); ++i) {
      ReleaseRewriteDriverImpl(*i);
    }
    deferred_release_rewrite_drivers_.clear();
  }

  // We scan for "leaked_rewrite_drivers" in apache/install/tests.mk.
  DCHECK(active_rewrite_drivers_.empty()) << "leaked_rewrite_drivers";
  STLDeleteElements(&active_rewrite_drivers_);
  STLDeleteElements(&available_rewrite_drivers_);
  decoding_driver_.reset(NULL);
}

void ResourceManager::InitWorkersAndDecodingDriver() {
  html_workers_ = factory_->WorkerPool(
      RewriteDriverFactory::kHtmlWorkers);
  rewrite_workers_ = factory_->WorkerPool(
      RewriteDriverFactory::kRewriteWorkers);
  low_priority_rewrite_workers_ = factory_->WorkerPool(
      RewriteDriverFactory::kLowPriorityRewriteWorkers);
  decoding_driver_.reset(NewUnmanagedRewriteDriver());
}

// TODO(jmarantz): consider moving this method to ResponseHeaders
void ResourceManager::SetDefaultLongCacheHeaders(
    const ContentType* content_type, ResponseHeaders* header) const {
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);

  header->RemoveAll(HttpAttributes::kContentType);
  if (content_type != NULL) {
    header->Add(HttpAttributes::kContentType, content_type->mime_type());
  }

  int64 now_ms = http_cache_->timer()->NowMs();
  header->SetDateAndCaching(now_ms, kGeneratedMaxAgeMs);

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

  // TODO(jmarantz): Replace last-modified headers by default?
  ConstStringStarVector v;
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

void ResourceManager::MergeNonCachingResponseHeaders(
    const ResponseHeaders& input_headers,
    ResponseHeaders* output_headers) {
  for (int i = 0, n = input_headers.NumAttributes(); i < n; ++i) {
    const GoogleString& name = input_headers.Name(i);
    if (!IsExcludedAttribute(name.c_str())) {
      output_headers->Add(name, input_headers.Value(i));
    }
  }
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

bool ResourceManager::Write(HttpStatus::Code status_code,
                            const StringPiece& contents,
                            OutputResource* output,
                            int64 origin_expire_time_ms,
                            MessageHandler* handler) {
  ResponseHeaders* meta_data = output->response_headers();
  SetDefaultLongCacheHeaders(output->type(), meta_data);
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

    if (output->kind() != kOnTheFlyResource) {
      http_cache_->Put(output->url(), &output->value_, handler);
    }

    // If our URL is derived from some pre-existing URL (and not invented by
    // us due to something like outlining), cache the mapping from original URL
    // to the constructed one.
    if (output->kind() != kOutlinedResource) {
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
//
// TODO(jmarantz): It would be nicer for all the cache-related
// twiddling for the new methodology (including both
// set_optimizable(true) and set_optimizable(false)) was in
// RewriteContext, perhaps right next to the Put; and if
// CacheComputedResourceMapping was not called if
// written_using_rewrite_context_flow at all.
void ResourceManager::CacheComputedResourceMapping(OutputResource* output,
    int64 origin_expire_time_ms, MessageHandler* handler) {
  GoogleString name_key = StrCat(kCacheKeyResourceNamePrefix,
                                 output->name_key());
  CachedResult* cached = output->EnsureCachedResultCreated();
  if (cached->optimizable()) {
    cached->set_url(output->url());
  }
  cached->set_origin_expiration_time_ms(origin_expire_time_ms);
  if (!output->written_using_rewrite_context_flow()) {
    output->SaveCachedResult(name_key, handler);
  }
}

bool ResourceManager::IsPagespeedResource(const GoogleUrl& url) {
  // Various things URL decoding produces which we ignore here.
  ResourceNamer namer;
  OutputResourceKind kind;
  RewriteFilter* filter;
  return decoding_driver_->DecodeOutputResourceName(url, &namer, &kind,
                                                    &filter);
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
    const ResponseHeaders* headers = resource->response_headers();
    int64 start_date_ms = headers->fetch_time_ms();
    int64 expire_ms = headers->CacheExpirationTimeMs();
    if (IsImminentlyExpiring(start_date_ms, expire_ms)) {
      resource->Freshen(handler);
    }
  }
}

ResourceManagerHttpCallback::ResourceManagerHttpCallback(
    Resource::AsyncCallback* resource_callback,
    ResourceManager* resource_manager)
    : OptionsAwareHTTPCacheCallback(
          resource_callback->resource()->rewrite_options()),
      resource_callback_(resource_callback),
      resource_manager_(resource_manager) {
  }


ResourceManagerHttpCallback::~ResourceManagerHttpCallback() {
}

void ResourceManagerHttpCallback::Done(HTTPCache::FindResult find_result) {
  ResourcePtr resource(resource_callback_->resource());
  MessageHandler* handler = resource_manager_->message_handler();
  switch (find_result) {
    case HTTPCache::kFound:
      resource->Link(http_value(), handler);
      resource->response_headers()->CopyFrom(*response_headers());
      resource->DetermineContentType();
      resource_manager_->RefreshIfImminentlyExpiring(resource.get(), handler);
      resource_callback_->Done(true);
      break;
    case HTTPCache::kRecentFetchFailedOrNotCacheable:
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
      resource_callback_->Done(false);
      break;
    case HTTPCache::kNotFound:
      // If not, load it asynchronously.
      resource->LoadAndCallback(resource_callback_, handler);
      break;
  }
  delete this;
}

// TODO(sligocki): Move into Resource? This would allow us to treat
// file- and URL-based resources differently as far as cacheability, etc.
// Specifically, we are now making a cache request for file-based resources
// which will always fail, for FileInputResources, we should just Load them.
// TODO(morlovich): Should this load non-cacheable + non-loaded resources?
void ResourceManager::ReadAsync(Resource::AsyncCallback* callback) {
  // If the resource is not already loaded, and this type of resource (e.g.
  // URL vs File vs Data) is cacheable, then try to load it.
  ResourcePtr resource = callback->resource();
  if (resource->loaded()) {
    RefreshIfImminentlyExpiring(resource.get(), message_handler_);
    callback->Done(true);
  } else if (resource->IsCacheable()) {
    ResourceManagerHttpCallback* resource_manager_callback =
        new ResourceManagerHttpCallback(callback, this);
    http_cache_->Find(resource->url(), message_handler_,
                      resource_manager_callback);
  }
}

NamedLock* ResourceManager::MakeCreationLock(const GoogleString& name) {
  const char kLockSuffix[] = ".outputlock";

  GoogleString lock_name = StrCat(lock_hasher_.Hash(name), kLockSuffix);
  return lock_manager_->CreateNamedLock(lock_name);
}

namespace {
// Constants governing resource lock timeouts.
// TODO(jmaessen): Set more appropriately?
const int64 kBreakLockMs = 30 * Timer::kSecondMs;
const int64 kBlockLockMs = 5 * Timer::kSecondMs;
}  // namespace

bool ResourceManager::TryLockForCreation(NamedLock* creation_lock) {
  return creation_lock->TryLockStealOld(kBreakLockMs);
}

void ResourceManager::LockForCreation(NamedLock* creation_lock,
                                      QueuedWorkerPool::Sequence* worker,
                                      Function* callback) {
  // TODO(jmaessen): It occurs to me that we probably ought to be
  // doing something like this if we *really* care about lock aging:
  // if (!creation_lock->LockTimedWaitStealOld(kBlockLockMs,
  //                                           kBreakLockMs)) {
  //   creation_lock->TryLockStealOld(0);  // Force lock steal
  // }
  // This updates the lock hold time so that another thread is less likely
  // to steal the lock while we're doing the blocking rewrite.
  creation_lock->LockTimedWaitStealOld(
      kBlockLockMs, kBreakLockMs,
      new QueuedWorkerPool::Sequence::AddFunction(worker, callback));
}

bool ResourceManager::HandleBeacon(const StringPiece& unparsed_url) {
  GoogleString url = unparsed_url.as_string();
  // TODO(abliss): proper query parsing
  size_t index = url.find(AddInstrumentationFilter::kLoadTag);
  if (index == GoogleString::npos) {
    return false;
  }
  url = url.substr(index + strlen(AddInstrumentationFilter::kLoadTag));
  int value = 0;
  if (!StringToInt(url, &value)) {
    return false;
  }
  rewrite_stats_->total_page_load_ms()->Add(value);
  rewrite_stats_->page_load_count()->Add(1);
  return true;
}

// TODO(jmaessen): Note that we *could* re-structure the
// rewrite_driver freelist code as follows: Keep a
// std::vector<RewriteDriver*> of all rewrite drivers.  Have each
// driver hold its index in the vector (as a number or iterator).
// Keep index of first in use.  To free, swap with first in use,
// adjusting indexes, and increment first in use.  To allocate,
// decrement first in use and return that driver.  If first in use was
// 0, allocate a fresh driver and push it.
//
// The benefit of Jan's idea is that we could avoid the overhead
// of keeping the RewriteDrivers in a std::set, which has log n
// insert/remove behavior, and instead get constant time and less
// memory overhead.

RewriteDriver* ResourceManager::NewCustomRewriteDriver(
    RewriteOptions* options) {
  RewriteDriver* rewrite_driver = NewUnmanagedRewriteDriver();
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    active_rewrite_drivers_.insert(rewrite_driver);
  }
  rewrite_driver->set_custom_options(options);
  rewrite_driver->AddFilters();
  return rewrite_driver;
}

RewriteDriver* ResourceManager::NewUnmanagedRewriteDriver() {
  RewriteDriver* rewrite_driver = new RewriteDriver(
      message_handler_, file_system_, url_async_fetcher_);
  rewrite_driver->SetAsynchronousRewrites(async_rewrites_);
  rewrite_driver->SetResourceManager(this);
  if (factory_ != NULL) {
    factory_->AddPlatformSpecificRewritePasses(rewrite_driver);
  }
  return rewrite_driver;
}

RewriteDriver* ResourceManager::NewRewriteDriver() {
  ScopedMutex lock(rewrite_drivers_mutex_.get());
  RewriteDriver* rewrite_driver = NULL;
  if (!available_rewrite_drivers_.empty()) {
    rewrite_driver = available_rewrite_drivers_.back();
    available_rewrite_drivers_.pop_back();
    rewrite_driver->SetAsynchronousRewrites(async_rewrites_);
  } else {
    rewrite_driver = NewUnmanagedRewriteDriver();
    rewrite_driver->AddFilters();
  }
  active_rewrite_drivers_.insert(rewrite_driver);
  return rewrite_driver;
}

void ResourceManager::ReleaseRewriteDriver(RewriteDriver* rewrite_driver) {
  ScopedMutex lock(rewrite_drivers_mutex_.get());
  ReleaseRewriteDriverImpl(rewrite_driver);
}

void ResourceManager::ReleaseRewriteDriverImpl(RewriteDriver* rewrite_driver) {
  if (trying_to_cleanup_rewrite_drivers_) {
    deferred_release_rewrite_drivers_.insert(rewrite_driver);
    return;
  }

  int count = active_rewrite_drivers_.erase(rewrite_driver);
  if (count != 1) {
    LOG(ERROR) << "ReleaseRewriteDriver called with driver not in active set.";
    DLOG(FATAL);
  } else {
    if (rewrite_driver->has_custom_options()) {
      delete rewrite_driver;
    } else {
      available_rewrite_drivers_.push_back(rewrite_driver);
      rewrite_driver->Clear();
    }
  }
}

void ResourceManager::ShutDownDrivers() {
  // Try to get any outstanding rewrites to complete, one-by-one.
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    // Prevent any rewrite completions from directly deleting drivers or
    // affecting active_rewrite_drivers_. We can now release the lock so
    // that the rewrites can call ReleaseRewriteDriver. Note that this is
    // making an assumption that we're not allocating new rewrite drivers
    // during the shutdown.
    trying_to_cleanup_rewrite_drivers_ = true;
  }

  if (!active_rewrite_drivers_.empty()) {
    message_handler_->Message(kInfo, "%d rewrite(s) still ongoing at exit",
                              static_cast<int>(active_rewrite_drivers_.size()));
  }

  for (RewriteDriverSet::iterator i = active_rewrite_drivers_.begin();
       i != active_rewrite_drivers_.end(); ++i) {
    // Warning: the driver may already have been mostly cleaned up except for
    // not getting into ReleaseRewriteDriver before our lock acquisition at
    // the start of this function; this code is relying on redundant
    // BoundedWaitForCompletion and Cleanup being safe when
    // trying_to_cleanup_rewrite_drivers_ is true.
    // ResourceManagerTest.ShutDownAssumptions() exists to cover this scenario.
    RewriteDriver* active = *i;
    active->BoundedWaitForCompletion(Timer::kSecondMs);
    active->Cleanup();  // Note: only cleans up if the rewrites are complete.
    // TODO(jmarantz): rename RewriteDriver::Cleanup to CleanupIfDone.
  }
}

size_t ResourceManager::num_active_rewrite_drivers() {
  ScopedMutex lock(rewrite_drivers_mutex_.get());
  return active_rewrite_drivers_.size();
}

RewriteOptions* ResourceManager::global_options() {
  if (base_class_options_.get() == NULL) {
    base_class_options_.reset(factory_->NewRewriteOptions());
    base_class_options_->CopyFrom(*factory_->default_options());
  }
  return base_class_options_.get();
}

RewriteOptions* ResourceManager::NewOptions() {
  return factory_->NewRewriteOptions();
}

void ResourceManager::ComputeSignature(RewriteOptions* rewrite_options) {
  rewrite_options->ComputeSignature(lock_hasher());
}

}  // namespace net_instaweb
