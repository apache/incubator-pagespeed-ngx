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

#include "net/instaweb/http/public/iframe_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

IframeFetcher::IframeFetcher() {
}

IframeFetcher::~IframeFetcher() {
}

void IframeFetcher::Fetch(const GoogleString& url,
                          MessageHandler* message_handler,
                          AsyncFetch* fetch) {
  fetch->response_headers()->SetStatusAndReason(HttpStatus::kOK);
  fetch->response_headers()->Add(HttpAttributes::kContentType, "text/html");

  // Avoid quirks-mode by specifying and HTML doctype.  This
  // simplifies the code below that tries to get the screen dimensions
  // via document.documentElement.
  fetch->Write("<!DOCTYPE html>", message_handler);
  fetch->Write("<html><head></head><body>", message_handler);

  // We want to size the iframe to fit the physical screen, so we
  // create the iframe in JS.  The code as I have it here seems to
  // work reasonably well even with orientation changes.  Any attempts
  // I've made to try to adjust the iframe size in response to
  // orientation changes seem to make the behavior bad (e.g. cutting
  // off half the screen).  So I'm inclined to leave it as is for now.
  GoogleString escaped_url;
  HtmlKeywords::Escape(url, &escaped_url);
  GoogleString createIframe = StrCat(
      "<script>\n"
      "var docElt = document.documentElement;\n"
      "var iframe = document.createElement('iframe');\n"
      "iframe.style = \"border-width:0px;\";\n"
      "iframe.src = \"", escaped_url, "\";\n"
      "iframe.width = docElt.clientWidth;\n"
      "iframe.height = docElt.clientHeight;\n"
      "document.body.appendChild(iframe);\n"
      "</script>");
  fetch->Write(createIframe, message_handler);
  fetch->Write("</body></html>", message_handler);
  fetch->Done(true);
}

}  // namespace net_instaweb
