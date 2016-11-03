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

#include "pagespeed/automatic/proxy_interface.h"

#include "base/callback.h"
#include "base/logging.h"
#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_timing_info.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/resource_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/automatic/proxy_fetch.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/hostname_util.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class MessageHandler;

namespace {

// Names for Statistics variables.
const char kTotalRequestCount[] = "all-requests";
const char kPagespeedRequestCount[] = "pagespeed-requests";
const char kRejectedRequestCount[] = "publisher-rejected-requests";
const char kRejectedRequestHtmlResponse[] = "Unable to serve "
    "content as the content is blocked by the administrator of the domain.";
const char kNoDomainConfigRequestCount[] = "without-domain-config-requests";
const char kNoDomainConfigResourceRequestCount[] =
    "without-domain-config-resource-requests";

}  // namespace

struct ProxyInterface::RequestData {
  bool is_resource_fetch;
  scoped_ptr<GoogleUrl> request_url;
  AsyncFetch* async_fetch;
  MessageHandler* handler;
};

ProxyInterface::ProxyInterface(StringPiece stats_prefix,
                               StringPiece hostname, int port,
                               ServerContext* server_context,
                               Statistics* stats)
    : server_context_(server_context),
      hostname_(hostname.as_string()),
      port_(port),
      all_requests_(stats->GetTimedVariable(
          StrCat(stats_prefix, kTotalRequestCount))),
      pagespeed_requests_(stats->GetTimedVariable(
          StrCat(stats_prefix, kPagespeedRequestCount))),
      rejected_requests_(stats->GetTimedVariable(
          StrCat(stats_prefix, kRejectedRequestCount))),
      requests_without_domain_config_(stats->GetTimedVariable(
          StrCat(stats_prefix, kNoDomainConfigRequestCount))),
      resource_requests_without_domain_config_(stats->GetTimedVariable(
          StrCat(stats_prefix,  kNoDomainConfigResourceRequestCount))) {
  proxy_fetch_factory_.reset(new ProxyFetchFactory(server_context));
}

ProxyInterface::~ProxyInterface() {
}

void ProxyInterface::InitStats(StringPiece stats_prefix,
                               Statistics* statistics) {
  statistics->AddTimedVariable(StrCat(stats_prefix, kTotalRequestCount),
                               Statistics::kDefaultGroup);
  statistics->AddTimedVariable(StrCat(stats_prefix, kPagespeedRequestCount),
                               Statistics::kDefaultGroup);
  statistics->AddTimedVariable(StrCat(stats_prefix, kRejectedRequestCount),
                               Statistics::kDefaultGroup);
  statistics->AddTimedVariable(
      StrCat(stats_prefix, kNoDomainConfigRequestCount),
      Statistics::kDefaultGroup);
  statistics->AddTimedVariable(
      StrCat(stats_prefix, kNoDomainConfigResourceRequestCount),
      Statistics::kDefaultGroup);
}

bool ProxyInterface::IsWellFormedUrl(const GoogleUrl& url) {
  bool ret = false;
  if (url.IsWebValid()) {
    if (url.has_path()) {
      StringPiece path = url.PathAndLeaf();
      GoogleString filename = url.ExtractFileName();
      int path_len = path.size() - filename.size();
      if (path_len >= 0) {
        ret = true;
      }
    } else {
      LOG(ERROR) << "URL has no path: " << url.Spec();
    }
  }
  return ret;
}

bool ProxyInterface::UrlAndPortMatchThisServer(const GoogleUrl& url) {
  bool ret = false;
  if (url.IsWebValid() && (url.EffectiveIntPort() == port_)) {
    // TODO(atulvasu): This should support matching the actual host this
    // machine can receive requests from. Ideally some flag control would
    // help. For example this server could be running multiple virtual
    // servers, and we would like to know what server we are catering to for
    // pagespeed only queries.
    //
    // Allow for exact hostname matches, as well as a URL typed into the
    // browser window like "box.localsite", which should match
    // "box.localsite.example.com".
    StringPiece host = url.Host();
    if (IsLocalhost(host, hostname_) ||
        strings::StartsWith(StringPiece(hostname_), StrCat(host, "."))) {
      ret = true;
    }
  }
  return ret;
}

