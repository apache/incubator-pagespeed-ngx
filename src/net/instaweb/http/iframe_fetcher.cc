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
  fetch->Write("<!DOCTYPE html>", message_handler);
  fetch->Write("<html><head></head><body>", message_handler);

  // TODO(jmarantz): setting width to 1000x2000 appears to work OK for one
  // site and one screen, but we should probably synthesize this iframe in JS
  // using the actual physical screen size (since we don't know the site size
  // and cannot query it in JS even after onload due to same-origin policy).
  fetch->Write("<iframe style=\"border-width:0px;\" "
               "width=\"1000\" height=\"2000\" src=\"",
               message_handler);
  GoogleString escaped_url;
  HtmlKeywords::Escape(url, &escaped_url);
  fetch->Write(escaped_url, message_handler);
  fetch->Write("\"></iframe>", message_handler);
  fetch->Write("</body></html>", message_handler);
  fetch->Done(true);
}

}  // namespace net_instaweb
