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
#include "net/instaweb/automatic/public/blink_flow.h"
#include "net/instaweb/automatic/public/blink_flow_critical_line.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/automatic/public/resource_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/blink_util.h"
#include "net/instaweb/rewriter/public/furious_util.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/property_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"

namespace net_instaweb {

class AbstractMutex;
class Layout;
class MessageHandler;

const char ProxyInterface::kBlinkRequestCount[] = "blink-requests";
const char ProxyInterface::kBlinkCriticalLineRequestCount[] =
    "blink-critical-line-requests";

namespace {

// Names for Statistics variables.
const char kTotalRequestCount[] = "all-requests";
const char kPagespeedRequestCount[] = "pagespeed-requests";
const char kBlinkRequestCount[] = "blink-requests";

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
      ProxyFetchPropertyCallbackCollector* property_callback,
      ProxyInterface* proxy_interface,
      RewriteOptions* query_options,
      MessageHandler* handler)
      : is_resource_fetch_(is_resource_fetch),
        request_url_(request_url),
        async_fetch_(async_fetch),
        property_callback_(property_callback),
        handler_(handler),
        proxy_interface_(proxy_interface),
        query_options_(query_options) {
  }
  virtual ~ProxyInterfaceUrlNamerCallback() {}
  virtual void Done(RewriteOptions* rewrite_options) {
    proxy_interface_->ProxyRequestCallback(
        is_resource_fetch_, request_url_, async_fetch_, rewrite_options,
        query_options_, property_callback_, handler_);
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
                               ResourceManager* manager,
                               Statistics* stats)
    : resource_manager_(manager),
      handler_(manager->message_handler()),
      hostname_(hostname.as_string()),
      port_(port),
      all_requests_(stats->GetTimedVariable(kTotalRequestCount)),
      pagespeed_requests_(stats->GetTimedVariable(kPagespeedRequestCount)),
      blink_requests_(stats->GetTimedVariable(kBlinkRequestCount)),
      blink_critical_line_requests_(
          stats->GetTimedVariable(kBlinkCriticalLineRequestCount)) {
  proxy_fetch_factory_.reset(new ProxyFetchFactory(manager));
}

ProxyInterface::~ProxyInterface() {
}

void ProxyInterface::Initialize(Statistics* statistics) {
  statistics->AddTimedVariable(kTotalRequestCount,
                               ResourceManager::kStatisticsGroup);
  statistics->AddTimedVariable(kPagespeedRequestCount,
                               ResourceManager::kStatisticsGroup);
  statistics->AddTimedVariable(kBlinkRequestCount,
                               ResourceManager::kStatisticsGroup);
  statistics->AddTimedVariable(kBlinkCriticalLineRequestCount,
                               ResourceManager::kStatisticsGroup);
  BlinkFlow::Initialize(statistics);
  BlinkFlowCriticalLine::Initialize(statistics);
}

void ProxyInterface::set_server_version(const StringPiece& server_version) {
  proxy_fetch_factory_->set_server_version(server_version);
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
    if ((host == "localhost") ||
        (host == "127.0.0.1") ||
        (host == "::1") ||
        // TODO(sligocki): Cover other representations of IPv6 localhost IP?
        (host == hostname_) ||
        StringPiece(hostname_).starts_with(StrCat(host, "."))) {
      ret = true;
    }
  }
  return ret;
}

bool ProxyInterface::Fetch(const GoogleString& requested_url_string,
                           MessageHandler* handler,
                           AsyncFetch* async_fetch) {
  const GoogleUrl requested_url(requested_url_string);
  bool is_get_or_head =
      (async_fetch->request_headers()->method() == RequestHeaders::kGet) ||
      (async_fetch->request_headers()->method() == RequestHeaders::kHead);

  bool done = false;

  all_requests_->IncBy(1);
  if (!(requested_url.is_valid() && IsWellFormedUrl(requested_url))) {
    LOG(ERROR) << "Bad URL, failing request: " << requested_url_string;
    async_fetch->response_headers()->SetStatusAndReason(HttpStatus::kNotFound);
    async_fetch->Done(false);
    done = true;
  } else {
    // Try to handle this as a .pagespeed. resource.
    if (resource_manager_->IsPagespeedResource(requested_url) &&
        is_get_or_head) {
      pagespeed_requests_->IncBy(1);
      ProxyRequest(true, requested_url, async_fetch, handler);
      LOG(INFO) << "Serving URL as pagespeed resource: "
                << requested_url.Spec();
    } else if (UrlAndPortMatchThisServer(requested_url)) {
      // Just respond with a 404 for now.
      async_fetch->response_headers()->SetStatusAndReason(
          HttpStatus::kNotFound);
      LOG(INFO) << "Returning 404 for URL: " << requested_url.Spec();
      async_fetch->Done(false);
      done = true;
    } else {
      // Otherwise we proxy it (rewriting if it is HTML).
      LOG(INFO) << "Proxying URL normally: " << requested_url.Spec();
      ProxyRequest(false, requested_url, async_fetch, handler);
    }
  }

  return done;
}