void ProxyInterface::Fetch(const GoogleString& requested_url_string,
                           MessageHandler* handler,
                           AsyncFetch* async_fetch) {
  GoogleUrl requested_url(requested_url_string);
  const bool is_get_or_head =
      (async_fetch->request_headers()->method() == RequestHeaders::kGet) ||
      (async_fetch->request_headers()->method() == RequestHeaders::kHead);

  all_requests_->IncBy(1);
  if (!(requested_url.IsWebValid() && IsWellFormedUrl(requested_url))) {
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
  RequestData* request_data = new RequestData;
  request_data->is_resource_fetch = is_resource_fetch;
  request_data->request_url.reset(new GoogleUrl);
  request_data->request_url->Reset(request_url);
  request_data->async_fetch = async_fetch;
  request_data->handler = handler;

  server_context_->rewrite_options_manager()->GetRewriteOptions(
      request_url,
      *async_fetch->request_headers(),
      NewCallback(this, &ProxyInterface::GetRewriteOptionsDone, request_data));
}

ProxyFetchPropertyCallbackCollector*
    ProxyInterface::InitiatePropertyCacheLookup(
    bool is_resource_fetch,
    const GoogleUrl& request_url,
    RewriteOptions* options,
    AsyncFetch* async_fetch) {
  return ProxyFetchFactory::InitiatePropertyCacheLookup(
      is_resource_fetch, request_url, server_context_, options, async_fetch);
}

void ProxyInterface::GetRewriteOptionsDone(RequestData* request_data,
                                           RewriteOptions* domain_options) {
  scoped_ptr<RequestData> request_data_deleter(request_data);
  scoped_ptr<RewriteOptions> scoped_domain_options(domain_options);
  bool is_resource_fetch = request_data->is_resource_fetch;
  GoogleUrl* request_url = request_data->request_url.get();
  AsyncFetch* async_fetch = request_data->async_fetch;
  MessageHandler* handler = request_data->handler;

  if (domain_options == NULL) {
    requests_without_domain_config_->IncBy(1);
    if (is_resource_fetch) {
      resource_requests_without_domain_config_->IncBy(1);
    }
  }

  // Parse the query options, headers, and cookies.
  RewriteQuery query;
  if (!server_context_->GetQueryOptions(async_fetch->request_context(),
                                        domain_options, request_url,
                                        async_fetch->request_headers(),
                                        NULL /* response_headers */, &query)) {
    async_fetch->response_headers()->SetStatusAndReason(
        HttpStatus::kMethodNotAllowed);
    async_fetch->Write("Invalid PageSpeed query-params/request headers",
                       handler);
    async_fetch->Done(false);
    return;
  }

  RewriteOptions* options = server_context_->GetCustomOptions(
      async_fetch->request_headers(), scoped_domain_options.release(),
      query.ReleaseOptions());
  GoogleString url_string;
  request_url->Spec().CopyToString(&url_string);
  RequestHeaders* request_headers = async_fetch->request_headers();
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
  if (!async_fetch->request_headers()->Lookup1(
          HttpAttributes::kXPageSpeedLoop)) {
    // In proxy mode (mpr) we must pass through the accept encoding to be able
    // to tell if the origin server is sending gzipped content when the client
    // is requesting it.
    async_fetch->request_headers()->RemoveAll(HttpAttributes::kAcceptEncoding);
  }
  // Note: We preserve the User-Agent and Cookies so that the origin servers
  // send us the correct HTML. We will need to consider this for caching HTML.

  async_fetch->request_context()->mutable_timing_info()->ProcessingStarted();

  int prior_experiment_id;
  bool cookie_found = experiment::GetExperimentCookieState(
      *async_fetch->request_headers(), &prior_experiment_id);

  AbstractLogRecord* log_record =  async_fetch->request_context()->log_record();
  {
    ScopedMutex lock(log_record->mutex());
    log_record->logging_info()->set_is_pagespeed_resource(is_resource_fetch);
    if (cookie_found) {
      log_record->logging_info()->set_prior_experiment_id(prior_experiment_id);
    }
  }

  // Start fetch and rewrite.  If GetCustomOptions found options for us,
  // the RewriteDriver created by StartNewProxyFetch will take ownership.
  if (is_resource_fetch) {
    // TODO(pulkitg): Set is_original_resource_cacheable to false if pagespeed
    // resource is not cacheable.
    const RewriteOptions* these_options =
        (options == NULL ? server_context_->global_options() : options);
    // TODO(sligocki): Should we be setting default options and then overriding
    // here? It seems like it would be better to only set once, but that
    // involves a lot of complicated code changes.
    async_fetch->request_context()->ResetOptions(
        these_options->ComputeHttpOptions());
    ResourceFetch::Start(*request_url, options, server_context_, async_fetch);
  } else {
    // TODO(nforman): If we are not running an experiment, remove the
    // experiment cookie.
    // If we don't already have custom options, and the global options say we're
    // running an experiment, then clone them into custom_options so we can
    // manipulate custom options without affecting the global options.
    if (options == NULL) {
      RewriteOptions* global_options = server_context_->global_options();
      if (global_options->running_experiment()) {
        options = global_options->Clone();
      }
    }
    bool need_to_store_experiment_data = false;
    if (options != NULL && options->running_experiment()) {
      need_to_store_experiment_data =
          server_context_->experiment_matcher()->ClassifyIntoExperiment(
              *async_fetch->request_headers(),
              *server_context_->user_agent_matcher(), options);
      options->set_need_to_store_experiment_data(need_to_store_experiment_data);
    }

    ProxyFetchPropertyCallbackCollector* property_callback = NULL;

    if (options == NULL ||
        (options->enabled() && options->IsAllowed(request_url->Spec()))) {
      // Ownership of "property_callback" is eventually assumed by ProxyFetch.
      property_callback = InitiatePropertyCacheLookup(
          is_resource_fetch, *request_url, options, async_fetch);
    }

    if (options != NULL) {
      server_context_->ComputeSignature(options);
      {
        ScopedMutex lock(log_record->mutex());
        log_record->logging_info()->set_options_signature_hash(
            server_context_->contents_hasher()->HashToUint64(
                options->signature()));
      }
    }

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
    // TODO(sligocki): Should we be setting default options and then overriding
    // here? It seems like it would be better to only set once, but that
    // involves a lot of complicated code changes.
    request_ctx->ResetOptions(driver->options()->ComputeHttpOptions());
    driver->SetRequestHeaders(*async_fetch->request_headers());
    // TODO(mmohabey): Factor out the below checks so that they are not
    // repeated in BlinkUtil::IsBlinkRequest().

    // Copy over any PageSpeed query parameters so we can re-add them if we
    // receive a redirection response to our fetch request.
    driver->set_pagespeed_query_params(
        query.pagespeed_query_params().ToEscapedString());
    // Copy over any PageSpeed cookies so we know which ones to clear in
    // ProxyFetch::HandleHeadersComplete().
    driver->set_pagespeed_option_cookies(
        query.pagespeed_option_cookies().ToEscapedString());

    // Takes ownership of property_callback.
    proxy_fetch_factory_->StartNewProxyFetch(
        url_string, async_fetch, driver, property_callback, NULL);
  }
}

}  // namespace net_instaweb
