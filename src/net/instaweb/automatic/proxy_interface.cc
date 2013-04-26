/*
 * Copyright 2011 Google Inc.
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

#include "net/instaweb/automatic/public/proxy_interface.h"

#include "base/logging.h"
#include "net/instaweb/automatic/public/blink_flow_critical_line.h"
#include "net/instaweb/automatic/public/cache_html_flow.h"
#include "net/instaweb/automatic/public/flush_early_flow.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/rewriter/public/blink_critical_line_data_finder.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/furious_matcher.h"
#include "net/instaweb/rewriter/public/resource_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hasher.h"
#include "net/instaweb/util/public/hostname_util.h"
#include "net/instaweb/util/public/ref_counted_ptr.h"
#include "net/instaweb/util/public/request_trace.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace net_instaweb {

class MessageHandler;

const char ProxyInterface::kBlinkRequestCount[] = "blink-requests";
const char ProxyInterface::kBlinkCriticalLineRequestCount[] =
    "blink-critical-line-requests";
const char ProxyInterface::kCacheHtmlRequestCount[] =
    "cache-html-requests";
namespace {

// Names for Statistics variables.
const char kTotalRequestCount[] = "all-requests";
const char kPagespeedRequestCount[] = "pagespeed-requests";
const char kBlinkRequestCount[] = "blink-requests";
const char kRejectedRequestCount[] = "publisher-rejected-requests";
const char kRejectedRequestHtmlResponse[] = "Unable to serve "
    "content as the content is blocked by the administrator of the domain.";

bool UrlMightHavePropertyCacheEntry(const GoogleUrl& url) {
  const ContentType* type = NameExtensionToContentType(url.LeafSansQuery());
  if (type == NULL) {
    return true;  // http://www.example.com/  -- no extension; could be HTML.
  }

  // Use a complete switch-statement rather than type()->IsHtmlLike()
  // so that every time we add a new content-type we make an explicit
  // decision about whether it should induce a pcache read.
  //
  // TODO(jmarantz): currently this returns false for ".txt".  Thus we will
  // do no optimizations relying on property-cache on HTML files ending with
  // ".txt".  We should determine whether this is the right thing or not.
  switch (type->type()) {
    case ContentType::kHtml:
    case ContentType::kXhtml:
    case ContentType::kCeHtml:
      return true;
    case ContentType::kJavascript:
    case ContentType::kCss:
    case ContentType::kText:
    case ContentType::kXml:
    case ContentType::kPng:
    case ContentType::kGif:
    case ContentType::kJpeg:
    case ContentType::kSwf:
    case ContentType::kWebp:
    case ContentType::kIco:
    case ContentType::kPdf:
    case ContentType::kOther:
    case ContentType::kJson:
    case ContentType::kVideo:
    case ContentType::kOctetStream:
      return false;
  }
  LOG(DFATAL) << "URL " << url.Spec() << ": unexpected type:" << type->type()
              << "; " << type->mime_type() << "; " << type->file_extension();
  return false;
}

// Provides a callback whose Done() function is executed once we have
// rewrite options.
class ProxyInterfaceUrlNamerCallback : public UrlNamer::Callback {
 public:
  ProxyInterfaceUrlNamerCallback(
      bool is_resource_fetch,
      GoogleUrl* request_url,
      AsyncFetch* async_fetch,
      ProxyInterface* proxy_interface,
      RewriteOptions* query_options,
      MessageHandler* handler)
      : is_resource_fetch_(is_resource_fetch),
        request_url_(request_url),
        async_fetch_(async_fetch),
        property_callback_(NULL),
        handler_(handler),
        proxy_interface_(proxy_interface),
        query_options_(query_options) {
  }
  virtual ~ProxyInterfaceUrlNamerCallback() {}
  virtual void Done(RewriteOptions* rewrite_options) {
    proxy_interface_->ProxyRequestCallback(
        is_resource_fetch_, request_url_, async_fetch_, rewrite_options,
        query_options_, handler_);
    delete this;
  }

 private:
  bool is_resource_fetch_;
  GoogleUrl* request_url_;
  AsyncFetch* async_fetch_;
  ProxyFetchPropertyCallbackCollector* property_callback_;
  MessageHandler* handler_;
  ProxyInterface* proxy_interface_;
  RewriteOptions* query_options_;

  DISALLOW_COPY_AND_ASSIGN(ProxyInterfaceUrlNamerCallback);
};

}  // namespace

ProxyInterface::ProxyInterface(const StringPiece& hostname, int port,
                               ServerContext* server_context,
                               Statistics* stats)
    : server_context_(server_context),
      fetcher_(NULL),
      timer_(NULL),
      handler_(server_context->message_handler()),
      hostname_(hostname.as_string()),
      port_(port),
      all_requests_(stats->GetTimedVariable(kTotalRequestCount)),
      pagespeed_requests_(stats->GetTimedVariable(kPagespeedRequestCount)),
      blink_requests_(stats->GetTimedVariable(kBlinkRequestCount)),
      blink_critical_line_requests_(
          stats->GetTimedVariable(kBlinkCriticalLineRequestCount)),
      cache_html_flow_requests_(
          stats->GetTimedVariable(kCacheHtmlRequestCount)),
      rejected_requests_(stats->GetTimedVariable(kRejectedRequestCount)) {
  proxy_fetch_factory_.reset(new ProxyFetchFactory(server_context));
}

ProxyInterface::~ProxyInterface() {
}

void ProxyInterface::InitStats(Statistics* statistics) {
  statistics->AddTimedVariable(kTotalRequestCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kPagespeedRequestCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kBlinkRequestCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kBlinkCriticalLineRequestCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kCacheHtmlRequestCount,
                               ServerContext::kStatisticsGroup);
  statistics->AddTimedVariable(kRejectedRequestCount,
                               ServerContext::kStatisticsGroup);
  BlinkFlowCriticalLine::InitStats(statistics);
  CacheHtmlFlow::InitStats(statistics);
  FlushEarlyFlow::InitStats(statistics);
}

bool ProxyInterface::IsWellFormedUrl(const GoogleUrl& url) {
  bool ret = false;
  if (url.is_valid()) {
    if (url.has_path()) {
      StringPiece path = url.PathAndLeaf();
      GoogleString filename = url.ExtractFileName();
      int path_len = path.size() - filename.size();
      if (path_len >= 0) {
        ret = true;
      }
    } else if (!url.has_scheme()) {
      LOG(ERROR) << "URL has no scheme: " << url.Spec();
    } else {
      LOG(ERROR) << "URL has no path: " << url.Spec();
    }
  }
  return ret;
}

bool ProxyInterface::UrlAndPortMatchThisServer(const GoogleUrl& url) {
  bool ret = false;
  if (url.is_valid() && (url.EffectiveIntPort() == port_)) {
    // TODO(atulvasu): This should support matching the actual host this
    // machine can receive requests from. Ideally some flag control would
    // help. For example this server could be running multiple virtual
    // servers, and we would like to know what server we are catering to for
    // pagespeed only queries.
    //
    // Allow for exact hostname matches, as well as a URL typed into the
    // browser window like "exeda.cam", which should match
    // "exeda.cam.corp.google.com".
    StringPiece host = url.Host();
    if (IsLocalhost(host, hostname_) ||
        StringPiece(hostname_).starts_with(StrCat(host, "."))) {
      ret = true;
    }
  }
  return ret;
}

void ProxyInterface::Fetch(const GoogleString& requested_url_string,
                           MessageHandler* handler,
                           AsyncFetch* async_fetch) {
  const GoogleUrl requested_url(requested_url_string);
  const bool is_get_or_head =
      (async_fetch->request_headers()->method() == RequestHeaders::kGet) ||
      (async_fetch->request_headers()->method() == RequestHeaders::kHead);

  all_requests_->IncBy(1);
  if (!(requested_url.is_valid() && IsWellFormedUrl(requested_url))) {
    LOG(WARNING) << "Bad URL, failing request: " << requested_url_string;
    async_fetch->response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
    async_fetch->Done(false);
  } else {
    // Try to handle this as a .pagespeed. resource.
    if (is_get_or_head && server_context_->IsPagespeedResource(requested_url)) {
      pagespeed_requests_->IncBy(1);
      LOG(INFO) << "Serving URL as pagespeed resource: "
                << requested_url.Spec();
      ProxyRequest(true, requested_url, async_fetch, handler);
    } else if (UrlAndPortMatchThisServer(requested_url)) {
      // Just respond with a 404 for now.
      async_fetch->response_headers()->SetStatusAndReason(
          HttpStatus::kNotFound);
      LOG(INFO) << "Returning 404 for URL: " << requested_url.Spec();
      async_fetch->Done(false);
    } else {
      // Otherwise we proxy it (rewriting if it is HTML).
      LOG(INFO) << "Proxying URL normally: " << requested_url.Spec();
      ProxyRequest(false, requested_url, async_fetch, handler);
    }
  }
}

void ProxyInterface::ProxyRequest(bool is_resource_fetch,
                                  const GoogleUrl& request_url,
                                  AsyncFetch* async_fetch,
                                  MessageHandler* handler) {
  scoped_ptr<GoogleUrl> gurl(new GoogleUrl);
  gurl->Reset(request_url);

  // Stripping ModPagespeed query params before the property cache lookup to
  // make cache key consistent for both lookup and storing in cache.
  ServerContext::OptionsBoolPair query_options_success =
      server_context_->GetQueryOptions(gurl.get(),
                                       async_fetch->request_headers(),
                                       NULL);

  if (!query_options_success.second) {
    async_fetch->response_headers()->SetStatusAndReason(
        HttpStatus::kMethodNotAllowed);
    async_fetch->Write("Invalid PageSpeed query-params/request headers",
                       handler);
    async_fetch->Done(false);
    return;
  }

  // Owned by ProxyInterfaceUrlNamerCallback.
  GoogleUrl* released_gurl(gurl.release());

  ProxyInterfaceUrlNamerCallback* proxy_interface_url_namer_callback =
      new ProxyInterfaceUrlNamerCallback(is_resource_fetch, released_gurl,
                                         async_fetch, this,
                                         query_options_success.first, handler);

  server_context_->url_namer()->DecodeOptions(
      *released_gurl, *async_fetch->request_headers(),
      proxy_interface_url_namer_callback, handler);
}

ProxyFetchPropertyCallbackCollector*
    ProxyInterface::InitiatePropertyCacheLookup(
    bool is_resource_fetch,
    const GoogleUrl& request_url,
    RewriteOptions* options,
    AsyncFetch* async_fetch,
    const bool requires_blink_cohort,
    bool* added_page_property_callback) {
  RequestContextPtr request_ctx = async_fetch->request_context();
  DCHECK(request_ctx.get() != NULL);
  if (request_ctx->root_trace_context() != NULL) {
    request_ctx->root_trace_context()->TracePrintf(
        "PropertyCache lookup start");
  }
  StringPiece user_agent =
      async_fetch->request_headers()->Lookup1(HttpAttributes::kUserAgent);
  UserAgentMatcher::DeviceType device_type =
      server_context_->user_agent_matcher()->GetDeviceTypeForUA(user_agent);

  scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
      new ProxyFetchPropertyCallbackCollector(
          server_context_, request_url.Spec(), request_ctx, options,
          device_type));
  bool added_callback = false;
  PropertyPageStarVector property_callbacks;

  ProxyFetchPropertyCallback* client_callback = NULL;
  ProxyFetchPropertyCallback* property_callback = NULL;
  ProxyFetchPropertyCallback* fallback_property_callback = NULL;
  PropertyCache* page_property_cache = server_context_->page_property_cache();
  PropertyCache* client_property_cache =
      server_context_->client_property_cache();
  if (!is_resource_fetch &&
      server_context_->page_property_cache()->enabled() &&
      UrlMightHavePropertyCacheEntry(request_url) &&
      async_fetch->request_headers()->method() == RequestHeaders::kGet) {
    if (options != NULL) {
      server_context_->ComputeSignature(options);
    }
    AbstractMutex* mutex = server_context_->thread_system()->NewMutex();
    const StringPiece& device_type_suffix =
        UserAgentMatcher::DeviceTypeSuffix(device_type);
    GoogleString page_key = server_context_->GetPagePropertyCacheKey(
        request_url.Spec(), options, device_type_suffix);
    property_callback = new ProxyFetchPropertyCallback(
        ProxyFetchPropertyCallback::kPropertyCachePage,
        page_property_cache, page_key, device_type,
        callback_collector.get(), mutex);
    callback_collector->AddCallback(property_callback);
    added_callback = true;
    if (added_page_property_callback != NULL) {
      *added_page_property_callback = true;
    }
    // Trigger property cache lookup for the requests which contains query param
    // as cache key without query params. The result of this lookup will be used
    // if actual property page does not contains property value.
    if (options != NULL &&
        options->use_fallback_property_cache_values() &&
        request_url.has_query()) {
      GoogleString fallback_page_key =
          server_context_->GetFallbackPagePropertyCacheKey(
              request_url.AllExceptQuery(), options, device_type_suffix);
      fallback_property_callback =
          new ProxyFetchPropertyCallback(
              ProxyFetchPropertyCallback::kPropertyCacheFallbackPage,
              page_property_cache, fallback_page_key, device_type,
              callback_collector.get(),
              server_context_->thread_system()->NewMutex());
      callback_collector->AddCallback(fallback_property_callback);
    }
  }

  // Initiate client property cache lookup.
  if (async_fetch != NULL) {
    const char* client_id = async_fetch->request_headers()->Lookup1(
        HttpAttributes::kXGooglePagespeedClientId);
    if (client_id != NULL) {
      if (client_property_cache->enabled()) {
        AbstractMutex* mutex = server_context_->thread_system()->NewMutex();
        client_callback = new ProxyFetchPropertyCallback(
            ProxyFetchPropertyCallback::kClientPropertyCachePage,
            client_property_cache, client_id,
            UserAgentMatcher::kEndOfDeviceType,
            callback_collector.get(), mutex);
        callback_collector->AddCallback(client_callback);
        added_callback = true;
      }
    }
  }

  // All callbacks need to be registered before Reads to avoid race.
  PropertyCache::CohortVector cohort_list_without_blink = GetCohortList(false);
  if (property_callback != NULL) {
    page_property_cache->ReadWithCohorts(
        requires_blink_cohort ?
            GetCohortList(true) : cohort_list_without_blink,
        property_callback);
  }

  if (fallback_property_callback != NULL) {
    // Always read property page with fallback values without blink as there is
    // no property in BlinkCohort which can used fallback values.
    page_property_cache->ReadWithCohorts(cohort_list_without_blink,
                                         fallback_property_callback);
  }

  if (client_callback != NULL) {
    client_property_cache->Read(client_callback);
  }

  if (added_callback) {
    AbstractLogRecord* log_record = request_ctx->log_record();
    log_record->SetTimeToPcacheStart(server_context_->timer()->NowMs());
  } else {
    callback_collector.reset(NULL);
  }
  return callback_collector.release();
}

PropertyCache::CohortVector ProxyInterface::GetCohortList(
    bool requires_blink_cohort) const {
  PropertyCache* page_property_cache = server_context_->page_property_cache();
  const PropertyCache::CohortVector cohort_list =
      page_property_cache->GetAllCohorts();
  if (requires_blink_cohort) {
    return cohort_list;
  }

  PropertyCache::CohortVector cohort_list_without_blink;
  for (int i = 0, m = cohort_list.size(); i < m; ++i) {
    if (cohort_list[i]->name() ==
        BlinkCriticalLineDataFinder::kBlinkCohort) {
      continue;
    }
    cohort_list_without_blink.push_back(cohort_list[i]);
  }
  return cohort_list_without_blink;
}

void ProxyInterface::ProxyRequestCallback(
    bool is_resource_fetch,
    GoogleUrl* url,
    AsyncFetch* async_fetch,
    RewriteOptions* domain_options,
    RewriteOptions* query_options,
    MessageHandler* handler) {
  scoped_ptr<GoogleUrl> request_url(url);
  RewriteOptions* options = server_context_->GetCustomOptions(
      async_fetch->request_headers(), domain_options, query_options);
  GoogleString url_string;
  RequestHeaders* request_headers = async_fetch->request_headers();
  request_url->Spec().CopyToString(&url_string);
  if (options != NULL &&
      options->IsRequestDeclined(url_string, request_headers)) {
    rejected_requests_->IncBy(1);
    ResponseHeaders* response_headers = async_fetch->response_headers();
    response_headers->SetStatusAndReason(HttpStatus::kProxyDeclinedRequest);
    response_headers->Replace(HttpAttributes::kContentType,
                              kContentTypeText.mime_type());
    response_headers->Replace(HttpAttributes::kCacheControl,
                              "private, max-age=0");
    async_fetch->Write(kRejectedRequestHtmlResponse, handler);
    async_fetch->Done(false);
    delete options;
    return;
  }

  // Update request_headers.
  // We deal with encodings. So strip the users Accept-Encoding headers.
  async_fetch->request_headers()->RemoveAll(HttpAttributes::kAcceptEncoding);
  // Note: We preserve the User-Agent and Cookies so that the origin servers
  // send us the correct HTML. We will need to consider this for caching HTML.

  AbstractLogRecord* log_record = async_fetch->request_context()->log_record();
  log_record->SetTimeToStartProcessing(server_context_->timer()->NowMs());
  {
    ScopedMutex lock(log_record->mutex());
    log_record->logging_info()->set_is_pagespeed_resource(is_resource_fetch);
  }

  // Start fetch and rewrite.  If GetCustomOptions found options for us,
  // the RewriteDriver created by StartNewProxyFetch will take ownership.
  if (is_resource_fetch) {
    // TODO(sligocki): Set using_spdy appropriately.
    bool using_spdy = false;
    // TODO(pulkitg): Set is_original_resource_cacheable to false if pagespeed
    // resource is not cacheable.
    ResourceFetch::Start(*request_url, options, using_spdy,
                         server_context_, async_fetch);
  } else {
    // TODO(nforman): If we are not running an experiment, remove the
    // furious cookie.
    // If we don't already have custom options, and the global options
    // say we're running furious, then clone them into custom_options so we
    // can manipulate custom options without affecting the global options.
    if (options == NULL) {
      RewriteOptions* global_options = server_context_->global_options();
      if (global_options->running_furious()) {
        options = global_options->Clone();
      }
    }
    // TODO(anupama): Adapt the below furious experiment logic for
    // FlushEarlyFlow as well.
    bool need_to_store_experiment_data = false;
    if (options != NULL && options->running_furious()) {
      need_to_store_experiment_data = server_context_->furious_matcher()->
          ClassifyIntoExperiment(*async_fetch->request_headers(), options);
      options->set_need_to_store_experiment_data(need_to_store_experiment_data);
    }
    const char* user_agent = async_fetch->request_headers()->Lookup1(
        HttpAttributes::kUserAgent);
    const bool is_blink_request = BlinkUtil::IsBlinkRequest(
        *request_url, async_fetch, options, user_agent, server_context_,
        RewriteOptions::kPrioritizeVisibleContent);
    const bool apply_blink_critical_line =
        BlinkUtil::ShouldApplyBlinkFlowCriticalLine(server_context_,
                                                    options);
    bool page_callback_added = false;

    // Whether it's a cache html request should not change despite the fact
    // a new driver is created later on.
    const bool is_cache_html_request = BlinkUtil::IsBlinkRequest(
        *request_url, async_fetch, options,
        user_agent, server_context_,
        RewriteOptions::kCachePartialHtml);

    const bool requires_blink_cohort =
        (is_blink_request && apply_blink_critical_line) ||
        is_cache_html_request;

    // Ownership of "property_callback" is eventually assumed by either
    // CacheHtmlFlow or ProxyFetch.
    ProxyFetchPropertyCallbackCollector* property_callback =
        InitiatePropertyCacheLookup(is_resource_fetch,
                                    *request_url,
                                    options,
                                    async_fetch,
                                    requires_blink_cohort,
                                    &page_callback_added);

    if (options != NULL) {
      server_context_->ComputeSignature(options);
      {
        ScopedMutex lock(log_record->mutex());
        log_record->logging_info()->set_options_signature_hash(
            server_context_->contents_hasher()->HashToUint64(
                options->signature()));
      }
    }

    if (is_blink_request && apply_blink_critical_line && page_callback_added) {
      // In blink flow, we have to modify RewriteOptions after the
      // property cache read is completed. Hence, we clear the signature to
      // unfreeze RewriteOptions, which was frozen during signature computation
      // for generating key for property cache read.
      // Warning: Please note that using this method is extremely risky and
      // should be avoided as much as possible. If you are planning to use
      // this, please discuss this with your team-mates and ensure that you
      // clearly understand its implications. Also, please do repeat this
      // warning at every place you use this method.
      options->ClearSignatureWithCaution();

      // TODO(rahulbansal): Remove this LOG once we expect to have
      // a lot of such requests.
      LOG(INFO) << "Triggering Blink flow critical line for url "
                << url_string;
      blink_critical_line_requests_->IncBy(1);
      BlinkFlowCriticalLine::Start(url_string, async_fetch, options,
                                   proxy_fetch_factory_.get(),
                                   server_context_,
                                   // Takes ownership of property_callback.
                                   property_callback);
    } else {
      RewriteDriver* driver = NULL;
      RequestContextPtr request_ctx = async_fetch->request_context();
      DCHECK(request_ctx.get() != NULL) << "Async fetch must have a request"
                                        << "context but does not.";
      if (options == NULL) {
        driver = server_context_->NewRewriteDriver(request_ctx);
      } else {
        // NewCustomRewriteDriver takes ownership of custom_options_.
        driver = server_context_->NewCustomRewriteDriver(options, request_ctx);
      }

      // TODO(mmohabey): Remove duplicate setting of user agent and
      // request headers for different flows.
      if (user_agent != NULL) {
        VLOG(1) << "Setting user-agent to " << user_agent;
        driver->SetUserAgent(user_agent);
      } else {
        VLOG(1) << "User-agent empty";
      }
      driver->set_request_headers(async_fetch->request_headers());
      // TODO(mmohabey): Factor out the below checks so that they are not
      // repeated in BlinkUtil::IsBlinkRequest().

      if (driver->options() != NULL && driver->options()->enabled() &&
          property_callback != NULL &&
          driver->options()->IsAllowed(url_string)) {
        if (is_cache_html_request) {
          cache_html_flow_requests_->IncBy(1);
          CacheHtmlFlow::Start(url_string,
                               async_fetch,
                               driver,
                               proxy_fetch_factory_.get(),
                               // Takes ownership of property_callback.
                               property_callback);

          return;
        }
        // NOTE: The FlushEarly flow will run in parallel with the ProxyFetch,
        // but will not begin (FlushEarlyFlwow::FlushEarly) until the
        // PropertyCache lookup has completed.
        // Also it does NOT take ownership of property_callback.
        // FlushEarlyFlow might not start if the request is not GET or if the
        // useragent is unsupported etc.
        FlushEarlyFlow::TryStart(url_string, &async_fetch, driver,
                                 proxy_fetch_factory_.get(),
                                 property_callback);
      }
      // Takes ownership of property_callback.
      proxy_fetch_factory_->StartNewProxyFetch(
          url_string, async_fetch, driver, property_callback, NULL);
    }
  }
}

}  // namespace net_instaweb
