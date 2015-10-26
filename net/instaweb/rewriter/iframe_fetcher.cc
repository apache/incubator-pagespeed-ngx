/*
 * Copyright 2015 Google Inc.
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

#include "net/instaweb/rewriter/public/iframe_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"
#include "net/instaweb/rewriter/public/mobilize_rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

const char IframeFetcher::kIframeId[] = "psmob-iframe";

namespace {

bool MustProxyFetch(const GoogleUrl& gurl) {
  StringPiece path = gurl.PathSansQuery();
  return ((path == "/favicon.ico") || (path == "/robots.txt"));
}

}  // namespace

IframeFetcher::IframeFetcher(const RewriteOptions* options,
                             const UserAgentMatcher* matcher,
                             UrlAsyncFetcher* proxy_fetcher)
    : options_(options),
      user_agent_matcher_(matcher),
      proxy_fetcher_(proxy_fetcher) {
  // This normally gets called in the HtmlParse constructor, but since
  // that doesn't get called here we need to initialize it ourselves.
  HtmlKeywords::Init();
}

IframeFetcher::~IframeFetcher() {
}

void IframeFetcher::Fetch(const GoogleString& url,
                          MessageHandler* message_handler,
                          AsyncFetch* fetch) {
  // It's bad to serve some resources as an HTML iframe response,
  // so proxy them.
  GoogleUrl gurl(url);
  if (!gurl.IsWebValid() || MustProxyFetch(gurl)) {
    proxy_fetcher_->Fetch(url, message_handler, fetch);
    return;
  }

  GoogleString mapped_url, host_header;
  const DomainLawyer* lawyer = options_->domain_lawyer();
  bool mapped_to_self = false;
  if (lawyer->proxy_suffix().empty()) {
    bool is_proxy;
    if (!options_->domain_lawyer()->MapOrigin(
            url, &mapped_url, &host_header, &is_proxy)) {
      mapped_to_self = true;
    }
  } else {
    GoogleString origin_host;
    if (!lawyer->StripProxySuffix(gurl, &mapped_url, &origin_host)) {
      mapped_to_self = true;
    }
  }

  GoogleString escaped_url;
  HtmlKeywords::Escape(mapped_url, &escaped_url);

  fetch->response_headers()->Add(HttpAttributes::kContentType, "text/html");
  const char* user_agent = fetch->request_headers()->Lookup1(
      HttpAttributes::kUserAgent);

  if (mapped_to_self || (mapped_url == url)) {
    // We would cause a redirect loop or an iframe-loop if we allow this to
    // happen, so just fail.
    RespondWithError(escaped_url, fetch, message_handler);
  } else if (!options_->mob_iframe_disable() &&
             (user_agent != NULL) &&
             SupportedDevice(user_agent) &&
             MobilizeRewriteFilter::IsApplicableFor(options_, user_agent,
                                                    user_agent_matcher_) &&
             /* Note: we will turn off mobilize in noscript mode, where we
                want to redirect, too, since the iframe relies on a script
                TODO(morlovich): May be cleaner to have a "in noscript mode"
                    predicate instead.
                */
             options_->Enabled(RewriteOptions::kMobilize)) {
    RespondWithIframe(escaped_url, fetch, message_handler);
  } else {
    RespondWithRedirect(mapped_url, escaped_url, fetch, message_handler);
  }
  fetch->Done(true);
}

bool IframeFetcher::SupportedDevice(const char* user_agent) const {
  return user_agent_matcher_->SupportsMobilization(user_agent);
}

void IframeFetcher::RespondWithIframe(const GoogleString& escaped_url,
                                      AsyncFetch* fetch,
                                      MessageHandler* message_handler) {
  fetch->response_headers()->SetStatusAndReason(HttpStatus::kOK);

  // The viewport should be configured to match the viewport of the page being
  // iframed.
  GoogleString viewport;
  GoogleString scrolling_attribute;
  if (options_->mob_iframe_viewport() != "none") {
    GoogleString escaped_viewport_content;
    HtmlKeywords::Escape(options_->mob_iframe_viewport(),
                         &escaped_viewport_content);
    viewport = StrCat("<meta name=\"viewport\" content=\"",
                      escaped_viewport_content, "\">");

    const char* user_agent =
        fetch->request_headers()->Lookup1(HttpAttributes::kUserAgent);
    if (user_agent_matcher_->IsiOSUserAgent(user_agent)) {
      // Setting scrolling="no" on the iframe keeps the iframe from expanding to
      // be too large on iOS devices.
      scrolling_attribute = " scrolling=\"no\"";
    }
  }

  GoogleString head =
      StrCat("<head><link rel=\"canonical\" href=\"", escaped_url,
             "\"><meta charset=\"utf-8\">", viewport, "</head>");
  GoogleString body = StrCat(
      "<body class=\"mob-iframe\">"
      "<div id=\"psmob-iframe-container\">"
      "<div id=\"psmob-spacer\"></div>"
      "<iframe id=\"",
      kIframeId, "\" src=\"", escaped_url, "\"", scrolling_attribute,
      "></iframe></div></body>");

  // Avoid quirks-mode by specifying an HTML doctype.
  fetch->Write(StrCat("<!DOCTYPE html><html>", head, body, "</html>"),
               message_handler);
}

void IframeFetcher::RespondWithRedirect(const GoogleString& url,
                                        const GoogleString& escaped_url,
                                        AsyncFetch* fetch,
                                        MessageHandler* message_handler) {
  ResponseHeaders* response = fetch->response_headers();
  response->SetStatusAndReason(HttpStatus::kTemporaryRedirect);
  response->Add(HttpAttributes::kLocation, url);
  response->Add(DomainRewriteFilter::kStickyRedirectHeader, "on");
  response->Add(HttpAttributes::kCacheControl, "private, max-age=0");

  fetch->Write(StrCat("<html><body>Redirecting to ", escaped_url,
                      "</body></html>"), message_handler);
}

void IframeFetcher::RespondWithError(const GoogleString& escaped_url,
                                     AsyncFetch* fetch,
                                     MessageHandler* message_handler) {
  ResponseHeaders* response = fetch->response_headers();
  response->SetStatusAndReason(HttpStatus::kNotImplemented);
  fetch->Write(StrCat("<html><body>Error: redirecting to ", escaped_url,
                      " would cause a recirect loop.</body></html>"),
               message_handler);
}

}  // namespace net_instaweb
