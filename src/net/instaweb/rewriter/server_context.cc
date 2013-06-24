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

#include "net/instaweb/rewriter/public/server_context.h"

#include <algorithm>                   // for std::binary_search
#include <cstddef>                     // for size_t
#include <set>

#include "base/logging.h"               // for operator<<, etc
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"
#include "net/instaweb/rewriter/public/cache_html_info_finder.h"
#include "net/instaweb/rewriter/public/critical_css_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/flush_early_info_finder.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_driver_pool.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"        // for int64
#include "net/instaweb/util/public/cache_interface.h"
#include "net/instaweb/util/public/dynamic_annotations.h"  // RunningOnValgrind
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/md5_hasher.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/named_lock_manager.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"          // for STLDeleteElements
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_synchronizer.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/url_to_filename_encoder.h"

namespace net_instaweb {

class RewriteFilter;

namespace {

// Define the various query parameter keys sent by instrumentation beacons.
const char kBeaconUrlQueryParam[] = "url";
const char kBeaconEtsQueryParam[] = "ets";
const char kBeaconOptionsHashQueryParam[] = "oh";
const char kBeaconCriticalImagesQueryParam[] = "ci";
const char kBeaconCriticalCssQueryParam[] = "cs";
const char kBeaconNonceQueryParam[] = "n";

const char kFallbackPageCacheKeyQuerySuffix[] = "@";
const char kFallbackPageCacheKeyBasePathSuffix[] = "#";

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

StringSet* CommaSeparatedStringToSet(StringPiece str) {
  StringPieceVector str_values;
  SplitStringPieceToVector(str, ",", &str_values, true);
  StringSet* set = new StringSet();
  for (StringPieceVector::const_iterator it = str_values.begin();
       it != str_values.end(); ++it) {
    set->insert(UrlToFilenameEncoder::Unescape(it->as_string()));
  }
  return set;
}

// Track a property cache lookup triggered from a beacon response. When
// complete, Done will update and and writeback the beacon cohort with the
// critical image set.
class BeaconPropertyCallback : public PropertyPage {
 public:
  BeaconPropertyCallback(
      ServerContext* server_context,
      const StringPiece& key,
      const RequestContextPtr& request_context,
      StringSet* html_critical_images_set,
      StringSet* css_critical_images_set,
      StringSet* critical_css_selector_set,
      StringPiece nonce)
      : PropertyPage(kPropertyCachePage, key, request_context,
                     server_context->thread_system()->NewMutex(),
                     server_context->page_property_cache()),
        server_context_(server_context),
        html_critical_images_set_(html_critical_images_set),
        css_critical_images_set_(css_critical_images_set),
        critical_css_selector_set_(critical_css_selector_set) {
    nonce.CopyToString(&nonce_);
  }

  const PropertyCache::CohortVector CohortList() {
    PropertyCache::CohortVector cohort_list;
    cohort_list.push_back(
         server_context_->page_property_cache()->GetCohort(
             RewriteDriver::kBeaconCohort));
    return cohort_list;
  }

  virtual ~BeaconPropertyCallback() {}

  virtual void Done(bool success) {
    BeaconCriticalImagesFinder::UpdateCriticalImagesCacheEntry(
        html_critical_images_set_.get(), css_critical_images_set_.get(),
        server_context_->beacon_cohort(), this);
    if (critical_css_selector_set_ != NULL) {
      server_context_->critical_selector_finder()->
          WriteCriticalSelectorsToPropertyCache(
              *critical_css_selector_set_, nonce_,
              server_context_->page_property_cache(), this,
              server_context_->message_handler());
    }

    WriteCohort(server_context_->beacon_cohort());
    delete this;
  }

