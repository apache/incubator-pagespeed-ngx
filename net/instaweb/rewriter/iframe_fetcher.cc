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
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

IframeFetcher::IframeFetcher(const RewriteOptions* options,
                             const UserAgentMatcher* matcher)
    : options_(options),
      user_agent_matcher_(matcher) {
}

IframeFetcher::~IframeFetcher() {
}

void IframeFetcher::Fetch(const GoogleString& url,
                          MessageHandler* message_handler,
                          AsyncFetch* fetch) {
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
    GoogleUrl gurl(url);
    GoogleString origin_host;
    if (!gurl.IsWebValid() ||
        !lawyer->StripProxySuffix(gurl, &mapped_url, &origin_host)) {
      mapped_to_self = true;
    }
  }

  GoogleString escaped_url;
  HtmlKeywords::Escape(mapped_url, &escaped_url);

  fetch->response_headers()->Add(HttpAttributes::kContentType, "text/html");

  if (mapped_to_self || (mapped_url == url)) {
    // We would cause a redirect loop or an iframe-loop if we allow this to
    // happen, so just fail.
    RespondWithError(escaped_url, fetch, message_handler);
  } else if (SupportedDevice(fetch->request_headers()->Lookup1(
      HttpAttributes::kUserAgent))) {
    RespondWithIframe(escaped_url, fetch, message_handler);
  } else {
    RespondWithRedirect(mapped_url, escaped_url, fetch, message_handler);
  }
  fetch->Done(true);
}

bool IframeFetcher::SupportedDevice(const char* user_agent) const {
  return ((user_agent != NULL) &&
          user_agent_matcher_->SupportsMobilization(user_agent));
}

void IframeFetcher::RespondWithIframe(const GoogleString& escaped_url,
                                      AsyncFetch* fetch,
                                      MessageHandler* message_handler) {
  fetch->response_headers()->SetStatusAndReason(HttpStatus::kOK);

  // Avoid quirks-mode by specifying an HTML doctype.  This
  // simplifies the code below that tries to get the screen dimensions
  // via document.documentElement.
  fetch->Write("<!DOCTYPE html>", message_handler);
  fetch->Write("<html><head>"
               "<meta charset=\"utf-8\">"
               "</head><body class=\"mob-iframe\">",
               message_handler);

  // We want to size the iframe to fit the physical screen, so we
  // create the iframe in JS.  The code as I have it here seems to
  // work reasonably well even with orientation changes.  Any attempts
  // I've made to try to adjust the iframe size in response to
  // orientation changes seem to make the behavior bad (e.g. cutting
  // off half the screen).  So I'm inclined to leave it as is for now.
  GoogleString createIframe = StrCat(
      "<script>\n"
      "var docElt = document.documentElement;\n"
      "var iframe = document.createElement('iframe');\n"
      "iframe.id = 'psmob-iframe';\n"
      "iframe.src = '", escaped_url, "';\n"
      "iframe.frameborder = '0';\n"
      "iframe.width = '100%';\n"
      "iframe.height = '100%';\n"
      "document.body.appendChild(iframe);\n"
      "</script>");
  fetch->Write(createIframe, message_handler);
  fetch->Write("</body></html>", message_handler);
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
