/*
 * Copyright 2013 Google Inc.
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

// Author: morlovich@google.com (Maksim Orlovich)
//
// Contains UserAgentSensitiveTestFetcher, which appends the UA string as a
// query param before delegating to another fetcher. Meant for use in
// unit tests.

#include "net/instaweb/http/public/ua_sensitive_test_fetcher.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

UserAgentSensitiveTestFetcher::UserAgentSensitiveTestFetcher(
    UrlAsyncFetcher* base_fetcher)
    : base_fetcher_(base_fetcher) {}

void UserAgentSensitiveTestFetcher::Fetch(const GoogleString& url,
                                          MessageHandler* message_handler,
                                          AsyncFetch* fetch) {
  GoogleUrl parsed_url(url);
  CHECK(parsed_url.IsWebValid());
  if (!fetch->request_context()->IsSessionAuthorizedFetchOrigin(
          parsed_url.Origin().as_string())) {
    fetch->Done(false);
    return;
  }
  GoogleString ua_string;
  const char* specified_ua =
      fetch->request_headers()->Lookup1(HttpAttributes::kUserAgent);
  ua_string = (specified_ua == NULL ? "unknown" : specified_ua);

  scoped_ptr<GoogleUrl> with_ua(
      parsed_url.CopyAndAddEscapedQueryParam(
          "UA", GoogleUrl::Escape(ua_string)));
  base_fetcher_->Fetch(with_ua->Spec().as_string(), message_handler, fetch);
}

bool UserAgentSensitiveTestFetcher::SupportsHttps() const {
  return base_fetcher_->SupportsHttps();
}

}  // namespace net_instaweb