RewriteOptions* ProxyInterface::GetCustomOptions(
    GoogleUrl* request_url, RequestHeaders* request_headers,
    RewriteOptions* domain_options, RewriteOptions* query_options,
    MessageHandler* handler) {
  RewriteOptions* options = resource_manager_->global_options();
  scoped_ptr<RewriteOptions> custom_options;
  scoped_ptr<RewriteOptions> scoped_domain_options(domain_options);
  if (scoped_domain_options.get() != NULL) {
    custom_options.reset(resource_manager_->NewOptions());
    custom_options->Merge(*options);
    custom_options->Merge(*scoped_domain_options.get());
    options = custom_options.get();
  }

  scoped_ptr<RewriteOptions> query_options_ptr(query_options);
  // Check query params & request-headers
  if (query_options_ptr.get() != NULL) {
    // Subtle memory management to handle deleting any domain_options
    // after the merge, and transferring ownership to the caller for
    // the new merged options.
    scoped_ptr<RewriteOptions> options_buffer(custom_options.release());
    custom_options.reset(resource_manager_->NewOptions());
    custom_options->Merge(*options);
    custom_options->Merge(*query_options);
    // Don't run any experiments if this is a special query-params request.
    custom_options->set_running_furious_experiment(false);
  }

  return custom_options.release();
}

