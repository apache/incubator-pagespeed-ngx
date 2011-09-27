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

#include <unistd.h>
#include <utility>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/instaweb/automatic/public/proxy_fetch.h"
#include "net/instaweb/automatic/public/resource_fetch.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/abstract_mutex.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/query_params.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/timer.h"
#include "net/instaweb/util/public/writer.h"

namespace net_instaweb {

// TODO(sligocki): Share these constants with Apache flow.
const char kModPagespeed[] = "ModPagespeed";
const char kModPagespeedFilters[] = "ModPagespeedFilters";
// TODO(sligocki): What do we want here?
const char kModPagespeedVersion[] = "VERSION_PLACEHOLDER";

// Histogram names.
const char kFetchLatencyHistogram[] = "Fetch Latency Histogram";
const char kRewriteLatencyHistogram[] = "Rewrite Latency Histogram";

// TimedVariable names.
const char kTotalFetchLatencyInMs[] = "total_fetch_count";
const char kTotalRewriteLatencyInMs[] = "total_rewrite_count";

ProxyInterface::ProxyInterface(const StringPiece& hostname, int port,
                               ResourceManager* manager,
                               Statistics* stats)
    : resource_manager_(manager),
      handler_(manager->message_handler()),
      hostname_(hostname.as_string()),
      port_(port) {
  // Add histograms we want in Page Speed Automatic.
  stats->AddHistogram(kFetchLatencyHistogram);
  stats->AddHistogram(kRewriteLatencyHistogram);
  stats->AddTimedVariable(kTotalFetchLatencyInMs,
                          ResourceManager::kStatisticsGroup);
  stats->AddTimedVariable(kTotalRewriteLatencyInMs,
                          ResourceManager::kStatisticsGroup);
  fetch_latency_histogram_ = stats->GetHistogram(kFetchLatencyHistogram);
  rewrite_latency_histogram_ = stats->GetHistogram(kRewriteLatencyHistogram);
  // Timers are not guaranteed to go forward in time, however
  // Histograms will CHECK-fail given a negative value unless
  // EnableNegativeBuckets is called, allowing bars to be created with
  // negative x-axis labels in the histogram.
  fetch_latency_histogram_->EnableNegativeBuckets();
  rewrite_latency_histogram_->EnableNegativeBuckets();

  total_fetch_count_ = stats->GetTimedVariable(kTotalFetchLatencyInMs);
  total_rewrite_count_ = stats->GetTimedVariable(kTotalRewriteLatencyInMs);

  proxy_fetch_factory_.reset(
      new ProxyFetchFactory(manager,
                            rewrite_latency_histogram_, total_rewrite_count_));
}

ProxyInterface::~ProxyInterface() {
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

bool ProxyInterface::StreamingFetch(const GoogleString& requested_url_string,
                                    const RequestHeaders& request_headers,
                                    ResponseHeaders* response_headers,
                                    Writer* response_writer,
                                    MessageHandler* handler,
                                    Callback* callback) {
  const GoogleUrl requested_url(requested_url_string);
  const bool is_get = request_headers.method() == RequestHeaders::kGet;

  bool done = false;
  if (!(requested_url.is_valid() && IsWellFormedUrl(requested_url))) {
    LOG(ERROR) << "Bad URL, failing request: " << requested_url_string;
    response_headers->SetStatusAndReason(HttpStatus::kNotFound);
    callback->Done(false);
    done = true;
  } else {
    LOG(INFO) << "Proxying URL: " << requested_url.Spec();

    // Add X-Mod-Pagespeed header to all requests (should we only add this to
    // successful HTML requests like we do in Apache)?
    response_headers->Add(kModPagespeedHeader, kModPagespeedVersion);

    // Try to handle this as a .pagespeed. resource.
    if (resource_manager_->IsPagespeedResource(requested_url) && is_get) {
      ResourceFetch::Start(resource_manager_,
                           requested_url, request_headers,
                           response_headers, response_writer,
                           handler, fetch_latency_histogram_,
                           total_fetch_count_, callback);
      LOG(INFO) << "Serving URL as pagespeed resource";
    } else if (UrlAndPortMatchThisServer(requested_url)) {
      // Just respond with a 404 for now.
      response_headers->SetStatusAndReason(HttpStatus::kNotFound);
      callback->Done(false);
      done = true;
    } else {
      // Otherwise we proxy it (rewriting if it is HTML).
      LOG(INFO) << "Proxying URL normally";
      ProxyRequest(requested_url, request_headers,
                   response_headers, response_writer, handler, callback);
    }
  }

  return done;
}

ProxyInterface::OptionsBoolPair ProxyInterface::GetCustomOptions(
    const GoogleUrl& request_url, const RequestHeaders& request_headers,
    MessageHandler* handler) {
  RewriteOptions* options = resource_manager_->options();
  scoped_ptr<RewriteOptions> custom_options;
  scoped_ptr<RewriteOptions> domain_options(resource_manager_->url_namer()
      ->DecodeOptions(request_url, request_headers, handler));
  if (domain_options.get() != NULL) {
    custom_options.reset(resource_manager_->NewOptions());
    custom_options->Merge(*options, *domain_options.get());
    options = custom_options.get();
  }

  // Check query params & reqeust-headers for
  QueryParams params;
  params.Parse(request_url.Query());
  scoped_ptr<RewriteOptions> query_options(resource_manager_->NewOptions());
  switch (RewriteQuery::Scan(params, request_headers, query_options.get(),
                             handler)) {
    case RewriteQuery::kInvalid:
      return OptionsBoolPair(NULL, false);
      break;
    case RewriteQuery::kNoneFound:
      break;
    case RewriteQuery::kSuccess: {
      // Subtle memory management to handle deleting any domain_options
      // after the merge, and transferring ownership to the caller for
      // the new merged options.
      scoped_ptr<RewriteOptions> options_buffer(custom_options.release());
      custom_options.reset(resource_manager_->NewOptions());
      custom_options->Merge(*options, *query_options.get());
      break;
    }
  }
  return OptionsBoolPair(custom_options.release(), true);
}

void ProxyInterface::ProxyRequest(const GoogleUrl& request_url,
                                  const RequestHeaders& request_headers,
                                  ResponseHeaders* response_headers,
                                  Writer* response_writer,
                                  MessageHandler* handler,
                                  Callback* callback) {
  OptionsBoolPair custom_options_success = GetCustomOptions(
      request_url, request_headers, handler);
  if (!custom_options_success.second) {
    response_writer->Write("Invalid PageSpeed query-params/request headers",
                           handler);
    response_headers->SetStatusAndReason(HttpStatus::kMethodNotAllowed);
    callback->Done(false);
  }

  RequestHeaders custom_headers;
  custom_headers.CopyFrom(request_headers);

  // Update request_headers.
  // We deal with encodings. So strip the users Accept-Encoding headers.
  custom_headers.RemoveAll(HttpAttributes::kAcceptEncoding);
  // Note: We preserve the User-Agent and Cookies so that the origin servers
  // send us the correct HTML. We will need to consider this for caching HTML.

  // Start fetch and rewrite.  If GetCustomOptions found options for us,
  // the RewriteDriver created by StartNewProxyFetch will take ownership.
  proxy_fetch_factory_->StartNewProxyFetch(
      request_url.Spec().as_string(), custom_headers,
      custom_options_success.first, response_headers, response_writer,
      callback);
}

}  // namespace net_instaweb