 private:
  ServerContext* server_context_;
  scoped_ptr<StringSet> html_critical_images_set_;
  scoped_ptr<StringSet> css_critical_images_set_;
  scoped_ptr<StringSet> critical_css_selector_set_;
  GoogleString nonce_;
  DISALLOW_COPY_AND_ASSIGN(BeaconPropertyCallback);
};

}  // namespace

const int64 ServerContext::kGeneratedMaxAgeMs = Timer::kYearMs;

// Statistics group names.
const char ServerContext::kStatisticsGroup[] = "Statistics";

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
const char ServerContext::kCacheKeyResourceNamePrefix[] = "rname/";

// We set etags for our output resources to "W/0".  The "W" means
// that this etag indicates a functional consistency, but is not
// guaranteeing byte-consistency.  This distinction is important because
// we serve different bytes for clients that do not accept gzip.
//
// This value is a shared constant so that it can also be used in
// the Apache-specific code that repairs headers after mod_headers
// alters them.
const char ServerContext::kResourceEtagValue[] = "W/\"0\"";

class GlobalOptionsRewriteDriverPool : public RewriteDriverPool {
 public:
  explicit GlobalOptionsRewriteDriverPool(ServerContext* context)
      : server_context_(context) {
  }

  virtual RewriteOptions* TargetOptions() const {
    return server_context_->global_options();
  }

 private:
  ServerContext* server_context_;
};

ServerContext::ServerContext(RewriteDriverFactory* factory)
    : thread_system_(factory->thread_system()),
      rewrite_stats_(NULL),
      file_system_(factory->file_system()),
      filename_encoder_(NULL),
      url_namer_(NULL),
      user_agent_matcher_(NULL),
      scheduler_(factory->scheduler()),
      default_system_fetcher_(NULL),
      default_distributed_fetcher_(NULL),
      hasher_(NULL),
      lock_hasher_(RewriteOptions::kHashBytes),
      contents_hasher_(21),
      statistics_(NULL),
      filesystem_metadata_cache_(NULL),
      metadata_cache_(NULL),
      store_outputs_in_file_system_(false),
      response_headers_finalized_(true),
      enable_property_cache_(true),
      lock_manager_(NULL),
      message_handler_(NULL),
      dom_cohort_(NULL),
      blink_cohort_(NULL),
      beacon_cohort_(NULL),
      available_rewrite_drivers_(new GlobalOptionsRewriteDriverPool(this)),
      trying_to_cleanup_rewrite_drivers_(false),
      factory_(factory),
      rewrite_drivers_mutex_(thread_system_->NewMutex()),
      html_workers_(NULL),
      rewrite_workers_(NULL),
      low_priority_rewrite_workers_(NULL),
      static_asset_manager_(NULL),
      thread_synchronizer_(new ThreadSynchronizer(thread_system_)),
      experiment_matcher_(factory_->NewExperimentMatcher()),
      usage_data_reporter_(factory_->usage_data_reporter()),
      simple_random_(thread_system_->NewMutex()) {
  // Make sure the excluded-attributes are in abc order so binary_search works.
  // Make sure to use the same comparator that we pass to the binary_search.
#ifndef NDEBUG
  for (int i = 1, n = arraysize(kExcludedAttributes); i < n; ++i) {
    DCHECK(CharStarCompareInsensitive()(kExcludedAttributes[i - 1],
                                        kExcludedAttributes[i]));
  }
#endif
}

ServerContext::~ServerContext() {
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

  // We scan for "leaked_rewrite_drivers" in apache/install/Makefile.tests
  if (!active_rewrite_drivers_.empty()) {
    message_handler_->Message(
        kError, "ServerContext: %d leaked_rewrite_drivers on destruction",
        static_cast<int>(active_rewrite_drivers_.size()));
#ifndef NDEBUG
    for (RewriteDriverSet::iterator p = active_rewrite_drivers_.begin(),
             e = active_rewrite_drivers_.end(); p != e; ++p) {
      RewriteDriver* driver = *p;
      // During load-test, print some detail about leaked drivers.  It
      // appears that looking deep into the leaked driver's detached
      // contexts crashes during shutdown, however, so disable that.
      //
      // TODO(jmarantz): investigate why that is so we can show the detail.
      driver->PrintStateToErrorLog(false /* show_detached_contexts */);
    }
#endif
  }
  STLDeleteElements(&active_rewrite_drivers_);
  available_rewrite_drivers_.reset();
  STLDeleteElements(&additional_driver_pools_);
  decoding_driver_.reset(NULL);
}

void ServerContext::InitWorkersAndDecodingDriver() {
  html_workers_ = factory_->WorkerPool(RewriteDriverFactory::kHtmlWorkers);
  rewrite_workers_ = factory_->WorkerPool(
      RewriteDriverFactory::kRewriteWorkers);
  low_priority_rewrite_workers_ = factory_->WorkerPool(
      RewriteDriverFactory::kLowPriorityRewriteWorkers);
  decoding_driver_.reset(NewUnmanagedRewriteDriver(
      NULL, global_options()->Clone(), RequestContextPtr(NULL)));
  // Apply platform configuration mutation for consistency's sake.
  factory_->ApplyPlatformSpecificConfiguration(decoding_driver_.get());
  // Inserts platform-specific rewriters into the resource_filter_map_, so that
  // the decoding process can recognize those rewriter ids.
  factory_->AddPlatformSpecificDecodingPasses(decoding_driver_.get());
  // This call is for backwards compatibility.  When adding new platform
  // specific rewriters to implementations of RewriteDriverFactory, please
  // do not rely on this call to include them in the decoding process.  Instead,
  // add them to your implementation of AddPlatformSpecificDecodingPasses.
  factory_->AddPlatformSpecificRewritePasses(decoding_driver_.get());
}

// TODO(jmarantz): consider moving this method to ResponseHeaders
void ServerContext::SetDefaultLongCacheHeadersWithCharset(
    const ContentType* content_type, StringPiece charset,
    ResponseHeaders* header) const {
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);