ProxyInterface::OptionsBoolPair ProxyInterface::GetQueryOptions(
    GoogleUrl* request_url, RequestHeaders* request_headers,
    MessageHandler* handler) {
  scoped_ptr<RewriteOptions> query_options;
  bool success = false;
  switch (RewriteQuery::Scan(
              resource_manager_->factory(), request_url, request_headers,
              &query_options, handler)) {
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
  return OptionsBoolPair(query_options.release(), success);
}

void ProxyInterface::ProxyRequest(bool is_resource_fetch,
                                  const GoogleUrl& request_url,
                                  AsyncFetch* async_fetch,
                                  MessageHandler* handler) {
  scoped_ptr<GoogleUrl> gurl(new GoogleUrl);
  gurl->Reset(request_url);

  // Stripping ModPagespeed query params before the property cache lookup to
  // make cache key consistent for both lookup and storing in cache.
  OptionsBoolPair query_options_success = GetQueryOptions(
      gurl.get(), async_fetch->request_headers(), handler);

  if (!query_options_success.second) {
    async_fetch->response_headers()->SetStatusAndReason(
        HttpStatus::kMethodNotAllowed);
    async_fetch->Write("Invalid PageSpeed query-params/request headers",
                       handler);
    async_fetch->Done(false);
    return;
  }

  // Initiate pcache lookups early, before we know the RewriteOptions,
  // in order to avoid adding latency to the serving flow. This has
  // the downside of adding more cache pressure. OTOH we do a lot of
  // cache lookups for HTML files: usually one per resource. So adding
  // one more shouldn't significantly increase the cache RPC pressure.
  // One thing to look out for is if we serve a lot of JPGs that don't
  // end in .jpg or .jpeg -- we'll pessimistically assume they are HTML
  // and do pcache lookups for them.
  ProxyFetchPropertyCallbackCollector* callback_collector =
      new ProxyFetchPropertyCallbackCollector(resource_manager_);
  if (!InitiatePropertyCacheLookup(is_resource_fetch,
                                   *gurl.get(),
                                   async_fetch,
                                   callback_collector)) {
    // Didn't need the collector after all
    delete callback_collector;
    callback_collector = NULL;
  }

  // Owned by ProxyInterfaceUrlNamerCallback.
  GoogleUrl* released_gurl(gurl.release());

  ProxyInterfaceUrlNamerCallback* proxy_interface_url_namer_callback =
      new ProxyInterfaceUrlNamerCallback(is_resource_fetch, released_gurl,
                                         async_fetch, callback_collector, this,
                                         query_options_success.first, handler);

  resource_manager_->url_namer()->DecodeOptions(
      *released_gurl, *async_fetch->request_headers(),
      proxy_interface_url_namer_callback, handler);
}

bool ProxyInterface::InitiatePropertyCacheLookup(
    bool is_resource_fetch,
    const GoogleUrl& request_url,
    AsyncFetch* async_fetch,
    ProxyFetchPropertyCallbackCollector* callback_collector) {
  bool added_callback = false;
  ProxyFetchPropertyCallback* property_callback = NULL;
  PropertyCache* page_property_cache = NULL;
  if (!is_resource_fetch &&
      resource_manager_->page_property_cache()->enabled() &&
      UrlMightHavePropertyCacheEntry(request_url)) {
    page_property_cache = resource_manager_->page_property_cache();
    AbstractMutex* mutex = resource_manager_->thread_system()->NewMutex();
    property_callback = new ProxyFetchPropertyCallback(
        ProxyFetchPropertyCallback::kPagePropertyCache,
        callback_collector, mutex);
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
  const char* client_id = async_fetch->request_headers()->Lookup1(
      HttpAttributes::kXGooglePagespeedClientId);
  if (client_id != NULL) {
    PropertyCache* client_property_cache =
        resource_manager_->client_property_cache();
    if (client_property_cache->enabled()) {
      AbstractMutex* mutex = resource_manager_->thread_system()->NewMutex();
      ProxyFetchPropertyCallback* callback =
          new ProxyFetchPropertyCallback(
              ProxyFetchPropertyCallback::kClientPropertyCache,
              callback_collector, mutex);
      callback_collector->AddCallback(callback);
      added_callback = true;
      client_property_cache->Read(client_id, callback);
    }
  }

  if (page_property_cache != NULL) {
    page_property_cache->Read(request_url.Spec(), property_callback);
  }

  return added_callback;
}

void ProxyInterface::ProxyRequestCallback(
    bool is_resource_fetch,
    GoogleUrl* request_url,
    AsyncFetch* async_fetch,
    RewriteOptions* domain_options,
    RewriteOptions* query_options,
    ProxyFetchPropertyCallbackCollector* property_callback,
    MessageHandler* handler) {
  RewriteOptions* options = GetCustomOptions(
      request_url, async_fetch->request_headers(), domain_options,
      query_options, handler);

  // Update request_headers.
  // We deal with encodings. So strip the users Accept-Encoding headers.
  async_fetch->request_headers()->RemoveAll(HttpAttributes::kAcceptEncoding);
  // Note: We preserve the User-Agent and Cookies so that the origin servers
  // send us the correct HTML. We will need to consider this for caching HTML.

  // Start fetch and rewrite.  If GetCustomOptions found options for us,
  // the RewriteDriver created by StartNewProxyFetch will take ownership.
  if (is_resource_fetch) {
    ResourceFetch::Start(resource_manager_, *request_url, async_fetch,
                         options, proxy_fetch_factory_->server_version());
  } else {
    // TODO(nforman): If we are not running an experiment, remove the
    // furious cookie.
    // If we don't already have custom options, and the global options
    // say we're running furious, then clone them into custom_options so we
    // can manipulate custom options without affecting the global options.
    const RewriteOptions* global_options =
        resource_manager_->global_options();
    if (options == NULL && global_options->running_furious()) {
      options = global_options->Clone();
    }
    const char* user_agent = async_fetch->request_headers()->Lookup1(
        HttpAttributes::kUserAgent);
    const Layout* layout = BlinkUtil::ExtractBlinkLayout(*request_url,
                                                         options, user_agent);
    bool is_blink_request = BlinkUtil::IsBlinkRequest(
        *request_url, async_fetch->request_headers(),
        options, user_agent, resource_manager_->user_agent_matcher());
    bool apply_blink_critical_line =
        BlinkUtil::ShouldApplyBlinkFlowCriticalLine(resource_manager_,
                                                    property_callback,
                                                    options);
    if (is_blink_request && apply_blink_critical_line) {
      // TODO(rahulbansal): Remove this LOG once we expect to have
      // a lot of such requests.
      LOG(INFO) << "Triggering Blink flow critical line for url "
                << request_url->Spec().as_string();
      blink_critical_line_requests_->IncBy(1);
      BlinkFlowCriticalLine::Start(request_url->Spec().as_string(),
                                   async_fetch, options,
                                   proxy_fetch_factory_.get(),
                                   resource_manager_, property_callback);
      // BlinkFlowCriticalLine takes ownership of property_callback.
      // NULL it here so that we do not detach it below.
      property_callback = NULL;
    } else if (is_blink_request && layout != NULL) {
      // TODO(rahulbansal): Remove this LOG once we expect to have
      // Blink requests.
      LOG(INFO) << "Triggering Blink flow for url "
                << request_url->Spec().as_string();
      blink_requests_->IncBy(1);
      BlinkFlow::Start(request_url->Spec().as_string(), async_fetch, layout,
                       options, proxy_fetch_factory_.get(),
                       resource_manager_);
      // TODO(jmarantz): provide property-cache data to blink.
    } else {
      RewriteDriver* driver = NULL;
      bool need_cookie = false;
      if (options != NULL && options->running_furious()) {
        int furious_value = furious::kFuriousNotSet;
        if (!furious::GetFuriousCookieState(*async_fetch->request_headers(),
                                            &furious_value)) {
          furious_value = furious::DetermineFuriousState(options);
          need_cookie = true;
        }
        options->SetFuriousState(furious_value);
      }
      if (options == NULL) {
        driver = resource_manager_->NewRewriteDriver();
      } else {
        resource_manager_->ComputeSignature(options);
        // NewCustomRewriteDriver takes ownership of custom_options_.
        driver = resource_manager_->NewCustomRewriteDriver(options);
      }
      driver->set_need_furious_cookie(need_cookie);
      proxy_fetch_factory_->StartNewProxyFetch(
          request_url->Spec().as_string(), async_fetch, driver,
          property_callback);
      // ProxyFetch takes ownership of property_callback.
      // NULL it here so that we do not detach it below.
      property_callback = NULL;
    }
  }

  if (property_callback != NULL) {
    // If management of the callback was not transferred to proxy fetch,
    // then we must detach it so it deletes itself when complete.
    property_callback->Detach();
  }
  delete request_url;
}

}  // namespace net_instaweb
