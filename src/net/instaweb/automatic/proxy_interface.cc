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
#include "base/scoped_ptr.h"
#include "net/instaweb/automatic/public/blink_flow_critical_line.h"
#include "net/instaweb/automatic/public/flush_early_flow.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/automatic/public/resource_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/furious_matcher.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/hostname_util.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;
class MessageHandler;

const char ProxyInterface::kBlinkRequestCount[] = "blink-requests";
const char ProxyInterface::kBlinkCriticalLineRequestCount[] =
    "blink-critical-line-requests";

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
    case ContentType::kPdf:
    case ContentType::kOther:
    case ContentType::kJson:
      return false;
  }
  LOG(DFATAL) << "URL " << url.Spec() << ": unexpected type:" << type->type()
              << "; " << type->mime_type() << "; " << type->file_extension();
  return false;
}

bool HasRejectedHeader(const StringPiece& header_name,
                       const RequestHeaders* request_headers,
                       const RewriteOptions* options) {
  ConstStringStarVector header_values;
  if (request_headers->Lookup(header_name, &header_values)) {
    for (int i = 0, n = header_values.size(); i < n; ++i) {
      if (options->IsRejectedRequest(header_name, *header_values[i])) {
        return true;
      }
    }
  }
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
                               ServerContext* manager,
                               Statistics* stats)
    : server_context_(manager),
      fetcher_(NULL),
      timer_(NULL),
      handler_(manager->message_handler()),
      hostname_(hostname.as_string()),
      port_(port),
      all_requests_(stats->GetTimedVariable(kTotalRequestCount)),
      pagespeed_requests_(stats->GetTimedVariable(kPagespeedRequestCount)),
      blink_requests_(stats->GetTimedVariable(kBlinkRequestCount)),
      blink_critical_line_requests_(
          stats->GetTimedVariable(kBlinkCriticalLineRequestCount)),
      rejected_requests_(stats->GetTimedVariable(kRejectedRequestCount)) {
  proxy_fetch_factory_.reset(new ProxyFetchFactory(manager));
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
  statistics->AddTimedVariable(kRejectedRequestCount,
                               ServerContext::kStatisticsGroup);
  BlinkFlowCriticalLine::InitStats(statistics);
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
  bool is_get_or_head =
      (async_fetch->request_headers()->method() == RequestHeaders::kGet) ||
      (async_fetch->request_headers()->method() == RequestHeaders::kHead);

  all_requests_->IncBy(1);
  if (!(requested_url.is_valid() && IsWellFormedUrl(requested_url))) {
    LOG(WARNING) << "Bad URL, failing request: " << requested_url_string;
    async_fetch->response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
    async_fetch->Done(false);
  } else {
    // Try to handle this as a .pagespeed. resource.
    if (server_context_->IsPagespeedResource(requested_url) &&
        is_get_or_head) {
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
                                       async_fetch->request_headers());

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
    AsyncFetch* async_fetch) {
  scoped_ptr<ProxyFetchPropertyCallbackCollector> callback_collector(
      new ProxyFetchPropertyCallbackCollector(
          server_context_, request_url.Spec(), options));
  bool added_callback = false;
  ProxyFetchPropertyCallback* property_callback = NULL;
  PropertyCache* page_property_cache = NULL;
  if (!is_resource_fetch &&
      server_context_->page_property_cache()->enabled() &&
      UrlMightHavePropertyCacheEntry(request_url)) {
    page_property_cache = server_context_->page_property_cache();
    AbstractMutex* mutex = server_context_->thread_system()->NewMutex();
    if (options != NULL) {
      server_context_->ComputeSignature(options);
      property_callback = new ProxyFetchPropertyCallback(
          ProxyFetchPropertyCallback::kPagePropertyCache,
          StrCat(request_url.Spec(), "_", options->signature()),
          callback_collector.get(), mutex);
    } else {
      property_callback = new ProxyFetchPropertyCallback(
          ProxyFetchPropertyCallback::kPagePropertyCache,
          request_url.Spec(),
          callback_collector.get(), mutex);
    }
    callback_collector->AddCallback(property_callback);
    added_callback = true;

    // Don't initiate the Read until the client_id lookup, if any, has had
    // an opportunity to establish its callback.  Otherwise we race the
    // completion of this pcache lookup against adding the client-cache
    // lookup's callback.  Also this would cause the unit-test
    // ProxyInterfaceTest.BothClientAndPropertyCache to hang on two
    // Wait calls when the test only sets up one Signal.
  }

  // Initiate client property cache lookup.
  if (async_fetch != NULL) {
    const char* client_id = async_fetch->request_headers()->Lookup1(
        HttpAttributes::kXGooglePagespeedClientId);
    if (client_id != NULL) {
      PropertyCache* client_property_cache =
          server_context_->client_property_cache();
      if (client_property_cache->enabled()) {
        AbstractMutex* mutex = server_context_->thread_system()->NewMutex();
        ProxyFetchPropertyCallback* callback =
            new ProxyFetchPropertyCallback(
                ProxyFetchPropertyCallback::kClientPropertyCache,
                client_id,
                callback_collector.get(), mutex);
        callback_collector->AddCallback(callback);
        added_callback = true;
        client_property_cache->Read(callback);
      }
    }
  }
  if (page_property_cache != NULL) {
    page_property_cache->Read(property_callback);
  }
  if (!added_callback) {
    callback_collector.reset(NULL);
  }
  return callback_collector.release();
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
  if ((options != NULL) &&
      (options->IsRejectedUrl(url_string) ||
       HasRejectedHeader(
           HttpAttributes::kUserAgent, request_headers, options) ||
       HasRejectedHeader(
           HttpAttributes::kXForwardedFor, request_headers, options))) {
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

  scoped_ptr<ProxyFetchPropertyCallbackCollector> property_callback;

  // Update request_headers.
  // We deal with encodings. So strip the users Accept-Encoding headers.
  async_fetch->request_headers()->RemoveAll(HttpAttributes::kAcceptEncoding);
  // Note: We preserve the User-Agent and Cookies so that the origin servers
  // send us the correct HTML. We will need to consider this for caching HTML.

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
    bool is_blink_request = BlinkUtil::IsBlinkRequest(
        *request_url, async_fetch, options, user_agent,
        server_context_->user_agent_matcher());
    bool apply_blink_critical_line =
        BlinkUtil::ShouldApplyBlinkFlowCriticalLine(server_context_,
                                                    options);
    if (is_blink_request && apply_blink_critical_line) {
      property_callback.reset(InitiatePropertyCacheLookup(
          is_resource_fetch, *request_url, options, async_fetch));
    }
    if (is_blink_request && apply_blink_critical_line &&
        property_callback.get() != NULL) {
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
                                   property_callback.release());
    } else {
      RewriteDriver* driver = NULL;
      // Starting property cache lookup after the furious state is set.
      property_callback.reset(InitiatePropertyCacheLookup(
          is_resource_fetch, *request_url, options, async_fetch));
      if (options == NULL) {
        driver = server_context_->NewRewriteDriver();
      } else {
        // NewCustomRewriteDriver takes ownership of custom_options_.
        driver = server_context_->NewCustomRewriteDriver(options);
      }
      driver->set_log_record(async_fetch->log_record());

      // TODO(mmohabey): Remove duplicate setting of user agent for different
      // flows.
      if (user_agent != NULL) {
        VLOG(1) << "Setting user-agent to " << user_agent;
        driver->set_user_agent(user_agent);
      } else {
        VLOG(1) << "User-agent empty";
      }
      if (property_callback != NULL &&
        FlushEarlyFlow::CanFlushEarly(url_string, async_fetch, driver)) {
        FlushEarlyFlow::Start(url_string, &async_fetch, driver,
                              proxy_fetch_factory_.get(),
                              property_callback.get());
      }
      proxy_fetch_factory_->StartNewProxyFetch(
          url_string, async_fetch, driver, property_callback.release(), NULL);
    }
  }

  if (property_callback.get() != NULL) {
    // If management of the callback was not transferred to proxy fetch,
    // then we must detach it so it deletes itself when complete.
    property_callback.release()->Detach();
  }
}

}  // namespace net_instaweb
