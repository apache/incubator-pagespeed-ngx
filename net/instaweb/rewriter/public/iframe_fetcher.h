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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IFRAME_FETCHER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IFRAME_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;

// Fakes a fetch of a URL by synthesizing HTML with an empty head
// and a body that consists solely of the URL as an iframe src.
class IframeFetcher : public UrlAsyncFetcher {
 public:
  // Id of iframe element inserted by fetcher.
  static const char kIframeId[];

  IframeFetcher(const RewriteOptions* options, const UserAgentMatcher* matcher,
                UrlAsyncFetcher* proxy_fetcher);

  virtual ~IframeFetcher();
  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

 private:
  bool SupportedDevice(const char* user_agent) const;
  void RespondWithIframe(const GoogleString& escaped_url,
                         AsyncFetch* fetch,
                         MessageHandler* message_handler);
  void RespondWithRedirect(const GoogleString& url,
                           const GoogleString& escaped_url,
                           AsyncFetch* fetch,
                           MessageHandler* message_handler);
  void RespondWithError(const GoogleString& escaped_url,
                        AsyncFetch* fetch,
                        MessageHandler* message_handler);

  const RewriteOptions* options_;
  const UserAgentMatcher* user_agent_matcher_;
  UrlAsyncFetcher* proxy_fetcher_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IFRAME_FETCHER_H_