  header->RemoveAll(HttpAttributes::kContentType);
  if (content_type != NULL) {
    GoogleString header_val = content_type->mime_type();
    if (!charset.empty()) {
      // Note: if charset was quoted, content_type's parsing would not unquote
      // it, so here we just append it back in instead of quoting it again.
      StrAppend(&header_val, "; charset=", charset);
    }
    header->Add(HttpAttributes::kContentType, header_val);
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

void ServerContext::MergeNonCachingResponseHeaders(
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
void ServerContext::SetContentType(const ContentType* content_type,
                                     ResponseHeaders* header) {
  CHECK(content_type != NULL);
  header->Replace(HttpAttributes::kContentType, content_type->mime_type());
  header->ComputeCaching();
}

void ServerContext::set_filename_prefix(const StringPiece& file_prefix) {
  file_prefix.CopyToString(&file_prefix_);
}

void ServerContext::ApplyInputCacheControl(const ResourceVector& inputs,
                                           ResponseHeaders* headers) {
  headers->ComputeCaching();
  bool proxy_cacheable = headers->IsProxyCacheable();
  bool browser_cacheable = headers->IsBrowserCacheable();
  bool no_store = headers->HasValue(HttpAttributes::kCacheControl,
                                    "no-store");
  int64 max_age = headers->cache_ttl_ms();
  for (int i = 0, n = inputs.size(); i < n; ++i) {
    const ResourcePtr& input_resource(inputs[i]);
    if (input_resource.get() != NULL && input_resource->HttpStatusOk()) {
      ResponseHeaders* input_headers = input_resource->response_headers();
      input_headers->ComputeCaching();
      if (input_headers->cache_ttl_ms() < max_age) {
        max_age = input_headers->cache_ttl_ms();
      }
      proxy_cacheable &= input_headers->IsProxyCacheable();
      browser_cacheable &= input_headers->IsBrowserCacheable();
      no_store |= input_headers->HasValue(HttpAttributes::kCacheControl,
                                          "no-store");
    }
  }
  if (browser_cacheable) {
    if (proxy_cacheable) {
      return;
    } else {
      headers->SetDateAndCaching(headers->date_ms(), max_age,
                                 ",private" /*cache_control_suffix*/);
    }
  } else {
    GoogleString directives = ",no-cache";
    if (no_store) {
      directives += ",no-store";
    }
    headers->SetDateAndCaching(headers->date_ms(), 0 /*ttl*/, directives);
  }
  headers->ComputeCaching();
}

void ServerContext::AddOriginalContentLengthHeader(
    const ResourceVector& inputs, ResponseHeaders* headers) {
  // Determine the total original content length for input resource, and
  // use this to set the X-Original-Content-Length header in the output.
  int64 input_size = 0;
  for (int i = 0, n = inputs.size(); i < n; ++i) {
    const ResourcePtr& input_resource(inputs[i]);
    ResponseHeaders* input_headers = input_resource->response_headers();
    const char* original_content_length_header = input_headers->Lookup1(
        HttpAttributes::kXOriginalContentLength);
    int64 original_content_length;
    if (original_content_length_header != NULL &&
        StringToInt64(original_content_length_header,
                      &original_content_length)) {
      input_size += original_content_length;
    }
  }
  // Only add the header if there were actual input resources with
  // known sizes involved (which is not always the case, e.g., in tests where
  // synthetic input resources are used).
  if (input_size > 0) {
    headers->SetOriginalContentLength(input_size);
  }
}

bool ServerContext::IsPagespeedResource(const GoogleUrl& url) {
  ResourceNamer namer;
  OutputResourceKind kind;
  RewriteFilter* filter;
  return decoding_driver_->DecodeOutputResourceName(
      url, &namer, &kind, &filter);
}

NamedLock* ServerContext::MakeCreationLock(const GoogleString& name) {
  const char kLockSuffix[] = ".outputlock";

  GoogleString lock_name = StrCat(lock_hasher_.Hash(name), kLockSuffix);
  return lock_manager_->CreateNamedLock(lock_name);
}

NamedLock* ServerContext::MakeInputLock(const GoogleString& name) {
  const char kLockSuffix[] = ".lock";

  GoogleString lock_name = StrCat(lock_hasher_.Hash(name), kLockSuffix);
  return lock_manager_->CreateNamedLock(lock_name);
}

namespace {
// Constants governing resource lock timeouts.
// TODO(jmaessen): Set more appropriately?
const int64 kBreakLockMs = 30 * Timer::kSecondMs;
const int64 kBlockLockMs = 5 * Timer::kSecondMs;
}  // namespace

bool ServerContext::TryLockForCreation(NamedLock* creation_lock) {
  return creation_lock->TryLockStealOld(kBreakLockMs);
}

void ServerContext::LockForCreation(NamedLock* creation_lock,
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

bool ServerContext::HandleBeacon(StringPiece params,
                                 StringPiece user_agent,
                                 const RequestContextPtr& request_context) {
  // Beacons are of the form ets=load:xxx&url=.... and can be sent in either the
  // query params of a GET or the body of a POST.
  // Extract the URL. A valid URL parameter is required to attempt parsing of
  // the ets and critimg params. However, an invalid ets or critimg param will
  // not prevent attempting parsing of the other. This is because these values
  // are generated by separate client-side JS and that failure of one should not
  // prevent attempting to parse the other.
  QueryParams query_params;
  query_params.Parse(params);
  const GoogleString* query_param_str;
  GoogleUrl url_query_param;

  query_param_str = query_params.Lookup1(kBeaconUrlQueryParam);
  if (query_param_str != NULL) {
    // The url parameter sent by the beacon is encoded with EncodeURIComponent,
    // so decode it.
    url_query_param.Reset(UrlToFilenameEncoder::Unescape(*query_param_str));

    if (!url_query_param.is_valid()) {
      message_handler_->Message(kWarning,
                                "Invalid URL parameter in beacon: %s",
                                query_param_str->c_str());
      return false;
    }
  } else {
    message_handler_->Message(kWarning, "Missing URL parameter in beacon: %s",
                              params.as_string().c_str());
    return false;
  }

  bool status = true;

  // Extract the onload time from the ets query param.
  query_param_str = query_params.Lookup1(kBeaconEtsQueryParam);
  if (query_param_str != NULL) {
    int value = -1;

    size_t index = query_param_str->find(":");
    if (index != GoogleString::npos && index < query_param_str->size()) {
      GoogleString load_time_str = query_param_str->substr(index + 1);
      if (!(StringToInt(load_time_str, &value) && value >= 0)) {
        status = false;
      } else {
        rewrite_stats_->total_page_load_ms()->Add(value);
        rewrite_stats_->page_load_count()->Add(1);
        rewrite_stats_->beacon_timings_ms_histogram()->Add(value);
      }
    }
  }

  // Process data from critical image and CSS beacons.
  // Beacon contents are stored in the property cache, so bail out if it isn't
  // enabled.
  if (page_property_cache() == NULL || !page_property_cache()->enabled()) {
    return status;
  }
  // Make sure the beacon has the options hash, which is included in the
  // property cache key.
  const GoogleString* options_hash_param =
      query_params.Lookup1(kBeaconOptionsHashQueryParam);
  if (options_hash_param == NULL) {
    return status;
  }

  // Extract critical image URLs
  // TODO(jud): Add css critical image detection to the beacon.
  // Beacon property callback takes ownership of both critical images sets.
  scoped_ptr<StringSet> html_critical_images_set;
  scoped_ptr<StringSet> css_critical_images_set;
  query_param_str = query_params.Lookup1(kBeaconCriticalImagesQueryParam);
  if (query_param_str != NULL) {
    html_critical_images_set.reset(
        CommaSeparatedStringToSet(*query_param_str));
  }

  scoped_ptr<StringSet> critical_css_selector_set;
  query_param_str = query_params.Lookup1(kBeaconCriticalCssQueryParam);
  if (query_param_str != NULL) {
    critical_css_selector_set.reset(
        CommaSeparatedStringToSet(*query_param_str));
  }

  StringPiece nonce;
  query_param_str = query_params.Lookup1(kBeaconNonceQueryParam);
  if (query_param_str != NULL) {
    nonce.set(query_param_str->data(), query_param_str->size());
  }

  // Store the critical information in the property cache. This is done by
  // looking up the property page for the URL specified in the beacon, and
  // performing the page update and cohort write in
  // BeaconPropertyCallback::Done(). Done() is called when the read completes.
  if (html_critical_images_set != NULL ||
      css_critical_images_set != NULL ||
      critical_css_selector_set != NULL) {
    UserAgentMatcher::DeviceType device_type =
        user_agent_matcher()->GetDeviceTypeForUA(user_agent);
    StringPiece device_type_suffix =
        UserAgentMatcher::DeviceTypeSuffix(device_type);

    GoogleString key = GetPagePropertyCacheKey(
        url_query_param.Spec(),
        *options_hash_param,
        device_type_suffix);

    BeaconPropertyCallback* beacon_property_cb = new BeaconPropertyCallback(
        this, key, request_context,
        html_critical_images_set.release(),
        css_critical_images_set.release(),
        critical_css_selector_set.release(),
        nonce);
    page_property_cache()->ReadWithCohorts(beacon_property_cb->CohortList(),
                                           beacon_property_cb);
  }

  return status;
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

RewriteDriver* ServerContext::NewCustomRewriteDriver(
    RewriteOptions* options, const RequestContextPtr& request_ctx) {
  RewriteDriver* rewrite_driver = NewUnmanagedRewriteDriver(
      NULL /* no pool as custom*/,
      options,
      request_ctx);
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    active_rewrite_drivers_.insert(rewrite_driver);
  }
  if (factory_ != NULL) {
    factory_->ApplyPlatformSpecificConfiguration(rewrite_driver);
  }
  rewrite_driver->AddFilters();
  if (factory_ != NULL) {
    factory_->AddPlatformSpecificRewritePasses(rewrite_driver);
  }
  return rewrite_driver;
}

RewriteDriver* ServerContext::NewUnmanagedRewriteDriver(
    RewriteDriverPool* pool, RewriteOptions* options,
    const RequestContextPtr& request_ctx) {
  RewriteDriver* rewrite_driver = new RewriteDriver(
      message_handler_, file_system_, default_system_fetcher_);
  rewrite_driver->set_options_for_pool(pool, options);
  rewrite_driver->SetServerContext(this);
  rewrite_driver->ClearRequestProperties();
  rewrite_driver->set_request_context(request_ctx);
  if (has_default_distributed_fetcher()) {
    rewrite_driver->set_distributed_fetcher(default_distributed_fetcher_);
  }
  ApplySessionFetchers(request_ctx, rewrite_driver);
  return rewrite_driver;
}

RewriteDriver* ServerContext::NewRewriteDriver(
    const RequestContextPtr& request_ctx) {
  RewriteDriverPool* pool = SelectDriverPool(request_ctx->using_spdy());
  return NewRewriteDriverFromPool(pool, request_ctx);
}

RewriteDriver* ServerContext::NewRewriteDriverFromPool(
    RewriteDriverPool* pool, const RequestContextPtr& request_ctx) {
  RewriteDriver* rewrite_driver = NULL;

  RewriteOptions* options = pool->TargetOptions();
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    while ((rewrite_driver = pool->PopDriver()) != NULL) {
      // Note: there is currently some activity to make the RewriteOptions
      // signature insensitive to changes that need not affect the metadata
      // cache key.  As we are dependent on a comprehensive signature in
      // order to correctly determine whether we can recycle a RewriteDriver,
      // we would have to use a separate signature for metadata_cache_key
      // vs this purpose.
      //
      // So for now, let us keep all the options incorporated into the
      // signature, and revisit the issue of pulling options out if we
      // find we are having poor hit-rate in the metadata cache during
      // operations.
      if (rewrite_driver->options()->IsEqual(*options)) {
        break;
      } else {
        delete rewrite_driver;
        rewrite_driver = NULL;
      }
    }
  }

  if (rewrite_driver == NULL) {
    rewrite_driver = NewUnmanagedRewriteDriver(
        pool, options->Clone(), request_ctx);
    if (factory_ != NULL) {
      factory_->ApplyPlatformSpecificConfiguration(rewrite_driver);
    }
    rewrite_driver->AddFilters();
    if (factory_ != NULL) {
      factory_->AddPlatformSpecificRewritePasses(rewrite_driver);
    }
  } else {
    rewrite_driver->set_request_context(request_ctx);
    ApplySessionFetchers(request_ctx, rewrite_driver);
  }

  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    active_rewrite_drivers_.insert(rewrite_driver);
  }
  return rewrite_driver;
}

void ServerContext::ReleaseRewriteDriver(RewriteDriver* rewrite_driver) {
  ScopedMutex lock(rewrite_drivers_mutex_.get());
  ReleaseRewriteDriverImpl(rewrite_driver);
}

void ServerContext::ReleaseRewriteDriverImpl(RewriteDriver* rewrite_driver) {
  if (trying_to_cleanup_rewrite_drivers_) {
    deferred_release_rewrite_drivers_.insert(rewrite_driver);
    return;
  }

  int count = active_rewrite_drivers_.erase(rewrite_driver);
  if (count != 1) {
    LOG(ERROR) << "ReleaseRewriteDriver called with driver not in active set.";
    DLOG(FATAL);
  } else {
    RewriteDriverPool* pool = rewrite_driver->controlling_pool();
    if (pool == NULL) {
      delete rewrite_driver;
    } else {
      pool->RecycleDriver(rewrite_driver);
    }
  }
}

void ServerContext::ShutDownDrivers() {
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
    // ServerContextTest.ShutDownAssumptions() exists to cover this scenario.
    RewriteDriver* active = *i;
    int64 timeout_ms = Timer::kSecondMs;
    if (RunningOnValgrind()) {
      timeout_ms *= 20;
    }
    active->BoundedWaitFor(RewriteDriver::kWaitForShutDown, timeout_ms);
    active->Cleanup();  // Note: only cleans up if the rewrites are complete.
    // TODO(jmarantz): rename RewriteDriver::Cleanup to CleanupIfDone.
  }
}

size_t ServerContext::num_active_rewrite_drivers() {
  ScopedMutex lock(rewrite_drivers_mutex_.get());
  return active_rewrite_drivers_.size();
}

RewriteOptions* ServerContext::global_options() {
  if (base_class_options_.get() == NULL) {
    base_class_options_.reset(factory_->default_options()->Clone());
  }
  return base_class_options_.get();
}

const RewriteOptions* ServerContext::global_options() const {
  if (base_class_options_.get() == NULL) {
    return factory_->default_options();
  }
  return base_class_options_.get();
}

void ServerContext::reset_global_options(RewriteOptions* options) {
  base_class_options_.reset(options);
}

RewriteOptions* ServerContext::NewOptions() {
  return factory_->NewRewriteOptions();
}

ServerContext::OptionsBoolPair ServerContext::GetQueryOptions(
    GoogleUrl* request_url, RequestHeaders* request_headers,
    ResponseHeaders* response_headers) {
  scoped_ptr<RewriteOptions> query_options;
  bool success = false;
  switch (RewriteQuery::Scan(global_options()->add_options_to_urls(),
                             factory(), this, request_url, request_headers,
                             response_headers, &query_options,
                             message_handler_)) {
    case RewriteQuery::kInvalid:
      query_options.reset(NULL);
      break;
    case RewriteQuery::kNoneFound:
      query_options.reset(NULL);
      success = true;
      break;
    case RewriteQuery::kSuccess:
      success = true;
      break;
    default:
      query_options.reset(NULL);
  }
  // Note: success==false is treated as an error (we return 405 in
  // proxy_interface.cc), while query_options==NULL merely means there are no
  // query options.
  return OptionsBoolPair(query_options.release(), success);
}

void ServerContext::ScanSplitHtmlRequest(const RequestContextPtr& ctx,
                                         const RewriteOptions* options,
                                         GoogleUrl* url) {
  if (options == NULL || !options->Enabled(RewriteOptions::kSplitHtml)) {
    return;
  }
  QueryParams query_params;
  query_params.Parse(url->Query());

  if (query_params.RemoveAll(HttpAttributes::kXPsaSplitBtf)) {
    ctx->set_is_split_btf_request(true);
    GoogleString query_string = query_params.empty() ? "" :
          StrCat("?", query_params.ToString());
    url->Reset(
        StrCat(url->AllExceptQuery(), query_string, url->AllAfterQuery()));
  }
}

RewriteOptions* ServerContext::GetCustomOptions(RequestHeaders* request_headers,
                                                RewriteOptions* domain_options,
                                                RewriteOptions* query_options) {
  RewriteOptions* options = global_options();
  scoped_ptr<RewriteOptions> custom_options;
  scoped_ptr<RewriteOptions> scoped_domain_options(domain_options);
  if (scoped_domain_options.get() != NULL) {
    custom_options.reset(NewOptions());
    custom_options->Merge(*options);
    scoped_domain_options->Freeze();
    custom_options->Merge(*scoped_domain_options);
    options = custom_options.get();
  }

  scoped_ptr<RewriteOptions> query_options_ptr(query_options);
  // Check query params & request-headers
  if (query_options_ptr.get() != NULL) {
    // Subtle memory management to handle deleting any domain_options
    // after the merge, and transferring ownership to the caller for
    // the new merged options.
    scoped_ptr<RewriteOptions> options_buffer(custom_options.release());
    custom_options.reset(NewOptions());
    custom_options->Merge(*options);
    query_options->Freeze();
    custom_options->Merge(*query_options);
    // Don't run any experiments if this is a special query-params request.
    custom_options->set_running_experiment(false);
  }

  if (request_headers->IsXmlHttpRequest()) {
    // For XmlHttpRequests, disable filters that insert js. Otherwise, there
    // will be two copies of the same scripts in the html dom -- one from main
    // html page and another from html content fetched from ajax and this
    // will corrupt global variable state.
    // Sometimes, js present in the ajax request does not get executed.
    // TODO(sriharis): Set a flag in RewriteOptions indicating that we are
    // working with Ajax and thus should not assume the base URL is correct.
    // Note that there is no guarantee that the header will be set on an ajax
    // request and so the option will not be set for all ajax requests.
    if (custom_options == NULL) {
      custom_options.reset(options->Clone());
    }
    custom_options->DisableFiltersRequiringScriptExecution();
    custom_options->DisableFilter(RewriteOptions::kPrioritizeCriticalCss);
  }

  url_namer()->ConfigureCustomOptions(*request_headers, custom_options.get());

  return custom_options.release();
}

GoogleString ServerContext::GetPagePropertyCacheKey(
    StringPiece url, const RewriteOptions* options,
    StringPiece device_type_suffix) {
  GoogleString options_signature_hash;
  if (options != NULL) {
    // Should we use lock_hasher() instead of hasher() below?
    options_signature_hash = hasher()->Hash(options->signature());
  }
  return GetPagePropertyCacheKey(
      url, options_signature_hash, device_type_suffix);
}

GoogleString ServerContext::GetPagePropertyCacheKey(
    StringPiece url, StringPiece options_signature_hash,
    StringPiece device_type_suffix) {
  GoogleString result(url.as_string());
  if (!options_signature_hash.empty()) {
    StrAppend(&result, "_", options_signature_hash);
  }
  StrAppend(&result, device_type_suffix);
  return result;
}

GoogleString ServerContext::GetFallbackPagePropertyCacheKey(
    const GoogleUrl& request_url, const RewriteOptions* options,
    StringPiece device_type_suffix) {
  GoogleString key;
  GoogleString suffix;
  if (request_url.has_query()) {
    key = request_url.AllExceptQuery().as_string();
    suffix = kFallbackPageCacheKeyQuerySuffix;
  } else {
    GoogleString url(request_url.spec_c_str());
    int size = url.size();
    if (url[size - 1] == '/') {
      // It's common for site admins to canonicalize urls by redirecting "/a/b"
      // to "/a/b/".  In order to more effectively share fallback properties, we
      // strip the trailing '/' before dropping down a level.
      url.resize(size - 1);
    }
    GoogleUrl gurl(url);
    key = gurl.AllExceptLeaf().as_string();
    suffix = kFallbackPageCacheKeyBasePathSuffix;
  }
  return StrCat(GetPagePropertyCacheKey(key, options, device_type_suffix),
                suffix);
}

void ServerContext::ComputeSignature(RewriteOptions* rewrite_options) const {
  rewrite_options->ComputeSignature();
}

bool ServerContext::IsExcludedAttribute(const char* attribute) {
  const char** end = kExcludedAttributes + arraysize(kExcludedAttributes);
  return std::binary_search(kExcludedAttributes, end, attribute,
                            CharStarCompareInsensitive());
}

void ServerContext::set_enable_property_cache(bool enabled) {
  enable_property_cache_ = enabled;
  if (page_property_cache_.get() != NULL) {
    page_property_cache_->set_enabled(enabled);
  }
}

// TODO(jmarantz): simplify the cache ownership model so that the layered
// caches don't own one another; the ServerContext owns all the caches.
void ServerContext::MakePropertyCaches(CacheInterface* backend_cache) {
  // The property caches are L2-only.  We cannot use the L1 cache because
  // this data can get stale quickly.
  page_property_cache_.reset(MakePropertyCache(
      PropertyCache::kPagePropertyCacheKeyPrefix, backend_cache));
}

PropertyCache* ServerContext::MakePropertyCache(
    const GoogleString& cache_key_prefix, CacheInterface* cache) const {
  PropertyCache* pcache = new PropertyCache(
      cache_key_prefix, cache, timer(), statistics(), thread_system_);
  pcache->set_enabled(enable_property_cache_);
  return pcache;
}


void ServerContext::set_cache_html_info_finder(CacheHtmlInfoFinder* finder) {
  cache_html_info_finder_.reset(finder);
}

void ServerContext::set_critical_images_finder(CriticalImagesFinder* finder) {
  critical_images_finder_.reset(finder);
}

void ServerContext::set_critical_css_finder(CriticalCssFinder* finder) {
  critical_css_finder_.reset(finder);
}

void ServerContext::set_critical_selector_finder(
    CriticalSelectorFinder* finder) {
  critical_selector_finder_.reset(finder);
}

void ServerContext::set_flush_early_info_finder(FlushEarlyInfoFinder* finder) {
  flush_early_info_finder_.reset(finder);
}

RewriteDriverPool* ServerContext::SelectDriverPool(bool using_spdy) {
  return standard_rewrite_driver_pool();
}

void ServerContext::ApplySessionFetchers(const RequestContextPtr& req,
                                         RewriteDriver* driver) {
}

RequestProperties* ServerContext::NewRequestProperties() {
  RequestProperties* request_properties =
      new RequestProperties(user_agent_matcher());
  request_properties->SetPreferredImageQualities(
      factory_->preferred_webp_qualities(),
      factory_->preferred_jpeg_qualities());
  return request_properties;
}

void ServerContext::DeleteCacheOnDestruction(CacheInterface* cache) {
  factory_->TakeOwnership(cache);
}

}  // namespace net_instaweb
